// The lexer: turns source text into the list of tokens the parser reads (part of step 1, PARSE, in Pipeline.hpp: it runs on the text after
// Strings.cpp has turned text blocks, `${...}` and `else if` into plain code).
//
// The one idea: a `Scanner` walks the text left to right; at each position it skips blanks and comments, or takes the LONGEST token that
// starts there and appends it. Keywords are found in a small hash table and punctuation in tables grouped by first character, both built once
// from the token list in Lexer.hpp.
#include "front/Lexer.hpp"

namespace polyir::front {

  namespace {

    // ── the fixed spellings: keywords and punctuation ───────────────────────────────────────────────────────────────────────────────────────

    struct Spelling {
      StringView text;
      TokenKind kind;
    };

    // Three tables made from the one token list POLYIR_TOKEN_KINDS (Lexer.hpp): each macro below says what to write for one entry, and an
    // entry of a kind the table does not want writes nothing.
#define POLYIR_SPELLING(name, text) {text, TokenKind::name},
#define POLYIR_NO_SPELLING(name, text)
#define POLYIR_DISPLAY_QUOTED(name, text) "'" text "'",
#define POLYIR_DISPLAY_PLAIN(name, text) text,

    const Spelling kKeywords[] = {POLYIR_TOKEN_KINDS(POLYIR_NO_SPELLING, POLYIR_SPELLING, POLYIR_NO_SPELLING)};
    const Spelling kPunctuation[] = {POLYIR_TOKEN_KINDS(POLYIR_SPELLING, POLYIR_NO_SPELLING, POLYIR_NO_SPELLING)};
    const char* const kDisplay[] = {POLYIR_TOKEN_KINDS(POLYIR_DISPLAY_QUOTED, POLYIR_DISPLAY_QUOTED, POLYIR_DISPLAY_PLAIN)};

    // A hash table of the keywords with linear probing: a slot holds a keyword's index in kKeywords plus one (0 = empty slot).
    struct KeywordTable {
      static constexpr uint32_t kMask = 255;
      uint16_t slots[kMask + 1] = {};
    };

    TokenKind keywordKindOf(const KeywordTable& table, StringView word);

    uint32_t hashOf(StringView word);

    void insertKeyword(KeywordTable& table, uint16_t index);

    TokenKind keywordKindOf(const KeywordTable& table, StringView word) {
      for (uint32_t slot = hashOf(word);; slot = (slot + 1) & KeywordTable::kMask) {
        if (table.slots[slot] == 0) {
          return TokenKind::IDENT;
        }

        if (kKeywords[table.slots[slot] - 1].text == word) {
          return kKeywords[table.slots[slot] - 1].kind;
        }
      }
    }

    // FNV-1a, a standard simple string hash: start from its offset basis, then mix in each byte with xor and a multiply by its prime.
    constexpr uint32_t kFnvOffsetBasis = 2166136261u;
    constexpr uint32_t kFnvPrime = 16777619u;

    uint32_t hashOf(StringView word) {
      uint32_t hash = kFnvOffsetBasis;

      for (char c : word) {
        hash = (hash ^ static_cast<unsigned char>(c)) * kFnvPrime;
      }

      return hash & KeywordTable::kMask;
    }

    void insertKeyword(KeywordTable& table, uint16_t index) {
      uint32_t slot = hashOf(kKeywords[index].text);

      while (table.slots[slot] != 0) {
        slot = (slot + 1) & KeywordTable::kMask;
      }

      table.slots[slot] = static_cast<uint16_t>(index + 1);
    }

    // The punctuation tokens grouped by their first character, longest first, so the first one that matches is the longest match.
    struct PunctuationTable {
      Vector<Spelling> byFirstChar[128];  // one group per ASCII character
    };

    const Spelling* longestPunctuation(const PunctuationTable& table, StringView rest);

    void sortLongestFirst(Vector<Spelling>& group);

    // `& 127` keeps a non-ASCII byte inside the 128-entry table; its group cannot match, because every punctuation spelling is ASCII.
    const Spelling* longestPunctuation(const PunctuationTable& table, StringView rest) {
      for (const Spelling& spelling : table.byFirstChar[static_cast<unsigned char>(rest[0]) & 127]) {
        if (rest.substr(0, spelling.text.size()) == spelling.text) {
          return &spelling;
        }
      }

      return nullptr;
    }

    KeywordTable makeKeywordTable() {
      KeywordTable table;

      for (uint16_t i = 0; i < sizeof(kKeywords) / sizeof(kKeywords[0]); i++) {
        insertKeyword(table, i);
      }

      return table;
    }

    PunctuationTable makePunctuationTable() {
      PunctuationTable table;

      for (const Spelling& spelling : kPunctuation) {
        table.byFirstChar[static_cast<unsigned char>(spelling.text[0])].push_back(spelling);
      }

      for (Vector<Spelling>& group : table.byFirstChar) {
        sortLongestFirst(group);
      }

      return table;
    }

    // The order sortLongestFirst wants: whether `first` is the longer spelling.
    bool isLonger(const Spelling& first, const Spelling& second) {
      return first.text.size() > second.text.size();
    }

    // The longer spellings first, so `longestPunctuation` finds `<<` before `<`.
    void sortLongestFirst(Vector<Spelling>& group) {
      sort(group.begin(), group.end(), isLonger);
    }

    bool isDigit(char c) {
      return c >= '0' && c <= '9';
    }

    bool isWordStart(char c) {
      return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
    }

    bool isWordChar(char c) {
      return isWordStart(c) || isDigit(c);
    }

    bool isBlank(char c) {
      return c == ' ' || c == '\t' || c == '\r' || c == '\n';
    }

    bool isEscapeLetter(char c) {
      return c == '"' || c == '\'' || c == '\\' || c == '$' || c == 'n' || c == 't' || c == 'r';
    }

    // ── the scanner ─────────────────────────────────────────────────────────────────────────────────────────────────────────────────────

    // Where the scan is in the text, and the tokens made so far.
    struct Scanner {
      StringView source;
      size_t offset = 0;      // where the next token starts
      uint32_t line = 1;
      uint32_t column = 1;
      LexResult result{};
    };

    LexResult scanAll(Scanner& scanner);

    bool hasMore(const Scanner& scanner);

    char charAt(const Scanner& scanner, size_t ahead);

    size_t lengthWhile(const Scanner& scanner, size_t from, bool (*accepts)(char));

    void advance(Scanner& scanner, size_t length);

    void emit(Scanner& scanner, TokenKind kind, size_t length);

    void failAtCurrentChar(Scanner& scanner);

    void skipBlanks(Scanner& scanner);

    void skipComment(Scanner& scanner);

    void scanToken(Scanner& scanner);

    void scanOther(Scanner& scanner, char c);

    void scanWord(Scanner& scanner);

    void scanNumber(Scanner& scanner);

    void scanString(Scanner& scanner);

    void scanChar(Scanner& scanner);

    size_t literalCharLength(const Scanner& scanner, size_t ahead, char quote);

    void scanPunctuation(Scanner& scanner);

    bool hasMore(const Scanner& scanner) {
      return scanner.offset < scanner.source.size();
    }

    char charAt(const Scanner& scanner, size_t ahead) {
      return scanner.offset + ahead < scanner.source.size() ? scanner.source[scanner.offset + ahead] : '\0';
    }

    LexResult scanAll(Scanner& scanner) {
      scanner.result.tokens.reserve(scanner.source.size() / 4 + 16);  // a guess (about one token per four characters), only to save regrowing

      while (hasMore(scanner) && Ok(scanner.result)) {
        scanToken(scanner);
      }

      if (Ok(scanner.result)) {
        emit(scanner, TokenKind::EOF_, 0);
      }

      return move(scanner.result);
    }

    // Columns count bytes, as the Nina lexer's do; a token never contains a newline.
    void advance(Scanner& scanner, size_t length) {
      scanner.column += static_cast<uint32_t>(length);
      scanner.offset += length;
    }

    void emit(Scanner& scanner, TokenKind kind, size_t length) {
      const Token token{kind, static_cast<uint32_t>(scanner.offset), static_cast<uint32_t>(length), scanner.line, scanner.column};
      scanner.result.tokens.push_back(token);
      advance(scanner, length);
    }

    // The scan stops here: "line:column: unexpected character 'c'".
    void failAtCurrentChar(Scanner& scanner) {
      const String where = to_string(scanner.line) + ":" + to_string(scanner.column);
      scanner.result.error = where + ": unexpected character '" + String(1, scanner.source[scanner.offset]) + "'";
    }

    // The length of the run at the current position: the first `from` characters (already known to belong), plus every following character
    // `accepts` takes.
    size_t lengthWhile(const Scanner& scanner, size_t from, bool (*accepts)(char)) {
      size_t length = from;

      while (scanner.offset + length < scanner.source.size() && accepts(scanner.source[scanner.offset + length])) {
        length++;
      }

      return length;
    }

    // One blank character; a newline moves the position to column 1 of the next line.
    void skipOneBlank(Scanner& scanner) {
      if (scanner.source[scanner.offset] == '\n') {
        scanner.line++;
        scanner.column = 1;
      } else {
        scanner.column++;
      }

      scanner.offset++;
    }

    void skipBlanks(Scanner& scanner) {
      while (hasMore(scanner) && isBlank(scanner.source[scanner.offset])) {
        skipOneBlank(scanner);
      }
    }

    // A `//` comment runs to the end of its line (or of the text, on the last line).
    void skipComment(Scanner& scanner) {
      size_t end = scanner.source.find('\n', scanner.offset);

      if (end == StringView::npos) {
        end = scanner.source.size();
      }

      advance(scanner, end - scanner.offset);
    }

    void scanToken(Scanner& scanner) {
      const char c = scanner.source[scanner.offset];

      if (isBlank(c)) {
        return skipBlanks(scanner);
      }

      if (c == '/' && charAt(scanner, 1) == '/') {
        return skipComment(scanner);
      }

      if (isWordStart(c)) {
        return scanWord(scanner);
      }

      if (isDigit(c)) {
        return scanNumber(scanner);
      }

      scanOther(scanner, c);
    }

    // A string, a character or punctuation.
    void scanOther(Scanner& scanner, char c) {
      if (c == '"') {
        return scanString(scanner);
      }

      if (c == '\'') {
        return scanChar(scanner);
      }

      scanPunctuation(scanner);
    }

    void scanWord(Scanner& scanner) {
      static const KeywordTable keywords = makeKeywordTable();
      const size_t length = lengthWhile(scanner, 1, isWordChar);
      emit(scanner, keywordKindOf(keywords, scanner.source.substr(scanner.offset, length)), length);
    }

    void scanNumber(Scanner& scanner) {
      const size_t whole = lengthWhile(scanner, 0, isDigit);
      const bool hasFraction = charAt(scanner, whole) == '.' && isDigit(charAt(scanner, whole + 1));

      if (!hasFraction) {
        return emit(scanner, TokenKind::INT_LIT, whole);
      }

      const size_t length = lengthWhile(scanner, whole + 1, isDigit);
      emit(scanner, TokenKind::FLOAT_LIT, length);
    }

    // The size of the character (one byte, or a `\` and its letter) `ahead` bytes on, inside a literal closed by `quote`. It is 0 when there is
    // none: the closing quote, a line end, the end of the text, or a `\` that is no escape.
    size_t literalCharLength(const Scanner& scanner, size_t ahead, char quote) {
      const char c = charAt(scanner, ahead);

      if (scanner.offset + ahead >= scanner.source.size() || c == quote || c == '\n' || c == '\r') {
        return 0;
      }

      if (c != '\\') {
        return 1;
      }

      const bool isEscape = isEscapeLetter(charAt(scanner, ahead + 1)) && scanner.offset + ahead + 1 < scanner.source.size();

      return isEscape ? 2 : 0;
    }

    // A string ends at the first quote that is not escaped (`\n \t \r \" \' \\ \$`). A newline or a bad escape before that makes it no token at all.
    void scanString(Scanner& scanner) {
      size_t body = 1;  // the opening quote

      while (literalCharLength(scanner, body, '"') > 0) {
        body += literalCharLength(scanner, body, '"');
      }

      const bool isClosed = charAt(scanner, body) == '"' && scanner.offset + body < scanner.source.size();

      if (isClosed) {
        return emit(scanner, TokenKind::STRING_LIT, body + 1);
      }

      failAtCurrentChar(scanner);
    }

    // A character is exactly one byte, or one escape, between quotes.
    void scanChar(Scanner& scanner) {
      const size_t size = literalCharLength(scanner, 1, '\'');
      const bool isClosed = size > 0 && charAt(scanner, 1 + size) == '\'' && scanner.offset + 1 + size < scanner.source.size();

      if (isClosed) {
        return emit(scanner, TokenKind::CHAR_LIT, size + 2);
      }

      failAtCurrentChar(scanner);
    }

    void scanPunctuation(Scanner& scanner) {
      static const PunctuationTable table = makePunctuationTable();
      const Spelling* found = longestPunctuation(table, scanner.source.substr(scanner.offset));

      if (found == nullptr) {
        return failAtCurrentChar(scanner);
      }

      emit(scanner, found->kind, found->text.size());
    }

  }  // namespace

  // How an error message shows a kind: `'{'` for a spelled token, `IDENT` for one with no fixed spelling.
  const char* KindDisplay(TokenKind kind) {
    return kDisplay[static_cast<size_t>(kind)];
  }

  LexResult LexSource(StringView source) {
    Scanner scanner{source};

    return scanAll(scanner);
  }

}  // namespace polyir::front
