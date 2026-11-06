// Turns the problems the checker left in the HIR into error messages (CheckDiagnose.nina).
//
// When the checker meets something wrong (an unknown name, a call with the wrong number of arguments) it does not stop. It puts a problem node
// (`ExprKind::Unsupported`) in the tree, remembering what went wrong and where. This file walks every function body, finds those nodes, and writes one
// message for each:
//
//     3:12: unknown name total
//
// where 3:12 is the line and column in the source. A program built from several files names the file too (`lib/helper.poly:3:12: unknown name total`).
// Several mistakes in one program are all reported together: per function, first the problems found in its statements (each statement's own expressions,
// then the statements nested in it), then its `return`s of the wrong kind.
#include "front/Checker.hpp"
#include "front/Pipeline.hpp"

namespace polyir::front {

  // `line:col: what`.
  String DiagnosticMessage(const String& what, Span at) {
    return to_string(at.line) + ":" + to_string(at.column) + ": " + what;
  }

  namespace {

    using Problems = Vector<String>;

    void inBlock(const StmtList& body, Problems& out);

    // A problem node gives one message; any other expression is searched for problems among its parts.
    void inExpr(const Expr& expr, Problems& out) {
      if (expr.kind == ExprKind::Unsupported) {
        return out.push_back(DiagnosticMessage(expr.text, expr.at));
      }

      if (expr.kind == ExprKind::ClosureLit) {
        return inBlock(expr.closureBody, out);
      }

      for (const ExprPtr& part : expr.operands) {
        inExpr(*part, out);
      }
    }

    void inExprs(const Vector<ExprPtr>& exprs, Problems& out) {
      for (const ExprPtr& expr : exprs) {
        inExpr(*expr, out);
      }
    }

    void inOptional(const ExprPtr& expr, Problems& out) {
      if (expr) {
        inExpr(*expr, out);
      }
    }

    // The value of a statement that has one, then the values `goto`, `branch` and `switch` pass to labels. (A `switch` reports only its default's
    // arguments, as the Nina checker does.)
    void inJump(const Stmt& stmt, Problems& out) {
      inOptional(stmt.value, out);
      inExprs(stmt.arguments, out);
      inExprs(stmt.elseArguments, out);
    }

    void inAssign(const Stmt& stmt, Problems& out) {
      inExpr(*stmt.target, out);
      inExpr(*stmt.value, out);
    }

    void inFor(const Stmt& stmt, Problems& out) {
      inExprs(stmt.initializers, out);
      inExpr(*stmt.value, out);
      inExprs(stmt.steps, out);
    }

    // The expressions a statement evaluates directly. (A label and a `try` have no value; a `var` may have none.)
    void inOwnExprs(const Stmt& stmt, Problems& out) {
      switch (stmt.kind) {
        case StmtKind::Assign:
          return inAssign(stmt, out);
        case StmtKind::For:
          return inFor(stmt, out);
        case StmtKind::Goto: case StmtKind::Branch: case StmtKind::Switch:
          return inJump(stmt, out);
        default:
          return inOptional(stmt.value, out);
      }
    }

    // A statement's own expressions first, then the statements nested inside it.
    void inStmt(const Stmt& stmt, Problems& out) {
      inOwnExprs(stmt, out);

      for (const StmtList* block : BlocksOf(stmt)) {
        inBlock(*block, out);
      }
    }

    void inBlock(const StmtList& body, Problems& out) {
      for (const StmtPtr& stmt : body) {
        inStmt(*stmt, out);
      }
    }

    void returnsIn(const StmtList& body, const TypePtr& result, Problems& out);

    // A `return` whose value is not of the function's declared result kind.
    void ownReturn(const Stmt& stmt, const TypePtr& result, Problems& out) {
      if (stmt.kind != StmtKind::Return) {
        return;
      }

      if (const Optional<String> why = Mismatch(result, stmt.value->type)) {
        out.push_back(DiagnosticMessage(*why, stmt.value->at));
      }
    }

    // The returns in the blocks of an `if`, `for`, `try` or `select`.
    void nestedReturns(const Stmt& stmt, const TypePtr& result, Problems& out) {
      for (const StmtList* block : BlocksOf(stmt)) {
        returnsIn(*block, result, out);
      }
    }

    // Every `return` in a function must give a value of the declared return type. (A coroutine's `return` is not checked: its declared type is what it
    // yields.)
    void returnsIn(const StmtList& body, const TypePtr& result, Problems& out) {
      for (const StmtPtr& stmt : body) {
        ownReturn(*stmt, result, out);
        nestedReturns(*stmt, result, out);
      }
    }

    // A `mutarray`, `sparse` or `mutmap` is a pointer to a header that is allocated when the container is created. A global is set up before any code
    // runs, so it could only start out as a null pointer.
    void globalProblems(const GlobalDef& global, Problems& out) {
      if (!IsGrowable(global.type)) {
        return;
      }

      const String message = "a global cannot be a mutarray, sparse or mutmap: create it in a function and pass it to the functions that use it";
      out.push_back(DiagnosticMessage(message, global.initializer->at));
    }

    // The messages found in one unit, with that unit's file in front (when it came from a file).
    void prefixed(const Session& session, uint32_t unit, const Problems& found, Problems& out) {
      const String& file = session.files[session.units[unit].file];

      for (const String& message : found) {
        out.push_back(file.empty() ? message : file + ":" + message);
      }
    }

  }  // namespace

  Vector<String> FunctionProblems(const Session& session, size_t index) {
    const FuncDef& function = session.module.functions[index];
    Problems found;
    inBlock(function.body, found);

    if (!function.isCoroutine) {
      returnsIn(function.body, function.result, found);
    }

    return found;
  }

  namespace {

    // One function's problems: those in its statements, then its returns of the wrong kind. A generic as declared is no code: its problems
    // only say it does not work for every type (Parametric.cpp).
    void functionProblems(const Session& session, size_t index, Problems& out) {
      if (IsParametric(session.module, session.module.functions[index].generic)) {
        return;
      }

      prefixed(session, session.sources[index].unit, FunctionProblems(session, index), out);
    }

    void globalDeclProblems(const Session& session, size_t index, Problems& out) {
      Problems found;
      globalProblems(session.module.globals[index], found);
      prefixed(session, session.globalUnits[index], found, out);
    }

  }  // namespace

  // Every problem in the program, in source order. Empty means the program is fine.
  Vector<String> Diagnose(const Session& session) {
    Problems out = session.declProblems;

    for (const DeclRef& ref : session.module.order) {
      if (ref.kind == DeclKind::Function) {
        functionProblems(session, ref.index, out);
      }

      if (ref.kind == DeclKind::Global) {
        globalDeclProblems(session, ref.index, out);
      }
    }

    return out;
  }

}  // namespace polyir::front
