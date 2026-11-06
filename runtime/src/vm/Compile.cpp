// HIR -> bytecode (src/vm/Bytecode.hpp). Every local becomes a numbered slot, every function, label, struct and extern a number, so the
// interpreter does no name lookup.
//
// The semantics were matched to the Nina-written reference VM (removed 2026-10-28, see core/docs/language/vm-proposal.md): a name is
// read, fitted, boxed and bound exactly where that VM did it, so the two agreed on every result and every trap. "The oracle" in the
// comments below means that reference behaviour; runtime/tests/vm_vs_native.sh now holds the VM to the native build.
//
// The one idea to read it by: compiling an expression emits code that leaves exactly one value on the operand stack, and compiling a
// statement emits code that leaves the stack as it found it. CompileProgram (at the bottom) works in two passes: first every
// declaration gets its number (so a call may name a later function), then every body is compiled. A jump forward to a pc not known
// yet is emitted with a placeholder and patched once the target is reached.
//
// Names used throughout: `c` is the Compiler (the program and the function being built), `p` the Pool alone, `e` the expression and
// `s` the statement being compiled.
#include "polyir/Std.hpp"
#include "vm/Bytecode.hpp"

namespace polyir::vm {

  namespace {

    // ── the program being built ──

    // The Program so far, and the lookups that give each constant, shape, global and extern its number (each is added once).
    struct Pool {
      const Module* module = nullptr;
      Program program;
      Map<String, int32_t> constantIndex;
      Map<String, int32_t> shapeIndex;
      Map<String, int32_t> globalIndex;
      Map<String, int32_t> externIndex;
      int32_t globalsBound = 0;  // how many globals may be read: they are bound one by one, so an initializer sees only the earlier ones
    };

    // A label of the function being compiled: its parameters' slots, its pc once it is reached, and the jumps waiting for that pc.
    struct LabelInfo {
      Vector<int32_t> slots;
      int32_t pc = -1;
      Vector<int32_t> pending;
    };

    struct Binding {
      String name;
      int32_t slot = 0;
    };

    // One Code being built: its scopes (newest last, a lookup walks back for shadowing), the statement lists being compiled (innermost
    // last: a goto looks for its label from the innermost outward, as the oracle did), which slots hold a box rather than a
    // value, which may hold the unbound marker, and the labels of each list (a sibling list may reuse a name).
    struct FunctionBuild {
      Code code;
      Vector<Vector<Binding>> scopes;
      Vector<const StmtList*> lists;
      Vector<bool> boxed;
      Map<int32_t, String> checked;  // a label parameter (or a closure capture): read and written through LoadBound/StoreBound
      Map<const StmtList*, Map<String, LabelInfo>> labels;
    };

    // What every compile function takes: the whole program being built, and the one function being built in it.
    struct Compiler {
      Pool& pool;
      FunctionBuild& function;
    };

    // What a function or a closure body compiles from.
    struct Signature {
      String name;
      Vector<String> captures;
      Vector<bool> captureBoxed;  // per capture: a box shared with the enclosing function, or a plain value (a `let`, a label parameter)
      Vector<String> params;
      const StmtList* body = nullptr;
      bool isCoroutine = false;
      bool returnsVoid = false;
    };

    void compileExpr(Compiler& c, const Expr& e);

    void compileStmts(Compiler& c, const StmtList& stmts);

    Code compileBody(Pool& pool, const Signature& signature);

    // ── types ──
    bool isUnsignedType(const TypePtr& t) {
      return t && t->kind == TypeKind::UInt;
    }

    bool isVoidType(const TypePtr& t) {
      return !t || t->kind == TypeKind::Void;
    }

    // An integer's bits, 64 for everything else (a float constant is carried at 64 too), as the oracle measured widths.
    int widthOf(const TypePtr& t) {
      return t && IsInteger(*t) ? t->bits : 64;
    }

    // The facts the operators read without looking at the type again: its kind, its width and its signedness.
    void describeNumber(Shape& s, const Type& t) {
      s.kind = t.kind;
      s.isFloat = t.kind == TypeKind::Float;
      s.isSigned = t.kind != TypeKind::UInt;
      s.bits = IsNumber(t) ? t.bits : 64;
    }

    Shape shapeOf(const TypePtr& t) {
      Shape s;
      s.type = t;

      if (t) {
        describeNumber(s, *t);
      }

      return s;
    }

    int32_t shape(Pool& p, const TypePtr& t) {
      const String key = TypeText(t);
      const auto found = p.shapeIndex.find(key);

      if (found != p.shapeIndex.end()) {
        return found->second;
      }

      const auto at = static_cast<int32_t>(p.program.shapes.size());
      p.program.shapes.push_back(shapeOf(t));
      p.shapeIndex[key] = at;

      return at;
    }

    // ── constants ──
    int32_t constant(Pool& p, const String& key, const Value& v) {
      const auto found = p.constantIndex.find(key);

      if (found != p.constantIndex.end()) {
        return found->second;
      }

      const auto at = static_cast<int32_t>(p.program.constants.size());
      p.program.constants.push_back(v);
      p.constantIndex[key] = at;

      return at;
    }

    int32_t stringConst(Pool& p, const String& text) {
      return constant(p, "s:" + text, StrNoIdentity(text));
    }

    int32_t intConst(Pool& p, int64_t v, int bits, bool isSigned) {
      const String key = "i" + to_string(bits) + (isSigned ? "s:" : "u:") + to_string(v);

      return constant(p, key, IntValue(v, bits, isSigned));
    }

    // Keyed by the bit pattern, so -0.0 and 0.0 (and the NaNs) stay apart.
    int32_t floatConst(Pool& p, double v, int bits) {
      uint64_t raw = 0;
      memcpy(&raw, &v, sizeof raw);

      return constant(p, "f" + to_string(bits) + ":" + to_string(raw), FloatValue(v, bits));
    }

    int32_t boolConst(Pool& p, bool b) {
      return constant(p, b ? "b:1" : "b:0", BoolValue(b));
    }

    int32_t defaultConst(Pool& p, const TypePtr& t) {
      if (!t) {
        return constant(p, "void", VoidValue());
      }

      return constant(p, "d:" + TypeText(t), DefaultOf(t));
    }

    // ── emitting ──
    int32_t here(const FunctionBuild& function) {
      return static_cast<int32_t>(function.code.instructions.size());
    }

    int32_t emit(FunctionBuild& function, Op op, int32_t a = 0, int32_t b = 0, int32_t c = 0) {
      function.code.instructions.push_back(Instruction{op, a, b, c});

      return here(function) - 1;
    }

    void patch(FunctionBuild& function, int32_t at, int32_t target) {
      function.code.instructions[static_cast<size_t>(at)].a = target;
    }

    void emitTrap(Compiler& c, const String& message) {
      emit(c.function, Op::Trap, stringConst(c.pool, message));
    }

    // ── slots and scopes ──
    // The oracle kept a parameter, a `var` and a loop counter in a heap box (a closure shares it, an assignment is seen through it).
    // It kept everything else (a `let`, a label parameter, a caught code) as a plain value. The boxes take heap ids, so the VM
    // follows it exactly.
    int32_t allocSlot(FunctionBuild& function, bool boxed) {
      const int32_t slot = function.code.slots++;
      function.boxed.push_back(boxed);

      return slot;
    }

    void bind(FunctionBuild& function, const String& name, int32_t slot) {
      function.scopes.back().push_back(Binding{name, slot});
    }

    int32_t declare(FunctionBuild& function, const String& name, bool boxed) {
      const int32_t slot = allocSlot(function, boxed);
      bind(function, name, slot);

      return slot;
    }

    // The slot `name` is bound to, newest binding first (shadowing); -1 when the function has no such name.
    int32_t lookup(const FunctionBuild& function, const String& name) {
      for (auto scope = function.scopes.rbegin(); scope != function.scopes.rend(); ++scope) {
        for (auto b = scope->rbegin(); b != scope->rend(); ++b) {
          if (b->name == name) {
            return b->slot;
          }
        }
      }

      return -1;
    }

    bool isBoxed(const FunctionBuild& function, int32_t slot) {
      return function.boxed[static_cast<size_t>(slot)];
    }

    // A fresh binding: the value goes into the slot, and into a fresh box when the slot is a boxed one.
    void storeNew(FunctionBuild& function, int32_t slot) {
      emit(function, Op::Store, slot);

      if (isBoxed(function, slot)) {
        emit(function, Op::BoxSlot, slot);
      }
    }

    // A slot that may hold the unbound marker (a label parameter nothing jumped to): it is read and written with a check.
    bool isCheckedSlot(const FunctionBuild& function, int32_t slot) {
      return function.checked.find(slot) != function.checked.end();
    }

    // LoadBound or StoreBound: the access with its check, carrying the name the trap says and whether the slot holds a box.
    void emitCheckedAccess(Compiler& c, int32_t slot, Op op) {
      const String& name = c.function.checked.at(slot);
      emit(c.function, op, slot, stringConst(c.pool, name), isBoxed(c.function, slot) ? 1 : 0);
    }

    void assignSlot(Compiler& c, int32_t slot) {
      if (isCheckedSlot(c.function, slot)) {
        emitCheckedAccess(c, slot, Op::StoreBound);
      } else {
        emit(c.function, isBoxed(c.function, slot) ? Op::StoreBoxed : Op::Store, slot);
      }
    }

    void loadSlot(Compiler& c, int32_t slot) {
      if (isCheckedSlot(c.function, slot)) {
        emitCheckedAccess(c, slot, Op::LoadBound);
      } else {
        emit(c.function, isBoxed(c.function, slot) ? Op::LoadBoxed : Op::Load, slot);
      }
    }

    void openScope(FunctionBuild& function) {
      function.scopes.emplace_back();
    }

    void closeScope(FunctionBuild& function) {
      function.scopes.pop_back();
    }

    // ── names: locals first, then globals, else the oracle's "not bound" trap ──
    int32_t globalOf(const Pool& p, const String& name) {
      const auto found = p.globalIndex.find(name);

      if (found == p.globalIndex.end() || found->second >= p.globalsBound) {
        return -1;
      }

      return found->second;
    }

    void loadGlobal(Compiler& c, const String& name) {
      const int32_t global = globalOf(c.pool, name);

      if (global >= 0) {
        emit(c.function, Op::LoadGlobal, global);
      } else {
        emitTrap(c, "'" + name + "' is not bound");
      }
    }

    void loadName(Compiler& c, const String& name) {
      const int32_t slot = lookup(c.function, name);

      if (slot >= 0) {
        loadSlot(c, slot);
      } else {
        loadGlobal(c, name);
      }
    }

    void storeGlobal(Compiler& c, const String& name) {
      const int32_t global = globalOf(c.pool, name);

      if (global >= 0) {
        emit(c.function, Op::StoreGlobal, global);
      } else {
        emitTrap(c, "'" + name + "' is not bound");
      }
    }

    void storeName(Compiler& c, const String& name) {
      const int32_t slot = lookup(c.function, name);

      if (slot >= 0) {
        assignSlot(c, slot);
      } else {
        storeGlobal(c, name);
      }
    }

    // ── the pre-scan of labels: parameter slots now, pcs when reached ──
    void scanLabel(FunctionBuild& function, const StmtList& list, const Stmt& s) {
      LabelInfo info;

      for (const String& p : s.parameterNames) {
        info.slots.push_back(allocSlot(function, false));
        function.checked[info.slots.back()] = p;
      }

      function.labels[&list][s.name] = move(info);
    }

    // Every list's labels are known before any goto is compiled (a closure body is its own function, with its own lists).
    void scanLabels(FunctionBuild& function, const StmtList& stmts) {
      for (const StmtPtr& s : stmts) {
        if (!s) {
          continue;
        }

        if (s->kind == StmtKind::Label) {
          scanLabel(function, stmts, *s);
        }

        scanLabels(function, s->body);
        scanLabels(function, s->elseBody);
      }
    }

    // The label a goto reaches: the nearest enclosing list that declares the name (a list beside it is out of reach); null when none does.
    LabelInfo* labelNamed(FunctionBuild& function, const String& name) {
      for (auto list = function.lists.rbegin(); list != function.lists.rend(); ++list) {
        Map<String, LabelInfo>& labels = function.labels[*list];
        const auto found = labels.find(name);

        if (found != labels.end()) {
          return &found->second;
        }
      }

      return nullptr;
    }

    // ── expressions ──
    const Expr& operand(const Expr& e, size_t i) {
      if (i >= e.operands.size() || !e.operands[i]) {
        throw RuntimeError("malformed HIR: an operand is missing");
      }

      return *e.operands[i];
    }

    void compileAll(Compiler& c, const Vector<ExprPtr>& es, size_t from = 0) {
      for (size_t i = from; i < es.size(); i++) {
        if (es[i]) {
          compileExpr(c, *es[i]);
        }
      }
    }

    void fit(Compiler& c, const TypePtr& t) {
      if (t) {
        emit(c.function, Op::Fit, shape(c.pool, t));
      }
    }

    // An Int constant keeps its declared width and signedness.
    void compileIntConst(Compiler& c, const Expr& e) {
      emit(c.function, Op::PushConst, intConst(c.pool, e.intValue, widthOf(e.type), !isUnsignedType(e.type)));
    }

    void compileFloatConst(Compiler& c, const Expr& e) {
      emit(c.function, Op::PushConst, floatConst(c.pool, e.floatValue, widthOf(e.type)));
    }

    void compileBoolConst(Compiler& c, const Expr& e) {
      emit(c.function, Op::PushConst, boolConst(c.pool, e.boolValue));
    }

    void compileStringConst(Compiler& c, const Expr& e) {
      emit(c.function, Op::PushString, stringConst(c.pool, e.text));
    }

    void compileConst(Compiler& c, const Expr& e) {
      switch (e.kind) {
        case ExprKind::ConstInt:
          return compileIntConst(c, e);
        case ExprKind::ConstFloat:
          return compileFloatConst(c, e);
        case ExprKind::ConstBool:
          return compileBoolConst(c, e);
        default:
          return compileStringConst(c, e);
      }
    }

    // Both sides passed the test: `&&` is true, `||` false; the jumps that decided early land on the opposite constant.
    void joinLogical(Compiler& c, bool isAnd, int32_t first, int32_t second) {
      emit(c.function, Op::PushConst, boolConst(c.pool, isAnd));
      const int32_t skip = emit(c.function, Op::Jump);

      patch(c.function, first, here(c.function));
      patch(c.function, second, here(c.function));
      emit(c.function, Op::PushConst, boolConst(c.pool, !isAnd));
      patch(c.function, skip, here(c.function));
    }

    // `a && b` / `a || b`: the right side runs only when the left does not decide; the result is a Bool either way.
    void compileLogical(Compiler& c, const Expr& e) {
      const bool isAnd = e.binaryOp == BinOp::LogAnd;
      const Op decided = isAnd ? Op::JumpIfFalse : Op::JumpIfTrue;

      compileExpr(c, operand(e, 0));
      const int32_t first = emit(c.function, decided);
      compileExpr(c, operand(e, 1));
      joinLogical(c, isAnd, first, emit(c.function, decided));
    }

    // Both sides evaluated, left first; the operator works at the left side's type and wraps to the result's.
    void compileBinary(Compiler& c, const Expr& e) {
      if (IsLogical(e.binaryOp)) {
        return compileLogical(c, e);
      }

      compileExpr(c, operand(e, 0));
      compileExpr(c, operand(e, 1));
      emit(c.function, Op::Binary, static_cast<int32_t>(e.binaryOp), shape(c.pool, operand(e, 0).type), shape(c.pool, e.type));
    }

    void compileUnary(Compiler& c, const Expr& e) {
      compileExpr(c, operand(e, 0));
      emit(c.function, Op::Unary, static_cast<int32_t>(e.unaryOp), shape(c.pool, operand(e, 0).type), shape(c.pool, e.type));
    }

    void compileConvert(Compiler& c, const Expr& e) {
      compileExpr(c, operand(e, 0));
      emit(c.function, Op::Convert, shape(c.pool, e.type));
    }

    // A declared function's index, or -1.
    int32_t functionNumber(const Pool& p, const String& name) {
      const auto found = p.program.functionIndex.find(name);

      return found == p.program.functionIndex.end() ? -1 : found->second;
    }

    // An extern's index, or -1.
    int32_t externNumber(const Pool& p, const String& name) {
      const auto found = p.externIndex.find(name);

      return found == p.externIndex.end() ? -1 : found->second;
    }

    // A name that is either a declared function or an extern: `forFunction` with the function's index, else `forExtern` with the
    // extern's. A call passes its argument count; a reference passes 0.
    void emitByName(Compiler& c, const String& name, Op forFunction, Op forExtern, int32_t argumentCount) {
      const int32_t calleeFunction = functionNumber(c.pool, name);
      const int32_t calleeExtern = externNumber(c.pool, name);

      if (calleeFunction >= 0) {
        emit(c.function, forFunction, calleeFunction, argumentCount);
      } else if (calleeExtern >= 0) {
        emit(c.function, forExtern, calleeExtern, argumentCount);
      } else {
        throw RuntimeError("the VM has no function '" + name + "'");
      }
    }

    void compileCall(Compiler& c, const Expr& e) {
      compileAll(c, e.operands);
      emitByName(c, e.name, Op::Call, Op::CallExtern, static_cast<int32_t>(e.operands.size()));
    }

    void compileCallIndirect(Compiler& c, const Expr& e) {
      compileAll(c, e.operands);
      emit(c.function, Op::CallIndirect, 0, static_cast<int32_t>(e.operands.size()) - 1);
    }

    // Debug markers and releases: no-ops whose operands are not evaluated (a `FreeBox` receiver is a name, not a read).
    bool isMarker(Builtin op) {
      return op == Builtin::Loc || op == Builtin::Var || op == Builtin::Free || op == Builtin::FreeBox;
    }

    // `v.yielded()` inside a coroutine: the value is yielded, and the answer the next resume brings is the expression's value.
    void compileYielded(Compiler& c, const Expr& e) {
      compileExpr(c, operand(e, 0));
      emit(c.function, Op::Yielded);
    }

    // The receiver first, then the arguments; the receiver's type picks the shape the builtin works on.
    void compileBuiltinCall(Compiler& c, const Expr& e) {
      compileExpr(c, operand(e, 0));
      compileAll(c, e.operands, 1);
      const auto argumentCount = static_cast<int32_t>(e.operands.size()) - 1;
      emit(c.function, Op::Builtin, static_cast<int32_t>(e.builtin), argumentCount, shape(c.pool, operand(e, 0).type));
    }

    void compileBuiltin(Compiler& c, const Expr& e) {
      if (isMarker(e.builtin)) {
        emit(c.function, Op::PushVoid);
      } else if (e.builtin == Builtin::CoroutineYield) {
        compileYielded(c, e);
      } else {
        compileBuiltinCall(c, e);
      }
    }

    int32_t structNamed(Pool& p, const String& name) {
      const auto found = p.program.structIndex.find(name);

      if (found == p.program.structIndex.end()) {
        throw RuntimeError("the VM has no struct '" + name + "'");
      }

      return found->second;
    }

    // The field values are all evaluated (into hidden slots) before the struct is allocated, as the oracle did it. A nested
    // literal therefore takes the earlier heap id. Then the struct of defaults has its fields filled in the order written.
    int32_t keepValue(Compiler& c, const Expr& value) {
      compileExpr(c, value);
      const int32_t slot = allocSlot(c.function, false);
      emit(c.function, Op::Store, slot);

      return slot;
    }

    // The field the i-th operand fills: the one named, else the i-th (a positional literal).
    int32_t fieldIndexOf(const Expr& e, size_t i) {
      return i < e.fieldIndices.size() ? e.fieldIndices[i] : static_cast<int32_t>(i);
    }

    void fillField(Compiler& c, int32_t slot, int32_t field) {
      emit(c.function, Op::Load, slot);
      emit(c.function, Op::SetField, field);
    }

    void compileStructLit(Compiler& c, const Expr& e) {
      Vector<int32_t> kept;

      for (size_t i = 0; i < e.operands.size(); i++) {
        kept.push_back(keepValue(c, operand(e, i)));
      }

      emit(c.function, Op::NewStruct, structNamed(c.pool, e.name));

      for (size_t i = 0; i < kept.size(); i++) {
        fillField(c, kept[i], fieldIndexOf(e, i));
      }
    }

    void compileFieldGet(Compiler& c, const Expr& e) {
      compileExpr(c, operand(e, 0));
      emit(c.function, Op::GetField, e.fieldIndex);
    }

    void compileIndexGet(Compiler& c, const Expr& e) {
      compileAll(c, e.operands);
      const auto indexCount = static_cast<int32_t>(e.operands.size()) - 1;
      emit(c.function, Op::GetIndex, indexCount, shape(c.pool, operand(e, 0).type), shape(c.pool, e.type));
    }

    void compileNewContainer(Compiler& c, const Expr& e) {
      compileAll(c, e.operands);
      emit(c.function, Op::NewContainer, shape(c.pool, e.type), static_cast<int32_t>(e.operands.size()));
    }

    // An extern taken as a value is a function value too (in the oracle, a function value of its name): calling it reaches the extern.
    void compileFuncRef(Compiler& c, const Expr& e) {
      emitByName(c, e.name, Op::FuncRef, Op::ExternRef, 0);
    }

    // The captures the enclosing function binds, each a box or a plain value as the enclosing slot is. A name it does not bind is read
    // as a global inside the body, as the oracle skipped it.
    void localCaptures(const FunctionBuild& function, const Vector<String>& names, Signature& signature) {
      for (const String& name : names) {
        const int32_t slot = lookup(function, name);

        if (slot < 0) {
          continue;
        }

        signature.captures.push_back(name);
        signature.captureBoxed.push_back(isBoxed(function, slot));
      }
    }

    bool closureReturnsVoid(const TypePtr& t) {
      return !t || !IsCallable(*t) || isVoidType(t->element);
    }

    Signature closureSignature(const FunctionBuild& parent, const Expr& e, int32_t index) {
      Signature signature;
      signature.name = parent.code.name + "$closure" + to_string(index);
      localCaptures(parent, e.captures, signature);

      signature.params = e.parameterNames;
      signature.body = &e.closureBody;
      signature.returnsVoid = closureReturnsVoid(e.type);

      return signature;
    }

    // The body becomes a Code appended after every declared function; its captured boxes (or values) are pushed in capture order.
    // This closure's index is reserved first: closures nested in the body are appended while it compiles and must get later
    // numbers. The body is compiled into a local because compiling it may grow `functions`, which would move the reserved entry.
    void compileClosure(Compiler& c, const Expr& e) {
      const auto index = static_cast<int32_t>(c.pool.program.functions.size());
      const Signature signature = closureSignature(c.function, e, index);

      for (const String& name : signature.captures) {
        emit(c.function, Op::Load, lookup(c.function, name));
      }

      c.pool.program.functions.emplace_back();
      Code body = compileBody(c.pool, signature);
      c.pool.program.functions[static_cast<size_t>(index)] = move(body);
      emit(c.function, Op::MakeClosure, index, static_cast<int32_t>(signature.captures.size()));
    }

    void compileWrapped(Compiler& c, const Expr& e) {
      compileExpr(c, operand(e, 0));

      if (e.kind == ExprKind::VariantLit) {
        emit(c.function, Op::Variant, e.fieldIndex);
      } else {
        emit(c.function, Op::Either, e.boolValue ? 1 : 0);
      }
    }

    void compileVecLit(Compiler& c, const Expr& e) {
      compileAll(c, e.operands);
      emit(c.function, Op::NewArray, static_cast<int32_t>(e.operands.size()));
    }

    // An expression the VM has no code for traps when reached, naming the construct when the front end kept its text.
    void compileUnsupported(Compiler& c, const Expr& e) {
      if (e.kind == ExprKind::Unsupported) {
        return emitTrap(c, "unsupported construct: " + e.text);
      }

      emitTrap(c, "unsupported construct");
    }

    // Array literals, new containers, closures and the variant/either wrappers; any other kind is unsupported.
    void compileLiteralOrClosure(Compiler& c, const Expr& e) {
      switch (e.kind) {
        case ExprKind::VecLit:
          return compileVecLit(c, e);
        case ExprKind::NewContainer:
          return compileNewContainer(c, e);
        case ExprKind::ClosureLit:
          return compileClosure(c, e);
        case ExprKind::VariantLit: case ExprKind::EitherLit:
          return compileWrapped(c, e);
        default:
          return compileUnsupported(c, e);
      }
    }

    void compileCallExpr(Compiler& c, const Expr& e) {
      if (e.kind == ExprKind::Call) {
        return compileCall(c, e);
      }

      if (e.kind == ExprKind::CallIndirect) {
        return compileCallIndirect(c, e);
      }

      compileBuiltin(c, e);
    }

    // Calls, struct literals, and reads of a field or an index; the other kinds go on to compileLiteralOrClosure.
    void compileCallOrObject(Compiler& c, const Expr& e) {
      switch (e.kind) {
        case ExprKind::Call: case ExprKind::CallIndirect: case ExprKind::CallBuiltin:
          return compileCallExpr(c, e);
        case ExprKind::StructLit:
          return compileStructLit(c, e);
        case ExprKind::FieldGet:
          return compileFieldGet(c, e);
        case ExprKind::IndexGet:
          return compileIndexGet(c, e);
        default:
          return compileLiteralOrClosure(c, e);
      }
    }

    // A name: a global's box, a function, or a local slot (a `LoadSlot` reads a local too).
    void compileName(Compiler& c, const Expr& e) {
      if (e.kind == ExprKind::Global) {
        return loadGlobal(c, e.name);
      }

      if (e.kind == ExprKind::FuncRef) {
        return compileFuncRef(c, e);
      }

      loadName(c, e.name);
    }

    void compileOperator(Compiler& c, const Expr& e) {
      if (e.kind == ExprKind::Binary) {
        return compileBinary(c, e);
      }

      if (e.kind == ExprKind::Unary) {
        return compileUnary(c, e);
      }

      compileConvert(c, e);
    }

    // The dispatch on the expression's kind runs in steps, each `default:` handing on the kinds it does not take:
    // compileExpr -> compileCallOrObject -> compileLiteralOrClosure -> compileUnsupported.
    void compileExpr(Compiler& c, const Expr& e) {
      switch (e.kind) {
        case ExprKind::ConstInt: case ExprKind::ConstFloat: case ExprKind::ConstBool: case ExprKind::ConstString:
          return compileConst(c, e);
        case ExprKind::Local: case ExprKind::LoadSlot: case ExprKind::Global: case ExprKind::FuncRef:
          return compileName(c, e);
        case ExprKind::Binary: case ExprKind::Unary: case ExprKind::Cast: case ExprKind::Coerce:
          return compileOperator(c, e);
        default:
          return compileCallOrObject(c, e);
      }
    }

    // ── statements ──
    void compileStmt(Compiler& c, const Stmt& s);

    void compileLet(Compiler& c, const Stmt& s) {
      if (s.value) {
        compileExpr(c, *s.value);
        fit(c, s.type);
      } else {
        emit(c.function, Op::PushConst, defaultConst(c.pool, s.type));
      }

      storeNew(c.function, declare(c.function, s.name, s.kind == StmtKind::Var));  // bound after the value: `let x = x + 1` reads the outer x
    }

    // The value is evaluated before the object, in the oracle's order, so it waits in a hidden slot.
    void assignField(Compiler& c, const Expr& target) {
      const int32_t kept = allocSlot(c.function, false);
      emit(c.function, Op::Store, kept);

      compileExpr(c, operand(target, 0));
      emit(c.function, Op::Load, kept);
      emit(c.function, Op::SetField, target.fieldIndex);
      emit(c.function, Op::Pop);
    }

    void assignIndex(Compiler& c, const Expr& target) {
      const int32_t kept = allocSlot(c.function, false);
      emit(c.function, Op::Store, kept);
      compileAll(c, target.operands);
      emit(c.function, Op::Load, kept);
      emit(c.function, Op::SetIndex, static_cast<int32_t>(target.operands.size()) - 1, shape(c.pool, operand(target, 0).type));
    }

    void storeTarget(Compiler& c, const Expr& target) {
      switch (target.kind) {
        case ExprKind::Local: case ExprKind::LoadSlot:
          return storeName(c, target.name);
        case ExprKind::Global:
          return storeGlobal(c, target.name);
        case ExprKind::FieldGet:
          return assignField(c, target);
        case ExprKind::IndexGet:
          return assignIndex(c, target);
        default:
          return emitTrap(c, "assigning to this kind of target");
      }
    }

    void compileAssign(Compiler& c, const Stmt& s) {
      if (!s.value || !s.target) {
        throw RuntimeError("malformed HIR: an assignment without a target or a value");
      }

      compileExpr(c, *s.value);
      fit(c, s.target->type);
      storeTarget(c, *s.target);
    }

    void compileIf(Compiler& c, const Stmt& s) {
      if (s.value) {
        compileExpr(c, *s.value);
      }

      const int32_t toElse = emit(c.function, Op::JumpIfFalse);
      compileStmts(c, s.body);
      const int32_t toEnd = emit(c.function, Op::Jump);

      patch(c.function, toElse, here(c.function));
      compileStmts(c, s.elseBody);
      patch(c.function, toEnd, here(c.function));
    }

    // The counters are bound one by one (a later initializer sees the earlier ones), each in one box for the whole loop.
    void loopCounters(Compiler& c, const Stmt& s) {
      for (size_t i = 0; i < s.names.size(); i++) {
        if (i < s.initializers.size() && s.initializers[i]) {
          compileExpr(c, *s.initializers[i]);
        } else {
          emit(c.function, Op::PushVoid);
        }

        storeNew(c.function, declare(c.function, s.names[i], true));
      }
    }

    // The steps assign the counters in order (with no fit, as the oracle assigned them); a failed step ends the loop.
    void loopSteps(Compiler& c, const Stmt& s) {
      for (size_t i = 0; i < s.steps.size() && i < s.names.size(); i++) {
        if (!s.steps[i]) {
          continue;
        }

        compileExpr(c, *s.steps[i]);
        storeName(c, s.names[i]);
      }
    }

    // Test, body, steps, back to the test: the loop ends when the test fails.
    void loopRounds(Compiler& c, const Stmt& s) {
      const int32_t test = here(c.function);

      if (s.value) {
        compileExpr(c, *s.value);
      }

      const int32_t exit = emit(c.function, Op::JumpIfFalse);
      compileStmts(c, s.body);
      loopSteps(c, s);
      emit(c.function, Op::Jump, test);
      patch(c.function, exit, here(c.function));
    }

    void compileFor(Compiler& c, const Stmt& s) {
      openScope(c.function);
      loopCounters(c, s);
      loopRounds(c, s);
      closeScope(c.function);
    }

    // A void return carries nothing worth evaluating (the printer writes a placeholder expression there).
    void compileReturn(Compiler& c, const Stmt& s) {
      if (!s.value || isVoidType(s.value->type)) {
        emit(c.function, Op::ReturnVoid);
        return;
      }

      compileExpr(c, *s.value);
      emit(c.function, Op::Return);
    }

    // A named catch: the code lands in the name's slot (a plain value, as the oracle bound it) and the body runs.
    void compileCatch(Compiler& c, const Stmt& s, Handler& h) {
      openScope(c.function);
      h.slot = declare(c.function, s.name, false);
      compileStmts(c, s.elseBody);
      closeScope(c.function);
    }

    // A cleanup (no name): the code waits in a hidden slot while the body runs, then the throw goes on.
    void compileCleanup(Compiler& c, const Stmt& s, Handler& h) {
      h.slot = allocSlot(c.function, false);
      compileStmts(c, s.elseBody);
      emit(c.function, Op::Load, h.slot);
      emit(c.function, Op::Throw);
    }

    // The handler sits after the body (which jumps over it). Handler ranges nest, so Run.cpp's handlerFor picks the narrowest one
    // covering the pc, which is the innermost `try`.
    void compileHandler(Compiler& c, const Stmt& s, Handler h) {
      const int32_t skip = emit(c.function, Op::Jump);
      h.catchPc = here(c.function);

      if (s.name.empty()) {
        compileCleanup(c, s, h);
      } else {
        compileCatch(c, s, h);
      }

      c.function.code.handlers.push_back(h);
      patch(c.function, skip, here(c.function));
    }

    void compileTry(Compiler& c, const Stmt& s) {
      Handler h;
      h.from = here(c.function);
      compileStmts(c, s.body);
      h.to = here(c.function);
      compileHandler(c, s, h);
    }

    // Falling through into a label (no goto) binds none of its parameters, as the oracle treated the label as a no-op. The marker goes
    // into their slots on that path only, so a read traps "'p' is not bound" instead of giving an old value. A goto lands past it.
    void unbindParams(Compiler& c, const LabelInfo& info) {
      for (const int32_t slot : info.slots) {
        emit(c.function, Op::PushConst, constant(c.pool, "unbound", UnboundMarker()));
        emit(c.function, Op::Store, slot);
      }
    }

    // Reaching a label binds its parameters for the rest of the list; the jumps that came first learn its pc now.
    void compileLabel(Compiler& c, const Stmt& s) {
      LabelInfo& info = c.function.labels[c.function.lists.back()][s.name];
      unbindParams(c, info);
      info.pc = here(c.function);

      for (const int32_t at : info.pending) {
        patch(c.function, at, info.pc);
      }

      for (size_t i = 0; i < s.parameterNames.size() && i < info.slots.size(); i++) {
        bind(c.function, s.parameterNames[i], info.slots[i]);
      }
    }

    void emitJump(FunctionBuild& function, LabelInfo& info) {
      const int32_t at = emit(function, Op::Jump, info.pc);

      if (info.pc < 0) {
        info.pending.push_back(at);
      }
    }

    // The arguments are all evaluated first, then stored (last first, off the stack), so `goto L(b, a)` swaps correctly.
    void storeLabelArgs(Compiler& c, const LabelInfo& info, size_t given) {
      for (size_t i = given; i > info.slots.size(); i--) {
        emit(c.function, Op::Pop);
      }

      for (size_t i = min(given, info.slots.size()); i-- > 0;) {
        storeNew(c.function, info.slots[i]);
      }
    }

    // A goto to no label of the enclosing lists traps only when it runs (after its arguments). This matches the oracle, whose goto
    // travelled to the function boundary. The program compiles and runs up to it.
    void jumpTo(Compiler& c, const String& label, const Vector<ExprPtr>& args) {
      compileAll(c, args);
      LabelInfo* info = labelNamed(c.function, label);

      if (info == nullptr) {
        return emitTrap(c, "no label '" + label + "' in this function");
      }

      storeLabelArgs(c, *info, args.size());
      emitJump(c.function, *info);
    }

    void compileBranch(Compiler& c, const Stmt& s) {
      if (s.value) {
        compileExpr(c, *s.value);
      }

      const int32_t toElse = emit(c.function, Op::JumpIfFalse);
      jumpTo(c, s.name, s.arguments);
      patch(c.function, toElse, here(c.function));
      jumpTo(c, s.elseName, s.elseArguments);
    }

    const Vector<ExprPtr>& caseArgsOf(const Stmt& s, size_t i) {
      static const Vector<ExprPtr> none;

      return i < s.caseArguments.size() ? s.caseArguments[i] : none;
    }

    // One stub per case, each entering its label with its own arguments; the default's stub comes last.
    void addCaseStub(Compiler& c, const Stmt& s, size_t i, JumpTable& table) {
      table.values.push_back(s.caseValues[i]);
      table.targets.push_back(here(c.function));
      jumpTo(c, s.caseLabels[i], caseArgsOf(s, i));
    }

    JumpTable switchStubs(Compiler& c, const Stmt& s) {
      JumpTable table;

      for (size_t i = 0; i < s.caseLabels.size() && i < s.caseValues.size(); i++) {
        addCaseStub(c, s, i, table);
      }

      table.otherwise = here(c.function);
      jumpTo(c, s.name, s.arguments);

      return table;
    }

    void compileSwitch(Compiler& c, const Stmt& s) {
      if (s.value) {
        compileExpr(c, *s.value);
      }

      const auto tableIndex = static_cast<int32_t>(c.function.code.tables.size());
      c.function.code.tables.emplace_back();
      emit(c.function, Op::Switch, tableIndex);
      JumpTable table = switchStubs(c, s);
      c.function.code.tables[static_cast<size_t>(tableIndex)] = move(table);
    }

    void compileYield(Compiler& c, const Stmt& s) {
      if (!c.function.code.isCoroutine) {
        return emitTrap(c, "a yield outside a coroutine");
      }

      if (s.value) {
        compileExpr(c, *s.value);
      } else {
        emit(c.function, Op::PushVoid);
      }

      emit(c.function, Op::Yield);
    }

    void compileThrow(Compiler& c, const Stmt& s) {
      if (s.value) {
        compileExpr(c, *s.value);
      }

      emit(c.function, Op::Throw);
    }

    // An expression statement evaluates its expression and drops the value.
    void compileExprStmt(Compiler& c, const Stmt& s) {
      if (!s.value) {
        return;
      }

      compileExpr(c, *s.value);
      emit(c.function, Op::Pop);
    }

    // Return, try, throw and yield: the statements that leave the straight line. Whatever kind is left is an expression statement.
    void compileExitStmt(Compiler& c, const Stmt& s) {
      switch (s.kind) {
        case StmtKind::Return:
          return compileReturn(c, s);
        case StmtKind::Try:
          return compileTry(c, s);
        case StmtKind::Throw:
          return compileThrow(c, s);
        case StmtKind::Yield:
          return compileYield(c, s);
        default:
          return compileExprStmt(c, s);
      }
    }

    // The statements that jump between labels; the other kinds go on to compileExitStmt.
    void compileLabelJump(Compiler& c, const Stmt& s) {
      switch (s.kind) {
        case StmtKind::Label:
          return compileLabel(c, s);
        case StmtKind::Goto:
          return jumpTo(c, s.name, s.arguments);
        case StmtKind::Branch:
          return compileBranch(c, s);
        case StmtKind::Switch:
          return compileSwitch(c, s);
        default:
          return compileExitStmt(c, s);
      }
    }

    // The dispatch on the statement's kind runs in steps, each `default:` handing on the kinds it does not take:
    // compileStmt -> compileLabelJump -> compileExitStmt -> compileExprStmt.
    void compileStmt(Compiler& c, const Stmt& s) {
      switch (s.kind) {
        case StmtKind::Let: case StmtKind::Var:
          return compileLet(c, s);
        case StmtKind::Assign:
          return compileAssign(c, s);
        case StmtKind::If:
          return compileIf(c, s);
        case StmtKind::For:
          return compileFor(c, s);
        default:
          return compileLabelJump(c, s);
      }
    }

    // A block: its names and labels stay inside it.
    void compileStmts(Compiler& c, const StmtList& stmts) {
      openScope(c.function);
      c.function.lists.push_back(&stmts);

      for (const StmtPtr& s : stmts) {
        if (s) {
          compileStmt(c, *s);
        }
      }

      c.function.lists.pop_back();
      closeScope(c.function);
    }

    // ── functions ──
    // A closure's captures are its first slots, each a box or a value, as the enclosing slot was. They are checked, since a label
    // parameter met by fall-through may have been captured.
    void bindCaptures(FunctionBuild& function, const Signature& signature) {
      for (size_t i = 0; i < signature.captures.size(); i++) {
        const int32_t slot = declare(function, signature.captures[i], i < signature.captureBoxed.size() && signature.captureBoxed[i]);
        function.checked[slot] = signature.captures[i];
      }
    }

    // The parameters come after the captures, each boxed at entry as the oracle boxed them. A coroutine's parameters are boxed by
    // the call instead (Run.cpp), before its frame waits for the first resume. So the heap ids come in the oracle's order.
    void bindParameters(FunctionBuild& function, const Signature& signature) {
      for (const String& p : signature.params) {
        const int32_t slot = declare(function, p, true);

        if (!signature.isCoroutine) {
          emit(function, Op::BoxSlot, slot);
        }
      }
    }

    // Opens the body's scope with the captures and the parameters bound in it.
    void bindEntry(FunctionBuild& function, const Signature& signature) {
      openScope(function);
      bindCaptures(function, signature);
      bindParameters(function, signature);
    }

    void compileBodyStmts(Compiler& c, const StmtList* body) {
      if (!body) {
        return;
      }

      scanLabels(c.function, *body);
      compileStmts(c, *body);
    }

    FunctionBuild newFunctionBuild(const Signature& signature) {
      FunctionBuild function;
      function.code.name = signature.name;
      function.code.parameters = static_cast<int32_t>(signature.params.size());
      function.code.captures = static_cast<int32_t>(signature.captures.size());

      function.code.isCoroutine = signature.isCoroutine;
      function.code.returnsVoid = signature.returnsVoid;

      return function;
    }

    Code compileBody(Pool& pool, const Signature& signature) {
      FunctionBuild function = newFunctionBuild(signature);
      Compiler c{pool, function};
      bindEntry(function, signature);

      compileBodyStmts(c, signature.body);
      emit(function, Op::ReturnVoid);  // falling off the end returns void
      closeScope(function);

      return move(function.code);
    }

    Signature functionSignature(const FuncDef& f) {
      Signature signature;
      signature.name = f.name;

      for (const Parameter& p : f.parameters) {
        signature.params.push_back(p.name);
      }

      signature.body = &f.body;
      signature.isCoroutine = f.isCoroutine;
      signature.returnsVoid = isVoidType(f.result);

      return signature;
    }

    void initValue(Compiler& c, const GlobalDef& g) {
      if (g.initializer) {
        compileExpr(c, *g.initializer);
      } else {
        emit(c.function, Op::PushConst, defaultConst(c.pool, g.type));
      }
    }

    // A global's initializer as a tiny function: it evaluates the expression and returns it.
    Signature initSignature(const GlobalDef& g) {
      Signature signature;
      signature.name = g.name;

      return signature;
    }

    Code compileInit(Pool& pool, const GlobalDef& g) {
      FunctionBuild function = newFunctionBuild(initSignature(g));
      Compiler c{pool, function};
      openScope(function);
      initValue(c, g);
      emit(function, Op::Return);

      return move(function.code);
    }

    // ── declarations ──
    StructLayout layoutOf(const StructDef& s) {
      StructLayout layout;
      layout.name = s.name;

      for (const Field& f : s.fields) {
        layout.defaults.push_back(DefaultOf(f.type));
        layout.fieldTypes.push_back(f.type);
      }

      return layout;
    }

    void declareStructs(Pool& p) {
      for (const StructDef& s : p.module->structs) {
        p.program.structIndex[s.name] = static_cast<int32_t>(p.program.structs.size());
        p.program.structs.push_back(layoutOf(s));
      }
    }

    // A binding written `native=SYMBOL` names the C symbol; without one the extern's own name is the symbol.
    String nativeBinding(const Vector<String>& bindings, const String& name) {
      const String prefix = "native=";

      for (const String& binding : bindings) {
        if (binding.starts_with(prefix)) {
          return binding.substr(prefix.size());
        }
      }

      return name;
    }

    // The C symbol an extern calls: its `native "symbol"` binding (a HostMember with no owner), else its own name.
    String symbolOf(const Module& m, const String& name) {
      for (const HostMember& h : m.hostMembers) {
        if (h.function == name && h.owner.empty()) {
          return nativeBinding(h.bindings, name);
        }
      }

      return name;
    }

    ExternInfo externInfoOf(const Module& m, const ExternDef& e) {
      ExternInfo info;
      info.name = e.name;
      info.symbol = symbolOf(m, e.name);

      for (const Parameter& param : e.parameters) {
        info.parameters.push_back(param.type);
      }

      info.result = e.result;

      return info;
    }

    void declareExterns(Pool& p) {
      for (const ExternDef& e : p.module->externs) {
        p.externIndex[e.name] = static_cast<int32_t>(p.program.externs.size());
        p.program.externs.push_back(externInfoOf(*p.module, e));
      }
    }

    void declareInterfaces(Pool& p) {
      for (const InterfaceDef& i : p.module->interfaces) {
        InterfaceInfo info;
        info.name = i.name;

        for (const InterfaceMethod& m : i.methods) {
          info.methods.push_back(m.name);
        }

        p.program.interfaces.push_back(move(info));
      }
    }

    void declareGlobals(Pool& p) {
      for (const GlobalDef& g : p.module->globals) {
        p.globalIndex[g.name] = static_cast<int32_t>(p.program.globals.size());
        p.program.globals.push_back(g.name);
      }
    }

    // Placeholders first, so every declared function has its index (module order) before any body, or closure, is compiled.
    void declareFunctions(Pool& p) {
      for (const FuncDef& f : p.module->functions) {
        p.program.functionIndex[f.name] = static_cast<int32_t>(p.program.functions.size());
        p.program.functions.emplace_back();
      }
    }

    void collectLibraries(Pool& p) {
      for (const HostImport& h : p.module->hostImports) {
        if (h.target == "native") {
          p.program.nativeLibraries.push_back(h.module);
        }
      }
    }

    // Initializers run in order, each seeing only the globals before it (a later one reads as "not bound", as in the oracle).
    void compileGlobals(Pool& p) {
      for (size_t i = 0; i < p.module->globals.size(); i++) {
        p.globalsBound = static_cast<int32_t>(i);
        p.program.globalInits.push_back(compileInit(p, p.module->globals[i]));
      }

      p.globalsBound = static_cast<int32_t>(p.module->globals.size());
    }

    // Each body is compiled into a local first: compiling it appends its closures to `functions`, which may move the entries.
    void compileFunctions(Pool& p) {
      for (size_t i = 0; i < p.module->functions.size(); i++) {
        Code code = compileBody(p, functionSignature(p.module->functions[i]));
        p.program.functions[i] = move(code);
      }
    }

    // Everything gets its number before any body is compiled: a call or a struct literal may name a later declaration.
    void declareAll(Pool& p) {
      declareStructs(p);
      declareExterns(p);
      declareInterfaces(p);
      declareGlobals(p);
      declareFunctions(p);

      collectLibraries(p);
    }

  }  // namespace

  // A `Select` runs as the `if` chain it means (Hir.hpp `WithoutSelects`).
  Program CompileProgram(const Module& module) {
    const Module plain = WithoutSelects(module);

    Pool p;
    p.module = &plain;
    declareAll(p);
    compileGlobals(p);
    compileFunctions(p);

    return move(p.program);
  }

}  // namespace polyir::vm
