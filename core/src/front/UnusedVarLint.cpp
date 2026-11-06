// The unused-`var` lint (UnusedVarLint.nina). A `var` that is never the target of an assignment anywhere in its own function is better as a `let`.
// It is a whole-function scan, not block-scope aware, and it walks exactly the statements and expressions the Nina lint walks: a `var` is found in
// the nested blocks of `if`, `for` and `try` (not inside closures); an assignment is found in the same blocks, in the value of a `let`, `var`,
// assignment, `return` or `throw`, in the condition of an `if`, and inside closure literals reached from those (never from a bare call statement).
#include "front/HirBuild.hpp"
#include "front/Lint.hpp"
#include "front/Pipeline.hpp"

namespace polyir::front {

  namespace {

    // The program the lint reads.
    struct UnusedVarLint {
      const Program& program;
    };

    Vector<String> lintAll(UnusedVarLint& lint, const String& file);

    void varsInBlock(const UnusedVarLint& lint, const AstBlock& block, Vector<Ident>& out);

    // The `var`s a statement declares, and those in its nested blocks.
    void varsInStmt(const UnusedVarLint& lint, const AstStmt& stmt, Vector<Ident>& out);

    void assignedInBlock(const UnusedVarLint& lint, const AstBlock& block, Set<Symbol>& out);

    // The names a statement assigns (and the closures in its values assign).
    void assignedInStmt(const UnusedVarLint& lint, const AstStmt& stmt, Set<Symbol>& out);

    void assignedInArms(const UnusedVarLint& lint, Range<AstArm> arms, Set<Symbol>& out);

    void assignedInAssign(const UnusedVarLint& lint, const AstStmt& stmt, Set<Symbol>& out);

    void assignedInClosuresOf(const UnusedVarLint& lint, ExprId id, Set<Symbol>& out);

    void assignedInBranches(const UnusedVarLint& lint, const AstStmt& stmt, Set<Symbol>& out);

    void assignedInClosuresOfNode(const UnusedVarLint& lint, const AstExpr& expr, Set<Symbol>& out);

    void assignedInClosuresOfOperands(const UnusedVarLint& lint, const AstExpr& expr, Set<Symbol>& out);

    void assignedInClosuresOfPostfix(const UnusedVarLint& lint, const AstExpr& expr, Set<Symbol>& out);

    void assignedInClosuresOfList(const UnusedVarLint& lint, Range<ExprId> list, Set<Symbol>& out);

    void assignedInClosuresOfInits(const UnusedVarLint& lint, Range<AstFieldInit> inits, Set<Symbol>& out);

    Set<Symbol> assignedIn(const UnusedVarLint& lint, const AstFuncDecl& func);

    Vector<String> neverAssigned(const UnusedVarLint& lint, const Vector<Ident>& declared, const Set<Symbol>& assigned, const String& owner,
                                 const String& file);

    Vector<String> checkFunction(const UnusedVarLint& lint, const AstFuncDecl& func, const String& owner, const String& file);

    Vector<String> checkItem(const UnusedVarLint& lint, const AstItem& item, const String& file);

    Vector<String> checkMethod(const UnusedVarLint& lint, const AstFuncDecl& method, const String& owner, const String& file);

    Vector<String> checkMethods(const UnusedVarLint& lint, const AstStructDecl& decl, const String& file);

    // The warning for one `var`, at its name.
    String message(const UnusedVarLint& lint, const Ident& declared, const String& owner, const String& file);

    const AstStmt& stmtAt(const UnusedVarLint& lint, StmtId id);

    const AstStmt& stmtAt(const UnusedVarLint& lint, StmtId id) {
      return lint.program.stmts[id];
    }

    void append(Vector<String>& all, const Vector<String>& more) {
      all.insert(all.end(), more.begin(), more.end());
    }

    void varsInBlock(const UnusedVarLint& lint, const AstBlock& block, Vector<Ident>& out) {
      for (StmtId id : lint.program.stmtRefs[block.stmts]) {
        varsInStmt(lint, stmtAt(lint, id), out);
      }
    }

    // The `var`s a statement declares, and those in its nested blocks (an arm's too).
    void varsInStmt(const UnusedVarLint& lint, const AstStmt& stmt, Vector<Ident>& out) {
      if (stmt.kind == AstStmtKind::Var) {
        out.push_back(stmt.name);
      }

      for (const AstArm& arm : lint.program.arms[stmt.arms]) {
        varsInBlock(lint, arm.body, out);
      }

      const bool nested = stmt.kind == AstStmtKind::If || stmt.kind == AstStmtKind::For || stmt.kind == AstStmtKind::Try;

      if (!nested) {
        return;
      }

      varsInBlock(lint, stmt.body, out);

      if (stmt.kind != AstStmtKind::For) {
        varsInBlock(lint, stmt.other, out);
      }
    }

    void assignedInBlock(const UnusedVarLint& lint, const AstBlock& block, Set<Symbol>& out) {
      for (StmtId id : lint.program.stmtRefs[block.stmts]) {
        assignedInStmt(lint, stmtAt(lint, id), out);
      }
    }

    // The names a statement assigns (and the closures in its values assign).
    void assignedInStmt(const UnusedVarLint& lint, const AstStmt& stmt, Set<Symbol>& out) {
      switch (stmt.kind) {
        case AstStmtKind::Let: case AstStmtKind::Var: case AstStmtKind::Return: case AstStmtKind::Throw:
          return assignedInClosuresOf(lint, stmt.value, out);
        case AstStmtKind::Expr:
          return assignedInAssign(lint, stmt, out);
        case AstStmtKind::If: case AstStmtKind::Try:
          return assignedInBranches(lint, stmt, out);
        case AstStmtKind::For:
          return assignedInBlock(lint, stmt.body, out);
        case AstStmtKind::Match:
          return assignedInArms(lint, stmt.arms, out);
        default:
          return;
      }
    }

    // What each arm of a `match` assigns: in its block, and in the closures of its condition, guard and value.
    void assignedInArms(const UnusedVarLint& lint, Range<AstArm> arms, Set<Symbol>& out) {
      for (const AstArm& arm : lint.program.arms[arms]) {
        for (const ExprId id : {arm.cond, arm.guard, arm.value}) {
          assignedInClosuresOf(lint, id, out);
        }

        assignedInBlock(lint, arm.body, out);
      }
    }

    // The condition of an `if` (its closures), then both blocks of an `if` or a `try`.
    void assignedInBranches(const UnusedVarLint& lint, const AstStmt& stmt, Set<Symbol>& out) {
      if (stmt.kind == AstStmtKind::If) {
        assignedInClosuresOf(lint, stmt.cond, out);
      }

      assignedInBlock(lint, stmt.body, out);
      assignedInBlock(lint, stmt.other, out);
    }

    // `x = v`, `x[i] = v` and `x.f = v` assign `x`; a statement that only calls something assigns nothing.
    void assignedInAssign(const UnusedVarLint& lint, const AstStmt& stmt, Set<Symbol>& out) {
      if (!stmt.isAssign) {
        return;
      }

      const AstExpr& target = lint.program.exprs[stmt.target];
      const AstExpr& base = target.kind == AstExprKind::Postfix ? lint.program.exprs[target.first] : target;

      if (base.kind == AstExprKind::Name) {
        out.insert(base.name.name);
      }

      assignedInClosuresOf(lint, stmt.value, out);
    }

    // The names assigned inside the closure literals of an expression.
    void assignedInClosuresOf(const UnusedVarLint& lint, ExprId id, Set<Symbol>& out) {
      if (Valid(id)) {
        assignedInClosuresOfNode(lint, lint.program.exprs[id], out);
      }
    }

    void assignedInClosuresOfNode(const UnusedVarLint& lint, const AstExpr& expr, Set<Symbol>& out) {
      switch (expr.kind) {
        case AstExprKind::Call: case AstExprKind::VecLit:
          return assignedInClosuresOfList(lint, expr.args, out);
        case AstExprKind::StructLit:
          return assignedInClosuresOfInits(lint, expr.inits, out);
        case AstExprKind::Closure:
          return assignedInBlock(lint, expr.body, out);
        case AstExprKind::Postfix:
          return assignedInClosuresOfPostfix(lint, expr, out);
        case AstExprKind::Match:
          return assignedInArms(lint, expr.arms, out);
        default:
          return assignedInClosuresOfOperands(lint, expr, out);
      }
    }

    // The binary operators, the unary ones, a parenthesis, and the capacity of a `mutmap` or `mutset`: the operands, entered as they are.
    void assignedInClosuresOfOperands(const UnusedVarLint& lint, const AstExpr& expr, Set<Symbol>& out) {
      switch (expr.kind) {
        case AstExprKind::Binary: case AstExprKind::Neg: case AstExprKind::ValCopy: case AstExprKind::LogNot: case AstExprKind::BitNot:
        case AstExprKind::Paren: case AstExprKind::NewMutmap: case AstExprKind::NewMutset:
          break;
        default:
          return;
      }

      assignedInClosuresOf(lint, expr.first, out);
      assignedInClosuresOf(lint, expr.second, out);
    }

    void assignedInClosuresOfPostfix(const UnusedVarLint& lint, const AstExpr& expr, Set<Symbol>& out) {
      assignedInClosuresOf(lint, expr.first, out);

      for (const AstSuffix& suffix : lint.program.suffixes[expr.suffixes]) {
        if (suffix.kind != AstSuffixKind::Field) {
          assignedInClosuresOfList(lint, suffix.args, out);
        }
      }
    }

    void assignedInClosuresOfList(const UnusedVarLint& lint, Range<ExprId> list, Set<Symbol>& out) {
      for (ExprId id : lint.program.exprRefs[list]) {
        assignedInClosuresOf(lint, id, out);
      }
    }

    void assignedInClosuresOfInits(const UnusedVarLint& lint, Range<AstFieldInit> inits, Set<Symbol>& out) {
      for (const AstFieldInit& init : lint.program.inits[inits]) {
        assignedInClosuresOf(lint, init.value, out);
      }
    }

    // The warning for one `var`, at its name.
    String message(const UnusedVarLint& lint, const Ident& declared, const String& owner, const String& file) {
      const String name(TextOf(lint.program, declared.name));
      const String what = "'" + name + "' is declared `var` but never reassigned in '" + owner + "' — use `let` instead";
      const String located = DiagnosticMessage(what, SpanAt(declared.pos));

      return file.empty() ? located : file + ":" + located;
    }

    Set<Symbol> assignedIn(const UnusedVarLint& lint, const AstFuncDecl& func) {
      Set<Symbol> assigned;
      assignedInBlock(lint, func.body, assigned);

      return assigned;
    }

    Vector<String> checkFunction(const UnusedVarLint& lint, const AstFuncDecl& func, const String& owner, const String& file) {
      Vector<Ident> declared;
      varsInBlock(lint, func.body, declared);

      return neverAssigned(lint, declared, assignedIn(lint, func), owner, file);
    }

    // A warning for each declared `var` that is not in the assigned set.
    Vector<String> neverAssigned(const UnusedVarLint& lint, const Vector<Ident>& declared, const Set<Symbol>& assigned, const String& owner,
                                 const String& file) {
      Vector<String> problems;

      for (const Ident& var : declared) {
        if (assigned.count(var.name) == 0) {
          problems.push_back(message(lint, var, owner, file));
        }
      }

      return problems;
    }

    Vector<String> checkMethod(const UnusedVarLint& lint, const AstFuncDecl& method, const String& owner, const String& file) {
      return checkFunction(lint, method, owner + "." + String(TextOf(lint.program, method.name.name)), file);
    }

    Vector<String> checkMethods(const UnusedVarLint& lint, const AstStructDecl& decl, const String& file) {
      Vector<String> problems;
      const String owner(TextOf(lint.program, decl.name.name));

      for (const AstMember& member : lint.program.members[decl.members]) {
        if (member.isMethod) {
          append(problems, checkMethod(lint, lint.program.funcs[member.method], owner, file));
        }
      }

      return problems;
    }

    Vector<String> checkItem(const UnusedVarLint& lint, const AstItem& item, const String& file) {
      if (item.kind == ItemKind::Struct) {
        return checkMethods(lint, lint.program.structs[item.index], file);
      }

      if (item.kind != ItemKind::Func && item.kind != ItemKind::CoroutineFunc) {
        return {};
      }

      const AstFuncDecl& func = lint.program.funcs.items[item.index];

      return checkFunction(lint, func, String(TextOf(lint.program, func.name.name)), file);
    }

    Vector<String> lintAll(UnusedVarLint& lint, const String& file) {
      Vector<String> problems;

      for (const AstItem& item : lint.program.items) {
        append(problems, checkItem(lint, item, file));
      }

      return problems;
    }

  }  // namespace

  Vector<String> LintWarnings(const Program& program, const String& file) {
    UnusedVarLint lint{program};

    return lintAll(lint, file);
  }

}  // namespace polyir::front
