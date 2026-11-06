// The LLVM compiler's core: it turns a checked polyir Module (HIR) into one LLVM IR text module. Each function is written by a
// FunctionEmitter (LlvmEmitter.hpp), one expression or statement at a time through the handler tables; this file holds the scalar parts
// (constants, arithmetic, comparisons, conversions, calls, structs) and puts the module together: struct types, globals, functions, the
// entry point that starts the garbage collector, and debug information. The containers, closures, coroutines and the rest live in the
// other Llvm*.cpp files. Every value the emitter makes is a Value: the IR text that names it (a register or a constant) and its type.
#include "polyir/Std.hpp"
#include "polyir/LlvmCompiler.hpp"

#include "emit/llvm/LlvmEmitter.hpp"

#include <sstream>
#include <stdexcept>

namespace polyir {

  namespace llvm {

    void Fail(const String& message) {
      throw RuntimeError(message);
    }

    String StringGlobal(ModuleContext& context, const String& text) {
      const auto known = context.strings_.find(text);

      if (known != context.strings_.end()) {
        return known->second;
      }

      return AddString(context, text);
    }

    // The 64-bit FNV-1a hash starts from its offset basis and multiplies by its prime. Every `@.str.<hash>` name depends on them.
    const unsigned long long kFnvOffsetBasis = 1469598103934665603ULL;
    const unsigned long long kFnvPrime = 1099511628211ULL;

    // The twelve hex digits a string constant is named by: the low 48 bits of the text's FNV-1a hash.
    static String textHash(const String& text) {
      char digits[17];
      unsigned long long hash = kFnvOffsetBasis;

      for (const unsigned char c : text) {
        hash = (hash ^ c) * kFnvPrime;
      }

      snprintf(digits, sizeof digits, "%012llx", hash & 0xffffffffffffULL);

      return String(digits);
    }

    // A string constant is named for its text, so the same text from two compilations is one constant when their IR is put together.
    String AddString(ModuleContext& context, const String& text) {
      const String name = "@.str." + textHash(text);
      context.strings_[text] = name;
      context.stringLines_.push_back(name + " = private unnamed_addr constant [" + to_string(text.size() + 1) + " x i8] c\"" + StringBytes(text) + "\\00\"");

      return name;
    }

    void AddDeclaration(ModuleContext& context, const String& line) {
      const bool constant = line.compare(0, 6, "@.str.") == 0;

      if (!constant) {
        Need(context, line);
      } else if (find(context.stringLines_.begin(), context.stringLines_.end(), line) == context.stringLines_.end()) {
        context.stringLines_.push_back(line);
      }
    }

    void DeclareThrowFlags(ModuleContext& context) {
      if (!context.throws_) {
        return;
      }

      Need(context, "@thrown = global i1 false");
      Need(context, "@thrown_value = global i64 0");
    }

    String SafeName(const String& name) {
      String out;

      for (const char c : name) {
        const bool plain = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '.';
        out += plain ? c : '_';
      }

      return out;
    }

    String StructName(const String& name) {
      return "%" + QuotedName(name);
    }

    String Typed(const Value& value) {
      return LlvmTypeText(value.type) + " " + value.text;
    }

    String VtableName(const String& structure, const String& interfaceName) {
      return "@vtable_" + SafeName(structure) + "_" + SafeName(interfaceName);
    }

    // The writer of one function, ready to write: the names its closures capture are boxed, and its debug scope is open.
    FunctionEmitter MakeFunctionEmitter(ModuleContext& context, const FuncDef& function, Vector<Capture> captures) {
      FunctionEmitter emitter{context, function, move(captures)};
      emitter.boxed_ = CapturedNames(function.body);
      emitter.assigned_ = AssignedNames(function.body);

      for (const Capture& capture : emitter.captures_) {
        emitter.boxed_.insert(capture.name);
      }

      OpenDebugScope(emitter);

      return emitter;
    }

    String EmitFunction(ModuleContext& context, const FuncDef& function, Vector<Capture> captures) {
      FunctionEmitter emitter = MakeFunctionEmitter(context, function, move(captures));

      return Emit(emitter);
    }

    void OpenDebugScope(FunctionEmitter& emitter) {
      if (!Enabled(Debug(emitter.context_))) {
        return;
      }

      emitter.debugScope_ = DebugFunction(Debug(emitter.context_), emitter.function_);
      MarkLocation(emitter, emitter.function_.at);
    }

    // Instructions written from here on belong to this position in the source (a position the front end did not give keeps the one before).
    void MarkLocation(FunctionEmitter& emitter, const Span& at) {
      if (emitter.debugScope_ < 0 || (at.line == 0 && emitter.location_ >= 0)) {
        return;
      }

      emitter.sourceLine_ = at.line;
      emitter.location_ = Location(Debug(emitter.context_), at.line, at.column, emitter.debugScope_);
    }

    String LocationSuffix(const FunctionEmitter& emitter) {
      return emitter.location_ < 0 ? "" : ", !dbg !" + to_string(emitter.location_);
    }

    // `llvm.dbg.declare` tells a debugger which slot holds a variable. A name a closure captures lives in a heap cell and is left out.
    void DescribeVariable(FunctionEmitter& emitter, const String& name, const Slot& slot, int argumentNumber) {
      if (emitter.debugScope_ < 0 || slot.boxed || name.compare(0, 2, "__") == 0) {
        return;
      }

      Need(emitter.context_, "declare void @llvm.dbg.declare(metadata, metadata, metadata)");
      const int node = Variable(Debug(emitter.context_), name, argumentNumber, emitter.debugScope_, emitter.sourceLine_, slot.type);
      Line(emitter, "call void @llvm.dbg.declare(metadata ptr " + slot.pointer + ", metadata !" + to_string(node) + ", metadata !DIExpression())");
    }

    // ── blocks and instructions ──────────────────────────────────────────────────────────────────────────────────────────

    void StartBlock(FunctionEmitter& emitter, const String& name) {
      emitter.lines_.push_back(name + ":");
      emitter.currentBlock_ = name;
      emitter.terminated_ = false;
    }

    void EnsureOpen(FunctionEmitter& emitter) {
      if (emitter.terminated_) {
        StartBlock(emitter, UniqueName(emitter, "dead"));
      }
    }

    void Line(FunctionEmitter& emitter, const String& text) {
      EnsureOpen(emitter);
      emitter.lines_.push_back("  " + text + LocationSuffix(emitter));
    }

    // A `ret` leaves the function, so its frame record is unlinked first (LlvmRoots.cpp).
    void Terminate(FunctionEmitter& emitter, const String& text) {
      if (text.compare(0, 4, "ret ") == 0) {
        LeaveFrame(emitter);
      }

      Line(emitter, text);
      emitter.terminated_ = true;
    }

    String UniqueName(FunctionEmitter& emitter, const String& base) {
      String name = base;

      while (emitter.names_.count(name) != 0) {
        name = base + to_string(++emitter.suffixes_[base]);
      }

      emitter.names_.insert(name);

      return name;
    }

    namespace {

      // What each instruction's result is called.
      const Map<String, String> kOpcodeNames = {{"add", "add"}, {"fadd", "add"}, {"sub", "sub"}, {"fsub", "sub"}, {"mul", "mul"}, {"fmul", "mul"},
          {"sdiv", "div"}, {"udiv", "div"}, {"fdiv", "div"}, {"srem", "rem"}, {"urem", "rem"}, {"frem", "rem"}, {"and", "and"}, {"or", "or"},
          {"xor", "xor"}, {"shl", "shl"}, {"ashr", "shr"}, {"lshr", "shr"}, {"icmp", "cmp"}, {"fcmp", "cmp"}, {"select", "cond"}, {"phi", "phi"},
          {"call", "call"}, {"getelementptr", "ptr"}, {"sext", "conv"}, {"zext", "conv"}, {"trunc", "conv"}, {"sitofp", "conv"}, {"uitofp", "conv"},
          {"fptosi", "conv"}, {"fptoui", "conv"}, {"fpext", "conv"}, {"fptrunc", "conv"}, {"bitcast", "conv"}, {"ptrtoint", "conv"},
          {"inttoptr", "conv"}, {"extractvalue", "elem"}, {"extractelement", "elem"}, {"insertelement", "vec"}, {"shufflevector", "shuffle"},
          {"atomicrmw", "old"}, {"cmpxchg", "swap"}, {"fneg", "neg"}};

      // A load is named after the variable it reads (`total.val`), anything else after what it does.
      String loadName(const String& text) {
        const size_t at = text.find("ptr %");
        const size_t end = text.find_first_of(" ,", at + 5);
        String variable = at == String::npos ? "" : text.substr(at + 5, end - at - 5);
        const size_t suffix = variable.rfind(".addr");
        variable = suffix == String::npos ? variable : variable.substr(0, suffix);

        return variable.empty() ? "val" : variable + ".val";
      }

    }  // namespace

    String ValueName(const String& text) {
      const String opcode = text.substr(0, text.find(' '));

      if (opcode == "load") {
        return loadName(text);
      }

      const auto known = kOpcodeNames.find(opcode);

      return known == kOpcodeNames.end() ? "tmp" : known->second;
    }

    String Instruction(FunctionEmitter& emitter, const String& text) {
      const String name = "%" + UniqueName(emitter, ValueName(text));
      Line(emitter, name + " = " + text);

      return name;
    }

    // A variable that holds a reference (or the address of its cell) has its slot in the function's frame record, where the collector sees it.
    Slot MakeSlot(FunctionEmitter& emitter, const String& name, const TypePtr& type, const String& slotName) {
      const String base = SafeName(slotName.empty() ? name : slotName);
      const bool boxed = emitter.boxed_.count(name) != 0;

      if (HoldsRoot(emitter, type, boxed)) {
        return Slot{RootSlot(emitter, base), type, boxed, ""};
      }

      const String pointer = "%" + UniqueName(emitter, base);
      emitter.allocas_.push_back("  " + pointer + " = alloca " + (boxed ? String("ptr") : LlvmTypeText(type)));

      return Slot{pointer, type, boxed, ""};
    }

    // A name a closure captures lives in a heap cell of its own, made each time the name is declared. The slot holds the cell's address.
    void AllocateCell(FunctionEmitter& emitter, const Slot& slot) {
      if (slot.boxed) {
        Store(emitter, Allocate(emitter, "8", LayoutOf(slot.type)), slot.pointer);
      }
    }

    String AddressOfSlot(FunctionEmitter& emitter, const Slot& slot) {
      return slot.boxed ? Instruction(emitter, "load ptr, ptr " + slot.pointer) : slot.pointer;
    }

    // A captured variable's cell is an object the closures share, so a store into it goes through the write barrier.
    void SlotStore(FunctionEmitter& emitter, const Value& value, const Slot& slot) {
      if (slot.boxed) {
        HeapStore(emitter, value, AddressOfSlot(emitter, slot));
      } else {
        Store(emitter, value, slot.pointer);
      }
    }

    // A new variable: its slot (and cell, when captured), holding the starting value.
    Slot BindVariable(FunctionEmitter& emitter, const String& name, const Value& start, const String& slotName, int argumentNumber) {
      const Slot slot = MakeSlot(emitter, name, start.type, slotName);
      AllocateCell(emitter, slot);
      Store(emitter, start, AddressOfSlot(emitter, slot));
      DescribeVariable(emitter, name, slot, argumentNumber);
      Declare(emitter, name, slot);

      return slot;
    }

    Slot Lookup(const FunctionEmitter& emitter, const String& name) {
      for (auto scope = emitter.scopes_.rbegin(); scope != emitter.scopes_.rend(); ++scope) {
        const auto found = scope->find(name);

        if (found != scope->end()) {
          return found->second;
        }
      }

      Fail("unknown variable '" + name + "' in " + emitter.function_.name);
    }

    // ── expressions ──────────────────────────────────────────────────────────────────────────────────────────────────────

    namespace {

      const String& floatOpcode(BinOp op) {
        static const Map<BinOp, String> opcodes = {{BinOp::Add, "fadd"}, {BinOp::Sub, "fsub"}, {BinOp::Mul, "fmul"}, {BinOp::Div, "fdiv"}, {BinOp::Rem, "frem"}};

        return opcodes.at(op);
      }

      const String& integerOpcode(BinOp op) {
        static const Map<BinOp, String> opcodes = {{BinOp::Add, "add"}, {BinOp::Sub, "sub"}, {BinOp::Mul, "mul"}, {BinOp::And, "and"}, {BinOp::Or, "or"}, {BinOp::Xor, "xor"}};

        return opcodes.at(op);
      }

      String relationOf(BinOp op) {
        static const char* const order[] = {"eq", "ne", "lt", "le", "gt", "ge"};

        return order[static_cast<int>(op) - static_cast<int>(BinOp::Eq)];
      }

      String floatRelation(const String& relation) {
        static const Map<String, String> floats = {{"eq", "oeq"}, {"ne", "une"}, {"lt", "olt"}, {"le", "ole"}, {"gt", "ogt"}, {"ge", "oge"}};

        return "fcmp " + floats.at(relation);
      }

      String integerRelation(const TypePtr& type, const String& relation) {
        const bool plain = relation == "eq" || relation == "ne" || !IsInteger(*type);

        return String("icmp ") + (plain ? "" : (IsSigned(*type) ? "s" : "u")) + relation;
      }

      // The comparison instruction (`icmp slt`, `fcmp olt`, ...) for the operator on operands of the type.
      String compareOpcode(BinOp op, const TypePtr& type) {
        const String relation = relationOf(op);

        return IsFloat(*type) ? floatRelation(relation) : integerRelation(type, relation);
      }

      bool isSignedValue(const TypePtr& type) {
        return IsSigned(*type) && !IsBoolean(*type);
      }

    }  // namespace

    const Map<ExprKind, FunctionEmitter::ExprHandler> FunctionEmitter::kExprHandlers = {
      {ExprKind::ConstInt, &Constant},
      {ExprKind::ConstFloat, &Constant},
      {ExprKind::ConstBool, &Constant},
      {ExprKind::ConstString, &Constant},
      {ExprKind::Local, &Load},
      {ExprKind::Global, &Load},
      {ExprKind::LoadSlot, &Load},
      {ExprKind::Binary, &Binary},
      {ExprKind::Unary, &Unary},
      {ExprKind::Cast, &CastExpr},
      {ExprKind::Coerce, &CastExpr},
      {ExprKind::Call, &Call},
      {ExprKind::StructLit, &StructLit},
      {ExprKind::FieldGet, &FieldGet},
      {ExprKind::FuncRef, &FuncRef},
      {ExprKind::CallIndirect, &CallIndirect},
      {ExprKind::CallBuiltin, &BuiltinCall},
      {ExprKind::IndexGet, &IndexGet},
      {ExprKind::VecLit, &VecLit},
      {ExprKind::NewContainer, &NewContainer},
      {ExprKind::ClosureLit, &ClosureLit},
      {ExprKind::VariantLit, &VariantLit},
      {ExprKind::EitherLit, &EitherLit},
      {ExprKind::Unsupported, &Unsupported}};

    FunctionEmitter::ExprHandler ExprHandlerFor(ExprKind kind) {
      const auto found = FunctionEmitter::kExprHandlers.find(kind);

      if (found == FunctionEmitter::kExprHandlers.end()) {
        Fail("unknown expression");
      }

      return found->second;
    }

    Value EmitExpr(FunctionEmitter& emitter, const Expr& expr) {
      const FunctionEmitter::ExprHandler handler = ExprHandlerFor(expr.kind);
      const int saved = emitter.location_;
      MarkLocation(emitter, expr.at);

      const Value result = handler(emitter, expr);
      KeepRoot(emitter, expr, result);
      emitter.location_ = saved;

      return result;
    }

    Value CastExpr(FunctionEmitter& emitter, const Expr& expr) {
      return Convert(emitter, EmitExpr(emitter, *expr.operands[0]), expr.type);
    }

    Value Unsupported(FunctionEmitter&, const Expr& expr) {
      Fail("unsupported construct: " + expr.text);
    }

    Value Constant(FunctionEmitter& emitter, const Expr& expr) {
      switch (expr.kind) {
        case ExprKind::ConstInt:
          return Value{IntText(expr.intValue, expr.type->bits), expr.type};
        case ExprKind::ConstFloat:
          return Value{FloatText(expr.floatValue, expr.type->bits), expr.type};
        case ExprKind::ConstBool:
          return Value{expr.boolValue ? "true" : "false", expr.type};
        default:
          return Value{StringGlobal(emitter.context_, expr.text), expr.type};
      }
    }

    Value Load(FunctionEmitter& emitter, const Expr& expr) {
      if (expr.kind != ExprKind::Global && !Lookup(emitter, expr.name).value.empty()) {
        return Value{Lookup(emitter, expr.name).value, expr.type};
      }

      const String pointer = expr.kind == ExprKind::Global ? "@" + QuotedName(expr.name) : AddressOfSlot(emitter, Lookup(emitter, expr.name));

      return Value{Instruction(emitter, "load " + LlvmTypeText(expr.type) + ", ptr " + pointer), expr.type};
    }

    namespace {

      bool isShift(BinOp op) {
        return op == BinOp::Shl || op == BinOp::Shr;
      }

    }  // namespace

    Value Binary(FunctionEmitter& emitter, const Expr& expr) {
      if (IsLogical(expr.binaryOp)) {
        return ShortCircuit(emitter, expr);
      }

      const Value left = EmitExpr(emitter, *expr.operands[0]);
      const Value right = EmitExpr(emitter, *expr.operands[1]);
      const TypePtr to = OperandType(expr, left, right);

      return BinaryOperation(emitter, expr.binaryOp, Fit(emitter, left, to), isShift(expr.binaryOp) ? right : Fit(emitter, right, to));
    }

    // The type both operands are brought to. The front end leaves a literal as an i64 beside a narrower number, and a call's i32 beside an i64.
    TypePtr OperandType(const Expr& expr, const Value& left, const Value& right) {
      if (!IsNumber(*left.type) || !IsNumber(*right.type)) {
        return left.type;
      }

      if (!IsComparison(expr.binaryOp)) {
        return IsNumber(*expr.type) ? expr.type : left.type;
      }

      return left.type->bits >= right.type->bits ? left.type : right.type;
    }

    // A number of another width is converted; anything else is already right.
    Value Fit(FunctionEmitter& emitter, const Value& value, const TypePtr& to) {
      const bool numbers = IsNumber(*value.type) && IsNumber(*to);

      if (value.type->kind == TypeKind::Vec && to->kind == TypeKind::Vec) {
        return FitVector(emitter, value, to);
      }

      return numbers && !SameType(value.type, to) ? Convert(emitter, value, to) : value;
    }

    namespace {

      // The cast between two element types of the same kind: floats narrow with `fptrunc` and widen with `fpext`, integers with `trunc` and `sext`/`zext`.
      String vectorCast(const TypePtr& from, const TypePtr& into) {
        if (into->bits < from->bits) {
          return IsFloat(*into) ? "fptrunc" : "trunc";
        }

        return IsFloat(*into) ? "fpext" : (IsSigned(*from) ? "sext" : "zext");
      }

    }  // namespace

    // A vector of one float (or integer) width to the same count of another: one cast of the whole vector.
    Value FitVector(FunctionEmitter& emitter, const Value& value, const TypePtr& to) {
      const TypePtr from = value.type->element;

      if (SameType(from, to->element) || IsFloat(*from) != IsFloat(*to->element)) {
        return value;
      }

      return CastTo(emitter, vectorCast(from, to->element), value, to);
    }


    Value BinaryOperation(FunctionEmitter& emitter, BinOp op, const Value& left, const Value& right) {
      if (op == BinOp::Identical || op == BinOp::NotIdentical) {
        return Identity(emitter, op, left, right);
      }

      return IsComparison(op) ? Comparison(emitter, op, left, right) : Arithmetic(emitter, op, left, right);
    }

    // `left <opcode> right`, of the type of the left operand.
    Value WithOpcode(FunctionEmitter& emitter, const String& opcode, const Value& left, const Value& right) {
      return Value{Instruction(emitter, opcode + " " + Typed(left) + ", " + right.text), left.type};
    }

    Value Arithmetic(FunctionEmitter& emitter, BinOp op, const Value& left, const Value& right) {
      if (op == BinOp::Shl || op == BinOp::Shr) {
        return Shift(emitter, op, left, right);
      }

      if (op == BinOp::Div || op == BinOp::Rem) {
        return IsFloat(*ScalarOf(left.type)) ? WithOpcode(emitter, floatOpcode(op), left, right) : Division(emitter, op, left, right);
      }

      return WithOpcode(emitter, IsFloat(*ScalarOf(left.type)) ? floatOpcode(op) : integerOpcode(op), left, right);
    }

    // A zero divisor stops the program (as the VM does; LLVM leaves it undefined). Signed `/` and `%` by -1 wrap instead of trapping:
    // the divisor is swapped for 1 and the result chosen afterwards.
    Value Division(FunctionEmitter& emitter, BinOp op, const Value& left, const Value& right) {
      const bool remainder = op == BinOp::Rem;
      TrapUnless(emitter, Instruction(emitter, "icmp ne " + Typed(right) + ", 0"), "stopped, an integer division by zero");

      if (!IsSigned(*left.type)) {
        return WithOpcode(emitter, remainder ? "urem" : "udiv", left, right);
      }

      return SignedDivision(emitter, remainder, left, right);
    }

    Value SignedDivision(FunctionEmitter& emitter, bool remainder, const Value& left, const Value& right) {
      const String t = LlvmTypeText(left.type);
      const String minusOne = Instruction(emitter, "icmp eq " + Typed(right) + ", -1");
      const String safe = Instruction(emitter, "select i1 " + minusOne + ", " + t + " 1, " + Typed(right));
      const String result = Instruction(emitter, String(remainder ? "srem " : "sdiv ") + Typed(left) + ", " + safe);
      const String special = remainder ? "0" : Instruction(emitter, "sub " + t + " 0, " + left.text);

      return Value{Instruction(emitter, "select i1 " + minusOne + ", " + t + " " + special + ", " + t + " " + result), left.type};
    }

    Value Resized(FunctionEmitter& emitter, const Value& value, int bits, bool isSigned) {
      const int have = value.type->bits;

      if (have == bits) {
        return value;
      }

      const TypePtr target = MakeInt(bits);
      const String how = have > bits ? "trunc" : (isSigned ? "sext" : "zext");

      return Value{Instruction(emitter, how + " " + Typed(value) + " to " + LlvmTypeText(target)), target};
    }

    // A shift takes its count modulo the width, as the machine does.
    Value Shift(FunctionEmitter& emitter, BinOp op, const Value& left, const Value& right) {
      const int bits = left.type->bits;
      const Value count = Resized(emitter, right, bits, false);
      const String masked = Instruction(emitter, "and " + Typed(count) + ", " + to_string(bits - 1));
      const String name = op == BinOp::Shl ? "shl" : (IsSigned(*left.type) ? "ashr" : "lshr");

      return Value{Instruction(emitter, name + " " + Typed(left) + ", " + masked), left.type};
    }

    // `==` and `!=` on strings compare the text.
    Value StringComparison(FunctionEmitter& emitter, BinOp op, const Value& left, const Value& right) {
      const Value same = StringEquals(emitter, left, right);

      if (op != BinOp::Eq && op != BinOp::Ne) {
        Fail("strings are compared with == and != only");
      }

      return op == BinOp::Eq ? same : Value{Instruction(emitter, "xor i1 " + same.text + ", true"), MakeBool()};
    }

    // `a === b`: the same object.
    Value Identity(FunctionEmitter& emitter, BinOp op, const Value& left, const Value& right) {
      return Value{Instruction(emitter, String("icmp ") + (op == BinOp::Identical ? "eq " : "ne ") + Typed(left) + ", " + right.text), MakeBool()};
    }

    Value Comparison(FunctionEmitter& emitter, BinOp op, const Value& left, const Value& right) {
      const TypeKind kind = left.type->kind;

      if (kind == TypeKind::String) {
        return StringComparison(emitter, op, left, right);
      }

      if (kind == TypeKind::Hashmap || kind == TypeKind::Set) {
        return MapComparison(emitter, op, left, right);
      }

      if (HasEqualityHelper(emitter, left.type)) {
        return HelperComparison(emitter, op, left, right);
      }

      if (IsValueStruct(emitter, left.type)) {
        return StructComparison(emitter, op, left, right);
      }

      return Value{Instruction(emitter, compareOpcode(op, left.type) + " " + Typed(left) + ", " + right.text), MakeBool()};
    }

    // `a && b` and `a || b` only work out `b` when `a` does not decide.
    Value ShortCircuit(FunctionEmitter& emitter, const Expr& expr) {
      const bool isAnd = expr.binaryOp == BinOp::LogAnd;
      const Value left = EmitExpr(emitter, *expr.operands[0]);
      const String leftBlock = emitter.currentBlock_;

      const String rhs = UniqueName(emitter, "logic.rhs");
      const String end = UniqueName(emitter, "logic.end");
      Terminate(emitter, "br i1 " + left.text + ", label %" + (isAnd ? rhs : end) + ", label %" + (isAnd ? end : rhs));

      return LogicEnd(emitter, *expr.operands[1], isAnd ? "false" : "true", leftBlock, rhs, end);
    }

    // The right operand in its own block, and the block after it where the result is `decided` or the right operand.
    Value LogicEnd(FunctionEmitter& emitter, const Expr& operand, const String& decided, const String& leftBlock, const String& rhs, const String& end) {
      StartBlock(emitter, rhs);
      const Value right = EmitExpr(emitter, operand);
      const String rightBlock = emitter.currentBlock_;
      BranchTo(emitter, end);
      StartBlock(emitter, end);

      return Value{Instruction(emitter, "phi i1 [ " + decided + ", %" + leftBlock + " ], [ " + right.text + ", %" + rightBlock + " ]"), MakeBool()};
    }

    Value Unary(FunctionEmitter& emitter, const Expr& expr) {
      const Value value = EmitExpr(emitter, *expr.operands[0]);

      if (expr.unaryOp == UnOp::Neg) {
        return Value{Instruction(emitter, IsFloat(*ScalarOf(value.type)) ? "fneg " + Typed(value) : "sub " + LlvmTypeText(value.type) + " 0, " + value.text), value.type};
      }

      const String all = expr.unaryOp == UnOp::LogNot ? "true" : "-1";

      return Value{Instruction(emitter, "xor " + Typed(value) + ", " + all), value.type};
    }

    Value Convert(FunctionEmitter& emitter, const Value& from, const TypePtr& to) {
      if (SameType(from.type, to)) {
        return from;
      }

      if (!IsNumber(*from.type) || !IsNumber(*to)) {
        return ConvertAddress(emitter, from, to);
      }

      return IsFloat(*from.type) ? ConvertFromFloat(emitter, from, to) : ConvertFromInteger(emitter, from, to);
    }

    // `from to` with a conversion instruction such as `fpext` or `sitofp`.
    Value CastTo(FunctionEmitter& emitter, const String& opcode, const Value& from, const TypePtr& to) {
      return Value{Instruction(emitter, opcode + " " + Typed(from) + " to " + LlvmTypeText(to)), to};
    }

    // A pointer and an integer (an address handed to a library function) convert with `ptrtoint` and `inttoptr`; two pointers are the same value.
    Value ConvertAddress(FunctionEmitter& emitter, const Value& from, const TypePtr& to) {
      if (IsInteger(*to) && !IsNumber(*from.type)) {
        return CastTo(emitter, "ptrtoint", from, to);
      }

      if (IsInteger(*from.type) && !IsNumber(*to)) {
        return CastTo(emitter, "inttoptr", from, to);
      }

      return Value{from.text, to};
    }

    Value ConvertFromFloat(FunctionEmitter& emitter, const Value& from, const TypePtr& to) {
      if (IsFloat(*to)) {
        return CastTo(emitter, to->bits > from.type->bits ? "fpext" : "fptrunc", from, to);
      }

      if (IsBoolean(*to)) {
        return Value{Instruction(emitter, "fcmp une " + Typed(from) + ", " + ZeroText(from.type)), to};
      }

      return CastTo(emitter, IsSigned(*to) ? "fptosi" : "fptoui", from, to);
    }

    Value ConvertFromInteger(FunctionEmitter& emitter, const Value& from, const TypePtr& to) {
      if (IsFloat(*to)) {
        return CastTo(emitter, isSignedValue(from.type) ? "sitofp" : "uitofp", from, to);
      }

      if (IsBoolean(*to)) {
        return Value{Instruction(emitter, "icmp ne " + Typed(from) + ", 0"), to};
      }

      return Value{Resized(emitter, from, to->bits, isSignedValue(from.type)).text, to};
    }

    // The operands from `from` on, worked out in order and written as a call's arguments.
    String ArgumentList(FunctionEmitter& emitter, const Expr& expr, size_t from) {
      String out;

      for (size_t i = from; i < expr.operands.size(); i++) {
        out += (i == from ? "" : ", ") + Typed(EmitExpr(emitter, *expr.operands[i]));
      }

      return out;
    }

    // A call of one of the program's own functions (not a C function's) may throw, and may reach a safe point.
    bool CallsOwn(FunctionEmitter& emitter, const String& name) {
      const bool own = FindExtern(ModuleOf(emitter.context_), name) == nullptr;

      if (own) {
        NoteCall(emitter);
      }

      return own;
    }

    Value Call(FunctionEmitter& emitter, const Expr& expr) {
      const String args = ArgumentList(emitter, expr, 0);
      const bool mayThrow = CallsOwn(emitter, expr.name) && Throws(emitter.context_);
      const Value result{CallInstruction(emitter, expr.type, "@" + QuotedName(SymbolName(expr.name)), args), expr.type};

      if (mayThrow) {
        PassOnThrown(emitter);
      }

      return result;
    }

    namespace {

      using BuiltinFamily = bool (*)(FunctionEmitter&, const Expr&, Value&);

      // Every family of built-ins: each writes the ones it knows (filling `out`) and answers false for the rest. The first family that
      // takes a built-in writes it, and the order matters where two take the same one: `free` of an optional, either or variant goes to
      // SumBuiltin and of a closure to ClosureBuiltin, and a map's or a set's own built-ins (`len`, `copy`, `free`, ...) to MapBuiltin,
      // all ahead of ContainerBuiltin, which takes those for every other container.
      const BuiltinFamily kBuiltinFamilies[] = {SumBuiltin, ClosureBuiltin, MapBuiltin, ContainerBuiltin, StringBuiltin, AtomicBuiltin,
                                                ShuffleBuiltin, InterfaceBuiltin, CoroutineBuiltin};

    }  // namespace

    // A call of a built-in (`len`, `push`, a map's `get`, ...), written by the first family that takes it.
    Value BuiltinCall(FunctionEmitter& emitter, const Expr& expr) {
      Value out;

      for (const BuiltinFamily family : kBuiltinFamilies) {
        if (family(emitter, expr, out)) {
          return out;
        }
      }

      Fail(String("the builtin '") + BuiltinName(expr.builtin) + "' is not supported by the LLVM compiler yet");
    }

    String FieldAddress(FunctionEmitter& emitter, const Value& object, int index) {
      return Instruction(emitter, "getelementptr " + StructName(object.type->name) + ", ptr " + object.text + ", i32 0, i32 " + to_string(index));
    }

    // The size of a struct (LLVM IR has no sizeof). It is the address of the second struct in an array of them that starts at address 0.
    String StructSize(FunctionEmitter& emitter, const String& structType) {
      const String end = Instruction(emitter, "getelementptr " + structType + ", ptr null, i32 1");

      return Instruction(emitter, "ptrtoint ptr " + end + " to i64");
    }

    // A struct is a heap object. Its fields are worked out first (a field's call may reach a safe point, and the new block is not in any root
    // yet). Then the block is allocated and each field stored.
    Value StructLit(FunctionEmitter& emitter, const Expr& expr) {
      const Vector<Value> fields = ValuesOf(emitter, expr.operands);
      const Value block = Allocate(emitter, StructSize(emitter, StructName(expr.name)), StructLayout(emitter, expr.name));
      const Value object{block.text, expr.type};
      StoreFields(emitter, expr, object, fields);

      return object;
    }

    void StoreFields(FunctionEmitter& emitter, const Expr& expr, const Value& object, const Vector<Value>& fields) {
      for (size_t i = 0; i < fields.size(); i++) {
        Store(emitter, fields[i], FieldAddress(emitter, object, expr.fieldIndices[i]));
      }
    }

    Value FieldGet(FunctionEmitter& emitter, const Expr& expr) {
      const Value object = EmitExpr(emitter, *expr.operands[0]);

      return Value{Instruction(emitter, "load " + LlvmTypeText(expr.type) + ", ptr " + FieldAddress(emitter, object, expr.fieldIndex)), expr.type};
    }

    String AddressOf(FunctionEmitter& emitter, const Expr& target) {
      switch (target.kind) {
        case ExprKind::Local:
        case ExprKind::LoadSlot:
          return AddressOfSlot(emitter, Lookup(emitter, target.name));
        case ExprKind::Global:
          return "@" + QuotedName(target.name);
        case ExprKind::FieldGet:
          return FieldAddress(emitter, EmitExpr(emitter, *target.operands[0]), target.fieldIndex);
        default:
          return ElementTarget(emitter, target);
      }
    }

    String ElementTarget(FunctionEmitter& emitter, const Expr& target) {
      if (target.kind != ExprKind::IndexGet) {
        Fail("only a variable, a global, a field or an element can be assigned to");
      }

      const Value base = EmitExpr(emitter, *target.operands[0]);

      return ElementAddress(emitter, target, base, OperandValues(emitter, target, 1));
    }

    // ── statements ───────────────────────────────────────────────────────────────────────────────────────────────────────

    void EmitBlock(FunctionEmitter& emitter, const StmtList& statements) {
      for (const StmtPtr& statement : statements) {
        Statement(emitter, *statement);
      }
    }

    const Map<StmtKind, FunctionEmitter::StmtHandler> FunctionEmitter::kStmtHandlers = {
      {StmtKind::Let, &DeclareVariable},
      {StmtKind::Var, &DeclareVariable},
      {StmtKind::Assign, &Assign},
      {StmtKind::If, &IfStatement},
      {StmtKind::For, &ForStatement},
      {StmtKind::Return, &ReturnStatement},
      {StmtKind::ExprStmt, &ExpressionStatement},
      {StmtKind::Label, &LabelStatement},
      {StmtKind::Goto, &GotoStatement},
      {StmtKind::Branch, &BranchStatement},
      {StmtKind::Switch, &SwitchStatement},
      {StmtKind::Try, &TryStatement},
      {StmtKind::Throw, &ThrowStatement},
      {StmtKind::Yield, &YieldStatement}};

    void Statement(FunctionEmitter& emitter, const Stmt& statement) {
      const auto found = FunctionEmitter::kStmtHandlers.find(statement.kind);

      if (found == FunctionEmitter::kStmtHandlers.end()) {
        Fail("unknown statement");
      }

      MarkLocation(emitter, statement.at);
      found->second(emitter, statement);
    }

    void ExpressionStatement(FunctionEmitter& emitter, const Stmt& statement) {
      EmitExpr(emitter, *statement.value);
    }

    // The slot has the declared type, so the starting value is brought to it first (`var level: float = 1.0`: the literal is a double), as the VM does.
    void DeclareVariable(FunctionEmitter& emitter, const Stmt& statement) {
      const Value start = statement.value ? EmitExpr(emitter, *statement.value) : Value{ZeroText(statement.type), statement.type};
      BindVariable(emitter, statement.name, statement.type ? Fit(emitter, start, statement.type) : start);
    }

    // Whether an assignment writes into an object (a field, an element, a captured variable's cell) rather than a stack slot or a global.
    bool IntoAnObject(const FunctionEmitter& emitter, const Expr& target) {
      if (target.kind == ExprKind::Global) {
        return false;
      }

      if (target.kind == ExprKind::Local || target.kind == ExprKind::LoadSlot) {
        return Lookup(emitter, target.name).boxed;
      }

      return true;
    }

    void Assign(FunctionEmitter& emitter, const Stmt& statement) {
      const Value value = Fit(emitter, EmitExpr(emitter, *statement.value), statement.target->type);
      const String address = AddressOf(emitter, *statement.target);

      if (IntoAnObject(emitter, *statement.target)) {
        HeapStore(emitter, value, address);
      } else {
        Store(emitter, value, address);
      }
    }

    // The values of the expressions, worked out in order.
    Vector<Value> ValuesOf(FunctionEmitter& emitter, const Vector<ExprPtr>& exprs) {
      Vector<Value> values;

      for (const ExprPtr& expr : exprs) {
        values.push_back(EmitExpr(emitter, *expr));
      }

      return values;
    }

    void ScopedBlock(FunctionEmitter& emitter, const StmtList& statements) {
      emitter.scopes_.emplace_back();
      EmitBlock(emitter, statements);
      emitter.scopes_.pop_back();
    }

    // Branches on the condition to `yes` or `no`.
    void BranchIf(FunctionEmitter& emitter, const Expr& condition, const String& yes, const String& no) {
      const Value test = EmitExpr(emitter, condition);
      Terminate(emitter, "br i1 " + test.text + ", label %" + yes + ", label %" + no);
    }

    // A block of its own for the statements, ending with a jump to `next` unless they already left.
    void ArmBlock(FunctionEmitter& emitter, const String& name, const StmtList& body, const String& next) {
      StartBlock(emitter, name);
      ScopedBlock(emitter, body);
      JumpUnlessTerminated(emitter, next);
    }

    void IfStatement(FunctionEmitter& emitter, const Stmt& statement) {
      const String then = UniqueName(emitter, "if.then");
      const String otherwise = UniqueName(emitter, "if.else");
      const String end = UniqueName(emitter, "if.end");
      BranchIf(emitter, *statement.value, then, statement.elseBody.empty() ? end : otherwise);
      IfArms(emitter, statement, then, otherwise, end);
    }

    void IfArms(FunctionEmitter& emitter, const Stmt& statement, const String& then, const String& otherwise, const String& end) {
      ArmBlock(emitter, then, statement.body, end);

      if (!statement.elseBody.empty()) {
        ArmBlock(emitter, otherwise, statement.elseBody, end);
      }

      StartBlock(emitter, end);
    }

    // A `for` is its counters (declared before), a test, the body, then the steps (all worked out before any is stored).
    void ForStatement(FunctionEmitter& emitter, const Stmt& statement) {
      for (size_t i = 0; i < statement.names.size(); i++) {
        BindVariable(emitter, statement.names[i], EmitExpr(emitter, *statement.initializers[i]));
      }

      ForLoop(emitter, statement);
    }

    void ForLoop(FunctionEmitter& emitter, const Stmt& statement) {
      const String test = UniqueName(emitter, "for.test");
      const String body = UniqueName(emitter, "for.body");
      const String step = UniqueName(emitter, "for.step");
      const String end = UniqueName(emitter, "for.end");

      LoopTest(emitter, test, *statement.value, body, end);
      ArmBlock(emitter, body, statement.body, step);
      LoopStep(emitter, statement, step, test, end);
    }

    // The head of a loop is a safe point: a loop that allocates gives the collector a chance on every round.
    void LoopTest(FunctionEmitter& emitter, const String& test, const Expr& condition, const String& body, const String& end) {
      BranchTo(emitter, test);
      StartBlock(emitter, test);
      SafePoint(emitter);
      BranchIf(emitter, condition, body, end);
    }

    namespace {

      // A step is the next value of the loop variable in its place, so a step with no loop variable has nothing to assign. An example
      // is `for (; i < n; i = i + 1)` with `i` declared before the loop: the HIR keeps only the value. The steps, when each has its variable.
      const Vector<ExprPtr>& stepsWithTargets(const Stmt& statement) {
        if (statement.steps.size() <= statement.names.size()) {
          return statement.steps;
        }

        Fail("a for loop's steps assign its own variables (declared in its first part); this one has more steps than variables");
      }

    }  // namespace

    // The step block: new values for the counters, then back to the test; `end` follows.
    void LoopStep(FunctionEmitter& emitter, const Stmt& statement, const String& step, const String& test, const String& end) {
      StartBlock(emitter, step);
      const Vector<Value> next = ValuesOf(emitter, stepsWithTargets(statement));

      for (size_t i = 0; i < next.size(); i++) {
        SlotStore(emitter, next[i], Lookup(emitter, statement.names[i]));
      }

      BranchTo(emitter, test);
      StartBlock(emitter, end);
    }

    // A function with no result may still be left with `return g()` (polyir has no value-less return): the call runs, nothing is returned.
    void ReturnStatement(FunctionEmitter& emitter, const Stmt& statement) {
      if (emitter.function_.isCoroutine) {
        return LeaveCoroutine(emitter, statement);
      }

      if (statement.value && !IsVoid(*emitter.function_.result)) {
        return ReturnValue(emitter, statement);
      }

      if (statement.value) {
        EmitExpr(emitter, *statement.value);
      }

      Terminate(emitter, "ret void");
    }

    void ReturnValue(FunctionEmitter& emitter, const Stmt& statement) {
      const Value value = Fit(emitter, EmitExpr(emitter, *statement.value), emitter.function_.result);
      Terminate(emitter, "ret " + Typed(value));
    }

    // Each parameter is spilled to a slot of its own (in a cell when a closure captures it).
    void BindParameters(FunctionEmitter& emitter, String& declaration) {
      for (size_t i = 0; i < emitter.function_.parameters.size(); i++) {
        const Parameter& parameter = emitter.function_.parameters[i];
        const String argument = "%" + UniqueName(emitter, SafeName(parameter.name));
        declaration += (i == 0 ? "" : ", ") + LlvmTypeText(parameter.type) + " " + argument;
        BindVariable(emitter, parameter.name, Value{argument, parameter.type}, SafeName(parameter.name) + ".addr", static_cast<int>(i) + 1);
      }
    }

    // A closure's body starts by taking the cells of the names it captured out of its environment (its first parameter).
    void ReadCaptures(FunctionEmitter& emitter) {
      for (size_t k = 0; k < emitter.captures_.size(); k++) {
        const Slot slot = MakeSlot(emitter, emitter.captures_[k].name, emitter.captures_[k].type);
        const String environment = "%" + SafeName(emitter.function_.parameters[0].name);
        Store(emitter, LoadFrom(emitter, WordAddress(emitter, environment, static_cast<int64_t>(k)), MakePointer(emitter.captures_[k].type)), slot.pointer);
        Declare(emitter, emitter.captures_[k].name, slot);
      }
    }

    // Everything before the body: the parameters (the result is their declaration), the captures and the labels.
    String Prepare(FunctionEmitter& emitter) {
      emitter.scopes_.emplace_back();
      String parameters;
      BindParameters(emitter, parameters);
      ReadCaptures(emitter);

      CollectLabels(emitter);
      AllocateLabelCells(emitter);

      return parameters;
    }

    namespace {

      void appendLines(OStringStream& out, const Vector<String>& lines) {
        for (const String& line : lines) {
          out << line << "\n";
        }
      }

    }  // namespace

    // The start of the body is a safe point, except in a coroutine. A coroutine's start runs before the first resume, with its frame in no root yet.
    String Emit(FunctionEmitter& emitter) {
      const String parameters = Prepare(emitter);

      if (!emitter.function_.isCoroutine) {
        SafePoint(emitter);
      }

      OpenCoroutine(emitter);
      EmitBlock(emitter, emitter.function_.body);
      FinishBody(emitter);
      PatchPhis(emitter);

      return Definition(emitter, parameters);
    }

    String DefinitionScope(const FunctionEmitter& emitter) {
      return emitter.debugScope_ < 0 ? "" : " !dbg !" + to_string(emitter.debugScope_);
    }

    String Definition(FunctionEmitter& emitter, const String& parameters) {
      if (HasFrame(emitter)) {
        NeedCollector(emitter.context_);  // a function that only calls still links its record
      }

      OStringStream out;
      out << "define " << LlvmTypeText(emitter.function_.result) << " @" << QuotedName(SymbolName(emitter.function_.name)) << "(" << parameters << ")" << FunctionAttributes(emitter) << DefinitionScope(emitter) << " {\nentry:\n";
      appendLines(out, EntryLines(emitter));
      appendLines(out, TidyBlocks(FinishedLines(emitter)));
      out << "}\n";

      return out.str();
    }

    // ── the module ───────────────────────────────────────────────────────────────────────────────────────────────────────

    String StructDefinition(const StructDef& structure) {
      String fields;

      for (size_t i = 0; i < structure.fields.size(); i++) {
        fields += (i == 0 ? "" : ", ") + LlvmTypeText(structure.fields[i].type);
      }

      return StructName(structure.name) + " = type { " + fields + " }";
    }

    String ParameterTypes(const Vector<Parameter>& parameters) {
      String out;

      for (size_t i = 0; i < parameters.size(); i++) {
        out += (i == 0 ? "" : ", ") + LlvmTypeText(parameters[i].type);
      }

      return out;
    }

    // A global starts with a constant, or with zero until `polyir_init_globals` runs.
    String GlobalInitText(ModuleContext& context, const GlobalDef& global) {
      if (!IsConstantInit(global.initializer)) {
        return LlvmTypeText(global.type) + " zeroinitializer";
      }

      return ConstantInit(context, global);
    }

    // The text of a global's constant starting value.
    String ConstantInit(ModuleContext& context, const GlobalDef& global) {
      switch (global.initializer->kind) {
        case ExprKind::ConstInt:
          return LlvmTypeText(global.type) + " " + IntText(global.initializer->intValue, global.type->bits);
        case ExprKind::ConstFloat:
          return LlvmTypeText(global.type) + " " + FloatText(global.initializer->floatValue, global.type->bits);
        case ExprKind::ConstBool:
          return String("i1 ") + (global.initializer->boolValue ? "true" : "false");
        case ExprKind::ConstString:
          return "ptr " + StringGlobal(context, global.initializer->text);
        default:
          Fail("the starting value of global " + global.name + " must be a constant");
      }
    }

    String GlobalDefinition(ModuleContext& context, const GlobalDef& global) {
      return "@" + QuotedName(global.name) + " = global " + GlobalInitText(context, global);
    }

    // A throw nobody caught ends the program with a trap.
    String UncaughtCheck(ModuleContext& context) {
      if (!Throws(context)) {
        return "";
      }

      Need(context, "declare void @llvm.trap()");

      return "  %thrown = load i1, ptr @thrown\n  br i1 %thrown, label %uncaught, label %fine\nuncaught:\n  call void @llvm.trap()\n  unreachable\nfine:\n";
    }

    // The exit status of a result narrower or wider than 32 bits.
    String ResizedExit(const TypePtr& result) {
      const int bits = result->bits;
      const String how = bits > 32 ? "trunc" : (IsSigned(*result) && bits > 1 ? "sext" : "zext");

      return "  %c = " + how + " " + LlvmTypeText(result) + " %r to i32\n  ret i32 %c\n";
    }

    // How the result of `main` becomes the exit status (an i32).
    String ExitStatus(const TypePtr& result) {
      if (IsFloat(*result)) {
        return "  %c = fptosi " + LlvmTypeText(result) + " %r to i32\n  ret i32 %c\n";
      }

      if (result->bits == 32) {
        return "  ret i32 %r\n";
      }

      return ResizedExit(result);
    }

    // `int main()` for the program: it calls the user's `main` and hands its result to the system as the exit status.
    String EntryPoint(const FuncDef& main, const Module& module, ModuleContext& context) {
      const String setup = CollectorStart(module, context) + (HasGlobalInitializer(module) ? "  call void @polyir_init_globals()\n" : "");
      const String head = "define i32 @main() {\nentry:\n" + setup;

      if (IsVoid(*main.result)) {
        return head + "  call void @program_main()\n" + UncaughtCheck(context) + "  ret i32 0\n}\n";
      }

      return head + "  %r = call " + LlvmTypeText(main.result) + " @program_main()\n" + UncaughtCheck(context) + ExitStatus(main.result) + "}\n";
    }

    // The generated closure bodies and the entry point, after the module's own functions.
    void AppendGenerated(OStringStream& functions, const Module& module, ModuleContext& context) {
      for (const String& generated : Generated(context)) {
        functions << generated << "\n";
      }

      const FuncDef* main = FindFunction(module, "main");

      if (main != nullptr && main->parameters.empty()) {
        functions << EntryPoint(*main, module, context) << "\n";
      }
    }

    bool RawThrows(const Module& module) {
      for (const RawIr& raw : module.rawIr) {
        for (const String& line : raw.declarations) {
          if (line.compare(0, 8, "@thrown ") == 0) {
            return true;
          }
        }
      }

      return false;
    }

    String FunctionsText(const Module& module, ModuleContext& context) {
      OStringStream functions;

      for (const FuncDef& definition : module.functions) {
        functions << EmitFunction(context, definition) << "\n";
      }

      AppendGenerated(functions, module, context);

      return functions.str();
    }

    String TripleLine(const CompileOptions& options) {
      return options.targetTriple.empty() ? "" : "target triple = \"" + options.targetTriple + "\"\n";
    }

    void WriteHeader(OStringStream& out, const Module& module, const CompileOptions& options) {
      const String name = options.sourceFile.empty() ? module.name : options.sourceFile;
      out << "; ModuleID = '" << name << "'\nsource_filename = \"" << name << "\"\n" << TripleLine(options) << "\n";
    }

    void WriteDefinitions(OStringStream& out, const Module& module, ModuleContext& context) {
      for (const StructDef& structure : module.structs) {
        out << StructDefinition(structure) << "\n";
      }

      for (const GlobalDef& global : module.globals) {
        out << GlobalDefinition(context, global) << "\n";
      }
    }

    void WriteTables(OStringStream& out, const Module& module, const ModuleContext& context) {
      for (const String& table : Vtables(module)) {
        out << table << "\n";
      }

      for (const String& s : StringLines(context)) {
        out << s << "\n";
      }
    }

    // `declare ptr @calloc(i64, i64)`: the library function the compiler wanted, unless the program already declares one of that name.
    bool DeclaredByProgram(const Module& module, const String& declaration) {
      const size_t at = declaration.find('@');
      const size_t end = declaration.find('(', at);

      return at != String::npos && end != String::npos && FindExtern(module, declaration.substr(at + 1, end - at - 1)) != nullptr;
    }

    // A type definition the module already writes itself.
    bool DefinedByModule(const Module& module, const String& line) {
      if (line.empty() || line[0] != '%') {
        return false;
      }

      return any_of(module.structs.begin(), module.structs.end(), [&](const StructDef& structure) { return StructDefinition(structure) == line; });
    }

    void WriteNeeded(OStringStream& out, const Module& module, const ModuleContext& context) {
      for (const String& need : Needed(context)) {
        if (!DeclaredByProgram(module, need) && !DefinedByModule(module, need)) {
          out << need << "\n";
        }
      }
    }

    void WriteDeclarations(OStringStream& out, const Module& module, const ModuleContext& context) {
      for (const ExternDef& external : module.externs) {
        out << "declare " << LlvmTypeText(external.result) << " @" << QuotedName(external.name) << "(" << ParameterTypes(external.parameters) << ")\n";
      }

      WriteNeeded(out, module, context);
    }

    // The structs, globals, tables, strings and declarations, in the order they depend on each other.
    void WriteParts(OStringStream& out, const Module& module, ModuleContext& context) {
      WriteDefinitions(out, module, context);
      WriteTables(out, module, context);
      WriteDeclarations(out, module, context);
    }

    // The debug info the options ask for (none by default), its positions mapped through where the program's lines came from.
    DebugInfo DebugFor(const CompileOptions& options, const Module& module) {
      const String file = options.sourceFile.empty() ? "program" : options.sourceFile;
      DebugInfo debug = options.debugInfo ? MakeDebugInfo(file, options.sourceDirectory) : DebugInfo{};
      MapThrough(debug, module.fromLines);

      return debug;
    }

    // What a fragment needs joined to the module's own needs: its declarations, and the collector's functions when it allocates with them.
    void AdoptNeeds(const RawIr& raw, ModuleContext& context) {
      for (const String& line : raw.declarations) {
        AddDeclaration(context, line);
      }

      NeedCollectorFor(raw, context);
    }

    // The text of the fragments added with AddLlvm, and what they need joined to the module's own needs.
    String RawFunctions(const Module& module, ModuleContext& context) {
      String text;

      for (const RawIr& raw : module.rawIr) {
        const int offset = Adopt(Debug(context), raw.metadata, raw.namedMetadata);
        text += (raw.metadata.empty() ? raw.text : ShiftedMetadata(raw.text, offset)) + "\n";
        AdoptNeeds(raw, context);
      }

      return text;
    }

    // A wasm module never collects: it is a library its host calls, and the host's references are out of the collector's sight.
    bool CollectsFor(const CompileOptions& options) {
      return options.collectGarbage && options.targetTriple.find("wasm") == String::npos;
    }

    ModuleContext ContextFor(const Module& module, const CompileOptions& options) {
      ModuleContext context{module, DebugFor(options, module)};
      context.collects_ = CollectsFor(options);
      SetThrows(context, ThrowsAnywhere(module) || RawThrows(module));
      DeclareThrowFlags(context);

      return context;
    }

    String CompilePrepared(const Module& module, const CompileOptions& options) {
      ModuleContext context = ContextFor(module, options);
      const String functions = FunctionsText(module, context) + RawFunctions(module, context);

      OStringStream out;
      WriteHeader(out, module, options);
      WriteParts(out, module, context);
      out << "\n" << functions << Trailer(Debug(context));

      return out.str();
    }

    // The module with the comparison functions its `==` needs, compiled.
    String CompileModule(const Module& given, const CompileOptions& options) {
      return CompilePrepared(WithEqualityHelpers(WithGlobalInitializers(given)), options);
    }

  }  // namespace llvm

  // A `Select` is written as the `if` chain it means (Hir.hpp `WithoutSelects`); LLVM makes a switch of it where it can.
  CompileResult CompileToLlvm(const Module& module, const CompileOptions& options) {
    CompileResult result;
    try {
      result.ir = llvm::CompileModule(WithoutSelects(module), options);
    } catch (const Exception& problem) {
      result.errors.push_back(problem.what());
    }

    return result;
  }

}  // namespace polyir

namespace polyir {
  namespace llvm {

    namespace {

      // Everything the fragment relies on at module level: types, constants, library declarations and runtime helpers.
      Vector<String> fragmentDeclarations(const Module& module, const ModuleContext& context) {
        Vector<String> lines;

        for (const StructDef& structure : module.structs) {
          lines.push_back(StructDefinition(structure));
        }

        lines.insert(lines.end(), StringLines(context).begin(), StringLines(context).end());
        lines.insert(lines.end(), Needed(context).begin(), Needed(context).end());

        return lines;
      }

      ModuleContext fragmentContext(const Module& module, const CompileOptions& options) {
        ModuleContext context{module, DebugFor(options, module)};
        context.collects_ = CollectsFor(options);
        SetThrows(context, ThrowsAnywhere(module));
        DeclareThrowFlags(context);

        return context;
      }

      // The debug metadata the function used, once it is written.
      void addMetadata(FunctionIr& result, ModuleContext& context) {
        result.metadata = Nodes(Debug(context));
        result.namedMetadata = Named(Debug(context));
      }

      FunctionIr fragmentOf(const Module& module, const FuncDef& function, const CompileOptions& options) {
        ModuleContext context = fragmentContext(module, options);
        FunctionIr result;
        result.text = EmitFunction(context, function) + "\n";

        for (const String& generated : Generated(context)) {
          result.text += generated + "\n";
        }

        result.declarations = fragmentDeclarations(module, context);
        addMetadata(result, context);

        return result;
      }

    }  // namespace
  }  // namespace llvm

  FunctionIr CompileFunctionToLlvm(const Module& module, const String& function, const CompileOptions& options) {
    const Module plain = WithoutSelects(module);
    const FuncDef* found = FindFunction(plain, function);

    if (found == nullptr) {
      return FunctionIr{"", {}, {"no function called '" + function + "'"}, {}, {}};
    }

    try {
      return llvm::fragmentOf(plain, *found, options);
    } catch (const Exception& problem) {
      return FunctionIr{"", {}, {problem.what()}, {}, {}};
    }
  }

}  // namespace polyir
