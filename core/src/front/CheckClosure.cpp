// Closure literals (CheckContainers.nina `closureLiteral`, HirWalk.nina `names`): `closure(params) -> ret { body }`. The body sees the enclosing names and its
// own parameters, and `return` inside it is checked against the closure's own return type. `captures` lists every name the body mentions, in the order they
// are first met and repeats included (lowering keeps the ones that are really captured). Like the Nina walk, it looks into the statements that hold
// expressions and blocks (`let`, `var`, assignment, `if`, `for`, `return`, expression statements, `throw`, `try`), not into labels, jumps, `switch` or `yield`,
// nor into variant and either payloads.
#include "front/Checker.hpp"

namespace polyir::front {

  namespace {

    // Collects, in order, the names a run of statements mentions (repeats included).
    struct NameWalk {
      Vector<String>& names;
    };

    void walkBlock(NameWalk& walk, const StmtList& stmts);

    void walkStmt(NameWalk& walk, const Stmt& stmt);

    void walkOwnExprs(NameWalk& walk, const Stmt& stmt);

    bool takesValue(StmtKind kind);

    void walkAll(NameWalk& walk, const Vector<ExprPtr>& exprs);

    void walkExpr(NameWalk& walk, const Expr* expr);

    bool isName(ExprKind kind);

    bool hasParts(ExprKind kind);

    void walkBlock(NameWalk& walk, const StmtList& stmts) {
      for (const StmtPtr& stmt : stmts) {
        walkStmt(walk, *stmt);
      }
    }

    void walkStmt(NameWalk& walk, const Stmt& stmt) {
      walkOwnExprs(walk, stmt);

      for (const StmtList* block : BlocksOf(stmt)) {
        walkBlock(walk, *block);
      }
    }

    void walkOwnExprs(NameWalk& walk, const Stmt& stmt) {
      if (stmt.kind == StmtKind::Assign) {
        walkExpr(walk, stmt.target.get());
      }

      if (stmt.kind == StmtKind::For) {
        walkAll(walk, stmt.initializers);
      }

      if (takesValue(stmt.kind)) {
        walkExpr(walk, stmt.value.get());
      }

      if (stmt.kind == StmtKind::For) {
        walkAll(walk, stmt.steps);
      }
    }

    // The statements whose `value` the walk enters (the Nina walk skips the others).
    bool takesValue(StmtKind kind) {
      return kind != StmtKind::Label && kind != StmtKind::Goto && kind != StmtKind::Branch && kind != StmtKind::Switch && kind != StmtKind::Try &&
        kind != StmtKind::Yield;
    }

    void walkAll(NameWalk& walk, const Vector<ExprPtr>& exprs) {
      for (const ExprPtr& expr : exprs) {
        walkExpr(walk, expr.get());
      }
    }

    void walkExpr(NameWalk& walk, const Expr* expr) {
      if (expr == nullptr) {
        return;
      }

      if (isName(expr->kind)) {
        walk.names.push_back(expr->name);
      }

      if (expr->kind == ExprKind::ClosureLit) {
        walkBlock(walk, expr->closureBody);
      }

      if (hasParts(expr->kind)) {
        walkAll(walk, expr->operands);
      }
    }

    bool isName(ExprKind kind) {
      return kind == ExprKind::Local || kind == ExprKind::LoadSlot || kind == ExprKind::Global;
    }

    // The expressions whose operands the walk enters (not a variant or either payload).
    bool hasParts(ExprKind kind) {
      return kind == ExprKind::Binary || kind == ExprKind::Unary || kind == ExprKind::Cast || kind == ExprKind::Coerce || kind == ExprKind::Call ||
        kind == ExprKind::CallIndirect || kind == ExprKind::CallBuiltin || kind == ExprKind::StructLit || kind == ExprKind::FieldGet ||
        kind == ExprKind::IndexGet || kind == ExprKind::VecLit || kind == ExprKind::NewContainer;
    }

    // The parameters, bound for the body to see.
    Vector<TypePtr> bindClosureParams(Checker& check, const AstExpr& node, Vector<String>& names) {
      Vector<TypePtr> types;

      for (const AstParam& param : check.program->params[node.params]) {
        names.push_back(Str(check, param.name.name));
        types.push_back(ResolveType(check, param.type));
        BindName(check.scope, param.name.name, BindingKind::Local, types.back());
      }

      return types;
    }

    // The body, checked with the closure's parameters in scope and its own return type; the enclosing function's state comes back afterwards.
    StmtList closureBody(Checker& check, const AstExpr& node, const TypePtr& result) {
      const TypePtr outerResult = check.returnType;
      check.returnType = result;
      StmtList body = CheckBlock(check, node.body);
      check.returnType = outerResult;

      return body;
    }

    // The literal: its parameters and result type, the body, and the names the body mentions.
    SharedPtr<Expr> closureNode(Checker& check, const AstExpr& node, const TypePtr& result) {
      Vector<String> names;
      Vector<TypePtr> types = bindClosureParams(check, node, names);

      SharedPtr<Expr> made = NewExpr(ExprKind::ClosureLit, MakeClosure(types, result), NoSpan());
      made->closureBody = closureBody(check, node, result);
      made->parameterNames = move(names);
      made->parameterTypes = move(types);

      return made;
    }

    // The names bound inside the closure are dropped again once its body is checked.
    ExprPtr closureLiteral(Checker& check, const AstExpr& node) {
      const size_t mark = Mark(check.scope);
      SharedPtr<Expr> made = closureNode(check, node, ResolveType(check, node.type));
      Reset(check.scope, mark);
      NameWalk walk{made->captures};
      walkBlock(walk, made->closureBody);

      return made;
    }

  }  // namespace

  void RegisterClosures(Registry& registry) {
    AddExpr(registry, AstExprKind::Closure, closureLiteral);
  }

}  // namespace polyir::front
