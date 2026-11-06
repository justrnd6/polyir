// The tokens of polyir (the `lexer PolyirTokens` block of lang/src/Polyir.intent) and the hand-written scanner that makes them.
//
// The rules are the generated DFA lexer's: the longest match wins, a keyword beats IDENT on a tie, whitespace and `// ...` comments are skipped,
// and the first character nothing matches stops the scan with `unexpected character 'c'`. (So `>>` is always one SHR token, never two GT, and a
// string with no closing quote on its line is an error at its opening quote -- exactly as in the Nina front end.)
#pragma once

#include "polyir/Std.hpp"
#include "polyir/Export.hpp"

namespace polyir::front {

  // Every token kind, written once. This is a "table macro": the caller passes three macros, and each entry calls the one for its group --
  // PUNCT(name, text) for punctuation, WORD(name, text) for keywords and NAMED(name, display) for tokens with no fixed spelling. The enum below
  // and the lexer's spelling tables are all made from it, so they cannot disagree. The order is the Nina lexer's.
#define POLYIR_TOKEN_KINDS(PUNCT, WORD, NAMED) \
  PUNCT(LBRACE, "{") PUNCT(RBRACE, "}") PUNCT(LPAREN, "(") PUNCT(RPAREN, ")") PUNCT(LBRACKET, "[") PUNCT(RBRACKET, "]") \
  PUNCT(COLON, ":") PUNCT(SEMI, ";") PUNCT(COMMA, ",") PUNCT(DOT, ".") PUNCT(ARROW, "->") PUNCT(BIARROW, "<->") \
  PUNCT(FATARROW, "=>") PUNCT(QUESTION, "?") PUNCT(EQEQEQ, "===") PUNCT(NEQEQ, "!==") PUNCT(EQEQ, "==") PUNCT(NEQ, "!=") \
  PUNCT(LE, "<=") PUNCT(GE, ">=") PUNCT(SHL, "<<") PUNCT(SHR, ">>") PUNCT(LT, "<") PUNCT(GT, ">") PUNCT(EQ, "=") \
  PUNCT(PLUS, "+") PUNCT(MINUS, "-") PUNCT(STAR, "*") PUNCT(SLASH, "/") PUNCT(PERCENT, "%") PUNCT(AMPAMP, "&&") \
  PUNCT(AMP, "&") PUNCT(PIPEPIPE, "||") PUNCT(PIPE, "|") PUNCT(CARET, "^") PUNCT(TILDE, "~") PUNCT(BANG, "!") PUNCT(AT, "@") \
  WORD(FUNC, "func") WORD(EXTERN, "extern") WORD(IMPORT, "import") WORD(GLOBAL, "global") WORD(VEC, "vec") \
  WORD(DARRAY, "darray") WORD(ARRAY, "array") WORD(MUTARRAY, "mutarray") WORD(BUFFER, "buffer") WORD(MAP, "map") \
  WORD(MUTMAP, "mutmap") WORD(SET, "set") WORD(MUTSET, "mutset") WORD(GRID, "grid") WORD(TENSOR, "tensor") \
  WORD(SPARSE, "sparse") WORD(HASHMAP, "hashmap") WORD(META, "meta") WORD(STRUCT, "struct") WORD(INTERFACE, "interface") \
  WORD(TYPE, "type") WORD(STATIC, "static") WORD(LET, "let") WORD(VAR, "var") WORD(VAL, "val") WORD(IF, "if") \
  WORD(ELSE, "else") WORD(FOR, "for") WORD(IN, "in") WORD(SWITCH, "switch") WORD(LABEL, "label") WORD(GOTO, "goto") \
  WORD(BRANCH, "branch") WORD(RETURN, "return") WORD(TRY, "try") WORD(CATCH, "catch") WORD(THROW, "throw") \
  WORD(CLOSURE, "closure") WORD(VARIANT, "variant") WORD(EITHER, "either") WORD(EITHEROK, "eitherOk") \
  WORD(EITHERERR, "eitherErr") WORD(OPTIONAL, "optional") WORD(BRIDGE, "bridge") WORD(SOME, "Some") WORD(NONE, "None") \
  WORD(CORO, "coro") WORD(KERNEL, "kernel") WORD(UNIFORM, "uniform") WORD(INDEX, "index") WORD(WRITABLE, "writable") \
  WORD(WORKGROUP, "workgroup") WORD(YIELD, "yield") WORD(TRUE_, "true") WORD(FALSE_, "false") WORD(PTR, "ptr") \
  WORD(MATCH, "match") \
  NAMED(IDENT, "IDENT") NAMED(FLOAT_LIT, "FLOAT_LIT") NAMED(INT_LIT, "INT_LIT") NAMED(STRING_LIT, "STRING_LIT") \
  NAMED(CHAR_LIT, "CHAR_LIT") NAMED(EOF_, "end of input")

#define POLYIR_TOKEN_ENUM(name, text) name,
  enum class TokenKind : uint8_t { POLYIR_TOKEN_KINDS(POLYIR_TOKEN_ENUM, POLYIR_TOKEN_ENUM, POLYIR_TOKEN_ENUM) };
#undef POLYIR_TOKEN_ENUM

  // One token: where its text is in the source, and where it starts (1-based line and column).
  struct Token {
    TokenKind kind = TokenKind::EOF_;
    uint32_t offset = 0;
    uint32_t length = 0;
    uint32_t line = 1;
    uint32_t column = 1;
  };

  // A set of token kinds (there are fewer than 128), for the parser's lookahead tests.
  struct KindSet {
    uint64_t lowBits = 0;
    uint64_t highBits = 0;
  };

  // What the lexer makes: the tokens, or the first error.
  struct LexResult {
    Vector<Token> tokens;    // always ends with an EOF_ token, unless there is an error
    String error;            // "line:column: unexpected character 'c'", or empty
  };

  constexpr bool Has(const KindSet& set, TokenKind kind);

  constexpr KindSet With(const KindSet& set, TokenKind kind);

  bool Ok(const LexResult& result);

  // How a kind is written in an "expected ..." message: `'func'`, `'{'`, `IDENT`, `end of input`.
  POLYIR_API const char* KindDisplay(TokenKind kind);

  // The lexer's entry point: all the tokens of `source`.
  POLYIR_API LexResult LexSource(StringView source);

  // Kinds 0-63 are the bits of `lowBits`, kinds 64-127 the bits of `highBits`.
  inline constexpr bool Has(const KindSet& set, TokenKind kind) {
    const unsigned bit = static_cast<unsigned>(kind);

    if (bit < 64) {
      return ((set.lowBits >> bit) & 1) != 0;
    }

    return ((set.highBits >> (bit - 64)) & 1) != 0;
  }

  // A copy of `set` that also holds `kind`.
  inline constexpr KindSet With(const KindSet& set, TokenKind kind) {
    const unsigned bit = static_cast<unsigned>(kind);

    if (bit < 64) {
      return KindSet{set.lowBits | (uint64_t{1} << bit), set.highBits};
    }

    return KindSet{set.lowBits, set.highBits | (uint64_t{1} << (bit - 64))};
  }

  // The set of the listed kinds; ParserSets.hpp writes the grammar's sets with it.
  constexpr KindSet MakeKindSet(InitializerList<TokenKind> kinds) {
    KindSet set;

    for (TokenKind kind : kinds) {
      set = With(set, kind);
    }

    return set;
  }

  inline bool Ok(const LexResult& result) {
    return result.error.empty();
  }

  // The token's text in `source`.
  inline StringView TokenText(const Token& token, StringView source) {
    return source.substr(token.offset, token.length);
  }

}  // namespace polyir::front
