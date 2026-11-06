// `match` and the `switch` with block arms: their arms and the patterns they test.
//
//   match (subject) { pattern (if guard)? => value ... else => value }      a value; with no subject, each arm is a condition
//   match (subject) { pattern => { statements } ... }                      a statement: an arm is a block or one expression
//   switch (subject) { pattern => { statements } ... }                     a switch of blocks (a switch of jumps is ParserStmts.cpp's)
//
// An arm is told apart by how it is written. With a subject, a literal, `_`, a name, `Some(…)`, `None`, `Ok(…)`, `Err(…)`,
// `variant(N, …)` or a list `[…]`, joined by `|` and followed by `=>` or `if`, is a pattern. Any other expression is a
// condition. Arms are not separated by anything but a new line, so a `[`, `(` or `-` that starts a line starts the next arm.
#include "front/Parser.hpp"
#include "front/ParserSets.hpp"

namespace polyir::front {

  namespace {

    // What the arms of one construct may be.
    struct ArmRules {
      bool subject = false;  // there is a subject, so an arm may be a pattern
      bool blocks = false;   // an arm may give a block
      bool values = false;   // an arm may give one expression
    };

    StringView textAt(const Parser& parser, size_t ahead) {
      const Token& token = parser.tokens[min(parser.position + ahead, parser.tokens.size() - 1)];

      return parser.source.substr(token.offset, token.length);
    }

    bool opens(TokenKind kind) {
      return kind == K::LPAREN || kind == K::LBRACKET || kind == K::LBRACE;
    }

    bool closes(TokenKind kind) {
      return kind == K::RPAREN || kind == K::RBRACKET || kind == K::RBRACE;
    }

    // How many tokens the brackets opening `at` tokens ahead take, the closing one too; 0 when they never close.
    size_t bracketed(const Parser& parser, size_t at) {
      int depth = opens(KindAt(parser, at)) ? 1 : 0;
      size_t end = at + 1;

      while (depth > 0 && KindAt(parser, end) != K::EOF_) {
        const TokenKind kind = KindAt(parser, end);
        depth += opens(kind) ? 1 : (closes(kind) ? -1 : 0);
        end++;
      }

      return depth == 0 ? end - at : 0;
    }

    bool isLiteral(TokenKind kind) {
      const bool text = kind == K::STRING_LIT || kind == K::CHAR_LIT;

      return text || kind == K::INT_LIT || kind == K::FLOAT_LIT || kind == K::TRUE_ || kind == K::FALSE_;
    }

    // `Ok(` or `Err(`: the either cases, which are names everywhere else.
    bool atEitherCase(const Parser& parser, size_t at) {
      const StringView name = textAt(parser, at);
      const bool named = KindAt(parser, at) == K::IDENT && (name == "Ok" || name == "Err");

      return named && KindAt(parser, at + 1) == K::LPAREN;
    }

    // How many tokens one pattern starting `at` tokens ahead takes, or 0 when no pattern starts there.
    size_t patternLength(const Parser& parser, size_t at) {
      const TokenKind kind = KindAt(parser, at);
      const bool negative = kind == K::MINUS && (KindAt(parser, at + 1) == K::INT_LIT || KindAt(parser, at + 1) == K::FLOAT_LIT);

      if (negative) {
        return 2;
      }

      if (isLiteral(kind) || kind == K::NONE) {
        return 1;
      }

      if (kind == K::LBRACKET) {
        return bracketed(parser, at);
      }

      const bool payload = kind == K::SOME || kind == K::VARIANT || atEitherCase(parser, at);

      return payload ? 1 + bracketed(parser, at + 1) : (kind == K::IDENT ? 1 : 0);
    }

    // Whether the arm starting here is a pattern: patterns joined by `|`, then `=>` or `if`.
    bool atPatternArm(const Parser& parser) {
      size_t at = 0;
      size_t length = patternLength(parser, at);

      while (length > 0) {
        at += length;

        if (KindAt(parser, at) != K::PIPE) {
          return KindAt(parser, at) == K::FATARROW || KindAt(parser, at) == K::IF;
        }

        length = patternLength(parser, at + 1);
        at++;
      }

      return false;
    }

    PatternId addPattern(Parser& parser, const AstPattern& pattern) {
      return Add(parser.program.patterns, pattern);
    }

    // `"y"`, `0`, `-1`, `'c'` or `true`: the literal, as an expression the checker reads as a constant.
    void fillValue(Parser& parser, AstPattern& pattern) {
      pattern.kind = AstPatternKind::Value;

      if (!At(parser, K::MINUS)) {
        pattern.value = ParsePostfixBase(parser);

        return;
      }

      AstExpr negative{.kind = AstExprKind::Neg, .pos = PosOf(parser, Take(parser))};
      negative.first = ParsePostfixBase(parser);
      pattern.value = Add(parser.program.exprs, negative);
    }

    // `Some(p)`, `Ok(p)` or `Err(p)`, its keyword or name already read.
    void fillPayload(Parser& parser, AstPattern& pattern, AstPatternKind kind) {
      pattern.kind = kind;
      Expect(parser, K::LPAREN);
      pattern.first = ParsePattern(parser);
      Expect(parser, K::RPAREN);
    }

    // `variant(N, p)`, its keyword already read.
    void fillCase(Parser& parser, AstPattern& pattern) {
      pattern.kind = AstPatternKind::Case;
      Expect(parser, K::LPAREN);
      pattern.literal = ExpectLiteral(parser, K::INT_LIT);
      Expect(parser, K::COMMA);

      pattern.first = ParsePattern(parser);
      Expect(parser, K::RPAREN);
    }

    bool atRest(const Parser& parser) {
      return At(parser, K::DOT) && KindAt(parser, 1) == K::DOT && KindAt(parser, 2) == K::DOT;
    }

    // `...` or `...name`, the last thing in a list pattern.
    void fillRest(Parser& parser, AstPattern& pattern) {
      Take(parser);
      Take(parser);
      Take(parser);
      pattern.hasRest = true;

      if (At(parser, K::IDENT)) {
        pattern.name = ExpectIdent(parser);
      }
    }

    // The elements of a list pattern, up to its `]` or its `...`.
    Range<PatternId> listItems(Parser& parser) {
      const size_t mark = parser.scratch.patterns.size();

      while (!At(parser, K::RBRACKET) && !atRest(parser) && !parser.hasFailed) {
        parser.scratch.patterns.push_back(ParsePattern(parser));

        if (!Accept(parser, K::COMMA)) {
          break;
        }
      }

      return Take(parser.program.patternRefs, parser.scratch.patterns, mark);
    }

    // `[a, b]`, `[a, ...]`, `[a, ...rest]` or `[]`.
    void fillList(Parser& parser, AstPattern& pattern) {
      pattern.kind = AstPatternKind::List;
      Take(parser);
      pattern.items = listItems(parser);

      if (atRest(parser)) {
        fillRest(parser, pattern);
      }

      Expect(parser, K::RBRACKET);
    }

    // `_`, `Ok(p)`, `Err(p)`, or a name that binds what it matches.
    void fillNamed(Parser& parser, AstPattern& pattern) {
      if (atEitherCase(parser, 0)) {
        const bool ok = textAt(parser, 0) == "Ok";
        Take(parser);

        return fillPayload(parser, pattern, ok ? AstPatternKind::Ok : AstPatternKind::Err);
      }

      pattern.kind = textAt(parser, 0) == "_" ? AstPatternKind::Any : AstPatternKind::Name;
      pattern.name = ExpectIdent(parser);
    }

    void fillKeyword(Parser& parser, AstPattern& pattern) {
      const TokenKind kind = Take(parser).kind;

      if (kind == K::NONE) {
        pattern.kind = AstPatternKind::None;
      } else if (kind == K::SOME) {
        fillPayload(parser, pattern, AstPatternKind::Some);
      } else {
        fillCase(parser, pattern);
      }
    }

    void fillPattern(Parser& parser, AstPattern& pattern) {
      if (At(parser, K::IDENT)) {
        return fillNamed(parser, pattern);
      }

      if (At(parser, K::LBRACKET)) {
        return fillList(parser, pattern);
      }

      if (At(parser, K::SOME) || At(parser, K::NONE) || At(parser, K::VARIANT)) {
        return fillKeyword(parser, pattern);
      }

      fillValue(parser, pattern);
    }

    PatternId onePattern(Parser& parser) {
      AstPattern pattern;
      pattern.pos = PosOf(parser, Current(parser));
      RequireAny(parser, kPatternStart, kPatternStartMsg);
      fillPattern(parser, pattern);

      return addPattern(parser, pattern);
    }

    // `else`, a pattern with its guard, or a condition: whatever comes before the arm's `=>`.
    void armHead(Parser& parser, AstArm& arm, ArmRules rules) {
      if (Accept(parser, K::ELSE)) {
        arm.isElse = true;

        return;
      }

      if (!rules.subject || !atPatternArm(parser)) {
        arm.cond = ParseExpr(parser);

        return;
      }

      arm.pattern = ParsePattern(parser);
      arm.guard = Accept(parser, K::IF) ? ParseExpr(parser) : ExprId{};
    }

    // What the arm gives, after its `=>`: a block, or one expression.
    void armTail(Parser& parser, AstArm& arm, ArmRules rules) {
      if ((rules.blocks && At(parser, K::LBRACE)) || !rules.values) {
        arm.body = ParseBlock(parser);

        return;
      }

      arm.value = ParseExpr(parser);
    }

    AstArm parseArm(Parser& parser, ArmRules rules) {
      AstArm arm;
      arm.pos = PosOf(parser, Current(parser));
      armHead(parser, arm, rules);
      Expect(parser, K::FATARROW);

      armTail(parser, arm, rules);
      Accept(parser, K::COMMA);

      return arm;
    }

    // The arms up to the `}` that closes them (the `{` is already read), and that `}`.
    Range<AstArm> parseArms(Parser& parser, ArmRules rules) {
      const size_t mark = parser.scratch.arms.size();
      const bool outer = parser.inArm;
      parser.inArm = true;

      while (!At(parser, K::RBRACE) && !At(parser, K::EOF_)) {
        parser.scratch.arms.push_back(parseArm(parser, rules));
      }

      parser.inArm = outer;
      Expect(parser, K::RBRACE);

      return Take(parser.program.arms, parser.scratch.arms, mark);
    }

    // `(subject)` when there is one, then the `{` that opens the arms. Whether there was a subject.
    bool subjectOf(Parser& parser, ExprId& subject) {
      const bool has = Accept(parser, K::LPAREN);

      if (has) {
        subject = ParseExpr(parser);
        Expect(parser, K::RPAREN);
      }

      Expect(parser, K::LBRACE);

      return has;
    }

  }  // namespace

  PatternId ParsePattern(Parser& parser) {
    const PatternId first = onePattern(parser);

    if (!At(parser, K::PIPE)) {
      return first;
    }

    AstPattern alternatives{.kind = AstPatternKind::Or, .pos = parser.program.patterns[first].pos};
    const size_t mark = parser.scratch.patterns.size();
    parser.scratch.patterns.push_back(first);

    while (Accept(parser, K::PIPE)) {
      parser.scratch.patterns.push_back(onePattern(parser));
    }

    alternatives.items = Take(parser.program.patternRefs, parser.scratch.patterns, mark);

    return addPattern(parser, alternatives);
  }

  void FillMatch(Parser& parser, AstExpr& expr) {
    expr.kind = AstExprKind::Match;
    Take(parser);
    const bool subject = subjectOf(parser, expr.first);
    expr.arms = parseArms(parser, ArmRules{.subject = subject, .values = true});
  }

  void ParseMatchStmt(Parser& parser, AstStmt& stmt) {
    const bool subject = subjectOf(parser, stmt.value);
    stmt.arms = parseArms(parser, ArmRules{.subject = subject, .blocks = true, .values = true});
  }

  // `switch { … }` or `switch (subject) { … }` with block arms; for the second the `{` is already read.
  void ParseSwitchArms(Parser& parser, AstStmt& stmt, bool subject) {
    stmt.kind = AstStmtKind::Match;
    stmt.isSwitch = true;

    if (!subject) {
      Expect(parser, K::LBRACE);
    }

    stmt.arms = parseArms(parser, ArmRules{.subject = subject, .blocks = true});
  }

  // A switch of jumps starts its arms `0 => name(` or `else => name(`; any other switch has blocks.
  bool AtJumpArm(const Parser& parser) {
    const bool key = At(parser, K::INT_LIT) || At(parser, K::ELSE);

    return key && KindAt(parser, 1) == K::FATARROW && KindAt(parser, 2) == K::IDENT && KindAt(parser, 3) == K::LPAREN;
  }

  bool ArmEnds(const Parser& parser) {
    if (!parser.inArm || parser.position == 0 || parser.hasFailed) {
      return false;
    }

    const TokenKind kind = CurrentKind(parser);
    const bool starts = kind == K::LBRACKET || kind == K::LPAREN || kind == K::MINUS;

    return starts && Current(parser).line > parser.tokens[parser.position - 1].line;
  }

}  // namespace polyir::front
