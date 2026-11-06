// HIR to a WebAssembly binary for numbers and booleans (Wasm.hpp). Control flow is structured already, so an `if` is a wasm `if` and a `for` a `block` around a `loop`.
#include "polyir/Wasm.hpp"
#include "wasm/WasmBytes.hpp"

namespace polyir {

  namespace {

    using namespace wasm;

    // What the direct writer cannot do; WasmModule reports it in `errors`.
    struct Declined : RuntimeError {
      using RuntimeError::RuntimeError;
    };

    // Where a parameter or a local lives: its index among the function's locals, and its wasm type.
    struct Slot {
      uint32_t index = 0;
      ValueType type = I64;
    };

    ValueType valueTypeOf(const TypePtr& type) {
      if (IsBoolean(*type)) {
        return I32;
      }

      if (IsInteger(*type) && type->bits == 64) {
        return I64;
      }

      if (IsFloat(*type) && type->bits == 64) {
        return F64;
      }

      throw Declined("wasm without a toolchain handles i64, f64 and i1 only, not " + TypeText(*type) + " (install a wasm32 clang and wasm-ld for the rest)");
    }

    const Map<BinOp, uint8_t> kFloatOps = {{BinOp::Add, F64_ADD}, {BinOp::Sub, F64_SUB}, {BinOp::Mul, F64_MUL}, {BinOp::Div, F64_DIV}, {BinOp::Eq, F64_EQ}, {BinOp::Ne, F64_NE},
                                           {BinOp::Lt, F64_LT},   {BinOp::Le, F64_LE},   {BinOp::Gt, F64_GT},   {BinOp::Ge, F64_GE}};

    const Map<BinOp, uint8_t> kBoolOps = {{BinOp::Eq, I32_EQ}, {BinOp::Identical, I32_EQ}, {BinOp::Ne, I32_NE}, {BinOp::NotIdentical, I32_NE},
                                          {BinOp::Xor, I32_XOR}, {BinOp::And, I32_AND}, {BinOp::Or, I32_OR}};

    const Map<BinOp, uint8_t> kIntegerOps = {{BinOp::Add, I64_ADD}, {BinOp::Sub, I64_SUB}, {BinOp::Mul, I64_MUL}, {BinOp::And, I64_AND}, {BinOp::Or, I64_OR}, {BinOp::Xor, I64_XOR},
                                             {BinOp::Shl, I64_SHL}, {BinOp::Eq, I64_EQ},   {BinOp::Ne, I64_NE},   {BinOp::Identical, I64_EQ}, {BinOp::NotIdentical, I64_NE}};

    const Map<BinOp, uint8_t> kSignedOps = {{BinOp::Div, I64_DIV_S}, {BinOp::Rem, I64_REM_S}, {BinOp::Shr, I64_SHR_S}, {BinOp::Lt, I64_LT_S},
                                            {BinOp::Le, I64_LE_S},   {BinOp::Gt, I64_GT_S},   {BinOp::Ge, I64_GE_S}};

    const Map<BinOp, uint8_t> kUnsignedOps = {{BinOp::Div, I64_DIV_U}, {BinOp::Rem, I64_REM_U}, {BinOp::Shr, I64_SHR_U}, {BinOp::Lt, I64_LT_U},
                                              {BinOp::Le, I64_LE_U},   {BinOp::Gt, I64_GT_U},   {BinOp::Ge, I64_GE_U}};

    uint8_t opcodeIn(const Map<BinOp, uint8_t>& table, BinOp o) {
      const auto found = table.find(o);

      if (found == table.end()) {
        throw Declined("wasm: this operator needs the LLVM route and its toolchain (install a wasm32 clang and wasm-ld)");
      }

      return found->second;
    }

    uint8_t integerOpcode(BinOp o, bool isSigned) {
      if (kIntegerOps.count(o) != 0) {
        return kIntegerOps.at(o);
      }

      return opcodeIn(isSigned ? kSignedOps : kUnsignedOps, o);
    }

    uint8_t binaryOpcode(const Expr& e) {
      const TypePtr& type = e.operands[0]->type;
      const ValueType operandType = valueTypeOf(type);

      if (operandType == F64) {
        return opcodeIn(kFloatOps, e.binaryOp);
      }

      return operandType == I32 ? opcodeIn(kBoolOps, e.binaryOp) : integerOpcode(e.binaryOp, IsSigned(*type));
    }

    // The function being written: the index of every function of the module (for calls), its definition, the scopes that name its
    // locals (innermost last), the types of the locals beyond the parameters, the next local index, and the instructions so far.
    struct FunctionWriter {
      const Map<String, uint32_t>& functionIndices;
      const FuncDef& function;
      Vector<Map<String, Slot>> scopes{};
      Vector<ValueType> locals{};
      uint32_t nextIndex = 0;
      Bytes code{};
    };

    // Statements and expressions contain each other, so `block` and `expression` are both declared here, before the functions that call them.
    void block(FunctionWriter& writer, const StmtList& list);

    void expression(FunctionWriter& writer, const Expr& e);

    // ── the instructions and the locals ──

    void op(FunctionWriter& writer, uint8_t opcode) {
      writer.code.push_back(opcode);
    }

    void indexed(FunctionWriter& writer, uint8_t opcode, uint64_t index) {
      op(writer, opcode);
      UnsignedLeb(writer.code, index);
    }

    // A parameter or a local, numbered in the order they are declared; only a local adds to `locals`.
    Slot declare(FunctionWriter& writer, const String& name, const TypePtr& type, bool isParameter = false) {
      const Slot slot{writer.nextIndex++, valueTypeOf(type)};

      if (!isParameter) {
        writer.locals.push_back(slot.type);
      }

      writer.scopes.back()[name] = slot;

      return slot;
    }

    Slot slotOf(const FunctionWriter& writer, const String& name) {
      for (size_t i = writer.scopes.size(); i > 0; --i) {
        const auto found = writer.scopes[i - 1].find(name);

        if (found != writer.scopes[i - 1].end()) {
          return found->second;
        }
      }

      throw Declined("wasm: unknown variable " + name);
    }

    // ── expressions ──

    // A bool constant is 1 or 0; a whole-number constant its value.
    int64_t wholeValueOf(const Expr& e) {
      if (e.kind == ExprKind::ConstBool) {
        return e.boolValue ? 1 : 0;
      }

      return e.intValue;
    }

    void doubleConstant(FunctionWriter& writer, double value) {
      op(writer, F64_CONST);
      DoubleBytes(writer.code, value);
    }

    void constant(FunctionWriter& writer, const Expr& e) {
      if (e.kind == ExprKind::ConstFloat) {
        return doubleConstant(writer, e.floatValue);
      }

      op(writer, valueTypeOf(e.type) == I32 ? I32_CONST : I64_CONST);
      SignedLeb(writer.code, wholeValueOf(e));
    }

    void read(FunctionWriter& writer, const Expr& e) {
      indexed(writer, LOCAL_GET, slotOf(writer, e.name).index);
    }

    void call(FunctionWriter& writer, const Expr& e) {
      const auto found = writer.functionIndices.find(e.name);

      if (found == writer.functionIndices.end()) {
        throw Declined("wasm: " + e.name + " is not defined in the module (a runtime function needs the LLVM route and its toolchain)");
      }

      for (const ExprPtr& argument : e.operands) {
        expression(writer, *argument);
      }

      indexed(writer, CALL, found->second);
    }

    void bitNot(FunctionWriter& writer) {
      op(writer, I64_CONST);
      SignedLeb(writer.code, -1);
      op(writer, I64_XOR);
    }

    // 0 - x: the operand is already on the stack, so the zero goes under it through a swap through a local.
    void negateInteger(FunctionWriter& writer) {
      const Slot scratch = declare(writer, "", MakeInt(64));
      indexed(writer, LOCAL_SET, scratch.index);

      op(writer, I64_CONST);
      SignedLeb(writer.code, 0);
      indexed(writer, LOCAL_GET, scratch.index);
      op(writer, I64_SUB);
    }

    void unary(FunctionWriter& writer, const Expr& e) {
      expression(writer, *e.operands[0]);

      if (e.unaryOp == UnOp::LogNot) {
        return op(writer, I32_EQZ);
      }

      if (e.unaryOp == UnOp::BitNot) {
        return bitNot(writer);
      }

      if (valueTypeOf(e.type) == F64) {
        return op(writer, F64_NEG);
      }

      negateInteger(writer);
    }

    uint8_t conversionOpcode(ValueType from, ValueType to, bool isSigned, const Expr& e) {
      if (from == I64 && to == F64) {
        return isSigned ? F64_CONVERT_I64_S : F64_CONVERT_I64_U;
      }

      if (from == F64 && to == I64) {
        return I64_TRUNC_F64_S;
      }

      if (from == I32 && to == I64) {
        return I64_EXTEND_I32_U;
      }

      throw Declined("wasm: cannot convert to " + TypeText(*e.type));
    }

    void convert(FunctionWriter& writer, const Expr& e) {
      const Expr& operand = *e.operands[0];
      expression(writer, operand);
      const ValueType from = valueTypeOf(operand.type);
      const ValueType to = valueTypeOf(e.type);

      if (from != to) {
        op(writer, conversionOpcode(from, to, IsSigned(*operand.type), e));
      }
    }

    // One arm of the `if` a short circuit becomes: the right side where it decides the answer, else the constant the left side decided.
    void shortArm(FunctionWriter& writer, const Expr& e, bool whenTrue) {
      const bool isAnd = e.binaryOp == BinOp::LogAnd;

      if (whenTrue == isAnd) {
        return expression(writer, *e.operands[1]);
      }

      indexed(writer, I32_CONST, isAnd ? 0 : 1);
    }

    // `a and b` is `if a then b else false`, `a or b` is `if a then true else b`: the right side runs only when it must.
    void shortCircuit(FunctionWriter& writer, const Expr& e) {
      expression(writer, *e.operands[0]);
      op(writer, IF);
      op(writer, I32);

      shortArm(writer, e, true);
      op(writer, ELSE);
      shortArm(writer, e, false);
      op(writer, END);
    }

    void binary(FunctionWriter& writer, const Expr& e) {
      if (e.binaryOp == BinOp::LogAnd || e.binaryOp == BinOp::LogOr) {
        return shortCircuit(writer, e);
      }

      expression(writer, *e.operands[0]);
      expression(writer, *e.operands[1]);
      op(writer, binaryOpcode(e));
    }

    // The conversions and calls; any other kind of expression needs the LLVM route.
    void otherExpression(FunctionWriter& writer, const Expr& e) {
      switch (e.kind) {
        case ExprKind::Cast: case ExprKind::Coerce:
          return convert(writer, e);
        case ExprKind::Call:
          return call(writer, e);
        default:
          throw Declined("wasm without a toolchain does not handle this kind of expression (install a wasm32 clang and wasm-ld)");
      }
    }

    void expression(FunctionWriter& writer, const Expr& e) {
      switch (e.kind) {
        case ExprKind::ConstInt: case ExprKind::ConstFloat: case ExprKind::ConstBool:
          return constant(writer, e);
        case ExprKind::Local: case ExprKind::LoadSlot:
          return read(writer, e);
        case ExprKind::Binary:
          return binary(writer, e);
        case ExprKind::Unary:
          return unary(writer, e);
        default:
          return otherExpression(writer, e);
      }
    }

    // ── statements ──

    void declaration(FunctionWriter& writer, const Stmt& s) {
      if (s.value) {
        expression(writer, *s.value);
      }

      const Slot slot = declare(writer, s.name, s.type);

      if (s.value) {
        indexed(writer, LOCAL_SET, slot.index);
      }
    }

    void assignment(FunctionWriter& writer, const Stmt& s) {
      if (s.target->kind != ExprKind::Local && s.target->kind != ExprKind::LoadSlot) {
        throw Declined("wasm: only a variable can be assigned");
      }

      expression(writer, *s.value);
      indexed(writer, LOCAL_SET, slotOf(writer, s.target->name).index);
    }

    void branch(FunctionWriter& writer, const Stmt& s) {
      expression(writer, *s.value);
      op(writer, IF);
      op(writer, EMPTY_BLOCK);
      block(writer, s.body);

      if (!s.elseBody.empty()) {
        op(writer, ELSE);
      }

      if (!s.elseBody.empty()) {
        block(writer, s.elseBody);
      }

      op(writer, END);
    }

    void counter(FunctionWriter& writer, const Stmt& s, size_t i) {
      expression(writer, *s.initializers[i]);
      indexed(writer, LOCAL_SET, declare(writer, s.names[i], s.initializers[i]->type).index);
    }

    void step(FunctionWriter& writer, const Stmt& s, size_t i) {
      expression(writer, *s.steps[i]);
      indexed(writer, LOCAL_SET, slotOf(writer, s.names[i]).index);
    }

    // A block to break out of (`br 1` from inside the loop leaves it), around the loop to branch back to (`br 0` starts the next round).
    void openBlockAndLoop(FunctionWriter& writer) {
      op(writer, BLOCK);
      op(writer, EMPTY_BLOCK);
      op(writer, LOOP);
      op(writer, EMPTY_BLOCK);
    }

    void closeLoopAndBlock(FunctionWriter& writer) {
      op(writer, END);  // the loop
      op(writer, END);  // the block around it
    }

    // The test (leave when it is false), the body, the steps, and back to the test.
    void loopBody(FunctionWriter& writer, const Stmt& s) {
      expression(writer, *s.value);
      op(writer, I32_EQZ);
      indexed(writer, BR_IF, 1);
      block(writer, s.body);

      for (size_t i = 0; i < s.steps.size(); ++i) {
        step(writer, s, i);
      }

      indexed(writer, BR, 0);
    }

    void loop(FunctionWriter& writer, const Stmt& s) {
      writer.scopes.emplace_back();

      for (size_t i = 0; i < s.names.size(); ++i) {
        counter(writer, s, i);
      }

      openBlockAndLoop(writer);
      loopBody(writer, s);
      closeLoopAndBlock(writer);
      writer.scopes.pop_back();
    }

    void giveBack(FunctionWriter& writer, const Stmt& s) {
      if (s.value && !IsVoid(*s.value->type)) {
        expression(writer, *s.value);
      }

      op(writer, RETURN);
    }

    void discard(FunctionWriter& writer, const Stmt& s) {
      expression(writer, *s.value);

      if (!IsVoid(*s.value->type)) {
        op(writer, DROP);
      }
    }

    // A return and an expression statement; any other kind of statement needs the LLVM route.
    void otherStatement(FunctionWriter& writer, const Stmt& s) {
      switch (s.kind) {
        case StmtKind::Return:
          return giveBack(writer, s);
        case StmtKind::ExprStmt:
          return discard(writer, s);
        default:
          throw Declined("wasm without a toolchain does not handle this kind of statement (install a wasm32 clang and wasm-ld)");
      }
    }

    void statement(FunctionWriter& writer, const Stmt& s) {
      switch (s.kind) {
        case StmtKind::Let: case StmtKind::Var:
          return declaration(writer, s);
        case StmtKind::Assign:
          return assignment(writer, s);
        case StmtKind::If:
          return branch(writer, s);
        case StmtKind::For:
          return loop(writer, s);
        default:
          return otherStatement(writer, s);
      }
    }

    void block(FunctionWriter& writer, const StmtList& list) {
      writer.scopes.emplace_back();

      for (const StmtPtr& s : list) {
        statement(writer, *s);
      }

      writer.scopes.pop_back();
    }

    // ── a whole function ──

    FunctionBinary finished(FunctionWriter& writer, FunctionBinary out) {
      out.locals = writer.locals;
      out.code = writer.code;

      return out;
    }

    // `unreachable` closes the body of a function with a result, so the end of the body needs no value on the stack. The function's
    // value leaves through `return`.
    FunctionBinary write(FunctionWriter& writer) {
      FunctionBinary out;
      writer.scopes.emplace_back();

      for (const Parameter& p : writer.function.parameters) {
        out.parameters.push_back(declare(writer, p.name, p.type, true).type);
      }

      if (!IsVoid(*writer.function.result)) {
        out.results.push_back(valueTypeOf(writer.function.result));
      }

      block(writer, writer.function.body);

      if (!IsVoid(*writer.function.result)) {
        op(writer, UNREACHABLE);
      }

      return finished(writer, out);
    }

    Map<String, uint32_t> indicesOf(const Module& module) {
      Map<String, uint32_t> indices;

      for (const FuncDef& f : module.functions) {
        indices[f.name] = static_cast<uint32_t>(indices.size());
      }

      return indices;
    }

    String exportNameOf(const String& function, const Vector<WasmExport>& exports) {
      for (const WasmExport& e : exports) {
        if (e.function == function) {
          return e.exportName;
        }
      }

      return "";
    }

    FunctionBinary functionBinary(const Map<String, uint32_t>& indices, const FuncDef& f, const Vector<WasmExport>& exports) {
      FunctionWriter writer{indices, f};
      FunctionBinary binary = write(writer);
      binary.exportName = exportNameOf(f.name, exports);

      return binary;
    }

  }  // namespace

  static Vector<FunctionBinary> functionsOf(const Module& module, const Vector<WasmExport>& exports) {
    const Map<String, uint32_t> indices = indicesOf(module);
    Vector<FunctionBinary> functions;

    for (const FuncDef& f : module.functions) {
      functions.push_back(functionBinary(indices, f, exports));
    }

    return functions;
  }

  WasmResult WasmModule(const Module& module, const Vector<WasmExport>& exports) {
    WasmResult result;
    try {
      result.bytes = ModuleBytes(functionsOf(WithoutSelects(module), exports));  // a `Select` as the `if` chain it means
    } catch (const Declined& problem) {
      result.errors.push_back(problem.what());
    }

    return result;
  }

}  // namespace polyir
