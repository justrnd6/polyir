// Statements that move control around (CheckFlow.nina): labels, `goto`, `branch`, `switch`, `try`/`throw`, and `yield`. A label's parameters are visible
// to the statements after it, and a jump carries one argument per parameter. Jumps are checked in the scope of the jump itself, not of the label.
#include "front/Checker.hpp"

namespace polyir::front {

  namespace {

    // ── labels and jumps ────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────

    // A label's parameters are visible to the statements after it (checked with the type as written: a fixed array keeps its size).
    void bindLabelParam(Checker& check, Stmt& label, const AstParam& param) {
      label.parameterNames.push_back(Str(check, param.name.name));
      label.parameterTypes.push_back(ResolveType(check, param.type));
      BindName(check.scope, param.name.name, BindingKind::Local, label.parameterTypes.back());
    }

    StmtPtr labelStmt(Checker& check, const AstStmt& stmt) {
      StmtPtr made = NewStmt(StmtKind::Label, SpanAt(stmt.name.pos));
      made->name = Str(check, stmt.name.name);

      for (const AstParam& param : check.program->params[stmt.params]) {
        bindLabelParam(check, *made, param);
      }

      return made;
    }

    // `name(args)`: where a jump goes and what it carries.
    void jumpTo(Checker& check, const AstGotoTarget& target, String& name, Vector<ExprPtr>& args) {
      name = Str(check, target.name.name);
      args = CheckAll(check, target.args);
    }

    StmtPtr gotoStmt(Checker& check, const AstStmt& stmt) {
      StmtPtr made = NewStmt(StmtKind::Goto, SpanAt(stmt.goTo.name.pos));
      jumpTo(check, stmt.goTo, made->name, made->arguments);

      return made;
    }

    StmtPtr branchStmt(Checker& check, const AstStmt& stmt) {
      ExprPtr cond = CheckExpr(check, stmt.cond);
      StmtPtr made = NewStmt(StmtKind::Branch, cond->at);
      made->value = move(cond);
      jumpTo(check, stmt.goTo, made->name, made->arguments);
      jumpTo(check, stmt.goElse, made->elseName, made->elseArguments);

      return made;
    }

    void addCase(Checker& check, Stmt& made, const AstSwitchCase& arm) {
      made.caseValues.push_back(ParseInt(arm.value.text));
      made.caseLabels.emplace_back();
      made.caseArguments.emplace_back();
      jumpTo(check, arm.target, made.caseLabels.back(), made.caseArguments.back());
    }

    StmtPtr switchStmt(Checker& check, const AstStmt& stmt) {
      ExprPtr value = CheckExpr(check, stmt.value);
      StmtPtr made = NewStmt(StmtKind::Switch, value->at);
      made->value = move(value);

      for (const AstSwitchCase& arm : check.program->cases[stmt.cases]) {
        addCase(check, *made, arm);
      }

      jumpTo(check, stmt.goTo, made->name, made->arguments);

      return made;
    }

    // ── exceptions and coroutines ───────────────────────────────────────────────────────────────────────────────────────────────────────────────

    // The caught code is bound as an i64 in the catch block only.
    StmtList catchBlock(Checker& check, const AstStmt& stmt) {
      const size_t mark = Mark(check.scope);
      BindName(check.scope, stmt.name.name, BindingKind::Local, IntType(64));
      StmtList body = CheckBlock(check, stmt.other);
      Reset(check.scope, mark);

      return body;
    }

    StmtPtr tryStmt(Checker& check, const AstStmt& stmt) {
      StmtPtr made = NewStmt(StmtKind::Try, SpanAt(stmt.name.pos));
      made->body = CheckBlock(check, stmt.body);
      made->elseBody = catchBlock(check, stmt);
      made->name = Str(check, stmt.name.name);

      return made;
    }

    StmtPtr valueStmt(Checker& check, const AstStmt& stmt, StmtKind kind) {
      ExprPtr value = CheckExpr(check, stmt.value);
      StmtPtr made = NewStmt(kind, value->at);
      made->value = move(value);

      return made;
    }

    StmtPtr throwStmt(Checker& check, const AstStmt& stmt) {
      return valueStmt(check, stmt, StmtKind::Throw);
    }

    StmtPtr yieldStmt(Checker& check, const AstStmt& stmt) {
      return valueStmt(check, stmt, StmtKind::Yield);
    }

    struct StmtRow {
      AstStmtKind kind;
      StmtHandler handler;
    };

    const StmtRow kFlowStmts[] = {{AstStmtKind::Label, labelStmt}, {AstStmtKind::Goto, gotoStmt}, {AstStmtKind::Branch, branchStmt},
      {AstStmtKind::Switch, switchStmt}, {AstStmtKind::Try, tryStmt}, {AstStmtKind::Throw, throwStmt}, {AstStmtKind::Yield, yieldStmt}};

  }  // namespace

  void RegisterFlowStmts(Registry& registry) {
    for (const StmtRow& row : kFlowStmts) {
      AddStmt(registry, row.kind, row.handler);
    }
  }

}  // namespace polyir::front
