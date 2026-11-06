// Text blocks, interpolation and `else if`, before the lexer sees the source (Strings.nina). It is a text-to-text pass, the very first step of
// ParseSource.
//
// `else if (c) {…}` becomes `else { if (c) {…} }` (the closing brace after the whole chain), so the parser and everything after it only ever see
// an `else` block. Only an `else` straight after a block's `}` is an if statement's; one in an `if` value (`if (a) 1 else if (b) 2 else 3`) stays. A text block (`"""…"""` over several lines, its common indent removed) and a string with `${expr}` in it become plain one-line
// literals joined by `+`, each expression wrapped in `str(…)`: `"v${n}"` is `("v" + str(n))`. The result keeps the source's line count (the
// newlines a text block spans follow it inside the parentheses), so later positions still point at the right line. The literal escapes themselves
// (`\n \t \r \" \' \\ \$`) are the lexer's; `LiteralValue` decodes them.
#include "front/Strings.hpp"
#include "front/HirBuild.hpp"

namespace polyir::front {

  namespace {

    // One part of a string: literal text as it is written in the source (escapes not yet decoded), or an expression.
    struct StringPart {
      String text;
      bool isCode = false;
    };

    using Parts = Vector<StringPart>;
    using Lines = Vector<Parts>;

    constexpr char kBackslash = '\\';
    constexpr StringView kElse = "else";

    // How many characters of literal text the character at `i` is written with: an escape (`\n`, `\"`, ...) is two, any other character one.
    size_t writtenLength(StringView text, size_t i) {
      return text[i] == kBackslash ? 2 : 1;
    }

    // The brace depth of the code being copied, and the depths at which an `else if` opened a brace that is still to close.
    struct CodeDepth {
      int depth = 0;
      Vector<int> pending;
    };

    bool isWordChar(char c) {
      return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_';
    }

    bool isWhitespace(char c) {
      return c == ' ' || c == '\t' || c == '\n' || c == '\r';
    }

    // The word that starts after the blanks at `at` (empty when there is none).
    StringView wordAfter(StringView text, size_t at) {
      size_t start = at;

      while (start < text.size() && isWhitespace(text[start])) {
        start++;
      }

      size_t end = start;

      while (end < text.size() && isWordChar(text[end])) {
        end++;
      }

      return text.substr(min(start, text.size()), end - min(start, text.size()));
    }

    // Whether `else if` may occur in the source (the fast path's question).
    bool anyElseIf(StringView source) {
      for (size_t at = source.find(kElse); at != StringView::npos; at = source.find(kElse, at + kElse.size())) {
        if (wordAfter(source, at + kElse.size()) == "if") {
          return true;
        }
      }

      return false;
    }

    constexpr StringView kTextBlockQuotes = "\"\"\"";

    Parts withPart(Parts parts, const String& text, bool isCode) {
      if (text.empty() && !isCode) {
        return parts;
      }

      parts.push_back(StringPart{text, isCode});

      return parts;
    }

    // The parts as one expression: literals quoted, expressions passed through `str`.
    String partText(const StringPart& part) {
      return part.isCode ? "str(" + part.text + ")" : "\"" + part.text + "\"";
    }

    String joined(const Parts& parts) {
      if (parts.empty()) {
        return "\"\"";
      }

      String out = partText(parts[0]);

      for (size_t i = 1; i < parts.size(); i++) {
        out += " + " + partText(parts[i]);
      }

      return out;
    }

    bool anyCode(const Parts& parts) {
      for (const StringPart& part : parts) {
        if (part.isCode) {
          return true;
        }
      }

      return false;
    }

    // ── the scan: code copied as it is, except for the strings in it ──

    struct StringScan {
      const String& text;
      size_t position = 0;
    };

    String scanAll(StringScan& scan);

    char currentChar(const StringScan& scan);

    bool hasMore(const StringScan& scan);

    bool startsAt(const StringScan& scan, StringView word);

    bool closes(const StringScan& scan, bool inside, int depth);

    void code(StringScan& scan, String& out, bool inside);

    void codeChar(StringScan& scan, String& out, CodeDepth& depth);

    bool elseIfHere(const StringScan& scan);

    void openElseIf(StringScan& scan, String& out, CodeDepth& depth);

    void closeFinished(StringScan& scan, String& out, CodeDepth& depth);

    bool innerEnded(const StringScan& scan, const CodeDepth& depth);

    void codeStep(StringScan& scan, String& out);

    void copy(StringScan& scan, String& out, size_t count);

    void copyUntilLineEnd(StringScan& scan, String& out);

    void copyChar(StringScan& scan, String& out);

    void oneLine(StringScan& scan, String& out);

    void textBlock(StringScan& scan, String& out);

    bool takeClosingQuotes(StringScan& scan, StringView close);

    Parts parts(StringScan& scan, bool block);

    void partStep(StringScan& scan, Parts& found, String& literal);

    bool inString(const StringScan& scan, bool block);

    void literalStep(StringScan& scan, String& literal);

    StringPart expression(StringScan& scan);

    char currentChar(const StringScan& scan) {
      return scan.text[scan.position];
    }

    bool hasMore(const StringScan& scan) {
      return scan.position < scan.text.size();
    }

    bool startsAt(const StringScan& scan, StringView word) {
      return StringView(scan.text).substr(scan.position, word.size()) == word;
    }

    bool closes(const StringScan& scan, bool inside, int depth) {
      return inside && depth == 0 && currentChar(scan) == '}';
    }

    String scanAll(StringScan& scan) {
      String out;
      code(scan, out, false);

      return out;
    }

    // How a character changes the brace depth: `{` opens one, `}` closes one.
    int nesting(char c) {
      if (c == '{') {
        return 1;
      }

      if (c == '}') {
        return -1;
      }

      return 0;
    }

    // `inside` is true in a `${…}`: the scan stops (without taking it) at the `}` that closes it.
    void code(StringScan& scan, String& out, bool inside) {
      CodeDepth braces;

      while (hasMore(scan) && !closes(scan, inside, braces.depth)) {
        if (elseIfHere(scan)) {
          openElseIf(scan, out, braces);
        } else {
          codeChar(scan, out, braces);
        }
      }
    }

    void codeChar(StringScan& scan, String& out, CodeDepth& braces) {
      const char c = currentChar(scan);
      braces.depth += nesting(c);
      codeStep(scan, out);

      if (c == '}') {
        closeFinished(scan, out, braces);
      }
    }

    // ── else if (PolyirElseIf) ──

    // The last character before `position` that is not white space, or 0.
    char codeBefore(const StringScan& scan) {
      size_t at = scan.position;

      while (at > 0 && isspace(static_cast<unsigned char>(scan.text[at - 1]))) {
        at--;
      }

      return at > 0 ? scan.text[at - 1] : '\0';
    }

    bool elseIfHere(const StringScan& scan) {
      if (!startsAt(scan, kElse)) {
        return false;
      }

      if (scan.position > 0 && isWordChar(scan.text[scan.position - 1])) {
        return false;
      }

      return codeBefore(scan) == '}' && wordAfter(scan.text, scan.position + kElse.size()) == "if";
    }

    // `else` becomes `else {`; the brace closes when the chain it starts has ended.
    void openElseIf(StringScan& scan, String& out, CodeDepth& braces) {
      out += "else {";
      scan.position += kElse.size();
      braces.depth++;
      braces.pending.push_back(braces.depth);
    }

    void closeFinished(StringScan& scan, String& out, CodeDepth& braces) {
      while (innerEnded(scan, braces)) {
        out += " }";
        braces.depth--;
        braces.pending.pop_back();
      }
    }

    // The block just closed was the last of an `else if`'s chain: its brace is at the depth an `else if` opened, and no `else` follows.
    bool innerEnded(const StringScan& scan, const CodeDepth& braces) {
      if (braces.pending.empty() || braces.pending.back() != braces.depth) {
        return false;
      }

      return wordAfter(scan.text, scan.position) != kElse;
    }

    void codeStep(StringScan& scan, String& out) {
      if (startsAt(scan, "//")) {
        return copyUntilLineEnd(scan, out);
      }

      if (startsAt(scan, kTextBlockQuotes)) {
        return textBlock(scan, out);
      }

      if (currentChar(scan) == '"') {
        return oneLine(scan, out);
      }

      if (currentChar(scan) == '\'') {
        return copyChar(scan, out);
      }

      copy(scan, out, 1);
    }

    void copy(StringScan& scan, String& out, size_t count) {
      const size_t to = min(scan.position + count, scan.text.size());
      out += scan.text.substr(scan.position, to - scan.position);
      scan.position = to;
    }

    void copyUntilLineEnd(StringScan& scan, String& out) {
      size_t end = scan.text.find('\n', scan.position);

      if (end == String::npos) {
        end = scan.text.size();
      }

      copy(scan, out, end - scan.position);
    }

    // A character literal, `'"'` and `'\''` included: `'x'` is three characters, `'\x'` four.
    void copyChar(StringScan& scan, String& out) {
      const bool escaped = scan.position + 1 < scan.text.size() && scan.text[scan.position + 1] == kBackslash;
      copy(scan, out, escaped ? 4 : 3);
    }

    // ── one-line strings ──
    // Unchanged unless it holds a `${…}`; an unterminated one is left for the lexer to report.
    void oneLine(StringScan& scan, String& out) {
      const size_t from = scan.position;
      scan.position++;  // past the opening quote
      const Parts found = parts(scan, false);
      const bool closed = takeClosingQuotes(scan, "\"");

      if (closed && anyCode(found)) {
        out += "(" + joined(found) + ")";
      } else {
        out += scan.text.substr(from, scan.position - from);
      }
    }

    // Whether the scan stopped on `close` (rather than at a line end or the end of the text); takes it if so.
    bool takeClosingQuotes(StringScan& scan, StringView close) {
      if (!hasMore(scan) || !startsAt(scan, close)) {
        return false;
      }

      scan.position += close.size();

      return true;
    }

    // The parts up to the closing quote(s), which are left in place. `block` reads a text block (newlines allowed, closed by `"""`); otherwise a one-line
    // string (closed by `"`, stopped by a newline).
    Parts parts(StringScan& scan, bool block) {
      Parts found;
      String literal;

      while (inString(scan, block)) {
        partStep(scan, found, literal);
      }

      return withPart(move(found), literal, false);
    }

    // A `${…}` ends the literal text before it; anything else is more of that text.
    void partStep(StringScan& scan, Parts& found, String& literal) {
      if (!startsAt(scan, "${")) {
        return literalStep(scan, literal);
      }

      found = withPart(move(found), literal, false);
      found.push_back(expression(scan));
      literal.clear();
    }

    bool inString(const StringScan& scan, bool block) {
      if (!hasMore(scan)) {
        return false;
      }

      if (block) {
        return !startsAt(scan, kTextBlockQuotes);
      }

      return currentChar(scan) != '"' && currentChar(scan) != '\n' && currentChar(scan) != '\r';
    }

    // One character of literal text, or an escape (both of its characters, so `\"` and `\$` never end or interpolate).
    void literalStep(StringScan& scan, String& literal) {
      copy(scan, literal, currentChar(scan) == kBackslash ? 2 : 1);
    }

    // `${expr}`: the expression's own source (its strings desugared too), with the `${` and `}` taken.
    StringPart expression(StringScan& scan) {
      scan.position += 2;
      String inner;
      code(scan, inner, true);

      if (hasMore(scan)) {
        scan.position++;
      }

      return StringPart{move(inner), true};
    }

  }  // namespace

  // ── text blocks ──
  // A text block's layout, the Kotlin and Java way: the line after the opening `"""` starts the text; the indent all its lines share (the closing
  // `"""`'s own line counts) is removed; spaces at a line's end are dropped; the text ends with a newline when the closing `"""` is on a line of its own.
  // An expression that is alone on its line after k spaces keeps every line of its value at that indent (`indentRest`).
  namespace {

    bool isSpaceOrTab(char c) {
      return c == ' ' || c == '\t';
    }

    // A line without the `\r` of a Windows line end.
    String withoutReturn(String line) {
      if (!line.empty() && line.back() == '\r') {
        line.pop_back();
      }

      return line;
    }

    Vector<String> pieces(const StringPart& part) {
      if (part.isCode) {
        return {part.text};
      }

      Vector<String> out;

      for (String& piece : SplitOn(part.text, '\n')) {
        out.push_back(withoutReturn(move(piece)));
      }

      return out;
    }

    // The parts split at the newlines of their literal text.
    void addPieces(Lines& lines, const StringPart& part) {
      const Vector<String> each = pieces(part);

      for (size_t i = 0; i < each.size(); i++) {
        if (i > 0) {
          lines.emplace_back();
        }

        lines.back() = withPart(move(lines.back()), each[i], part.isCode);
      }
    }

    Lines blockLines(const Parts& parts) {
      Lines lines(1);

      for (const StringPart& part : parts) {
        addPieces(lines, part);
      }

      return lines;
    }

    // The newlines the block spans, outside its expressions (they keep their own).
    String newlines(const Parts& parts) {
      String out;

      for (const StringPart& part : parts) {
        if (!part.isCode) {
          out.append(static_cast<size_t>(count(part.text.begin(), part.text.end(), '\n')), '\n');
        }
      }

      return out;
    }

    bool isBlankText(const String& text) {
      return text.find_first_not_of(" \t\n\r") == String::npos;
    }

    bool isBlank(const Parts& line) {
      for (const StringPart& part : line) {
        if (part.isCode || !isBlankText(part.text)) {
          return false;
        }
      }

      return true;
    }

    // The spaces a line starts with (0 when it starts with an expression).
    size_t width(const Parts& line) {
      if (line.empty() || line[0].isCode) {
        return 0;
      }

      size_t spaces = 0;

      while (spaces < line[0].text.size() && isSpaceOrTab(line[0].text[spaces])) {
        spaces++;
      }

      return spaces;
    }

    // Where the search for the least indent starts when the closing `"""` does not count: more than any real line is indented. The number is
    // the original Strings.nina's, kept so the output stays the same.
    constexpr size_t kNoIndentYet = 1000000;

    size_t sharedIndent(const Lines& lines, bool closedAlone, size_t closing) {
      size_t least = closedAlone ? closing : kNoIndentYet;

      for (const Parts& line : lines) {
        if (!isBlank(line)) {
          least = min(least, width(line));
        }
      }

      return least;
    }

    String trimEnd(String text) {
      while (!text.empty() && isSpaceOrTab(text.back())) {
        text.pop_back();
      }

      return text;
    }

    // A line with `indent` spaces taken off its start and the spaces at its end dropped; a blank line is empty.
    Parts trimmed(Parts line, size_t indent) {
      if (isBlank(line)) {
        return {};
      }

      if (!line[0].isCode) {
        line[0].text = line[0].text.substr(min(indent, line[0].text.size()));
      }

      if (!line.back().isCode) {
        line.back().text = trimEnd(move(line.back().text));
      }

      return line;
    }

    Parts indentedLine(Parts line) {
      if (line.size() != 2 || line[0].isCode || !line[1].isCode) {
        return line;
      }

      if (!isBlankText(line[0].text) || line[0].text.empty()) {
        return line;
      }

      line[1].text = "indentRest(str(" + line[1].text + "), " + to_string(line[0].text.size()) + ")";

      return line;
    }

    // The character written at `i`, ready to sit inside a one-line literal: an escape `\x` as it is (both characters), a bare `"` as `\"`.
    String charInsideLiteral(const String& text, size_t i) {
      if (text[i] == kBackslash) {
        return text.substr(i, 2);
      }

      if (text[i] == '"') {
        return "\\\"";
      }

      return String(1, text[i]);
    }

    // `text` with `\"` for each `"` that is not already escaped.
    String withQuotesEscaped(const String& text) {
      String out;

      for (size_t i = 0; i < text.size(); i += writtenLength(text, i)) {
        out += charInsideLiteral(text, i);
      }

      return out;
    }

    // Literal text with each bare `"` escaped, so it can sit inside a one-line literal.
    StringPart quoted(StringPart part) {
      if (part.isCode || part.text.find('"') == String::npos) {
        return part;
      }

      return StringPart{withQuotesEscaped(part.text), false};
    }

    void appendLine(Parts& out, Parts line) {
      for (StringPart& part : indentedLine(move(line))) {
        if (part.isCode || !part.text.empty()) {
          out.push_back(quoted(move(part)));
        }
      }
    }

    // Every line's parts in order, a newline after each line but the last (and after the last too when the closing `"""` stood alone).
    Parts flattened(Lines lines, bool closedAlone) {
      Parts out;

      for (size_t i = 0; i < lines.size(); i++) {
        appendLine(out, move(lines[i]));

        if (i + 1 < lines.size() || closedAlone) {
          out.push_back(StringPart{"\\n", false});
        }
      }

      return out;
    }

    Lines withoutOpening(Lines all) {
      if (all.size() > 1 && isBlank(all[0])) {
        all.erase(all.begin());
      }

      return all;
    }

    // The lines as the parts of one string: indent removed, a newline after each line.
    Parts laidOut(const Lines& all) {
      Lines lines = withoutOpening(all);
      const size_t closing = width(lines.back());
      const bool closedAlone = isBlank(lines.back());

      if (closedAlone) {
        lines.pop_back();
      }

      const size_t indent = sharedIndent(lines, closedAlone, closing);

      for (Parts& line : lines) {
        line = trimmed(move(line), indent);
      }

      return flattened(move(lines), closedAlone);
    }

    void textBlock(StringScan& scan, String& out) {
      const size_t from = scan.position;
      scan.position += kTextBlockQuotes.size();
      const Parts found = parts(scan, true);
      const bool isClosed = takeClosingQuotes(scan, kTextBlockQuotes);

      if (isClosed) {
        out += "(" + joined(laidOut(blockLines(found))) + newlines(found) + ")";
      } else {
        out += scan.text.substr(from, scan.position - from);  // unterminated: left for the lexer to report
      }
    }

  }  // namespace

  // A source with no text block, no `${` and no `else if` (most of them) is returned as it is.
  String DesugaredSource(String source) {
    const bool plain = source.find(kTextBlockQuotes) == String::npos && source.find("${") == String::npos;

    if (plain && !anyElseIf(source)) {
      return source;
    }

    StringScan scan{source};

    return scanAll(scan);
  }

  // ── literal values ──

  namespace {

    // The character an escape letter stands for: `n` is a newline; `"`, `'`, `\` and `$` stand for themselves.
    char escapedChar(char c) {
      switch (c) {
        case 'n':
          return '\n';
        case 't':
          return '\t';
        case 'r':
          return '\r';
        default:
          return c;
      }
    }

    // The character the literal text at `i` stands for: an escape's meaning (its letter is the next character), or the character itself.
    char decodedCharAt(StringView text, size_t i) {
      if (text[i] != kBackslash) {
        return text[i];
      }

      return escapedChar(text[i + 1]);
    }

  }  // namespace

  String LiteralValue(StringView token) {
    const StringView inner = token.substr(1, token.size() - 2);  // without the quotes
    String out;

    for (size_t i = 0; i < inner.size(); i += writtenLength(inner, i)) {
      out += decodedCharAt(inner, i);
    }

    return out;
  }

  int64_t CharValue(StringView token) {
    if (token.size() < 2 || token[1] != kBackslash) {
      return CharCode(token);
    }

    return static_cast<unsigned char>(escapedChar(token[2]));
  }

}  // namespace polyir::front
