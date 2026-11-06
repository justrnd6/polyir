// HIR expressions as SPIR-V instructions (SpirvExpr.nina). Each returns the ID of the value it computed. SPIR-V is strictly typed, so operands are
// first converted to a common type, and a literal is written directly as a constant of the type it is used as.
#include "gpu/KernelCommon.hpp"
#include "gpu/SpirvLower.hpp"

namespace polyir {

  namespace {

    // The instruction numbers of the GLSL.std.450 extended instruction set that the math functions use.
    constexpr int kGlslFAbs = 4;
    constexpr int kGlslFloor = 8;
    constexpr int kGlslCeil = 9;
    constexpr int kGlslSin = 13;
    constexpr int kGlslCos = 14;
    constexpr int kGlslTan = 15;
    constexpr int kGlslPow = 26;
    constexpr int kGlslExp = 27;
    constexpr int kGlslLog = 28;
    constexpr int kGlslSqrt = 31;
    constexpr int kGlslFMin = 37;
    constexpr int kGlslFMax = 40;

    // The instruction of GLSL.std.450 for a C library function (anything not listed, the vector functions included, is FMax, as in the Nina writer).
    int mathCode(const String& callee) {
      static const Map<String, int> kCodes = {{"sqrtf", kGlslSqrt}, {"sinf", kGlslSin}, {"cosf", kGlslCos}, {"tanf", kGlslTan}, {"expf", kGlslExp},
        {"logf", kGlslLog}, {"powf", kGlslPow}, {"floorf", kGlslFloor}, {"ceilf", kGlslCeil}, {"fabsf", kGlslFAbs}, {"fminf", kGlslFMin}};
      const auto found = kCodes.find(callee);

      return found == kCodes.end() ? kGlslFMax : found->second;
    }

    bool comparesValues(BinOp op) {
      return op >= BinOp::Eq && op <= BinOp::Ge;
    }

    bool isArithmetic(BinOp op) {
      return op == BinOp::Add || op == BinOp::Sub || op == BinOp::Mul || op == BinOp::Div || op == BinOp::Rem;
    }

    bool isBitwise(BinOp op) {
      return op == BinOp::And || op == BinOp::Or || op == BinOp::Xor || op == BinOp::Shl || op == BinOp::Shr;
    }

    // `==`: ordered for floats (false when either side is not a number).
    int equalCode(bool isFloat, bool isFlag) {
      if (isFloat) {
        return OpFOrdEqual;
      }

      if (isFlag) {
        return OpLogicalEqual;
      }

      return OpIEqual;
    }

    // `!=`: unordered for floats, so it is true when either side is not a number, as in C.
    int notEqualCode(bool isFloat, bool isFlag) {
      if (isFloat) {
        return OpFUnordNotEqual;
      }

      if (isFlag) {
        return OpLogicalNotEqual;
      }

      return OpINotEqual;
    }

    // `<`, `<=`, `>`, `>=`: ordered for floats; flags order as signed whole numbers do. Any other operator that reaches here (`===`,
    // `!==`, `&&`, `||`) is given the codes of `>=`, as the Nina writer gives it.
    int orderCode(BinOp op, bool isFloat) {
      switch (op) {
        case BinOp::Lt:
          return isFloat ? OpFOrdLessThan : OpSLessThan;
        case BinOp::Le:
          return isFloat ? OpFOrdLessThanEqual : OpSLessThanEqual;
        case BinOp::Gt:
          return isFloat ? OpFOrdGreaterThan : OpSGreaterThan;
        default:
          return isFloat ? OpFOrdGreaterThanEqual : OpSGreaterThanEqual;
      }
    }

    int comparisonCode(BinOp op, bool isFloat, bool isFlag) {
      if (op == BinOp::Eq) {
        return equalCode(isFloat, isFlag);
      }

      if (op == BinOp::Ne) {
        return notEqualCode(isFloat, isFlag);
      }

      return orderCode(op, isFloat);
    }

    // `Rem` here is the whole-number one: a float remainder is a sequence of instructions (Remainder in SpirvAtomic.cpp).
    int arithmeticCode(BinOp op, bool isFloat) {
      switch (op) {
        case BinOp::Add:
          return isFloat ? OpFAdd : OpIAdd;
        case BinOp::Sub:
          return isFloat ? OpFSub : OpISub;
        case BinOp::Mul:
          return isFloat ? OpFMul : OpIMul;
        case BinOp::Div:
          return isFloat ? OpFDiv : OpSDiv;
        default:
          return OpSRem;
      }
    }

    // `&`, `|`, `^`, `<<` and `>>` on whole numbers (`>>` keeps the sign).
    int bitwiseCode(BinOp op) {
      switch (op) {
        case BinOp::And:
          return OpBitwiseAnd;
        case BinOp::Or:
          return OpBitwiseOr;
        case BinOp::Xor:
          return OpBitwiseXor;
        case BinOp::Shl:
          return OpShiftLeftLogical;
        default:
          return OpShiftRightArithmetic;
      }
    }

    // The opcode of an operator for floats, whole numbers or flags (`&` and `|` on flags are the logical ones).
    int opcodeOf(BinOp op, bool isFloat, bool isFlag) {
      if (isFlag && (op == BinOp::And || op == BinOp::Or)) {
        return op == BinOp::And ? OpLogicalAnd : OpLogicalOr;
      }

      if (isArithmetic(op)) {
        return arithmeticCode(op, isFloat);
      }

      if (isBitwise(op)) {
        return bitwiseCode(op);
      }

      return comparisonCode(op, isFloat, isFlag);
    }

    int unaryCode(UnOp op, bool isFloat) {
      switch (op) {
        case UnOp::Neg:
          return isFloat ? OpFNegate : OpSNegate;
        case UnOp::LogNot:
          return OpLogicalNot;
        default:
          return OpNot;
      }
    }

    // Both operands in one type: the left one's, unless it is a plain number and the right one a vector.
    TypePtr commonType(const TypePtr& left, const TypePtr& right) {
      return !SpvIsVector(left) && SpvIsVector(right) ? right : left;
    }

    // `&&` goes on to the right side when the left one is true, `||` when it is false; the other path jumps straight to `merge`.
    void branchOnLeft(SpvBody& body, bool isAnd, Id left, Id rightBlock, Id merge) {
      if (isAnd) {
        BranchIf(body.function, left, rightBlock, merge);
      } else {
        BranchIf(body.function, left, merge, rightBlock);
      }
    }

    // Computes the right side only on the path where the left side has not decided the answer, then joins the two paths.
    void rightSideWhenNeeded(SpvBody& body, const SpvScope& scope, const Expr& e, Id left, Id answer) {
      const Id rightBlock = Fresh(body.module);
      const Id merge = Fresh(body.module);
      SelectionMerge(body.function, merge);
      branchOnLeft(body, e.binaryOp == BinOp::LogAnd, left, rightBlock, merge);
      RightSide(body, scope, *e.operands[1], answer, rightBlock, merge);
    }

    // The kernel written earlier under this name; null when there is none.
    const SpvKnown* knownFunctionNamed(const Vector<SpvKnown>& functions, const String& name) {
      for (const SpvKnown& function : functions) {
        if (function.name == name) {
          return &function;
        }
      }

      return nullptr;
    }

    // The arguments that are not arrays, each converted to the type of the parameter it fills. An array parameter is a global the
    // callee reaches by name.
    Words callArguments(SpvBody& body, const SpvScope& scope, const Expr& e, const SpvKnown& known) {
      Words ids;

      for (const ExprPtr& argument : e.operands) {
        if (argument->type->kind == TypeKind::Pointer) {
          continue;
        }

        const TypePtr& parameterType = known.params[ids.size()];  // the next parameter: one per argument passed so far
        ids.push_back(ExprTo(body, scope, *argument, parameterType));
      }

      return ids;
    }

  }  // namespace

  Id LowerExpr(SpvBody& body, const SpvScope& scope, const Expr& e) {
    switch (e.kind) {
      case ExprKind::ConstInt: case ExprKind::ConstFloat: case ExprKind::ConstBool:
        return LowerLiteral(body, e);
      case ExprKind::Local: case ExprKind::LoadSlot:
        return Read(body, scope, e.name);
      case ExprKind::Binary:
        return Binary(body, scope, e);
      default:
        return Compound(body, scope, e);
    }
  }

  // A whole number is a constant of its own type; a float is 32-bit.
  Id LowerLiteral(SpvBody& body, const Expr& e) {
    if (e.kind == ExprKind::ConstInt) {
      return SpvConstant(body.module, e.type, static_cast<double>(e.intValue));
    }

    return e.kind == ExprKind::ConstFloat ? ConstFloat(body.module, e.floatValue) : ConstBool(body.module, e.boolValue);
  }

  Id Compound(SpvBody& body, const SpvScope& scope, const Expr& e) {
    switch (e.kind) {
      case ExprKind::Unary:
        return Unary(body, scope, e);
      case ExprKind::Cast: case ExprKind::Coerce:
        return ExprTo(body, scope, *e.operands[0], e.type);
      case ExprKind::VecLit:
        return VectorLiteral(body, scope, e);
      case ExprKind::IndexGet:
        return Index(body, scope, e);
      default:
        return e.kind == ExprKind::Call ? Call(body, scope, e) : Unsupported(body, "an expression the SPIR-V writer does not handle");
    }
  }

  // Records the problem (the module is then rejected) and hands back a placeholder 0, so lowering can carry on and report any
  // further problems.
  Id Unsupported(SpvBody& body, const String& what) {
    ReportProblem(body.module, what);

    return ConstInt(body.module, 32, 0);
  }

  // A `let` or a parameter is its value; a `var` or a loop variable is loaded from its variable.
  Id Read(SpvBody& body, const SpvScope& scope, const String& name) {
    for (auto b = scope.rbegin(); b != scope.rend(); ++b) {
      if (b->name == name) {
        return b->isVar ? Load(body.function, SpvType(body.module, b->type), b->id) : b->id;
      }
    }

    return Unsupported(body, "the name " + name + " is not in scope");
  }

  // `e` as a value of type `target`: a literal is made directly in that type, anything else is computed and converted.
  Id ExprTo(SpvBody& body, const SpvScope& scope, const Expr& e, const TypePtr& target) {
    if (e.kind == ExprKind::ConstInt) {
      return LiteralTo(body, scope, e, target, static_cast<double>(e.intValue));
    }

    if (e.kind == ExprKind::ConstFloat) {
      return LiteralTo(body, scope, e, target, e.floatValue);
    }

    const Id id = LowerExpr(body, scope, e);

    return Convert(body, id, e.type, target);
  }

  Id LiteralTo(SpvBody& body, const SpvScope& scope, const Expr& e, const TypePtr& target, double value) {
    if (SpvIsNumber(SpvScalar(target))) {
      return SpvConstant(body.module, target, value);
    }

    const Id id = LowerExpr(body, scope, e);

    return Convert(body, id, e.type, target);
  }

  // The same number of another width, an integer as a float or back, or a bool as 0 or 1. A scalar is copied into every lane of a vector.
  Id Convert(SpvBody& body, Id id, const TypePtr& from, const TypePtr& to) {
    const Id toType = SpvType(body.module, to);

    if (SpvType(body.module, from) == toType) {
      return id;
    }

    const bool spreadsScalarOverLanes = SpvIsVector(to) && !SpvIsVector(from);

    if (!spreadsScalarOverLanes) {
      return ConvertNumber(body, id, from, to);
    }

    const Id lane = Convert(body, id, from, SpvScalar(to));

    return ValueOf(body.function, OpCompositeConstruct, toType, SpvRepeat(lane, SpvLanes(to)));
  }

  Id ConvertNumber(SpvBody& body, Id id, const TypePtr& from, const TypePtr& to) {
    const Id toType = SpvType(body.module, to);

    if (SpvIsBool(from)) {
      return BoolToNumber(body, id, to);
    }

    if (SpvIsBool(to)) {
      return NumberToBool(body, id, from);
    }

    if (SpvIsFloat(from) && SpvIsFloat(to)) {
      return id;
    }

    if (SpvIsFloat(from) || SpvIsFloat(to)) {
      return ValueOf(body.function, SpvIsFloat(from) ? OpConvertFToS : OpConvertSToF, toType, {id});
    }

    return ValueOf(body.function, OpSConvert, toType, {id});
  }

  Id BoolToNumber(SpvBody& body, Id flag, const TypePtr& to) {
    const Id one = SpvConstant(body.module, to, 1.0);
    const Id zero = SpvConstant(body.module, to, 0.0);
    const Id type = SpvType(body.module, to);

    return ValueOf(body.function, OpSelect, type, {flag, one, zero});
  }

  Id NumberToBool(SpvBody& body, Id id, const TypePtr& from) {
    const Id zero = SpvConstant(body.module, from, 0.0);
    const Id type = TypeBool(body.module);

    return ValueOf(body.function, SpvIsFloat(from) ? OpFUnordNotEqual : OpINotEqual, type, {id, zero});
  }

  Id Binary(SpvBody& body, const SpvScope& scope, const Expr& e) {
    if (e.binaryOp == BinOp::LogAnd || e.binaryOp == BinOp::LogOr) {
      return ShortCircuit(body, scope, e);
    }

    const TypePtr common = commonType(e.operands[0]->type, e.operands[1]->type);
    const Id a = ExprTo(body, scope, *e.operands[0], common);
    const Id b = ExprTo(body, scope, *e.operands[1], common);

    return Arithmetic(body, e.binaryOp, common, a, b);
  }

  // `a && b` and `a || b` compute `b` only when `a` has not decided the answer, as in C. The answer is kept in a variable, and `b` is computed in
  // a block the decided case jumps over.
  Id ShortCircuit(SpvBody& body, const SpvScope& scope, const Expr& e) {
    const Id boolType = TypeBool(body.module);
    const Id answer = Variable(body.function, boolType);
    const Id left = ExprTo(body, scope, *e.operands[0], SpvFlag());
    Store(body.function, answer, left);
    rightSideWhenNeeded(body, scope, e, left, answer);

    return Load(body.function, boolType, answer);
  }

  void RightSide(SpvBody& body, const SpvScope& scope, const Expr& right, Id answer, Id block, Id merge) {
    Begin(body.function, block);
    const Id value = ExprTo(body, scope, right, SpvFlag());
    Store(body.function, answer, value);
    Branch(body.function, merge);
    Begin(body.function, merge);
  }

  // One arithmetic or comparison instruction on two values of type `t`.
  Id Arithmetic(SpvBody& body, BinOp op, const TypePtr& t, Id a, Id b) {
    if (op == BinOp::Rem && SpvIsFloat(t)) {
      return Remainder(body, t, a, b);
    }

    const int code = opcodeOf(op, SpvIsFloat(t), SpvIsBool(t));
    const Id type = comparesValues(op) ? TypeBool(body.module) : SpvType(body.module, t);

    return ValueOf(body.function, code, type, {a, b});
  }

  Id Unary(SpvBody& body, const SpvScope& scope, const Expr& e) {
    const TypePtr& t = e.operands[0]->type;
    const Id v = LowerExpr(body, scope, *e.operands[0]);
    const Id type = SpvType(body.module, t);

    return ValueOf(body.function, unaryCode(e.unaryOp, SpvIsFloat(t)), type, {v});
  }

  Id VectorLiteral(SpvBody& body, const SpvScope& scope, const Expr& e) {
    const TypePtr laneType = SpvScalar(e.type);
    Words ids;

    for (const ExprPtr& x : e.operands) {
      ids.push_back(ExprTo(body, scope, *x, laneType));
    }

    return ValueOf(body.function, OpCompositeConstruct, SpvType(body.module, e.type), ids);
  }

  // `base[i]`: a lane of a vector, or one slot of an array parameter or of workgroup memory.
  Id Index(SpvBody& body, const SpvScope& scope, const Expr& e) {
    if (SpvIsVector(e.operands[0]->type)) {
      return Lane(body, scope, e);
    }

    const SpvArray* array = ArrayOf(body, *e.operands[0]);

    if (array == nullptr) {
      return Unsupported(body, "only a vector or an array parameter can be indexed in a SPIR-V kernel");
    }

    const Id slot = ExprTo(body, scope, *e.operands[1], SpvInt32());

    return SlotValue(body, *array, slot);
  }

  // One lane of a vector: a literal position names it directly, any other is looked up at run time.
  Id Lane(SpvBody& body, const SpvScope& scope, const Expr& e) {
    const Id vector = LowerExpr(body, scope, *e.operands[0]);
    const Id type = SpvType(body.module, e.type);
    const Expr& position = *e.operands[1];

    if (position.kind == ExprKind::ConstInt) {
      return ValueOf(body.function, OpCompositeExtract, type, {vector, position.intValue});
    }

    const Id at = ExprTo(body, scope, position, SpvInt32());

    return ValueOf(body.function, OpVectorExtractDynamic, type, {vector, at});
  }

  // The array an expression names: an array parameter or workgroup memory is a local of that name.
  const SpvArray* ArrayOf(const SpvBody& body, const Expr& e) {
    return e.kind == ExprKind::Local ? SpvArrayNamed(body.writer.arrays, e.name) : nullptr;
  }

  // A pointer to a slot. A storage buffer is a struct holding one array, so its first index is the member (0). Workgroup memory is the array itself.
  Id SlotPointer(SpvBody& body, const SpvArray& array, Id slot) {
    const Id stored = StoredType(body, array);
    const Id pointer = TypePointer(body.module, array.storage, stored);

    if (array.storage == kStorageWorkgroup) {
      return ValueOf(body.function, OpAccessChain, pointer, {array.variable, slot});
    }

    const Id member = ConstInt(body.module, 32, 0);

    return ValueOf(body.function, OpAccessChain, pointer, {array.variable, member, slot});
  }

  // Raw words for an array updated with `atomicAddFloat`, its own type otherwise.
  Id StoredType(SpvBody& body, const SpvArray& array) {
    return array.holdsFloatBits ? TypeUint(body.module) : SpvType(body.module, array.element);
  }

  Id SlotValue(SpvBody& body, const SpvArray& array, Id slot) {
    const Id type = StoredType(body, array);
    const Id loaded = Load(body.function, type, SlotPointer(body, array, slot));

    if (!array.holdsFloatBits) {
      return loaded;
    }

    const Id floatType = TypeFloat(body.module);

    return ValueOf(body.function, OpBitcast, floatType, {loaded});
  }

  void StoreSlot(SpvBody& body, const SpvArray& array, Id slot, Id id) {
    Id stored = id;

    if (array.holdsFloatBits) {
      stored = ValueOf(body.function, OpBitcast, TypeUint(body.module), {id});
    }

    const Id pointer = SlotPointer(body, array, slot);
    Store(body.function, pointer, stored);
  }

  Id Call(SpvBody& body, const SpvScope& scope, const Expr& e) {
    if (IsKernelMath(e.name)) {
      return Math(body, scope, e);
    }

    if (IsKernelAtomic(e.name)) {
      return Atomic(body, scope, e);
    }

    if (IsGroupFunction(e.name)) {
      return Group(body, scope, e);
    }

    return KernelCall(body, scope, e);
  }

  // A math function: a built-in of GLSL.std.450, or the remainder sequence for `fmodf`.
  Id Math(SpvBody& body, const SpvScope& scope, const Expr& e) {
    const Id floatType = TypeFloat(body.module);
    Words ids;

    for (const ExprPtr& a : e.operands) {
      ids.push_back(ExprTo(body, scope, *a, SpvFloat32()));
    }

    if (e.name == "fmodf") {
      return Remainder(body, SpvFloat32(), ids[0], ids[1]);
    }

    return ValueOf(body.function, OpExtInst, floatType, Concat({body.writer.glsl, mathCode(e.name)}, ids));
  }

  // A call to a kernel written earlier: the arguments that are not arrays, each converted to the type of the parameter it fills.
  Id KernelCall(SpvBody& body, const SpvScope& scope, const Expr& e) {
    const SpvKnown* known = knownFunctionNamed(body.writer.functions, e.name);

    if (known == nullptr) {
      return Unsupported(body, e.name + " is not available to the SPIR-V writer here");
    }

    const Words ids = Concat({known->id}, callArguments(body, scope, e, *known));

    return ValueOf(body.function, OpFunctionCall, SpvType(body.module, known->returns), ids);
  }

}  // namespace polyir
