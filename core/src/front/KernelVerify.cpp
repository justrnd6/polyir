// The rules for one kernel (KernelVerify.nina): the signature, the launch shape, then every statement and expression of the body. A GPU runs one small
// program on thousands of elements, so a kernel may use numbers, small vectors, `uniform` and `writable` arrays, `let`/`var`, `if`, `for`, `return`,
// arithmetic, the math functions and calls to kernels defined above it; and nothing else (strings, structs, containers, closures, exceptions, `goto`,
// globals, `double`). Each violation is one message, in the order the Nina verifier finds them.
#include "front/Kernels.hpp"
#include "front/Pipeline.hpp"
#include "polyir/HirPrint.hpp"

namespace polyir::front {

  namespace {

    using Problems = Vector<String>;
    using Allowed = Vector<String>;

    void report(Problems& out, const String& what, const Span& at) {
      out.push_back(DiagnosticMessage(what, at));
    }

    // The math functions every GPU language has.
    bool isMath(const String& name) {
      static const Vector<String> kNames = {"sqrtf", "sinf", "cosf", "tanf", "expf", "logf", "powf", "floorf", "ceilf", "fabsf", "fminf", "fmaxf",
        "fmodf", "dot", "cross", "normalize", "length"};

      return IsOneOf(name, kNames);
    }

    bool isAtomic(const String& name) {
      return name == "atomicAdd" || name == "atomicAddFloat" || name == "atomicMin" || name == "atomicMax";
    }

    bool isGroupFunction(const String& name) {
      return name == "groupSize" || name == "localIndex" || name == "groupIndex" || name == "barrier";
    }

    // ── what the walk looks at ──

    // The sub-expressions the Nina walker enters: a closure, variant or either payload is not one of them.
    bool entersOperands(const Expr& expr) {
      return expr.kind != ExprKind::ClosureLit && expr.kind != ExprKind::VariantLit && expr.kind != ExprKind::EitherLit;
    }

    // The names of the functions an expression calls, in order.
    void callsInExpr(const Expr& expr, Vector<String>& out) {
      if (expr.kind == ExprKind::Call) {
        out.push_back(expr.name);
      }

      if (!entersOperands(expr)) {
        return;
      }

      for (const ExprPtr& operand : expr.operands) {
        callsInExpr(*operand, out);
      }
    }

    void callsInExprs(const Vector<ExprPtr>& list, Vector<String>& out) {
      for (const ExprPtr& expr : list) {
        callsInExpr(*expr, out);
      }
    }

    void callsInStmts(const StmtList& body, Vector<String>& out);

    // The statements whose `value` is an expression of their own (an `if`'s is its condition).
    bool hasPlainValue(StmtKind kind) {
      return kind == StmtKind::Let || kind == StmtKind::Var || kind == StmtKind::Return || kind == StmtKind::ExprStmt || kind == StmtKind::Throw ||
             kind == StmtKind::If;
    }

    void callsInAssign(const Stmt& stmt, Vector<String>& out) {
      callsInExpr(*stmt.target, out);
      callsInExpr(*stmt.value, out);
    }

    // A `for`'s initial values, condition and steps.
    void callsInForHeader(const Stmt& stmt, Vector<String>& out) {
      callsInExprs(stmt.initializers, out);
      callsInExpr(*stmt.value, out);
      callsInExprs(stmt.steps, out);
    }

    void callsInStmt(const Stmt& stmt, Vector<String>& out) {
      if (hasPlainValue(stmt.kind) && stmt.value) {
        callsInExpr(*stmt.value, out);
      }

      if (stmt.kind == StmtKind::Assign) {
        callsInAssign(stmt, out);
      }

      if (stmt.kind == StmtKind::For) {
        callsInForHeader(stmt, out);
      }

      if (stmt.kind == StmtKind::If || stmt.kind == StmtKind::For || stmt.kind == StmtKind::Try) {
        callsInStmts(stmt.body, out);
      }

      if (stmt.kind == StmtKind::If || stmt.kind == StmtKind::Try) {
        callsInStmts(stmt.elseBody, out);
      }
    }

    void callsInStmts(const StmtList& body, Vector<String>& out) {
      for (const StmtPtr& stmt : body) {
        callsInStmt(*stmt, out);
      }
    }

    bool callsBarrier(const StmtList& body) {
      Vector<String> names;
      callsInStmts(body, names);

      return IsOneOf("barrier", names);
    }

    // ── types and the signature ──

    void typeProblem(const TypePtr& type, const Span& at, Problems& out);

    void vectorProblem(const TypePtr& type, const Span& at, Problems& out) {
      if (type->size < 2 || type->size > 4) {
        return report(out, "a GPU vector has 2 to 4 elements", at);
      }

      typeProblem(type->element, at, out);
    }

    void floatProblem(const TypePtr& type, const Span& at, Problems& out) {
      if (type->bits == 64) {
        report(out, "double is not available on GPUs; use float", at);
      }
    }

    // The numbers a GPU has: integers, floats (not doubles), small vectors and matrices of them, and pointers to them.
    void typeProblem(const TypePtr& type, const Span& at, Problems& out) {
      switch (type->kind) {
        case TypeKind::Int:
          return;
        case TypeKind::Float:
          return floatProblem(type, at, out);
        case TypeKind::Vec: case TypeKind::Matrix:
          return vectorProblem(type, at, out);
        case TypeKind::Pointer:
          return typeProblem(type->element, at, out);
        default:
          return report(out, "the type " + TypeText(type) + " cannot be used in a kernel", at);
      }
    }

    void signature(const FuncDef& function, Problems& out) {
      if (IsVoid(*function.result)) {
        report(out, "a kernel must return a value: it writes one result per element", function.at);
      } else {
        typeProblem(function.result, function.at, out);
      }

      for (const Parameter& parameter : function.parameters) {
        typeProblem(parameter.type, function.at, out);
      }
    }

    // ── the launch shape ──

    // How many parameters have the role `wanted`.
    int countOf(const Vector<KernelRole>& roles, KernelRole wanted) {
      int found = 0;

      for (const KernelRole role : roles) {
        found += role == wanted ? 1 : 0;
      }

      return found;
    }

    void earlyReturns(const StmtList& body, bool isTopLevel, Vector<const Stmt*>& out);

    void returnsIn(const Stmt& stmt, bool isLast, Vector<const Stmt*>& out) {
      if (stmt.kind == StmtKind::Return && !isLast) {
        out.push_back(&stmt);
      }

      if (stmt.kind == StmtKind::If || stmt.kind == StmtKind::For) {
        earlyReturns(stmt.body, false, out);
      }

      if (stmt.kind == StmtKind::If) {
        earlyReturns(stmt.elseBody, false, out);
      }
    }

    // Every `return` except a final one at the top level of the body.
    void earlyReturns(const StmtList& body, bool isTopLevel, Vector<const Stmt*>& out) {
      for (size_t i = 0; i < body.size(); i++) {
        returnsIn(*body[i], isTopLevel && i + 1 == body.size(), out);
      }
    }

    void barrierReturns(const StmtList& body, Problems& out) {
      if (!callsBarrier(body)) {
        return;
      }

      Vector<const Stmt*> early;
      earlyReturns(body, true, early);
      const String message = "a kernel that calls barrier() may only return as its last statement: every thread must reach every barrier";

      for (const Stmt* returnStatement : early) {
        report(out, message, returnStatement->at);
      }
    }

    // Workgroup memory is shared by one group of threads, so the kernel must run in whole groups along one dimension.
    void groupProblems(const FuncDef& function, const Vector<KernelRole>& roles, Problems& out) {
      const String oneDimension = "workgroup memory needs a one-dimensional kernel: use a single `index` parameter";
      const String noElements = "a kernel with workgroup memory runs in whole groups, so it cannot take element inputs: read arrays with `uniform` and "
                                "check the index yourself";

      if (countOf(roles, KernelRole::Workgroup) == 0) {
        return;
      }

      if (countOf(roles, KernelRole::Index) > 1) {
        report(out, oneDimension, function.at);
      }

      if (countOf(roles, KernelRole::Element) > 0) {
        report(out, noElements, function.at);
      }
    }

    // How the kernel is launched: its `index` parameters are the grid's dimensions.
    void shape(const FuncDef& function, const Vector<KernelRole>& roles, Problems& out) {
      if (countOf(roles, KernelRole::Index) > 2) {
        report(out, "a kernel can have at most two `index` parameters: the column and the row", function.at);
      }

      groupProblems(function, roles, out);
      barrierReturns(function.body, out);
    }

    // ── expressions ──

    void verifyExpr(const Expr& expr, const Allowed& allowed, Problems& out);

    void verifyExprs(const Vector<ExprPtr>& list, const Allowed& allowed, Problems& out) {
      for (const ExprPtr& expr : list) {
        verifyExpr(*expr, allowed, out);
      }
    }

    // Whether `base` is the array parameter the kernel marked `mark` (`writable` or `workgroup`).
    bool isLocalMarked(const Expr& base, const Allowed& allowed, const char* mark) {
      return base.kind == ExprKind::Local && IsOneOf(String(mark) + " " + base.name, allowed);
    }

    bool isElementType(const TypePtr& array, TypeKind kind, int bits) {
      return array->kind == TypeKind::Pointer && array->element->kind == kind && array->element->bits == bits;
    }

    void atomicTypeProblem(const String& callee, const TypePtr& array, const Span& at, Problems& out) {
      const bool isFloat = callee == "atomicAddFloat";
      const bool fits = isElementType(array, isFloat ? TypeKind::Float : TypeKind::Int, 32);
      const String wanted = isFloat ? "a float array: ptr<float>" : "an i32 array: ptr<i32> (use atomicAddFloat for floats)";

      if (!fits) {
        report(out, callee + " works on " + wanted, at);
      }
    }

    // `atomicAdd(counts, slot, amount)` and its relatives work on one slot of a writable array of the right element type.
    void atomicProblem(const Expr& call, const Allowed& allowed, Problems& out) {
      if (call.operands.size() != 3 || !isLocalMarked(*call.operands[0], allowed, "writable")) {
        return report(out, call.name + " needs a writable array parameter, a slot and a value: " + call.name + "(array, slot, value)", call.at);
      }

      atomicTypeProblem(call.name, call.operands[0]->type, call.at, out);
    }

    // A kernel may call a kernel declared above it, unless that one has workgroup memory.
    void kernelCallProblem(const Expr& call, const Allowed& allowed, Problems& out) {
      for (const String& entry : allowed) {
        if (entry == call.name) {
          return;
        }

        if (entry == "grouped " + call.name) {
          return report(out, call.name + " has workgroup memory, so it cannot be called from another kernel", call.at);
        }
      }

      report(out, call.name + " is not a math function or a kernel defined above this one", call.at);
    }

    void callProblem(const Expr& call, const Allowed& allowed, Problems& out) {
      if (isMath(call.name) || isGroupFunction(call.name)) {
        return;
      }

      if (isAtomic(call.name)) {
        return atomicProblem(call, allowed, out);
      }

      kernelCallProblem(call, allowed, out);
    }

    // How a message names one kind of expression.
    struct KindName {
      ExprKind kind;
      const char* name;
    };

    const KindName kExpressionNames[] = {{ExprKind::ConstString, "a string"}, {ExprKind::Global, "a global"}, {ExprKind::FuncRef, "a function value"},
      {ExprKind::CallIndirect, "a call through a function value"}, {ExprKind::CallBuiltin, "a built-in operation"}, {ExprKind::StructLit, "a struct"},
      {ExprKind::FieldGet, "a struct field"}, {ExprKind::NewContainer, "a container"}, {ExprKind::ClosureLit, "a closure"},
      {ExprKind::Unsupported, "this expression"}};

    // What the expression is called in "... cannot be used in a kernel"; anything else left is a variant or either value.
    String expressionName(const Expr& expr) {
      for (const KindName& entry : kExpressionNames) {
        if (entry.kind == expr.kind) {
          return entry.name;
        }
      }

      return "a variant";
    }

    void indexProblem(const Expr& expr, Problems& out) {
      const TypeKind base = expr.operands[0]->type->kind;

      if (base == TypeKind::Vec || base == TypeKind::Pointer) {
        return;
      }

      report(out, "only a vector or a uniform array parameter can be indexed in a kernel", expr.at);
    }

    // The expressions a kernel may always use: numbers, locals, operators and vector literals.
    bool isAllowedKind(ExprKind kind) {
      return kind == ExprKind::ConstInt || kind == ExprKind::ConstFloat || kind == ExprKind::ConstBool || kind == ExprKind::Local ||
             kind == ExprKind::LoadSlot || kind == ExprKind::Binary || kind == ExprKind::Unary || kind == ExprKind::VecLit;
    }

    // The expression's own problem (if any).
    void ownProblem(const Expr& expr, const Allowed& allowed, Problems& out) {
      if (isAllowedKind(expr.kind)) {
        return;
      }

      if (expr.kind == ExprKind::Cast || expr.kind == ExprKind::Coerce) {
        return typeProblem(expr.type, expr.at, out);
      }

      if (expr.kind == ExprKind::Call) {
        return callProblem(expr, allowed, out);
      }

      if (expr.kind == ExprKind::IndexGet) {
        return indexProblem(expr, out);
      }

      report(out, expressionName(expr) + " cannot be used in a kernel", expr.at);
    }

    void verifyExpr(const Expr& expr, const Allowed& allowed, Problems& out) {
      ownProblem(expr, allowed, out);

      if (entersOperands(expr)) {
        verifyExprs(expr.operands, allowed, out);
      }
    }

    // ── statements ──

    void verifyBlock(const StmtList& body, const Allowed& allowed, Problems& out);

    // `out[k] = v`: the array must be a `writable` or `workgroup` parameter.
    void elementWrite(const Stmt& stmt, const Allowed& allowed, Problems& out) {
      const Expr& target = *stmt.target;
      const Expr& base = *target.operands[0];

      if (!isLocalMarked(base, allowed, "writable") && !isLocalMarked(base, allowed, "workgroup")) {
        return report(out, "a kernel can only write to an array parameter marked `writable` or `workgroup`", target.at);
      }

      for (size_t i = 1; i < target.operands.size(); i++) {
        verifyExpr(*target.operands[i], allowed, out);
      }

      verifyExpr(*stmt.value, allowed, out);
    }

    // A local `var` may be assigned to, and so may one element of a `writable` array. Any other array or field is read-only.
    void assignment(const Stmt& stmt, const Allowed& allowed, Problems& out) {
      const Expr& target = *stmt.target;

      if (target.kind == ExprKind::LoadSlot) {
        return verifyExpr(*stmt.value, allowed, out);
      }

      if (target.kind == ExprKind::IndexGet) {
        return elementWrite(stmt, allowed, out);
      }

      report(out, "a kernel can only assign to a local var or an element of a writable array", target.at);
    }

    // A `barrier()` is a meeting point for the whole group, so it cannot sit where some threads skip it.
    void verifyIf(const Stmt& stmt, const Allowed& allowed, Problems& out) {
      const String message = "barrier() cannot be inside an if: every thread of the group must reach it";
      verifyExpr(*stmt.value, allowed, out);

      if (callsBarrier(stmt.body) || callsBarrier(stmt.elseBody)) {
        report(out, message, stmt.at);
      }

      verifyBlock(stmt.body, allowed, out);
      verifyBlock(stmt.elseBody, allowed, out);
    }

    void verifyFor(const Stmt& stmt, const Allowed& allowed, Problems& out) {
      verifyExprs(stmt.initializers, allowed, out);
      verifyExpr(*stmt.value, allowed, out);
      verifyExprs(stmt.steps, allowed, out);
      verifyBlock(stmt.body, allowed, out);
    }

    // The expressions a kernel may use as a whole statement are the atomic updates (the old value is not wanted) and `barrier()`.
    void statementExpr(const Stmt& stmt, const Allowed& allowed, Problems& out) {
      const Expr& value = *stmt.value;
      const bool isAllowedCall = value.kind == ExprKind::Call && (isAtomic(value.name) || value.name == "barrier");

      if (isAllowedCall) {
        return verifyExpr(value, allowed, out);
      }

      report(out, "a kernel cannot have a statement that only evaluates an expression", stmt.at);
    }

    // How a message names one kind of statement.
    struct StmtKindName {
      StmtKind kind;
      const char* name;
    };

    const StmtKindName kStatementNames[] = {{StmtKind::Switch, "switch"}, {StmtKind::Label, "label"}, {StmtKind::Goto, "goto"},
      {StmtKind::Branch, "branch"}, {StmtKind::Try, "try"}, {StmtKind::Throw, "throw"}, {StmtKind::Select, "a switch of keys (write it with `if`)"}};

    // The statements a kernel cannot have; anything else left is a `yield`.
    const char* statementName(StmtKind kind) {
      for (const StmtKindName& entry : kStatementNames) {
        if (entry.kind == kind) {
          return entry.name;
        }
      }

      return "yield";
    }

    // The other statements: an `if`, a `for`, an expression statement, or one a GPU cannot run.
    void control(const Stmt& stmt, const Allowed& allowed, Problems& out) {
      switch (stmt.kind) {
        case StmtKind::If:
          return verifyIf(stmt, allowed, out);
        case StmtKind::For:
          return verifyFor(stmt, allowed, out);
        case StmtKind::ExprStmt:
          return statementExpr(stmt, allowed, out);
        default:
          return report(out, String("this statement cannot run on a GPU: ") + statementName(stmt.kind), stmt.at);
      }
    }

    // A `let` or `var` must have a GPU type; it, and a `return`, may hold any allowed expression.
    void verifyStmt(const Stmt& stmt, const Allowed& allowed, Problems& out) {
      const bool isBinding = stmt.kind == StmtKind::Let || stmt.kind == StmtKind::Var;

      if (isBinding) {
        typeProblem(stmt.type, stmt.at, out);
      }

      if (stmt.kind == StmtKind::Assign) {
        return assignment(stmt, allowed, out);
      }

      if (!isBinding && stmt.kind != StmtKind::Return) {
        return control(stmt, allowed, out);
      }

      if (stmt.value) {
        verifyExpr(*stmt.value, allowed, out);
      }
    }

    void verifyBlock(const StmtList& body, const Allowed& allowed, Problems& out) {
      for (const StmtPtr& stmt : body) {
        verifyStmt(*stmt, allowed, out);
      }
    }

    // What a kernel body may call and write: the kernels before it, and "writable x" / "workgroup x" for each such array parameter.
    Allowed allowedNames(const FuncDef& function, const Vector<KernelRole>& roles, const Vector<String>& before) {
      Allowed allowed = before;

      for (size_t i = 0; i < function.parameters.size() && i < roles.size(); i++) {
        if (roles[i] == KernelRole::Writable) {
          allowed.push_back("writable " + function.parameters[i].name);
        }

        if (roles[i] == KernelRole::Workgroup) {
          allowed.push_back("workgroup " + function.parameters[i].name);
        }
      }

      return allowed;
    }

  }  // namespace

  Vector<String> KernelProblems(const FuncDef& function, const Vector<KernelRole>& roles, const Vector<String>& before) {
    Problems out;
    signature(function, out);
    shape(function, roles, out);
    verifyBlock(function.body, allowedNames(function, roles, before), out);

    return out;
  }

}  // namespace polyir::front
