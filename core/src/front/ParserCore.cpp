// The parser's basic moves -- look at the current token, take it, expect a kind or fail -- and the entry point ParseSource, which runs
// the string desugaring (Strings.cpp), the lexer and the parser on one source text. Every other Parser*.cpp file is built from these moves.
#include "front/Strings.hpp"
#include "front/Parser.hpp"
#include "front/ParserSets.hpp"

namespace polyir::front {

  // Once an error is latched the parser sees only the EOF token, so nothing more can match.
  const Token& Current(const Parser& parser) {
    return parser.tokens[parser.hasFailed ? parser.tokens.size() - 1 : parser.position];
  }

  TokenKind KindAt(const Parser& parser, size_t ahead) {
    if (parser.hasFailed) {
      return TokenKind::EOF_;
    }

    return parser.tokens[min(parser.position + ahead, parser.tokens.size() - 1)].kind;
  }

  Token Take(Parser& parser) {
    const Token token = Current(parser);

    if (!parser.hasFailed && parser.position + 1 < parser.tokens.size()) {
      parser.position++;
    }

    return token;
  }

  bool Accept(Parser& parser, TokenKind kind) {
    if (CurrentKind(parser) != kind) {
      return false;
    }

    Take(parser);

    return true;
  }

  Token Expect(Parser& parser, TokenKind kind) {
    if (CurrentKind(parser) == kind) {
      return Take(parser);
    }

    Fail(parser, KindDisplay(kind));

    return Current(parser);
  }

  void ExpectTypeOpen(Parser& parser) {
    Expect(parser, TokenKind::LT);
    parser.typeLists++;
  }

  void ExpectTypeClose(Parser& parser) {
    parser.typeLists--;

    if (parser.halfShift) {
      parser.halfShift = false;
      return;
    }

    parser.halfShift = CurrentKind(parser) == TokenKind::SHR && parser.typeLists >= 1;
    Expect(parser, parser.halfShift ? TokenKind::SHR : TokenKind::GT);
  }

  Pos AcceptPos(Parser& parser, TokenKind kind) {
    return At(parser, kind) ? PosOf(parser, Take(parser)) : Pos{};
  }

  void RequireAny(Parser& parser, const KindSet& set, StringView expected) {
    if (!AtAny(parser, set)) {
      Fail(parser, expected);
    }
  }

  // "line:column: expected <list> (found 'text')" at the current token; only the first one is kept.
  void Fail(Parser& parser, StringView expected) {
    if (parser.hasFailed) {
      return;
    }

    const Token& token = Current(parser);
    const String found = String(TokenText(token, parser.source));
    parser.error = to_string(token.line) + ":" + to_string(token.column) + ": expected " + String(expected) + " (found '" + found + "')";
    parser.hasFailed = true;
    AddHint(parser);
  }

  // `darray` and `hashmap` were renamed; a parse error that stops at one of them says what to write instead.
  void AddHint(Parser& parser) {
    if (parser.error.find("(found 'darray')") != String::npos) {
      parser.error += " — darray was renamed: write mutarray<T> (changeable) or array<T> (unchangeable)";
    }

    if (parser.error.find("(found 'hashmap')") != String::npos) {
      parser.error += " — hashmap was renamed: write mutmap<K,V> (changeable) or map<K,V> (unchangeable)";
    }
  }

  StringView TextSince(const Parser& parser, size_t from) {
    const Token& first = parser.tokens[from];
    const Token& last = parser.tokens[parser.position - 1];

    return parser.source.substr(first.offset, last.offset + last.length - first.offset);
  }

  Ident IdentOf(Parser& parser, const Token& token) {
    return Ident{Intern(*parser.program.names, TokenText(token, parser.source)), PosOf(parser, token)};
  }

  Literal LiteralOf(const Parser& parser, const Token& token) {
    return Literal{TokenText(token, parser.source), PosOf(parser, token)};
  }

  // program: topLevel* EOF
  void ParseProgram(Parser& parser) {
    parser.program.items.reserve(parser.source.size() / 400 + 4);  // a guess (about one item per 400 bytes), only to save regrowing

    if (!AtModule(parser) && !AtInternal(parser) && !At(parser, K::AT)) {
      RequireAny(parser, kProgramStart, kProgramStartMsg);
    }

    while (AtItemStart(parser)) {
      ParseTopLevel(parser);
    }

    Expect(parser, TokenKind::EOF_);
  }

  namespace {

    // Room for the pools up front, guessed from the source size (about one expression per 6 bytes), so they rarely grow while parsing.
    void reserveFor(Program& program, size_t bytes) {
      program.exprs.items.reserve(bytes / 6 + 16);
      program.stmts.items.reserve(bytes / 24 + 16);
      program.types.items.reserve(bytes / 24 + 16);
      program.exprRefs.items.reserve(bytes / 12 + 16);
      program.stmtRefs.items.reserve(bytes / 24 + 16);
    }

    // The parse result: the program, and the error when there is one.
    ParseResult finish(Program&& program, String error) {
      ParseResult result;
      result.program = move(program);

      if (!error.empty()) {
        result.errors.push_back(move(error));
      }

      return result;
    }

    ParseResult parseTokens(Program&& program, const Vector<Token>& tokens) {
      reserveFor(program, program.source->size());
      Parser parser{program, tokens, *program.source};
      ParseProgram(parser);

      return finish(move(program), Error(parser));
    }

  }  // namespace

  // Text blocks and `else if` are rewritten first (Strings.cpp), then the text is lexed and parsed; the first error stops it.
  ParseResult ParseSource(String text, Interner& names) {
    Program program;
    program.names = &names;
    program.source = make_shared<const String>(DesugaredSource(move(text)));
    LexResult lexed = LexSource(*program.source);

    if (!Ok(lexed)) {
      return finish(move(program), lexed.error);
    }

    return parseTokens(move(program), lexed.tokens);
  }

  ParseResult ParseSource(String text) {
    return ParseSource(move(text), GlobalInterner());
  }

}  // namespace polyir::front
