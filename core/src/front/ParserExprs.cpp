// Expressions: precedence climbing over the binary operators, prefix operators, postfix suffixes and the primary ("postfixBase") forms.
//
// The one idea is precedence climbing: ParseBinary(minPrecedence) reads one operand, then keeps joining `operator operand` onto it while the
// next operator binds at least as tightly as `minPrecedence`; the right operand is parsed with `precedence + 1`, which makes every binary
// operator left associative (`a - b - c` is `(a - b) - c`). The levels are in kOperators, 9 (`* / %`) down to 1 (`||`).
#include "front/Parser.hpp"
#include "front/ParserSets.hpp"

namespace polyir::front {

  namespace {

    // ── operators, by token ─────────────────────────────────────────────────────────────────────────────────────────────────────────────────

    struct OperatorForm {
      TokenKind token;
      Operator op;
      int precedence;
    };

    constexpr OperatorForm kOperators[] = {
      {K::STAR, Operator::Mul, 9},        {K::SLASH, Operator::Div, 9},        {K::PERCENT, Operator::Rem, 9},
      {K::PLUS, Operator::Add, 8},        {K::MINUS, Operator::Sub, 8},        {K::SHL, Operator::Shl, 7},
      {K::SHR, Operator::Shr, 7},         {K::AMP, Operator::BitAnd, 6},       {K::CARET, Operator::BitXor, 5},
      {K::PIPE, Operator::BitOr, 4},      {K::EQEQ, Operator::Eq, 3},          {K::NEQ, Operator::Ne, 3},
      {K::LT, Operator::Lt, 3},           {K::LE, Operator::Le, 3},            {K::GT, Operator::Gt, 3},
      {K::GE, Operator::Ge, 3},           {K::EQEQEQ, Operator::Identical, 3}, {K::NEQEQ, Operator::NotIdentical, 3},
      {K::AMPAMP, Operator::LogAnd, 2},   {K::PIPEPIPE, Operator::LogOr, 1}};

    // Token kind -> operator form, so the climbing loop tests a token with one array read. A token that is no binary operator has
    // precedence 0.
    struct OperatorTable {
      OperatorForm byToken[128] = {};  // indexed by TokenKind (there are fewer than 128 kinds)
    };

    OperatorTable makeOperatorTable() {
      OperatorTable table;

      for (const OperatorForm& form : kOperators) {
        table.byToken[static_cast<size_t>(form.token)] = form;
      }

      return table;
    }

    const OperatorForm& operatorOf(TokenKind kind) {
      static const OperatorTable table = makeOperatorTable();

      return table.byToken[static_cast<size_t>(kind)];
    }

    int precedenceOf(TokenKind kind) {
      return operatorOf(kind).precedence;
    }

    // The node a prefix operator makes; Postfix means the token is no prefix operator.
    AstExprKind prefixKindOf(TokenKind kind) {
      switch (kind) {
        case K::MINUS:
          return AstExprKind::Neg;
        case K::VAL:
          return AstExprKind::ValCopy;
        case K::BANG:
          return AstExprKind::LogNot;
        case K::TILDE:
          return AstExprKind::BitNot;
        default:
          return AstExprKind::Postfix;
      }
    }

    // ── literals and constructors, by token ─────────────────────────────────────────────────────────────────────────────────────────────────

    struct LiteralForm {
      TokenKind token;
      AstExprKind kind;
    };

    constexpr LiteralForm kLiterals[] = {{K::INT_LIT, AstExprKind::Int},     {K::FLOAT_LIT, AstExprKind::Float}, {K::STRING_LIT, AstExprKind::String},
                                         {K::CHAR_LIT, AstExprKind::Char},   {K::TRUE_, AstExprKind::True},       {K::FALSE_, AstExprKind::False}};

    // A constructor keyword (`mutmap<K, V>(...)`), the node it makes and the optional parts its text has.
    struct CtorForm {
      TokenKind token;
      AstExprKind kind;
      CtorShape shape;
    };

    constexpr CtorForm kCtors[] = {
      {K::MUTARRAY, AstExprKind::NewMutarray, {}},
      {K::ARRAY, AstExprKind::NewArray, {}},
      {K::BUFFER, AstExprKind::NewBuffer, {.sized = true}},
      {K::SPARSE, AstExprKind::NewSparse, {}},
      {K::MAP, AstExprKind::NewMap, {.pair = true}},
      {K::MUTMAP, AstExprKind::NewMutmap, {.pair = true, .value = true, .optional = true}},
      {K::SET, AstExprKind::NewSet, {}},
      {K::MUTSET, AstExprKind::NewMutset, {.value = true, .optional = true}},
      {K::EITHEROK, AstExprKind::EitherOk, {.pair = true, .value = true}},
      {K::EITHERERR, AstExprKind::EitherErr, {.pair = true, .value = true}},
      {K::SOME, AstExprKind::Some, {.value = true}},
      {K::NONE, AstExprKind::None, {}}};

    // The literal row for a token, or nullptr.
    const LiteralForm* literalFormOf(TokenKind kind) {
      for (const LiteralForm& form : kLiterals) {
        if (form.token == kind) {
          return &form;
        }
      }

      return nullptr;
    }

    // The constructor row for a token, or nullptr.
    const CtorForm* ctorFormOf(TokenKind kind) {
      for (const CtorForm& form : kCtors) {
        if (form.token == kind) {
          return &form;
        }
      }

      return nullptr;
    }

    // `e, e, ...` (at least one expression), moved to the pool of expression lists.
    Range<ExprId> parseExprList(Parser& parser) {
      return CommaList(parser, parser.scratch.exprs, parser.program.exprRefs, [&] { return ParseExpr(parser); });
    }

    // The same list, or an empty one when the current token cannot start an expression.
    Range<ExprId> parseOptionalExprList(Parser& parser) {
      return OptionalCommaList(parser, kExprStart, parser.scratch.exprs, parser.program.exprRefs, [&] { return ParseExpr(parser); });
    }

    // `name(` followed by `ident :` opens a field list (`Point(x: 1, y: 2)`), never call arguments.
    bool atFieldList(const Parser& parser) {
      return At(parser, K::LPAREN) && KindAt(parser, 1) == K::IDENT && KindAt(parser, 2) == K::COLON;
    }

  }  // namespace

  // ── binary and unary operators ──────────────────────────────────────────────────────────────────────────────────────────────────────────

  ExprId ParseBinary(Parser& parser, int minPrecedence) {
    ExprId left = ParseUnary(parser);
    int precedence = precedenceOf(CurrentKind(parser));

    while (precedence >= minPrecedence && precedence > 0 && !ArmEnds(parser)) {
      left = ParseBinaryNode(parser, left, precedence);
      precedence = precedenceOf(CurrentKind(parser));
    }

    return left;
  }

  // `left operator right`, where the current token is the operator.
  ExprId ParseBinaryNode(Parser& parser, ExprId left, int precedence) {
    const Pos start = parser.program.exprs[left].pos;
    const Token operatorToken = Take(parser);
    const Operator op = operatorOf(operatorToken.kind).op;
    AstExpr expr{.kind = AstExprKind::Binary, .op = op, .pos = start, .opPos = PosOf(parser, operatorToken), .first = left};
    expr.second = ParseBinary(parser, precedence + 1);

    return AddExpr(parser, expr);
  }

  // `-x`, `!x`, `~x`, `val x` (a prefix operator applies to the unary expression after it), or a postfix expression.
  ExprId ParseUnary(Parser& parser) {
    RequireAny(parser, kExprStart, kExprStartMsg);
    const AstExprKind prefix = prefixKindOf(CurrentKind(parser));

    if (prefix == AstExprKind::Postfix) {
      return ParsePostfix(parser);
    }

    AstExpr expr{.kind = prefix, .pos = PosOf(parser, Take(parser))};
    expr.first = ParseUnary(parser);

    return AddExpr(parser, expr);
  }

  // ── postfix: `base.m(args) .f [i, j]` ───────────────────────────────────────────────────────────────────────────────────────────────────

  // A primary expression followed by any number of suffixes.
  ExprId ParsePostfix(Parser& parser) {
    const ExprId base = ParsePostfixBase(parser);

    if (!AtAny(parser, kSuffixStart) || ArmEnds(parser)) {
      return base;
    }

    AstExpr expr{.kind = AstExprKind::Postfix, .pos = parser.program.exprs[base].pos, .first = base};
    expr.suffixes = ParseSuffixes(parser);

    return AddExpr(parser, expr);
  }

  Range<AstSuffix> ParseSuffixes(Parser& parser) {
    const size_t mark = parser.scratch.suffixes.size();

    while (AtAny(parser, kSuffixStart) && !ArmEnds(parser)) {
      parser.scratch.suffixes.push_back(ParseSuffix(parser));
    }

    return Take(parser.program.suffixes, parser.scratch.suffixes, mark);
  }

  // `.name(args)` (the grammar looks two tokens ahead for the `(`; reading the name first decides the same way), `.name` or `[i, ...]`.
  AstSuffix ParseSuffix(Parser& parser) {
    AstSuffix suffix;
    const bool isIndex = At(parser, K::LBRACKET);
    suffix.pos = PosOf(parser, Take(parser));
    suffix.kind = isIndex ? AstSuffixKind::Index : AstSuffixKind::Field;

    if (isIndex) {
      ParseIndexArgs(parser, suffix);
    } else {
      ParseNameSuffix(parser, suffix);
    }

    return suffix;
  }

  // `[i, j]`, after its `[`.
  void ParseIndexArgs(Parser& parser, AstSuffix& suffix) {
    const bool inArm = parser.inArm;
    parser.inArm = false;

    suffix.args = parseExprList(parser);
    Expect(parser, K::RBRACKET);
    parser.inArm = inArm;
  }

  // `.name` or `.name(args)`, after its `.`.
  void ParseNameSuffix(Parser& parser, AstSuffix& suffix) {
    suffix.name = ExpectIdent(parser);

    if (!At(parser, K::LPAREN)) {
      return;
    }

    suffix.kind = AstSuffixKind::Method;
    suffix.args = ParseCallArgs(parser);
  }

  // ── primary expressions ─────────────────────────────────────────────────────────────────────────────────────────────────────────────────

  ExprId ParsePostfixBase(Parser& parser) {
    AstExpr expr;
    expr.pos = PosOf(parser, Current(parser));
    FillBase(parser, expr);

    return AddExpr(parser, expr);
  }

  void FillBase(Parser& parser, AstExpr& expr) {
    if (At(parser, K::IDENT)) {
      return FillIdent(parser, expr);
    }

    if (literalFormOf(CurrentKind(parser)) != nullptr) {
      return FillLiteral(parser, expr);
    }

    if (ctorFormOf(CurrentKind(parser)) != nullptr) {
      return FillCtor(parser, expr);
    }

    FillGrouped(parser, expr);
  }

  namespace {

    // `[...]`, `closure(...) -> T {...}` and `variant<...>(tag, v)`; anything else is no expression at all.
    void fillMade(Parser& parser, AstExpr& expr) {
      if (At(parser, K::LBRACKET)) {
        return FillListLit(parser, expr);
      }

      if (At(parser, K::CLOSURE)) {
        return FillClosure(parser, expr);
      }

      if (At(parser, K::VARIANT)) {
        return FillVariant(parser, expr);
      }

      Fail(parser, kPostfixStartMsg);
    }

  }  // namespace

  // `( e )`, `if (c) a else b`, `vec[...]`, and the forms `fillMade` reads.
  void FillGrouped(Parser& parser, AstExpr& expr) {
    if (At(parser, K::LPAREN)) {
      return FillParen(parser, expr);
    }

    if (At(parser, K::IF)) {
      return FillIfValue(parser, expr);
    }

    if (At(parser, K::MATCH)) {
      return FillMatch(parser, expr);
    }

    if (At(parser, K::VEC)) {
      return FillVecLit(parser, expr);
    }

    fillMade(parser, expr);
  }

  void FillLiteral(Parser& parser, AstExpr& expr) {
    expr.kind = literalFormOf(CurrentKind(parser))->kind;
    expr.literal = LiteralOf(parser, Take(parser));
  }

  namespace {

    // `Geometry.Point(x: 1)`: a struct named through its module, built with its fields named. How many tokens the path takes after the
    // first name (0 when there is none).
    size_t qualifiedLiteral(const Parser& parser) {
      size_t at = 0;

      while (KindAt(parser, at + 1) == K::DOT && KindAt(parser, at + 2) == K::IDENT) {
        at += 2;
      }

      const bool named = KindAt(parser, at + 1) == K::LPAREN && KindAt(parser, at + 2) == K::IDENT && KindAt(parser, at + 3) == K::COLON;

      return at > 0 && named ? at : 0;
    }

  }  // namespace

  // A name, a call `name(args)`, or a struct literal: `name { f: v }`, or the constructor call `name(f: v)`.
  void FillIdent(Parser& parser, AstExpr& expr) {
    expr.kind = AstExprKind::Name;

    if (qualifiedLiteral(parser) > 0) {
      expr.name = ParseTypeName(parser);

      return FillStructLit(parser, expr, K::RPAREN);
    }

    expr.name = IdentOf(parser, Take(parser));

    if (ArmEnds(parser)) {
      return;
    }

    if (atFieldList(parser)) {
      FillStructLit(parser, expr, K::RPAREN);
    } else if (At(parser, K::LPAREN)) {
      FillCall(parser, expr);
    } else if (At(parser, K::LBRACE)) {
      FillStructLit(parser, expr, K::RBRACE);
    }
  }

  void FillCall(Parser& parser, AstExpr& expr) {
    expr.kind = AstExprKind::Call;
    expr.args = ParseCallArgs(parser);
  }

  // `{ f: v, ... }` or `(f: v, ...)`, closed by `close`.
  void FillStructLit(Parser& parser, AstExpr& expr, TokenKind close) {
    expr.kind = AstExprKind::StructLit;
    Take(parser);
    expr.inits =
      OptionalCommaList(parser, kIdentStart, parser.scratch.inits, parser.program.inits, [&] { return ParseFieldInit(parser); });
    Expect(parser, close);
  }

  // Inside brackets an expression goes on over new lines, in an arm too (ArmEnds).
  Range<ExprId> ParseCallArgs(Parser& parser) {
    const bool inArm = parser.inArm;
    parser.inArm = false;

    Expect(parser, K::LPAREN);
    const Range<ExprId> args = parseOptionalExprList(parser);
    Expect(parser, K::RPAREN);
    parser.inArm = inArm;

    return args;
  }

  // `name: value` in a struct literal.
  AstFieldInit ParseFieldInit(Parser& parser) {
    AstFieldInit init;
    init.name = ExpectIdent(parser);
    Expect(parser, K::COLON);
    init.value = ParseExpr(parser);

    return init;
  }

  void FillParen(Parser& parser, AstExpr& expr) {
    const bool inArm = parser.inArm;
    parser.inArm = false;

    expr.kind = AstExprKind::Paren;
    Take(parser);
    expr.first = ParseExpr(parser);
    Expect(parser, K::RPAREN);
    parser.inArm = inArm;
  }

  // `if (c) a else b`: the value after `else` reaches as far as an expression goes, so `if (c) 1 else 2 + 3` is 2 + 3 when c fails.
  void FillIfValue(Parser& parser, AstExpr& expr) {
    expr.kind = AstExprKind::IfValue;
    Take(parser);
    Expect(parser, K::LPAREN);
    expr.first = ParseExpr(parser);
    Expect(parser, K::RPAREN);

    expr.second = ParseExpr(parser);
    Expect(parser, K::ELSE);
    expr.third = ParseExpr(parser);
  }

  void FillVecLit(Parser& parser, AstExpr& expr) {
    expr.kind = AstExprKind::VecLit;
    Take(parser);
    Expect(parser, K::LBRACKET);
    expr.args = parseOptionalExprList(parser);
    Expect(parser, K::RBRACKET);
  }

  // `[a, b, ...]`: at least one element (an empty list has nothing to give its type).
  void FillListLit(Parser& parser, AstExpr& expr) {
    const bool inArm = parser.inArm;
    parser.inArm = false;

    expr.kind = AstExprKind::ListLit;
    Take(parser);
    expr.args = parseExprList(parser);
    Expect(parser, K::RBRACKET);
    parser.inArm = inArm;
  }

  // `closure(params) -> ret { body }`
  void FillClosure(Parser& parser, AstExpr& expr) {
    expr.kind = AstExprKind::Closure;
    Take(parser);

    expr.params = ParseParamList(parser);
    Expect(parser, K::ARROW);
    expr.type = ParseType(parser);
    expr.body = ParseBlock(parser);
  }

  // `variant<A, B>(tag, value)`
  void FillVariant(Parser& parser, AstExpr& expr) {
    expr.kind = AstExprKind::Variant;
    Take(parser);

    ExpectTypeOpen(parser);
    expr.types = ParseTypeList(parser);
    ExpectTypeClose(parser);
    ParseVariantArgs(parser, expr);
  }

  // `(tag, value)`: the case number, then the payload.
  void ParseVariantArgs(Parser& parser, AstExpr& expr) {
    Expect(parser, K::LPAREN);
    expr.literal = ExpectLiteral(parser, K::INT_LIT);
    Expect(parser, K::COMMA);
    expr.first = ParseExpr(parser);
    Expect(parser, K::RPAREN);
  }

  // `kw<T>()`, `map<K, V>()`, `buffer<T, N>()`, `mutmap<K, V>(cap?)`, `Some<T>(x)`, ...
  void FillCtor(Parser& parser, AstExpr& expr) {
    const CtorForm& form = *ctorFormOf(CurrentKind(parser));
    expr.kind = form.kind;
    Take(parser);

    ExpectTypeOpen(parser);
    expr.type = ParseType(parser);
    ParseCtorTypes(parser, expr, form.shape);
    ParseCtorArgs(parser, expr, form.shape);
  }

  // The rest of the type arguments: `, V` for a pair, `, N` for a size; then the `>`.
  void ParseCtorTypes(Parser& parser, AstExpr& expr, const CtorShape& shape) {
    if (shape.pair) {
      expr.type2 = CommaType(parser);
    }

    if (shape.sized) {
      expr.literal = CommaSize(parser);
    }

    ExpectTypeClose(parser);
  }

  // `(value)`, `()`, or, for a constructor whose value is optional, either.
  void ParseCtorArgs(Parser& parser, AstExpr& expr, const CtorShape& shape) {
    Expect(parser, K::LPAREN);
    const bool isLeftOut = shape.optional && At(parser, K::RPAREN);

    if (shape.value && !isLeftOut) {
      expr.first = ParseExpr(parser);
    }

    Expect(parser, K::RPAREN);
  }

}  // namespace polyir::front
