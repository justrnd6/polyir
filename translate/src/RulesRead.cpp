// Reads a `.translate` page (Rules.hpp). The page is a document: `# Js` is its title, `source:` and `target:` name the languages
// it translates from and to, `style:` sets its layout, and each `## section` runs to the next heading. A rule is one line, `construct(parts) := value`; a longer template goes on indented
// lines under it, which may also hold `before:` and `uses:` lines. A line starting `//` is a comment.
#include "Pages.hpp"

namespace polyir::translate {

  namespace {

    // The page as it is read: the rules so far, the section, and the rule that indented lines add to.
    struct Reader {
      TranslateRules rules;
      String section;
      int line = 0;
      bool isOpen = false;  // the last rule read takes the indented lines after it
      String entry;         // the support file or import just read, which an indented `uses:` line adds to
      Vector<String> templateLines;
    };

    // The sections that hold no rules; every other section holds rules for its source's constructs.
    const char* const kSettingSections[] = {"operators", "reserved", "imports", "support", "spans"};

    void problem(Reader& reader, const String& message) {
      reader.rules.problems.push_back(reader.rules.file + ":" + to_string(reader.line) + ": " + message);
    }

    String trimmed(const String& text) {
      const size_t from = text.find_first_not_of(" \t");
      const size_t to = text.find_last_not_of(" \t\r");

      return from == String::npos ? "" : text.substr(from, to - from + 1);
    }

    bool startsWith(const String& text, const String& prefix) {
      return text.compare(0, prefix.size(), prefix) == 0;
    }

    size_t indentOf(const String& line) {
      const size_t at = line.find_first_not_of(' ');

      return at == String::npos ? line.size() : at;
    }

    // The pieces between commas that are not inside `(...)`, each trimmed. Angle brackets are operators here (`op: << >>`).
    Vector<String> splitTopLevel(const String& text, char separator) {
      Vector<String> parts;
      String current;
      int depth = 0;

      for (const char c : text) {
        depth += c == '(' ? 1 : c == ')' ? -1 : 0;

        if (c == separator && depth == 0) {
          parts.push_back(trimmed(current));
          current.clear();
        } else {
          current += c;
        }
      }

      parts.push_back(trimmed(current));

      return parts;
    }

    // ── templates ──

    bool isWordCharacter(char c) {
      return isalnum(static_cast<unsigned char>(c)) || c == '_';
    }

    // The end of the word starting at `at`.
    size_t wordEnd(const String& text, size_t at) {
      while (at < text.size() && isWordCharacter(text[at])) {
        at++;
      }

      return at;
    }

    // How long the hole `{name}` or `{name.role}` starting at `at` is, or 0 when no hole starts there.
    size_t holeLength(const String& text, size_t at) {
      if (text[at] != '{') {
        return 0;
      }

      size_t end = wordEnd(text, at + 1);
      const bool named = end > at + 1 && isalpha(static_cast<unsigned char>(text[at + 1]));

      if (named && end + 1 < text.size() && text[end] == '.' && isWordCharacter(text[end + 1])) {
        end = wordEnd(text, end + 1);
      }

      end += named && end < text.size() && text[end] == '?' ? 1 : 0;

      // `{tests| && }`: a separator runs to the brace that closes the hole.
      if (named && end < text.size() && text[end] == '|') {
        end = text.find('}', end);
      }

      return named && end < text.size() && text[end] == '}' ? end - at + 1 : 0;
    }

    // `{fields.param}` as the hole `fields` in the role `param`; `{nested?}` a hole whose line needs the part to have items.
    Piece wordHole(String inside) {
      const bool optional = !inside.empty() && inside.back() == '?';
      inside = optional ? inside.substr(0, inside.size() - 1) : inside;
      const size_t dot = inside.find('.');
      Piece piece = dot == String::npos ? Piece{"", inside, "", false} : Piece{"", inside.substr(0, dot), inside.substr(dot + 1), false};
      piece.optional = optional;

      return piece;
    }

    // `{tests| && }`: the hole, and what joins its part's items.
    Piece holePiece(String inside) {
      const size_t bar = inside.find('|');
      const String separator = bar == String::npos ? "" : inside.substr(bar + 1);
      inside = inside.substr(0, bar);

      Piece piece = wordHole(inside);
      piece.separator = separator;
      piece.joined = bar != String::npos;

      return piece;
    }

    void addLiteral(Vector<Piece>& pieces, String& literal) {
      if (!literal.empty()) {
        pieces.push_back(Piece{literal, "", "", false});
      }

      literal.clear();
    }

    // ── a rule's head and value ──

    RulePart partOf(const String& text) {
      const size_t colon = text.find(':');

      if (colon == String::npos) {
        return RulePart{text, ""};
      }

      const String pattern = trimmed(text.substr(colon + 1));
      IStringStream words(pattern);
      RulePart part{trimmed(text.substr(0, colon)), pattern};

      for (String word; words >> word;) {
        part.words.push_back(word);
      }

      return part;
    }

    // `field.param` as the construct `field` in the role `param`.
    void readConstruct(Rule& rule, const String& text) {
      const size_t dot = text.find('.');
      rule.construct = text.substr(0, dot);
      rule.role = dot == String::npos ? "" : text.substr(dot + 1);
    }

    // `binary(left: i32, op, right)` or a bare name, either maybe in a role (`field.param(name)`); false when it is neither.
    bool readHead(Rule& rule, const String& head) {
      const size_t open = head.find('(');
      readConstruct(rule, trimmed(head.substr(0, open)));

      if (open == String::npos) {
        return !rule.construct.empty() && rule.construct.find(' ') == String::npos;
      }

      if (head.back() != ')') {
        return false;
      }

      const String inside = trimmed(head.substr(open + 1, head.size() - open - 2));

      for (const String& part : inside.empty() ? Vector<String>{} : splitTopLevel(inside, ',')) {
        rule.parts.push_back(partOf(part));
      }

      return !rule.construct.empty();
    }

    // `unsupported "why"`: the reason inside the quotes (the text after the word, without them).
    String reasonOf(const String& value, const String& word) {
      const size_t open = value.find('"');
      const size_t close = value.rfind('"');

      return open == String::npos || close == open ? trimmed(value.substr(word.size())) : value.substr(open + 1, close - open - 1);
    }

    void readValue(Rule& rule, const String& value) {
      if (value == "todo") {
        rule.value = RuleValue::Todo;
      } else if (value == "lower" || value == "style") {
        rule.value = value == "lower" ? RuleValue::Lower : RuleValue::Style;
      } else if (startsWith(value, "unimplemented")) {
        rule.value = RuleValue::Unimplemented;
        rule.reason = reasonOf(value, "unimplemented");
      } else if (startsWith(value, "unsupported")) {
        rule.value = RuleValue::Unsupported;
        rule.reason = reasonOf(value, "unsupported");
      } else {
        rule.pieces = ReadTemplate(value);
      }
    }

    // ── the lines under a rule ──

    // The template lines under the rule, less the indentation they share, after the text on the rule's own line.
    String joinedTemplate(const Vector<String>& lines, const String& first) {
      size_t common = String::npos;

      for (const String& line : lines) {
        common = trimmed(line).empty() ? common : min(common, indentOf(line));
      }

      String text = first;

      for (const String& line : lines) {
        text += (text.empty() ? "" : "\n") + (line.size() > common ? line.substr(common) : String());
      }

      return text;
    }

    Rule& openRule(Reader& reader) {
      return reader.rules.rules.back();
    }

    // A template written back as text: its pieces, the holes in braces.
    String templateText(const Vector<Piece>& pieces) {
      String text;

      for (const Piece& piece : pieces) {
        text += piece.hole.empty() ? piece.text : "{" + piece.hole + (piece.role.empty() ? "" : "." + piece.role) + (piece.optional ? "?" : "") + "}";
      }

      return text;
    }

    // The open rule's template, with the lines read under it; blank lines at its end are not part of it.
    void closeRule(Reader& reader) {
      while (!reader.templateLines.empty() && trimmed(reader.templateLines.back()).empty()) {
        reader.templateLines.pop_back();
      }

      if (reader.isOpen && !reader.templateLines.empty()) {
        openRule(reader).pieces = ReadTemplate(joinedTemplate(reader.templateLines, templateText(openRule(reader).pieces)));
      }

      reader.isOpen = false;
      reader.templateLines.clear();
    }

    // `before:  nonlocal {outer}` (or `hoist:`): one line of the template, its indentation the spaces after the first one.
    void addLine(Vector<Piece>& pieces, const String& text) {
      if (!pieces.empty()) {
        pieces.push_back(Piece{"\n", "", "", false});
      }

      const Vector<Piece> line = ReadTemplate(text.empty() || text[0] != ' ' ? text : text.substr(1));
      pieces.insert(pieces.end(), line.begin(), line.end());
    }

    // `escape: ${ as \${`: what the rule writes in place of some text of its text parts.
    // The text an escape replaces, where `\n` stands for a line break and `\t` for a tab (`escape: \n as \\n`).
    String escaped(const String& text) {
      if (text == "\\n" || text == "\\t") {
        return text == "\\n" ? "\n" : "\t";
      }

      return text;
    }

    void addEscape(Reader& reader, const String& text) {
      const size_t as = text.find(" as ");

      if (as == String::npos) {
        return problem(reader, "`escape: " + text + "` is not `escape: text as replacement`");
      }

      openRule(reader).escapes.push_back({escaped(trimmed(text.substr(0, as))), trimmed(text.substr(as + 4))});
    }

    // `uses: a, b`: the support files and imports a rule (or a support file) needs.
    void addUses(Vector<String>& uses, const String& text) {
      const Vector<String> names = splitTopLevel(text, ',');
      uses.insert(uses.end(), names.begin(), names.end());
    }

    // A line under the open rule: `before:`, `uses:`, `escape:`, or a line of its template.
    void underRule(Reader& reader, const String& line) {
      const String text = trimmed(line);

      if (startsWith(text, "escape:")) {
        return addEscape(reader, trimmed(text.substr(7)));
      }

      if (startsWith(text, "before:")) {
        return addLine(openRule(reader).before, text.substr(7));
      }

      if (startsWith(text, "hoist:")) {
        return addLine(openRule(reader).hoist, text.substr(6));
      }

      if (startsWith(text, "uses:")) {
        return addUses(openRule(reader).uses, text.substr(5));
      }

      reader.templateLines.push_back(line);
    }

    // ── sections ──

    bool isRuleSection(const String& section) {
      return !IsSettingSection(section);
    }

    void ruleLine(Reader& reader, const String& head, const String& value) {
      Rule rule;
      rule.line = reader.line;
      rule.file = reader.rules.file;
      rule.isType = reader.section == "types" || reader.section == "type arguments";
      rule.isTypeArgument = reader.section == "type arguments";

      if (!readHead(rule, head)) {
        return problem(reader, "`" + head + "` is not a construct with its parts, like `if(cond, then)`");
      }

      readValue(rule, value);
      reader.rules.rules.push_back(rule);
      reader.isOpen = true;
    }

    // `levels: ||, &&, == !=, + -, * /`: the operators from loosest to tightest, those that bind alike together.
    void levelsLine(Reader& reader, const String& text) {
      for (const String& level : splitTopLevel(text, ',')) {
        Vector<String> tokens;
        IStringStream words(level);

        for (String word; words >> word;) {
          tokens.push_back(word);
        }

        reader.rules.levels.push_back(tokens);
      }
    }

    // `apart: == <`: the operators that do not chain.
    void apartLine(Reader& reader, const String& text) {
      IStringStream words(text);

      for (String word; words >> word;) {
        reader.rules.apart.insert(word);
      }
    }

    void reservedLine(Reader& reader, const String& text) {
      IStringStream words(text);

      for (String word; words >> word;) {
        if (word.back() == ',') {
          word.pop_back();
        }

        reader.rules.reserved.insert(word);
      }
    }

    void importLine(Reader& reader, const String& name, const String& line) {
      if (reader.rules.imports.count(name) == 0) {
        reader.rules.importOrder.push_back(name);
      }

      reader.rules.imports[name] = line;
    }

    // `line := #line {line} "{file}"` or `map := //# sourceMappingURL={map}`: a `\n` in it starts another line.
    void spansLine(Reader& reader, const String& name, const String& text) {
      if (name != "line" && name != "map") {
        return problem(reader, "a `## spans` section holds `line :=` and `map :=`, and `" + name + "` is neither");
      }

      String lines = text;

      for (size_t at = lines.find("\\n"); at != String::npos; at = lines.find("\\n", at + 1)) {
        lines.replace(at, 2, "\n");
      }

      reader.rules.spans[name] = lines;
    }

    // `left := right` in a section that is not rules: an operator's spelling, an import's line, a support file or a span line.
    void pairLine(Reader& reader, const String& left, const String& right) {
      reader.entry = reader.section == "support" || reader.section == "imports" ? left : "";

      if (reader.section == "operators") {
        reader.rules.operators[left] = right;
      } else if (reader.section == "imports") {
        importLine(reader, left, right);
      } else if (reader.section == "support") {
        reader.rules.support[left] = right;
      } else if (reader.section == "spans") {
        spansLine(reader, left, right);
      } else {
        problem(reader, "a `## " + reader.section + "` section holds no `:=` lines");
      }
    }

    // `  uses: stdlib, string` under a support file or an import: what it needs, written with it.
    void entryLine(Reader& reader, const String& text) {
      if (!startsWith(text, "uses:")) {
        return problem(reader, "only a `uses:` line goes under a support file or an import");
      }

      addUses(reader.rules.needs[reader.entry], text.substr(5));
    }

    // A line of a section that is not rules.
    void settingLine(Reader& reader, const String& text) {
      const size_t mark = text.find(":=");

      if (reader.section == "operators" && startsWith(text, "levels:")) {
        return levelsLine(reader, text.substr(7));
      }

      if (reader.section == "operators" && startsWith(text, "apart:")) {
        return apartLine(reader, text.substr(6));
      }

      if (reader.section == "reserved") {
        return reservedLine(reader, text);
      }

      if (mark == String::npos) {
        return problem(reader, "`" + text + "` is not a `name := value` line");
      }

      pairLine(reader, trimmed(text.substr(0, mark)), trimmed(text.substr(mark + 2)));
    }

    // ── the style line ──

    // `blocks colon`, `braces next-line`, `generics copies`, `qualify full`: the settings that turn something on. False for another key.
    bool switchSetting(Style& style, const String& key, const String& value) {
      if (key == "blocks" || key == "braces") {
        style.colon = style.colon || value == "colon";
        style.nextLine = style.nextLine || value == "next-line";

        return true;
      }

      // Each other switch: the setting it turns on, and the word that does.
      const Map<String, Pair<bool*, String>> switches = {{"generics", {&style.copies, "copies"}}, {"qualify", {&style.fullNames, "full"}},
        {"locals", {&style.uniqueLocals, "unique"}}, {"modules", {&style.flatNames, "flat"}}, {"imported", {&style.qualifyImported, "qualified"}}};
      const auto found = switches.find(key);

      if (found != switches.end()) {
        *found->second.first = value == found->second.second;
      }

      return found != switches.end();
    }

    // `names snake`, `paths ::`, `empty pass`, `extension js`: the settings that keep a word. False for another key.
    bool wordSetting(Style& style, const String& key, const String& value) {
      Map<String, String*> words = {{"names", &style.names}, {"paths", &style.paths}, {"empty", &style.empty}, {"extension", &style.extension},
                                     {"library", &style.library}};
      const auto word = words.find(key);

      if (word == words.end()) {
        return false;
      }

      *word->second = value;

      return true;
    }

    void styleSetting(Reader& reader, const String& key, const String& value) {
      Style& style = reader.rules.style;

      if (key == "indent") {
        style.indent = static_cast<int>(strtoll(value.c_str(), nullptr, 10));
      } else if (!switchSetting(style, key, value) && !wordSetting(style, key, value)) {
        problem(reader, "the style has no setting `" + key + "`");
      }
    }

    void styleLine(Reader& reader, const String& text) {
      for (const String& setting : splitTopLevel(text, ',')) {
        const size_t space = setting.find(' ');

        if (!setting.empty()) {
          styleSetting(reader, setting.substr(0, space), space == String::npos ? "" : trimmed(setting.substr(space + 1)));
        }
      }
    }

    // ── one line of the page ──

    // A section's name is checked against the page's source when the page is (Pages.cpp): each source has sections of its own.
    void headingLine(Reader& reader, const String& text) {
      if (startsWith(text, "## ")) {
        reader.section = trimmed(text.substr(3));
        reader.rules.sections.push_back(PageSection{reader.section, reader.line});
      } else {
        reader.rules.name = trimmed(text.substr(1));
      }
    }

    void sectionLine(Reader& reader, const String& text) {
      const size_t mark = text.find(":=");

      if (!isRuleSection(reader.section)) {
        return settingLine(reader, text);
      }

      if (mark == String::npos) {
        return problem(reader, "`" + text + "` is not a rule `construct(parts) := template`");
      }

      ruleLine(reader, trimmed(text.substr(0, mark)), trimmed(text.substr(mark + 2)));
    }

    bool isHeadLine(const String& text) {
      return startsWith(text, "base:") || startsWith(text, "source:") || startsWith(text, "target:");
    }

    // `base: Js`, `source: polyir`, `target: ts`: the lines at the head of a page.
    void headLine(Reader& reader, const String& text) {
      const size_t colon = text.find(':');
      const String word = text.substr(0, colon);
      const String value = trimmed(text.substr(colon + 1));

      (word == "base" ? reader.rules.base : word == "source" ? reader.rules.source : reader.rules.target) = value;
    }

    // An unindented line that is not blank or a comment.
    void topLine(Reader& reader, const String& text) {
      if (text[0] == '#') {
        return headingLine(reader, text);
      }

      if (startsWith(text, "style:")) {
        return styleLine(reader, text.substr(6));
      }

      if (reader.section.empty() && isHeadLine(text)) {
        return headLine(reader, text);
      }

      if (reader.section.empty()) {
        return problem(reader, "a rule before any `## section`");
      }

      sectionLine(reader, text);
    }

    void pageLine(Reader& reader, const String& line) {
      const String text = trimmed(line);

      if (reader.isOpen && (text.empty() || indentOf(line) > 0)) {
        return text.empty() ? reader.templateLines.push_back("") : underRule(reader, line);
      }

      if (!reader.entry.empty() && !text.empty() && indentOf(line) > 0) {
        return entryLine(reader, text);
      }

      closeRule(reader);
      reader.entry = text.empty() || startsWith(text, "//") ? reader.entry : "";

      if (!text.empty() && !startsWith(text, "//")) {
        topLine(reader, text);
      }
    }

  }  // namespace

  TranslateRules ReadRules(const String& text, const String& file) {
    Reader reader;
    reader.rules.file = file;
    IStringStream lines(text);

    for (String line; getline(lines, line);) {
      reader.line++;
      pageLine(reader, line);
    }

    closeRule(reader);

    return reader.rules;
  }

  // `{name}` is a hole when `name` is a word, `{name.role}` one in a role. Any other brace is text: `${{value}}` is `$`, `{`, the
  // hole, `}`.
  Vector<Piece> ReadTemplate(const String& text) {
    Vector<Piece> pieces;
    String literal;

    for (size_t at = 0; at < text.size(); at++) {
      const size_t length = holeLength(text, at);

      if (length > 0) {
        addLiteral(pieces, literal);
        pieces.push_back(holePiece(text.substr(at + 1, length - 2)));
        at += length - 1;
      } else {
        literal += text[at];
      }
    }

    addLiteral(pieces, literal);

    return pieces;
  }

  String RuleHead(const Rule& rule) {
    String parts;

    for (const RulePart& part : rule.parts) {
      parts += (parts.empty() ? "" : ", ") + part.name + (part.pattern.empty() ? "" : ": " + part.pattern);
    }

    return rule.parts.empty() ? RuleKey(rule) : RuleKey(rule) + "(" + parts + ")";
  }

  bool IsSettingSection(const String& section) {
    return any_of(begin(kSettingSections), end(kSettingSections), [&](const char* name) { return section == name; });
  }

  String RuleKey(const Rule& rule) {
    return rule.role.empty() ? rule.construct : rule.construct + "." + rule.role;
  }

}  // namespace polyir::translate
