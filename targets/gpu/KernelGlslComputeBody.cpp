// The text of a compute kernel's types, statements and expressions in Vulkan GLSL (KernelGlsl.nina `typeText`, `stmt`, `expr`). It differs from the
// Metal text where GLSL is stricter: a number stored in a number variable is converted explicitly (`int(acc + i)`), the right operand of an operator
// is converted to the left one's type, a whole-number constant too wide for 32 bits gets the suffix `l`, and a float remainder is `gpu_fmod`.
#include "gpu/KernelGlslComputeBody.hpp"
#include "gpu/KernelCommon.hpp"

namespace polyir {

  namespace {

    String intName(int bits) {
      if (bits == 1) {
        return "bool";
      }

      if (bits == 8) {
        return "int8_t";
      }

      if (bits == 16) {
        return "int16_t";
      }

      return bits == 32 ? "int" : "int64_t";
    }

    String vectorPrefix(const TypePtr& element) {
      if (element->kind != TypeKind::Int) {
        return "";
      }

      return element->bits == 1 ? "b" : "i";
    }

    bool isFloatLike(const TypePtr& t) {
      return t->kind == TypeKind::Float || (t->kind == TypeKind::Vec && isFloatLike(t->element));
    }

    // GLSL reads an unmarked literal as a 32-bit number, so one that does not fit is marked 64-bit.
    String intText(int64_t v) {
      const bool fitsIn32Bits = v >= -2147483648LL && v <= 2147483647LL;

      return fitsIn32Bits ? to_string(v) : to_string(v) + "l";
    }

    const String& groupSizeText() {
      static const String kText = to_string(kGroupSize);

      return kText;
    }

  }  // namespace

  String GlslComputeType(const TypePtr& t) {
    switch (t->kind) {
      case TypeKind::Int:
        return intName(t->bits);
      case TypeKind::Float:
        return "float";
      case TypeKind::Vec:
        return vectorPrefix(t->element) + "vec" + to_string(t->size);
      default:
        return "void";
    }
  }

  bool IsGlslNumber(const TypePtr& t) {
    return (t->kind == TypeKind::Int && t->bits > 1) || t->kind == TypeKind::Float;
  }

  namespace {

    String expressionCode(const Expr& e);

    String blockCode(const StmtList& stmts, int depth);

    // ── expressions ──

    String exprList(const Vector<ExprPtr>& es, size_t from = 0) {
      Vector<String> parts;

      for (size_t i = from; i < es.size(); i++) {
        parts.push_back(expressionCode(*es[i]));
      }

      return Joined(parts, ", ");
    }

    String remainder(const Expr& left, const Expr& right) {
      return "gpu_fmod(" + expressionCode(left) + ", " + expressionCode(right) + ")";
    }

    // GLSL will not mix an `int` with an `int64_t`: as in the LLVM lowering, the right operand is converted to the left operand's type.
    String matched(const Expr& left, const Expr& right) {
      const String leftType = GlslComputeType(left.type);
      const bool differ = IsGlslNumber(left.type) && IsGlslNumber(right.type) && leftType != GlslComputeType(right.type);

      return differ ? leftType + "(" + expressionCode(right) + ")" : expressionCode(right);
    }

    String binary(const Expr& e) {
      const Expr& left = *e.operands[0];

      if (e.binaryOp == BinOp::Rem && isFloatLike(left.type)) {
        return remainder(left, *e.operands[1]);
      }

      return "(" + expressionCode(left) + " " + OperatorSymbol(e.binaryOp) + " " + matched(left, *e.operands[1]) + ")";
    }

    String unary(const Expr& e) {
      return UnaryPrefix(e.unaryOp) + expressionCode(*e.operands[0]) + ")";
    }

    String atomicCall(const Expr& e) {
      const String slot = expressionCode(*e.operands[1]);
      const String amount = expressionCode(*e.operands[2]);

      if (e.name == "atomicAddFloat") {
        return "gpu_add_float_" + expressionCode(*e.operands[0]) + "(int(" + slot + "), " + amount + ")";
      }

      return e.name + "(" + expressionCode(*e.operands[0]) + "[int(" + slot + ")], " + amount + ")";
    }

    String groupCall(const Expr& e) {
      if (e.name == "groupSize") {
        return groupSizeText();
      }

      if (e.name == "localIndex") {
        return "(" + expressionCode(*e.operands[0]) + " % " + groupSizeText() + ")";
      }

      if (e.name == "groupIndex") {
        return "(" + expressionCode(*e.operands[0]) + " / " + groupSizeText() + ")";
      }

      return "gpu_barrier()";
    }

    // The called kernel reads its arrays by name, so a pointer argument is not passed.
    String itemCall(const Expr& e) {
      Vector<String> parts;

      for (const ExprPtr& argument : e.operands) {
        if (argument->type->kind != TypeKind::Pointer) {
          parts.push_back(expressionCode(*argument));
        }
      }

      return e.name + "_item(" + Joined(parts, ", ") + ")";
    }

    // `fmodf` is the `gpu_fmod` helper, like `%` on floats. The atomic updates are GLSL's own (a float is added with the compare-and-swap helper
    // declared next to its buffer). The workgroup functions become constants, the barrier helper, or arithmetic on the thread number.
    String call(const Expr& e) {
      if (e.name == "fmodf") {
        return remainder(*e.operands[0], *e.operands[1]);
      }

      if (IsKernelAtomic(e.name)) {
        return atomicCall(e);
      }

      if (IsGroupFunction(e.name)) {
        return groupCall(e);
      }

      if (IsKernelMath(e.name)) {
        return GlslMathName(e.name) + "(" + exprList(e.operands) + ")";
      }

      return itemCall(e);
    }

    String compound(const Expr& e) {
      switch (e.kind) {
        case ExprKind::Cast: case ExprKind::Coerce:
          return GlslComputeType(e.type) + "(" + expressionCode(*e.operands[0]) + ")";
        case ExprKind::VecLit:
          return GlslComputeType(e.type) + "(" + exprList(e.operands) + ")";
        case ExprKind::IndexGet:
          return expressionCode(*e.operands[0]) + "[int(" + exprList(e.operands, 1) + ")]";
        case ExprKind::Call:
          return call(e);
        default:
          return "0";
      }
    }

    String operation(const Expr& e) {
      switch (e.kind) {
        case ExprKind::Binary:
          return binary(e);
        case ExprKind::Unary:
          return unary(e);
        default:
          return compound(e);
      }
    }

    String expressionCode(const Expr& e) {
      switch (e.kind) {
        case ExprKind::ConstInt:
          return intText(e.intValue);
        case ExprKind::ConstFloat:
          return FloatLiteral(e.floatValue);
        case ExprKind::ConstBool:
          return e.boolValue ? "true" : "false";
        case ExprKind::Local: case ExprKind::LoadSlot:
          return GlslIdentifier(e.name);
        default:
          return operation(e);
      }
    }

    // ── statements ──

    // C++ (Metal) narrows silently when a wider value is stored in a narrower variable. GLSL refuses, so a number stored in a number variable is
    // converted.
    String fit(const Expr& e, const TypePtr& target) {
      return IsGlslNumber(target) ? GlslComputeType(target) + "(" + expressionCode(e) + ")" : expressionCode(e);
    }

    // A `let` is `const`; a `var` with no value starts at zero.
    String declaration(const Stmt& s, int depth) {
      const String start = s.value ? fit(*s.value, s.type) : GlslComputeType(s.type) + "(0)";

      return ShaderIndent(depth) + (s.kind == StmtKind::Let ? "const " : "") + GlslComputeType(s.type) + " " + GlslIdentifier(s.name) + " = " + start + ";\n";
    }

    String conditional(const Stmt& s, int depth) {
      const String head = ShaderIndent(depth) + "if (" + expressionCode(*s.value) + ") {\n" + blockCode(s.body, depth + 1);

      if (s.elseBody.empty()) {
        return head + ShaderIndent(depth) + "}\n";
      }

      return head + ShaderIndent(depth) + "} else {\n" + blockCode(s.elseBody, depth + 1) + ShaderIndent(depth) + "}\n";
    }

    String carried(const Stmt& s, int depth) {
      String out;

      for (size_t i = 0; i < s.names.size(); i++) {
        out += ShaderIndent(depth) + GlslComputeType(s.initializers[i]->type) + " " + GlslIdentifier(s.names[i]) + " = " + expressionCode(*s.initializers[i]) + ";\n";
      }

      return out;
    }

    String advance(const Stmt& s, int depth) {
      String out;

      for (size_t i = 0; i < s.names.size(); i++) {
        out += ShaderIndent(depth) + GlslComputeType(s.steps[i]->type) + " " + GlslIdentifier(s.names[i]) + "_next = " + expressionCode(*s.steps[i]) + ";\n";
      }

      for (size_t i = 0; i < s.names.size(); i++) {
        out += ShaderIndent(depth) + GlslIdentifier(s.names[i]) + " = " + GlslIdentifier(s.names[i]) + "_next;\n";
      }

      return out;
    }

    // Same loop shape as the Metal output: every new value is computed from the OLD ones first (`_next`), then stored.
    String loop(const Stmt& s, int depth) {
      const String pad = ShaderIndent(depth);

      return pad + "{\n" + carried(s, depth + 1) + pad + "    while (" + expressionCode(*s.value) + ") {\n" + blockCode(s.body, depth + 2) + advance(s, depth + 2) + pad +
        "    }\n" + pad + "}\n";
    }

    String flowCode(const Stmt& s, int depth) {
      switch (s.kind) {
        case StmtKind::Return:
          return ShaderIndent(depth) + "return " + (s.value ? expressionCode(*s.value) : String()) + ";\n";
        case StmtKind::If:
          return conditional(s, depth);
        case StmtKind::For:
          return loop(s, depth);
        default:
          return "";
      }
    }

    String statementCode(const Stmt& s, int depth) {
      const String pad = ShaderIndent(depth);

      switch (s.kind) {
        case StmtKind::Let: case StmtKind::Var:
          return declaration(s, depth);
        case StmtKind::Assign:
          return pad + expressionCode(*s.target) + " = " + fit(*s.value, s.target->type) + ";\n";
        case StmtKind::ExprStmt:
          return pad + expressionCode(*s.value) + ";\n";
        default:
          return flowCode(s, depth);
      }
    }

    String blockCode(const StmtList& stmts, int depth) {
      String out;

      for (const StmtPtr& s : stmts) {
        out += statementCode(*s, depth);
      }

      return out;
    }

  }  // namespace

  String GlslComputeBlockCode(const StmtList& stmts, int depth) {
    return blockCode(stmts, depth);
  }

}  // namespace polyir
