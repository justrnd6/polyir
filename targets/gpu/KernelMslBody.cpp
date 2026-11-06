// The text of a kernel's types, statements and expressions in Metal Shading Language (KernelMsl.nina `typeText`, `stmt`, `expr`).
#include "gpu/KernelMslBody.hpp"
#include "gpu/KernelCommon.hpp"

namespace polyir {

  namespace {

    String intName(int bits, bool isSigned) {
      if (bits == 1) {
        return "bool";
      }

      if (bits == 8) {
        return isSigned ? "char" : "uchar";
      }

      if (bits == 16) {
        return isSigned ? "short" : "ushort";
      }

      if (bits == 32) {
        return isSigned ? "int" : "uint";
      }

      return isSigned ? "long" : "ulong";
    }

    String scalarName(const TypePtr& t) {
      return IsInteger(*t) ? intName(t->bits, IsSigned(*t)) : "float";
    }

    bool isFloatType(const TypePtr& t) {
      return IsFloat(*t) || (t->kind == TypeKind::Vec && IsFloat(*t->element));
    }

  }  // namespace

  static String aggregateType(const TypePtr& t) {
    switch (t->kind) {
      case TypeKind::Vec:
        return scalarName(t->element) + to_string(t->size);
      case TypeKind::Matrix:
        return "float" + to_string(t->size) + "x" + to_string(t->size);
      case TypeKind::Pointer:
        return "device const " + MslType(t->element) + "*";
      default:
        return "void";
    }
  }

  String MslType(const TypePtr& t) {
    switch (t->kind) {
      case TypeKind::Int: case TypeKind::UInt:
        return intName(t->bits, IsSigned(*t));
      case TypeKind::Float:
        return "float";
      default:
        return aggregateType(t);
    }
  }

  String MslPointerType(const char* space, const TypePtr& t) {
    return t->kind == TypeKind::Pointer ? String(space) + " " + MslType(t->element) + "*" : MslType(t);
  }

  String MslParamType(const TypePtr& t, ParamRole role) {
    if (role == ParamRole::Writable) {
      return MslPointerType("device", t);
    }

    return role == ParamRole::Workgroup ? MslPointerType("threadgroup", t) : MslType(t);
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

    String binary(const Expr& e) {
      const Expr& left = *e.operands[0];

      if (e.binaryOp == BinOp::Rem && isFloatType(left.type)) {
        return MslMathName("fmodf") + "(" + expressionCode(left) + ", " + expressionCode(*e.operands[1]) + ")";
      }

      return "(" + expressionCode(left) + " " + OperatorSymbol(e.binaryOp) + " " + expressionCode(*e.operands[1]) + ")";
    }

    String unary(const Expr& e) {
      return UnaryPrefix(e.unaryOp) + expressionCode(*e.operands[0]) + ")";
    }

    String atomicCall(const char* operation, const Expr& e) {
      return String("atomic_fetch_") + operation + "_explicit(reinterpret_cast<device atomic_int*>(&" + expressionCode(*e.operands[0]) + "[" + expressionCode(*e.operands[1]) +
        "]), " + expressionCode(*e.operands[2]) + ", memory_order_relaxed)";
    }

    // The thread's place in its workgroup, from its number: a group is `kGroupSize` consecutive threads.
    String groupCall(const Expr& e) {
      const String size = to_string(kGroupSize);

      if (e.name == "groupSize") {
        return size;
      }

      if (e.name == "localIndex") {
        return "(" + expressionCode(*e.operands[0]) + " % " + size + ")";
      }

      if (e.name == "groupIndex") {
        return "(" + expressionCode(*e.operands[0]) + " / " + size + ")";
      }

      return "threadgroup_barrier(mem_flags::mem_threadgroup)";
    }

    // `atomicAdd(counts, slot, amount)` becomes Metal's atomic add on that slot (`atomicMin` and `atomicMax` likewise). Metal wants the slot viewed as an
    // `atomic_int`, hence the cast. A float is added with the helper KernelMsl.cpp writes out. The workgroup functions become constants and Metal's barrier.
    String call(const Expr& e) {
      if (e.name == "atomicAdd") {
        return atomicCall("add", e);
      }

      if (e.name == "atomicMin") {
        return atomicCall("min", e);
      }

      if (e.name == "atomicMax") {
        return atomicCall("max", e);
      }

      if (e.name == "atomicAddFloat") {
        return "__atomic_add_float(" + exprList(e.operands) + ")";
      }

      if (IsGroupFunction(e.name)) {
        return groupCall(e);
      }

      return (IsKernelMath(e.name) ? MslMathName(e.name) : e.name + "_item") + "(" + exprList(e.operands) + ")";
    }

    String compound(const Expr& e) {
      switch (e.kind) {
        case ExprKind::Cast: case ExprKind::Coerce:
          return "(" + MslType(e.type) + ")(" + expressionCode(*e.operands[0]) + ")";
        case ExprKind::VecLit:
          return MslType(e.type) + "(" + exprList(e.operands) + ")";
        case ExprKind::IndexGet:
          return expressionCode(*e.operands[0]) + "[" + exprList(e.operands, 1) + "]";
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
          return to_string(e.intValue);
        case ExprKind::ConstFloat:
          return FloatLiteral(e.floatValue);
        case ExprKind::ConstBool:
          return e.boolValue ? "true" : "false";
        case ExprKind::Local: case ExprKind::LoadSlot:
          return MslIdentifier(e.name);
        default:
          return operation(e);
      }
    }

    // ── statements ──

    // A `let` is `const`; a `var` with no value starts at zero.
    String declaration(const Stmt& s, int depth) {
      const String start = s.value ? expressionCode(*s.value) : MslType(s.type) + "(0)";

      return ShaderIndent(depth) + (s.kind == StmtKind::Let ? "const " : "") + MslType(s.type) + " " + MslIdentifier(s.name) + " = " + start + ";\n";
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
        out += ShaderIndent(depth) + MslType(s.initializers[i]->type) + " " + MslIdentifier(s.names[i]) + " = " + expressionCode(*s.initializers[i]) + ";\n";
      }

      return out;
    }

    String advance(const Stmt& s, int depth) {
      String out;

      for (size_t i = 0; i < s.names.size(); i++) {
        out += ShaderIndent(depth) + MslType(s.steps[i]->type) + " " + MslIdentifier(s.names[i]) + "_next = " + expressionCode(*s.steps[i]) + ";\n";
      }

      for (size_t i = 0; i < s.names.size(); i++) {
        out += ShaderIndent(depth) + MslIdentifier(s.names[i]) + " = " + MslIdentifier(s.names[i]) + "_next;\n";
      }

      return out;
    }

    // A polyir `for` carries named values from one round to the next. Each round computes every new value from the OLD ones first, then stores them,
    // so `for (i = 0, s = 0; ...; i = i + 1, s = s + i)` means the same here as on the CPU.
    String loop(const Stmt& s, int depth) {
      const String pad = ShaderIndent(depth);

      return pad + "{\n" + carried(s, depth + 1) + pad + "    while (" + expressionCode(*s.value) + ") {\n" + blockCode(s.body, depth + 2) + advance(s, depth + 2) + pad +
        "    }\n" + pad + "}\n";
    }

    String flowCode(const Stmt& s, int depth) {
      switch (s.kind) {
        case StmtKind::Return:
          return ShaderIndent(depth) + "return " + expressionCode(*s.value) + ";\n";
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
          return pad + expressionCode(*s.target) + " = " + expressionCode(*s.value) + ";\n";
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

  String MslBlockCode(const StmtList& stmts, int depth) {
    return blockCode(stmts, depth);
  }

}  // namespace polyir
