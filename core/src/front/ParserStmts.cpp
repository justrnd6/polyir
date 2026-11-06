// Blocks and statements. A block is `{ statement* }`; a statement is picked by its first token (kStmtForms): a keyword statement
// (`let`, `if`, `for`, `switch`, `goto`, ...) or else an expression statement, which may be an assignment `target = value`.
#include "front/Parser.hpp"
#include "front/ParserSets.hpp"

namespace polyir::front {

  namespace {

    struct StmtForm {
      TokenKind token;
      AstStmtKind kind;
    };

    constexpr StmtForm kStmtForms[] = {
      {K::LET, AstStmtKind::Let},       {K::VAL, AstStmtKind::Let},       {K::VAR, AstStmtKind::Var},       {K::IF, AstStmtKind::If},
      {K::FOR, AstStmtKind::For},       {K::SWITCH, AstStmtKind::Switch}, {K::LABEL, AstStmtKind::Label},   {K::GOTO, AstStmtKind::Goto},
      {K::BRANCH, AstStmtKind::Branch}, {K::RETURN, AstStmtKind::Return}, {K::TRY, AstStmtKind::Try},       {K::THROW, AstStmtKind::Throw},
      {K::YIELD, AstStmtKind::Yield},   {K::MATCH, AstStmtKind::Match}};

    // The statement a keyword starts; anything else starts an expression statement.
    AstStmtKind stmtKindOf(TokenKind kind) {
      for (const StmtForm& form : kStmtForms) {
        if (form.token == kind) {
          return form.kind;
        }
      }

      return AstStmtKind::Expr;
    }

    // `goto name(args)`, after its `goto`.
    void parseGoto(Parser& parser, AstStmt& stmt) {
      stmt.goTo = ParseGotoTarget(parser);
    }

  }  // namespace

  // ── blocks ──────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────

  // A block's statements end at their own new lines, not at an arm's (ArmEnds).
  AstBlock ParseBlock(Parser& parser) {
    AstBlock block;
    block.pos = PosOf(parser, Current(parser));
    const bool inArm = parser.inArm;
    parser.inArm = false;

    Expect(parser, K::LBRACE);
    block.stmts = ParseStmtList(parser);
    Expect(parser, K::RBRACE);
    parser.inArm = inArm;

    return block;
  }

  // The statements of a block, up to the first token that cannot start one.
  Range<StmtId> ParseStmtList(Parser& parser) {
    const size_t mark = parser.scratch.stmts.size();

    while (AtAny(parser, kStmtStart)) {
      parser.scratch.stmts.push_back(ParseStmt(parser));
    }

    return Take(parser.program.stmtRefs, parser.scratch.stmts, mark);
  }

  StmtId ParseStmt(Parser& parser) {
    AstStmt stmt;
    stmt.pos = PosOf(parser, Current(parser));
    FillStmt(parser, stmt);

    return Add(parser.program.stmts, stmt);
  }

  // A statement is told apart by its first token: the keyword statements by their keyword, the rest are `target (= value)?`.
  void FillStmt(Parser& parser, AstStmt& stmt) {
    stmt.kind = stmtKindOf(CurrentKind(parser));

    if (stmt.kind == AstStmtKind::Expr) {
      return ParseExprStmt(parser, stmt);
    }

    stmt.isVal = At(parser, K::VAL);
    Take(parser);
    FillKeywordStmt(parser, stmt);
  }

  // The keyword statements, split over three functions only to keep each one short: let/var/if/for here, switch/label/goto/branch in
  // FillFlowStmt, and try/return/throw/yield in FillValueStmt.
  void FillKeywordStmt(Parser& parser, AstStmt& stmt) {
    switch (stmt.kind) {
      case AstStmtKind::Let: case AstStmtKind::Var:
        return ParseLetVar(parser, stmt);
      case AstStmtKind::If:
        return ParseIf(parser, stmt);
      case AstStmtKind::For:
        return ParseFor(parser, stmt);
      default:
        return FillFlowStmt(parser, stmt);
    }
  }

  void FillFlowStmt(Parser& parser, AstStmt& stmt) {
    switch (stmt.kind) {
      case AstStmtKind::Switch:
        return ParseSwitch(parser, stmt);
      case AstStmtKind::Label:
        return ParseLabel(parser, stmt);
      case AstStmtKind::Goto:
        return parseGoto(parser, stmt);
      case AstStmtKind::Branch:
        return ParseBranch(parser, stmt);
      case AstStmtKind::Match:
        return ParseMatchStmt(parser, stmt);
      default:
        return FillValueStmt(parser, stmt);
    }
  }

  // `try` has its own parser. `return` may have no value (`returnStmt : at='return' value=expr?`); `throw` and `yield` always have one.
  void FillValueStmt(Parser& parser, AstStmt& stmt) {
    if (stmt.kind == AstStmtKind::Try) {
      return ParseTry(parser, stmt);
    }

    if (stmt.kind == AstStmtKind::Return && !AtAny(parser, kExprStart)) {
      return;
    }

    stmt.value = ParseExpr(parser);
  }

  // ── the statements ──────────────────────────────────────────────────────────────────────────────────────────────────────────────────────

  // `let|val|var name (: type)? (= value)?`: the type may be left out when there is a value.
  void ParseLetVar(Parser& parser, AstStmt& stmt) {
    stmt.name = ExpectIdent(parser);

    if (Accept(parser, K::COLON)) {
      stmt.type = ParseType(parser);
    }

    if (Accept(parser, K::EQ)) {
      stmt.value = ParseExpr(parser);
    }
  }

  // `if (cond) { ... } else { ... }`, after its `if`; the `else` block is optional. `else if` never reaches here: Strings.cpp has already
  // turned it into `else { if ... }`.
  void ParseIf(Parser& parser, AstStmt& stmt) {
    Expect(parser, K::LPAREN);
    stmt.cond = ParseExpr(parser);
    Expect(parser, K::RPAREN);
    stmt.body = ParseBlock(parser);

    if (Accept(parser, K::ELSE)) {
      stmt.other = ParseBlock(parser);
    }
  }

  // `for (inits; cond; steps) { ... }` or `for (name in value) { ... }`, after its `for`.
  void ParseFor(Parser& parser, AstStmt& stmt) {
    Expect(parser, K::LPAREN);

    if (At(parser, K::IDENT) && KindAt(parser, 1) == K::IN) {
      return ParseForIn(parser, stmt);
    }

    ParseForHeader(parser, stmt);
    Expect(parser, K::RPAREN);
    stmt.body = ParseBlock(parser);
  }

  // `inits; cond; steps`, between the parentheses.
  void ParseForHeader(Parser& parser, AstStmt& stmt) {
    stmt.inits = ParseForInits(parser);
    stmt.cond = ParseExpr(parser);
    Expect(parser, K::SEMI);
    stmt.steps = ParseForBindings(parser);
  }

  // `for (name in value) body`, after its `(` (an IDENT followed by `in` tells it from the counting form).
  void ParseForIn(Parser& parser, AstStmt& stmt) {
    stmt.isForIn = true;
    stmt.name = ExpectIdent(parser);
    Expect(parser, K::IN);
    stmt.value = ParseExpr(parser);
    Expect(parser, K::RPAREN);

    stmt.body = ParseBlock(parser);
  }

  // The bindings before the first `;` of a `for`, and that `;`.
  Range<AstForBinding> ParseForInits(Parser& parser) {
    const Range<AstForBinding> inits = ParseForBindings(parser);
    Expect(parser, K::SEMI);

    return inits;
  }

  // `name = value, ...`, possibly none.
  Range<AstForBinding> ParseForBindings(Parser& parser) {
    return OptionalCommaList(
      parser, kIdentStart, parser.scratch.bindings, parser.program.forBindings, [&] { return ParseForBinding(parser); });
  }

  AstForBinding ParseForBinding(Parser& parser) {
    AstForBinding binding;
    binding.name = ExpectIdent(parser);
    Expect(parser, K::EQ);
    binding.value = ParseExpr(parser);

    return binding;
  }

  // `switch (v) { 0 => a(x) 1 => b() else => c() }`, a switch of jumps; a switch of blocks is ParserMatch.cpp's.
  void ParseSwitch(Parser& parser, AstStmt& stmt) {
    if (At(parser, K::LBRACE)) {
      return ParseSwitchArms(parser, stmt, false);
    }

    Expect(parser, K::LPAREN);
    stmt.value = ParseExpr(parser);
    Expect(parser, K::RPAREN);
    Expect(parser, K::LBRACE);

    if (!AtJumpArm(parser)) {
      return ParseSwitchArms(parser, stmt, true);
    }

    stmt.cases = ParseSwitchCases(parser);
    ParseSwitchDefault(parser, stmt);
  }

  Range<AstSwitchCase> ParseSwitchCases(Parser& parser) {
    const size_t mark = parser.scratch.cases.size();

    while (At(parser, K::INT_LIT)) {
      parser.scratch.cases.push_back(ParseSwitchCase(parser));
    }

    return Take(parser.program.cases, parser.scratch.cases, mark);
  }

  AstSwitchCase ParseSwitchCase(Parser& parser) {
    AstSwitchCase arm;
    arm.value = LiteralOf(parser, Take(parser));
    Expect(parser, K::FATARROW);
    arm.target = ParseGotoTarget(parser);

    return arm;
  }

  // `else => target }`
  void ParseSwitchDefault(Parser& parser, AstStmt& stmt) {
    Expect(parser, K::ELSE);
    Expect(parser, K::FATARROW);
    stmt.goTo = ParseGotoTarget(parser);
    Expect(parser, K::RBRACE);
  }

  // `label name(params):`, after its `label`.
  void ParseLabel(Parser& parser, AstStmt& stmt) {
    stmt.name = ExpectIdent(parser);
    stmt.params = ParseParamList(parser);
    Expect(parser, K::COLON);
  }

  // `branch cond ? a(x) : b(y)`
  void ParseBranch(Parser& parser, AstStmt& stmt) {
    stmt.cond = ParseExpr(parser);
    Expect(parser, K::QUESTION);
    stmt.goTo = ParseGotoTarget(parser);
    Expect(parser, K::COLON);
    stmt.goElse = ParseGotoTarget(parser);
  }

  // `try { ... } catch (name) { ... }`, after its `try`.
  void ParseTry(Parser& parser, AstStmt& stmt) {
    stmt.body = ParseBlock(parser);

    Expect(parser, K::CATCH);
    Expect(parser, K::LPAREN);
    stmt.name = ExpectIdent(parser);
    Expect(parser, K::RPAREN);
    stmt.other = ParseBlock(parser);
  }

  // `target` or `target = value`.
  void ParseExprStmt(Parser& parser, AstStmt& stmt) {
    stmt.target = ParsePostfix(parser);

    if (!Accept(parser, K::EQ)) {
      return;
    }

    stmt.isAssign = true;
    stmt.value = ParseExpr(parser);
  }

  // `name(args)`
  AstGotoTarget ParseGotoTarget(Parser& parser) {
    AstGotoTarget target;
    target.name = ExpectIdent(parser);
    target.args = ParseCallArgs(parser);

    return target;
  }

}  // namespace polyir::front
