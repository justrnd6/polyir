// The syntax-level rules for coroutines, checked on the raw tree before anything is lowered (`PolyirCompiler.checkCoroRestrictions`, lang/src/Coroutines.nina):
//   * `yield` only appears in a `coro func`;
//   * a `coro func` yields neither inside the try-portion of a `try` nor inside a closure literal, and never `return`s (it ends by falling off the end).
// The messages carry no `line:column`. The walks look into `if`, `for` and `try` blocks only, as the Nina ones do, and a closure literal is found where the
// Nina walk finds one (not in a variant, either or optional payload, a label, a jump or a `switch`).
#include "front/Pipeline.hpp"

namespace polyir::front {

  namespace {

    // The program the walks read.
    struct Scan {
      const Program& program;
    };

    bool contains(const Scan& scan, const AstBlock& block, AstStmtKind wanted);

    bool yieldInTry(const Scan& scan, const AstBlock& block);

    void closures(const Scan& scan, const AstBlock& block, Vector<const AstExpr*>& found);

    bool containsIn(const Scan& scan, const AstStmt& stmt, AstStmtKind wanted);

    bool yieldInTryStmt(const Scan& scan, const AstStmt& stmt);

    void stmtClosures(const Scan& scan, const AstStmt& stmt, Vector<const AstExpr*>& found);

    bool jumps(AstStmtKind kind);

    bool holdsPayload(AstExprKind kind);

    void blockClosures(const Scan& scan, const AstStmt& stmt, Vector<const AstExpr*>& found);

    void exprsClosures(const Scan& scan, Range<ExprId> exprs, Vector<const AstExpr*>& found);

    void exprClosures(const Scan& scan, ExprId id, Vector<const AstExpr*>& found);

    void partClosures(const Scan& scan, const AstExpr& expr, Vector<const AstExpr*>& found);

    void armClosures(const Scan& scan, Range<AstArm> arms, Vector<const AstExpr*>& found);

    // Whether a statement of kind `wanted` is in the block, however deep in `if`, `for` and `try` blocks.
    bool contains(const Scan& scan, const AstBlock& block, AstStmtKind wanted) {
      for (const StmtId id : scan.program.stmtRefs[block.stmts]) {
        if (containsIn(scan, scan.program.stmts[id], wanted)) {
          return true;
        }
      }

      return false;
    }

    // A `yield` inside the try-portion of a `try`, however deep in `if`, `for` and the catch-portions.
    bool yieldInTry(const Scan& scan, const AstBlock& block) {
      for (const StmtId id : scan.program.stmtRefs[block.stmts]) {
        if (yieldInTryStmt(scan, scan.program.stmts[id])) {
          return true;
        }
      }

      return false;
    }

    // The closure literals in a block, each followed by the ones inside its own body.
    void closures(const Scan& scan, const AstBlock& block, Vector<const AstExpr*>& found) {
      for (const StmtId id : scan.program.stmtRefs[block.stmts]) {
        stmtClosures(scan, scan.program.stmts[id], found);
      }
    }

    // Whether any arm's block of a `match` (or a switch of blocks) holds a statement of kind `wanted`.
    bool armsContain(const Scan& scan, Range<AstArm> arms, AstStmtKind wanted) {
      for (const AstArm& arm : scan.program.arms[arms]) {
        if (contains(scan, arm.body, wanted)) {
          return true;
        }
      }

      return false;
    }

    bool containsIn(const Scan& scan, const AstStmt& stmt, AstStmtKind wanted) {
      if (stmt.kind == wanted) {
        return true;
      }

      if (stmt.kind == AstStmtKind::If || stmt.kind == AstStmtKind::Try) {
        return contains(scan, stmt.body, wanted) || contains(scan, stmt.other, wanted);
      }

      if (stmt.kind == AstStmtKind::Match) {
        return armsContain(scan, stmt.arms, wanted);
      }

      return stmt.kind == AstStmtKind::For && contains(scan, stmt.body, wanted);
    }

    bool yieldInArms(const Scan& scan, Range<AstArm> arms) {
      for (const AstArm& arm : scan.program.arms[arms]) {
        if (yieldInTry(scan, arm.body)) {
          return true;
        }
      }

      return false;
    }

    bool yieldInTryStmt(const Scan& scan, const AstStmt& stmt) {
      if (stmt.kind == AstStmtKind::If) {
        return yieldInTry(scan, stmt.body) || yieldInTry(scan, stmt.other);
      }

      if (stmt.kind == AstStmtKind::Match) {
        return yieldInArms(scan, stmt.arms);
      }

      if (stmt.kind == AstStmtKind::For) {
        return yieldInTry(scan, stmt.body);
      }

      return stmt.kind == AstStmtKind::Try && (contains(scan, stmt.body, AstStmtKind::Yield) || yieldInTry(scan, stmt.other));
    }

    void stmtClosures(const Scan& scan, const AstStmt& stmt, Vector<const AstExpr*>& found) {
      if (jumps(stmt.kind)) {
        return;
      }

      for (const ExprId id : {stmt.cond, stmt.target, stmt.value}) {
        exprClosures(scan, id, found);
      }

      for (const AstForBinding& init : scan.program.forBindings[stmt.inits]) {
        exprClosures(scan, init.value, found);
      }

      for (const AstForBinding& step : scan.program.forBindings[stmt.steps]) {
        exprClosures(scan, step.value, found);
      }

      if (stmt.kind == AstStmtKind::If || stmt.kind == AstStmtKind::For || stmt.kind == AstStmtKind::Try) {
        blockClosures(scan, stmt, found);
      }

      armClosures(scan, stmt.arms, found);
    }

    // The closures in each arm of a `match`: in its condition, guard and value, then in its block.
    void armClosures(const Scan& scan, Range<AstArm> arms, Vector<const AstExpr*>& found) {
      for (const AstArm& arm : scan.program.arms[arms]) {
        for (const ExprId id : {arm.cond, arm.guard, arm.value}) {
          exprClosures(scan, id, found);
        }

        closures(scan, arm.body, found);
      }
    }

    // The walks skip labels and jumps, as the Nina walks do (their arguments are not searched).
    bool jumps(AstStmtKind kind) {
      return kind == AstStmtKind::Label || kind == AstStmtKind::Goto || kind == AstStmtKind::Branch || kind == AstStmtKind::Switch;
    }

    // A variant, either or optional literal: the walks do not look into its payload.
    bool holdsPayload(AstExprKind kind) {
      return kind == AstExprKind::Variant || kind == AstExprKind::EitherOk || kind == AstExprKind::EitherErr || kind == AstExprKind::Some;
    }

    void blockClosures(const Scan& scan, const AstStmt& stmt, Vector<const AstExpr*>& found) {
      closures(scan, stmt.body, found);
      closures(scan, stmt.other, found);
    }

    void exprsClosures(const Scan& scan, Range<ExprId> exprs, Vector<const AstExpr*>& found) {
      for (const ExprId id : scan.program.exprRefs[exprs]) {
        exprClosures(scan, id, found);
      }
    }

    void exprClosures(const Scan& scan, ExprId id, Vector<const AstExpr*>& found) {
      if (!Valid(id)) {
        return;
      }

      const AstExpr& expr = scan.program.exprs[id];

      if (expr.kind == AstExprKind::Closure) {
        found.push_back(&expr);
      }

      if (expr.kind == AstExprKind::Closure) {
        closures(scan, expr.body, found);
      }

      if (!holdsPayload(expr.kind)) {
        partClosures(scan, expr, found);
      }
    }

    void partClosures(const Scan& scan, const AstExpr& expr, Vector<const AstExpr*>& found) {
      exprClosures(scan, expr.first, found);
      exprClosures(scan, expr.second, found);
      exprsClosures(scan, expr.args, found);

      for (const AstFieldInit& init : scan.program.inits[expr.inits]) {
        exprClosures(scan, init.value, found);
      }

      for (const AstSuffix& suffix : scan.program.suffixes[expr.suffixes]) {
        exprsClosures(scan, suffix.args, found);
      }

      armClosures(scan, expr.arms, found);
    }

    // `'name': `, the start of every message here.
    String quoted(const String& name) {
      return "'" + name + "': ";
    }

    String quotedName(const Program& program, const AstFuncDecl& decl) {
      return quoted(String(TextOf(program, decl.name.name)));
    }

    // `yield` inside the try-portion of a `try`.
    void checkYieldInTry(const Scan& scan, const AstFuncDecl& decl, const String& who, Vector<String>& errors) {
      const String why = "'yield' is not allowed inside a 'try' block's own try-portion (the coroutine's frame may be freed while an outer "
                         "exception handler still references it)";

      if (yieldInTry(scan, decl.body)) {
        errors.push_back(who + why + " \xe2\x80\x94 yield outside the try instead");  // an em dash
      }
    }

    // `yield` inside a closure literal: one problem for each closure that holds one.
    void checkYieldInClosures(const Scan& scan, const AstFuncDecl& decl, const String& who, Vector<String>& errors) {
      Vector<const AstExpr*> found;
      closures(scan, decl.body, found);
      const String why = "'yield' is not allowed inside a nested closure literal (the closure becomes a separate, non-coroutine function)";

      for (const AstExpr* closure : found) {
        if (contains(scan, closure->body, AstStmtKind::Yield)) {
          errors.push_back(who + why);
        }
      }
    }

    // A `coro func`: no `yield` in a try-portion or a closure, no `return`.
    void checkCoroutineFunc(const Program& program, const AstFuncDecl& decl, Vector<String>& errors) {
      const Scan scan{program};
      const String who = quotedName(program, decl);
      checkYieldInTry(scan, decl, who, errors);
      checkYieldInClosures(scan, decl, who, errors);
      const String noReturn = "a 'coro func' cannot contain a bare 'return' \xe2\x80\x94 completion is implicit (falling off the end)";

      if (contains(scan, decl.body, AstStmtKind::Return)) {
        errors.push_back(who + noReturn);
      }
    }

    // A plain function or method (`label` is how the message names it) may not `yield`.
    void checkNoYield(const Program& program, const String& label, const AstFuncDecl& decl, Vector<String>& errors) {
      if (contains(Scan{program}, decl.body, AstStmtKind::Yield)) {
        errors.push_back(quoted(label) + "'yield' can only appear inside a 'coro func' body");
      }
    }

    // A struct's methods, each named `Struct.method` in its messages.
    void checkStruct(const Program& program, const AstStructDecl& decl, Vector<String>& errors) {
      const String owner(TextOf(program, decl.name.name));

      for (const AstMember& member : program.members[decl.members]) {
        if (!member.isMethod) {
          continue;
        }

        const AstFuncDecl& method = program.funcs[member.method];
        checkNoYield(program, owner + "." + String(TextOf(program, method.name.name)), method, errors);
      }
    }

    void checkItem(const Program& program, const AstItem& item, Vector<String>& errors) {
      if (item.kind == ItemKind::Struct) {
        return checkStruct(program, program.structs[item.index], errors);
      }

      if (item.kind == ItemKind::CoroutineFunc) {
        return checkCoroutineFunc(program, program.funcs.items[item.index], errors);
      }

      if (item.kind != ItemKind::Func) {
        return;
      }

      const AstFuncDecl& decl = program.funcs.items[item.index];
      checkNoYield(program, String(TextOf(program, decl.name.name)), decl, errors);
    }

  }  // namespace

  Vector<String> CoroutineRestrictions(Session& session) {
    Vector<String> errors;

    for (const Unit& unit : session.units) {
      for (uint32_t i = unit.first; i < unit.last; i++) {
        checkItem(*unit.program, unit.program->items[i], errors);
      }
    }

    return errors;
  }

}  // namespace polyir::front
