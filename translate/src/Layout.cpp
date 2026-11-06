// A rule's template laid out as lines (Writer.hpp). A hole alone on its line takes its items one per line; any other hole is
// filled in its line. A block hole in a line opens a block as the style says (`{` and `}`, or `:` and indentation). An operand is
// put in parentheses when the operator next to its hole binds more tightly than the operand's own text does.
#include "Writer.hpp"

namespace polyir::translate {

  namespace {

    const char kHole = '\x01';

    // The parts of the program the engine fills itself with declarations: support files, imports, what rules hoisted.
    const char* const kEngineDeclarations[] = {"support", "hoisted", "imports"};

    // A template as one string, each hole one `kHole` character, as it is worked out: the holes' names and where they are, and
    // for each character whether it is inside quotes.
    struct Flat {
      String text;
      Vector<String> holes;
      Vector<String> roles;
      Vector<String> separators;
      Vector<bool> joined;
      Vector<size_t> at;
      Vector<bool> quoted;
    };

    // A line being written: the lines done, the one being built, the template line's own indentation, and what the text
    // after an empty list or a `:` block leaves out.
    struct LineOut {
      Lines lines;
      String current;
      String indent;
      bool afterColonBlock = false;
      bool dropComma = false;
      char dropClose = 0;  // an empty list took the `<` (or a type list the `[`) before it away, so this closer after it goes too
    };

    String rightTrimmed(const String& text) {
      const size_t end = text.find_last_not_of(' ');

      return end == String::npos ? "" : text.substr(0, end + 1);
    }

    bool isBlank(const String& text) {
      return text.find_first_not_of(' ') == String::npos;
    }

    bool isWordCharacter(char c) {
      return isalnum(static_cast<unsigned char>(c)) || c == '_';
    }

    // ── the template flattened ──

    void markQuotes(Flat& flat) {
      char quote = 0;

      for (const char c : flat.text) {
        const bool opens = quote == 0 && (c == '"' || c == '\'' || c == '`');
        flat.quoted.push_back(quote != 0 && c != quote);
        quote = opens ? c : (c == quote ? 0 : quote);
      }
    }

    Flat flatten(const Vector<Piece>& pieces) {
      Flat flat;

      for (const Piece& piece : pieces) {
        if (piece.hole.empty()) {
          flat.text += piece.text;
        } else {
          flat.holes.push_back(piece.hole);
          flat.roles.push_back(piece.role);
          flat.separators.push_back(piece.separator);
          flat.joined.push_back(piece.joined);

          flat.at.push_back(flat.text.size());
          flat.text += kHole;
        }
      }

      markQuotes(flat);

      return flat;
    }

    String holeAt(const Flat& flat, size_t position) {
      for (size_t i = 0; i < flat.at.size(); i++) {
        if (flat.at[i] == position) {
          return flat.holes[i];
        }
      }

      return "";
    }

    // ── operators and how tightly they bind, worked out once for a template ──

    bool isWord(const String& token) {
      return !token.empty() && isWordCharacter(token[0]);
    }

    // Whether `token` is at `at`, and a word token stands on its own there.
    bool tokenAt(const String& text, size_t at, const String& token) {
      if (text.compare(at, token.size(), token) != 0) {
        return false;
      }

      const bool before = at == 0 || !isWordCharacter(text[at - 1]);
      const bool after = at + token.size() >= text.size() || !isWordCharacter(text[at + token.size()]);

      return !isWord(token) || (before && after);
    }

    // The longest operator of the levels starting at `at`, or "".
    String tokenStartingAt(const PageIndex& index, const String& text, size_t at) {
      String found;

      for (const auto& [token, level] : index.levels) {
        if (token.size() > found.size() && tokenAt(text, at, token)) {
          found = token;
        }
      }

      return found;
    }

    String tokenEndingAt(const PageIndex& index, const String& text, size_t end) {
      String found;

      for (const auto& [token, level] : index.levels) {
        if (token.size() > found.size() && token.size() <= end && tokenAt(text, end - token.size(), token)) {
          found = token;
        }
      }

      return found;
    }

    // An operator's level; one the levels do not list binds loosest, so its operands are bracketed.
    int levelOf(const PageIndex& index, const String& token) {
      const auto found = index.levels.find(token);

      return found == index.levels.end() ? 0 : found->second;
    }

    // The operator just before `position` (spaces skipped) in the same bracket group, as a level. It is -1 when there is none,
    // and `kNodeOperator` when it is the node's own `{op}`.
    int levelBefore(const PageIndex& index, const Flat& flat, size_t position) {
      size_t i = position;

      while (i > 0 && flat.text[i - 1] == ' ') {
        i--;
      }

      if (i == 0 || String("([{,").find(flat.text[i - 1]) != String::npos) {
        return -1;
      }

      if (flat.text[i - 1] == kHole) {
        return holeAt(flat, i - 1) == "op" ? kNodeOperator : -1;
      }

      const String token = tokenEndingAt(index, flat.text, i);

      return token.empty() ? -1 : levelOf(index, token);
    }

    // An operator that does not chain (`apart:`) takes an operand of its own level on its left in brackets too.
    int levelAfter(const PageIndex& index, const Flat& flat, size_t position) {
      size_t i = position + 1;

      while (i < flat.text.size() && flat.text[i] == ' ') {
        i++;
      }

      if (i >= flat.text.size() || String(")]},;").find(flat.text[i]) != String::npos) {
        return -1;
      }

      if (flat.text[i] == kHole) {
        return holeAt(flat, i) == "op" ? kNodeOperator : -1;
      }

      const String token = tokenStartingAt(index, flat.text, i);

      return token.empty() ? -1 : levelOf(index, token) + (index.page.rules.apart.count(token) > 0 ? 1 : 0);
    }

    // The character before `position`, spaces skipped.
    char filledBefore(const Flat& flat, size_t position) {
      const size_t at = position == 0 ? String::npos : flat.text.find_last_not_of(' ', position - 1);

      return at == String::npos ? ' ' : flat.text[at];
    }

    // Whether hole `index` sits inside `<...>`: a type there is a type argument (`List<{elem}>`).
    bool isInAngles(const Flat& flat, size_t index) {
      int open = 0;

      for (size_t i = 0; i < flat.at[index]; i++) {
        const char c = flat.text[i];
        const bool paired = i + 1 < flat.text.size() && String("<=>-").find(flat.text[i + 1]) != String::npos;
        open += c == '<' && !paired ? 1 : (c == '>' && i > 0 && String("-=>").find(flat.text[i - 1]) == String::npos ? -1 : 0);
      }

      return open > 0;
    }

    // What the template alone says of how tightly hole `i`'s operand must bind: nothing in quotes, everything before a `.`.
    HoleForm holeForm(const PageIndex& index, const Flat& flat, size_t i) {
      const size_t position = flat.at[i];
      const char next = position + 1 < flat.text.size() ? flat.text[position + 1] : ' ';
      const char previous = position > 0 ? flat.text[position - 1] : ' ';
      HoleForm hole{flat.holes[i], flat.roles[i], isInAngles(flat, i), flat.separators[i], flat.joined[i]};

      hole.fixed = flat.quoted[position] ? 0 : String(".[(").find(next) != String::npos ? index.topLevel + 1 : -1;
      // A prefix operator's operand, or a cast's (`(int) {value}`), binds above every infix operator.
      hole.afterPrefix = String("-!~").find(previous) != String::npos || filledBefore(flat, position) == ')';
      hole.before = levelBefore(index, flat, position);
      hole.after = levelAfter(index, flat, position);

      return hole;
    }

    // The loosest operator outside the template's brackets and quotes, as a level (none: it binds as one piece). And whether
    // the node's own `{op}` is outside them too.
    void addLoosest(const PageIndex& index, const Flat& flat, TemplateForm& form) {
      int depth = 0;
      form.loosest = index.topLevel + 1;

      for (size_t i = 0; i < flat.text.size(); i++) {
        const char c = flat.text[i];
        depth += flat.quoted[i] ? 0 : (String("([{").find(c) != String::npos ? 1 : String(")]}").find(c) != String::npos ? -1 : 0);
        const String token = depth != 0 || flat.quoted[i] || c == kHole ? "" : tokenStartingAt(index, flat.text, i);

        form.opOutside = form.opOutside || (c == kHole && depth == 0 && holeAt(flat, i) == "op");
        form.loosest = token.empty() ? form.loosest : min(form.loosest, levelOf(index, token));
        i += token.empty() ? 0 : token.size() - 1;
      }
    }

    // `(byte) {value}`, `(int) ({left} + {right})`: a bracketed type and then its operand, a cast.
    bool isCastShaped(const Flat& flat) {
      int depth = 0;

      for (size_t i = 0; !flat.text.empty() && flat.text[0] == '(' && i < flat.text.size(); i++) {
        depth += flat.text[i] == '(' ? 1 : flat.text[i] == ')' ? -1 : 0;

        if (depth == 0) {
          return i + 2 < flat.text.size() && flat.text[i + 1] == ' ';
        }
      }

      return false;
    }

    // ── how tightly a node's text binds ──

    // The node's operator as the page spells it.
    String opText(const Writer& writer, const Node& node) {
      const NodePart* op = PartNamed(node, "op");
      const String text = op != nullptr && op->kind == PartKind::Text ? op->text : "";
      const auto spelled = writer.rules->operators.find(text);

      return spelled == writer.rules->operators.end() ? text : spelled->second;
    }

    // A level next to a hole, with the node's own operator's level for `kNodeOperator` (one more after it, when it does not chain).
    int levelNear(const Writer& writer, const Node& node, int level, bool after) {
      if (level != kNodeOperator) {
        return level;
      }

      const String op = opText(writer, node);
      const int apart = after && writer.rules->apart.count(op) > 0 ? 1 : 0;

      return after && op.empty() ? -1 : levelOf(*writer.index, op) + apart;
    }

    // How tightly the operand in a hole must bind: above the operators beside it.
    int requirementOf(const Writer& writer, const HoleForm& hole, const Node& node) {
      if (hole.fixed >= 0) {
        return hole.fixed;
      }

      if ((node.construct == "unary" && hole.name == "operand") || hole.afterPrefix) {
        return writer.index->topLevel;
      }

      const int before = levelNear(writer, node, hole.before, false);
      const int after = levelNear(writer, node, hole.after, true);

      return max(before < 0 ? 0 : before + 1, after < 0 ? 0 : after);
    }

    // A template that is only one value hole passes the value through, bracketed or not as its place wants.
    bool isPassThrough(const TemplateForm& form, const Node& node) {
      const NodePart* part = form.onlyOneHole ? PartNamed(node, form.holes[0].name) : nullptr;

      return part != nullptr && part->kind == PartKind::Value;
    }

    int loosestOf(const Writer& writer, const TemplateForm& form, const Node& node) {
      const bool byOp = form.opOutside && node.construct != "unary";

      return byOp ? min(form.loosest, levelOf(*writer.index, opText(writer, node))) : form.loosest;
    }

    // ── filling a line ──

    // The text added to the line from `from` on: what an empty list or a `:` block before it leaves out is skipped.
    void addText(LineOut& out, const String& text) {
      size_t from = out.dropComma && text.rfind(", ", 0) == 0 ? 2 : 0;

      // The closer goes, and the space after it when one is left before it: `static <{typeParams}> long` writes `static long`.
      if (out.dropClose != 0 && from < text.size() && text[from] == out.dropClose) {
        const bool spaced = from + 1 < text.size() && text[from + 1] == ' ' && !out.current.empty() && out.current.back() == ' ';
        from += spaced ? 2 : 1;
      }

      if (out.afterColonBlock) {
        from = min(text.size(), text.find_first_not_of(' ', from));
      }

      out.dropComma = false;
      out.dropClose = 0;
      out.afterColonBlock = out.afterColonBlock && from >= text.size();
      out.current.append(text, min(from, text.size()), String::npos);
    }

    void flush(LineOut& out) {
      out.lines.push_back(move(out.current));
      out.current.clear();
    }

    // Lines written inside a line: the first goes on the line, the rest under it at the line's indentation.
    void merge(LineOut& out, const Lines& lines) {
      for (size_t i = 0; i < lines.size(); i++) {
        if (i > 0) {
          flush(out);
          out.current = lines[i].empty() ? "" : out.indent + lines[i];
        } else {
          addText(out, lines[i]);
        }
      }
    }

    // Whether a part holds declarations, so a blank line goes between two when either takes several lines. The source says
    // which of its parts do (`declarations:`). A block node's statements do when the block it lays out holds them.
    bool holdsDeclarations(const Writer& writer, const Node& node, const NodePart& part) {
      const bool engine = any_of(begin(kEngineDeclarations), end(kEngineDeclarations), [&](const char* name) { return part.name == name; });
      const bool block = node.construct == "block" && part.name == "statements" && PartNamed(node, "declarations") != nullptr;

      return engine || block || writer.schema->declarations.count(part.name) > 0;
    }

    // The made lines moved to the end of `lines`, each after `indent` (an empty line stays empty).
    void addIndented(Lines& lines, Lines made, const String& indent) {
      for (String& line : made) {
        lines.push_back(line.empty() || indent.empty() ? move(line) : indent + line);
      }
    }

    // One item of a part in `role`: a list's item as an expression, a block's as a statement.
    Lines itemLines(Writer& writer, const NodePart& part, size_t i, const String& role) {
      const bool isList = part.kind == PartKind::List || part.kind == PartKind::Fact;

      return isList ? WriteNodeAs(writer, part.nodes[i], 0, role) : WriteStatementAs(writer, part.nodes[i], role);
    }

    // A part's items one per line, each line after `indent`.
    Lines spliced(Writer& writer, const NodePart& part, const String& indent, const String& role, bool apart) {
      Lines lines;
      bool lastLong = false;

      for (size_t i = 0; i < part.nodes.size(); i++) {
        Lines made = itemLines(writer, part, i, role);

        if (apart && i > 0 && (lastLong || made.size() > 1)) {
          lines.push_back("");
        }

        lastLong = made.size() > 1;
        addIndented(lines, move(made), indent);
      }

      return lines;
    }

    // An empty list takes the `, ` beside it away, or the `<` and `>` around it (`class {name}<{typeParams}>`). An empty list of
    // types takes away the `[` and `]` after a name too (Python's `class {name}[{typeParams}]`), not those of `Callable[[], int]`.
    void emptyList(LineOut& out, bool types) {
      const size_t size = out.current.size();
      const char last = size == 0 ? 0 : out.current.back();
      const bool comma = size >= 2 && out.current.compare(size - 2, 2, ", ") == 0;
      const bool named = size >= 2 && isWordCharacter(out.current[size - 2]);
      const bool bracket = !comma && (last == '<' || (types && last == '[' && named));

      out.current = out.current.substr(0, out.current.size() - (comma ? 2 : bracket ? 1 : 0));
      out.dropComma = !comma && !bracket;
      out.dropClose = bracket ? (last == '<' ? '>' : ']') : 0;
    }

    // The list's items in the line, joined by `, ` (text pieces by nothing), or by the hole's own separator.
    void listHole(Writer& writer, LineOut& out, const NodePart& part, const String& role, const HoleForm* hole) {
      const String separator = hole != nullptr && hole->joined ? hole->separator : part.name == "pieces" ? "" : ", ";

      if (part.nodes.empty()) {
        return hole != nullptr && hole->joined ? void() : emptyList(out, false);
      }

      for (size_t i = 0; i < part.nodes.size(); i++) {
        addText(out, i == 0 ? "" : separator);
        merge(out, WriteNodeAs(writer, part.nodes[i], 0, role));
      }
    }

    // `:` and the indented statements, or what the style says an empty block holds.
    void colonBlock(Writer& writer, LineOut& out, const Lines& inner) {
      out.current = rightTrimmed(out.current) + ":";
      flush(out);
      const String step(static_cast<size_t>(writer.rules->style.indent), ' ');

      if (inner.empty() && !writer.rules->style.empty.empty()) {
        out.lines.push_back(out.indent + step + writer.rules->style.empty);
      }

      for (const String& line : inner) {
        out.lines.push_back(line.empty() ? "" : out.indent + line);
      }

      out.current = out.indent;
      out.afterColonBlock = true;
    }

    // `{`, the statements one step in, then `}` on a line of its own; `braces next-line` puts the `{` on a line of its own too.
    void braceBlock(Writer& writer, LineOut& out, Lines inner) {
      if (writer.rules->style.nextLine) {
        out.current = rightTrimmed(out.current);
        flush(out);
        out.current = out.indent;
      }

      out.current += "{";
      flush(out);
      addIndented(out.lines, move(inner), out.indent);
      out.current = out.indent + "}";
    }

    // A block in a line, laid out as the style says. The rest of the line follows the block's end.
    // A block in a line as the style lays blocks out: braces or a colon.
    void styledBlock(Writer& writer, LineOut& out, const Node& node, const NodePart& part, const String& role) {
      const Style& style = writer.rules->style;
      Lines inner = spliced(writer, part, String(static_cast<size_t>(style.indent), ' '), role, holdsDeclarations(writer, node, part));

      if (style.colon) {
        return colonBlock(writer, out, inner);
      }

      return inner.empty() ? addText(out, "{}") : braceBlock(writer, out, move(inner));
    }

    // `block(statements, of)` for the block `of` (`func.body`): its statements as the construct holds them, and the fact
    // `declarations` when they are declarations.
    Node blockNode(const String& of, const NodePart& part, bool declarations) {
      Node block = MakeNode("block");
      AddNodes(block, "statements", PartKind::Block, part.nodes);
      AddText(block, "of", PartKind::Text, of);

      if (declarations) {
        AddFact(block, "declarations");
      }

      return block;
    }

    // The lines a `block` rule of the page's own lays the block out as; none when the style does (no such rule applies). A
    // block that no such rule may be for is not made into a node at all. A block rule's own blocks are the style's.
    Optional<Lines> ownBlock(Writer& writer, const Node& node, const NodePart& part) {
      const String of = node.construct + "." + part.name;
      const bool may = node.construct != "block" && (writer.index->everyBlockOwn || writer.index->ownBlocks.count(of) > 0);

      if (!may) {
        return nullopt;
      }

      const Node block = blockNode(of, part, holdsDeclarations(writer, node, part));
      const Rule* rule = ChooseRule(writer, block, false);

      return rule == nullptr || rule->value == RuleValue::Style ? nullopt : Optional<Lines>(WriteNode(writer, block, 0));
    }

    // A block in a line: by the page's own `block` rule for it, else as the style says.
    void blockHole(Writer& writer, LineOut& out, const Node& node, const NodePart& part, const String& role) {
      if (Optional<Lines> own = ownBlock(writer, node, part)) {
        return merge(out, *own);
      }

      styledBlock(writer, out, node, part, role);
    }

    // A hole alone on its line takes its part's items one per line at the line's indentation. A block of them is laid out by
    // the page's own `block` rule for it when there is one.
    Lines splicedPart(Writer& writer, const Node& node, const NodePart& part, const String& indent, const String& role) {
      Optional<Lines> own = part.kind == PartKind::Block ? ownBlock(writer, node, part) : nullopt;

      if (!own) {
        return spliced(writer, part, indent, role, holdsDeclarations(writer, node, part));
      }

      Lines lines;
      addIndented(lines, move(*own), indent);

      return lines;
    }

    // A host binding's `$0`, `$1` as the call's arguments.
    String bindingText(Writer& writer, const Node& node, const String& binding) {
      const NodePart* args = PartNamed(node, "args");
      String text = binding;

      for (size_t i = 0; args != nullptr && i < args->nodes.size(); i++) {
        const String mark = "$" + to_string(i);

        for (size_t at = text.find(mark); at != String::npos; at = text.find(mark, at + 1)) {
          text.replace(at, mark.size(), JoinLines(WriteNode(writer, args->nodes[i], 0)));
        }
      }

      return text;
    }

    String typesText(Writer& writer, const NodePart& part, bool asArgument, const String& role) {
      String text;

      for (const Node& node : part.nodes) {
        text += (text.empty() ? "" : ", ") + JoinLines(WriteType(writer, node, asArgument, role));
      }

      return text;
    }

    // The text with the rule's escapes: each `from` replaced by its `to`, left to right.
    String escapedText(const Writer& writer, const String& text) {
      if (writer.escapes == nullptr || writer.escapes->empty()) {
        return text;
      }

      String out;

      for (size_t at = 0; at < text.size();) {
        const Pair<String, String>* found = nullptr;

        for (size_t i = 0; writer.escapes != nullptr && found == nullptr && i < writer.escapes->size(); i++) {
          const Pair<String, String>& escape = (*writer.escapes)[i];
          found = !escape.first.empty() && text.compare(at, escape.first.size(), escape.first) == 0 ? &escape : nullptr;
        }

        out += found != nullptr ? found->second : String(1, text[at]);
        at += found != nullptr ? found->first.size() : 1;
      }

      return out;
    }

    String textOf(Writer& writer, const Node& node, const NodePart& part) {
      if (part.name == "op") {
        return opText(writer, node);
      }

      return part.name == "binding" ? bindingText(writer, node, part.text) : escapedText(writer, part.text);
    }

    // One node's template being filled: its form, and the requirement every hole takes when the template passes its one value
    // through (else each hole's own).
    struct Filling {
      const TemplateForm* form = nullptr;
      bool passes = false;
      int required = 0;
    };

    // The parts that hold items: a block, a statement, a list, or a fact made of nodes.
    void fillItems(Writer& writer, LineOut& out, const Node& node, const NodePart& part, const String& role) {
      switch (part.kind) {
        case PartKind::Block:
          return blockHole(writer, out, node, part, role);
        case PartKind::Statement:
          return merge(out, WriteStatementAs(writer, part.nodes[0], role));
        default:
          return part.nodes.empty() ? addText(out, part.text) : listHole(writer, out, part, role, nullptr);
      }
    }

    void fillHole(Writer& writer, LineOut& out, const Node& node, const Filling& filling, size_t index) {
      const HoleForm& hole = filling.form->holes[index];
      const NodePart* part = PartNamed(node, hole.name);

      switch (part == nullptr ? PartKind::Fact : part->kind) {
        case PartKind::Name:
          return addText(out, RenamedName(writer, part->text));
        case PartKind::Text:
          return addText(out, textOf(writer, node, *part));
        case PartKind::Value:
          return merge(out, WriteNodeAs(writer, part->nodes[0], filling.passes ? filling.required : requirementOf(writer, hole, node), hole.role));
        case PartKind::Type:
          return part->nodes.empty() ? emptyList(out, true) : addText(out, typesText(writer, *part, hole.inAngles, hole.role));
        default:
          return part == nullptr ? void() : part->kind == PartKind::List || hole.joined ? listHole(writer, out, *part, hole.role, &hole) : fillItems(writer, out, node, *part, hole.role);
      }
    }

    // ── template lines ──

    // A text piece's lines added to `lines`: its first goes on the last line, each line break starts another.
    void addTextLines(Vector<Vector<Piece>>& lines, const String& text) {
      size_t from = 0;

      for (size_t at = text.find('\n'); at != String::npos; at = text.find('\n', from)) {
        lines.back().push_back(Piece{text.substr(from, at - from), "", "", false});
        lines.emplace_back();
        from = at + 1;
      }

      lines.back().push_back(Piece{text.substr(from), "", "", false});
    }

    // The template cut at its line breaks.
    Vector<Vector<Piece>> templateLines(const Vector<Piece>& pieces) {
      Vector<Vector<Piece>> lines(1);

      for (const Piece& piece : pieces) {
        if (piece.hole.empty()) {
          addTextLines(lines, piece.text);
        } else {
          lines.back().push_back(piece);
        }
      }

      return lines;
    }

    // The spaces a template line starts with, in the text before its first hole.
    String indentOf(const Vector<Piece>& line) {
      String first;

      for (size_t i = 0; i < line.size() && line[i].hole.empty(); i++) {
        first += line[i].text;
      }

      return first.substr(0, min(first.size(), first.find_first_not_of(' ')));
    }

    // The one hole of a line that holds nothing else, when its part takes items: the hole, else null.
    const Piece* splicedHole(const Vector<Piece>& line, const LineForm& form, const Node& node) {
      const Piece* lone = form.lone == String::npos ? nullptr : &line[form.lone];
      const NodePart* part = lone == nullptr ? nullptr : PartNamed(node, lone->hole);
      const bool takesItems = part != nullptr && (part->kind == PartKind::Block || part->kind == PartKind::List || part->kind == PartKind::Statement || !part->nodes.empty());

      return takesItems ? lone : nullptr;
    }

    // The place of the line's one hole when nothing but spaces is beside it, else npos.
    size_t loneHole(const Vector<Piece>& line) {
      size_t found = String::npos;

      for (size_t i = 0; i < line.size(); i++) {
        if ((!line[i].hole.empty() && (found != String::npos || line[i].joined)) || (line[i].hole.empty() && !isBlank(line[i].text))) {
          return String::npos;
        }

        found = line[i].hole.empty() ? found : i;
      }

      return found;
    }

    // A line of a template that has several: one its holes left blank is left out.
    Lines filledLine(Writer& writer, const Vector<Piece>& line, const LineForm& form, const Node& node, const Filling& filling, size_t& next, bool dropsBlank) {
      LineOut out;
      out.indent = form.indent;
      bool hasHole = false;

      for (const Piece& piece : line) {
        if (piece.hole.empty()) {
          addText(out, piece.text);
        } else {
          hasHole = true;
          fillHole(writer, out, node, filling, next++);
        }
      }

      const bool closedByColon = out.afterColonBlock && isBlank(out.current);

      if (!closedByColon && (!isBlank(out.current) || !hasHole || !dropsBlank)) {
        flush(out);
      }

      return out.lines;
    }

    // Whether the line has a hole asking for items (`{nested?}`) whose part has none: then the line is left out.
    bool lacksItems(const Vector<Piece>& line, const Node& node) {
      for (const Piece& piece : line) {
        const NodePart* part = piece.optional ? PartNamed(node, piece.hole) : nullptr;

        if (piece.optional && (part == nullptr || (part->nodes.empty() && part->text.empty()))) {
          return true;
        }
      }

      return false;
    }

    size_t holeCount(const Vector<Piece>& line) {
      return static_cast<size_t>(count_if(line.begin(), line.end(), [](const Piece& piece) { return !piece.hole.empty(); }));
    }

    // One line of a template: a part's items spliced, nothing when it lacks the items it asks for, or the line filled.
    Lines templateLine(Writer& writer, const Vector<Piece>& line, const LineForm& form, const Node& node, const Filling& filling, size_t& next, bool dropsBlank) {
      if (form.optional && lacksItems(line, node)) {
        next += form.holes;

        return {};
      }

      if (const Piece* hole = splicedHole(line, form, node)) {
        next++;

        return splicedPart(writer, node, *PartNamed(node, hole->hole), form.indent, hole->role);
      }

      return filledLine(writer, line, form, node, filling, next, dropsBlank);
    }

    LineForm lineForm(const Vector<Piece>& line) {
      const bool optional = any_of(line.begin(), line.end(), [](const Piece& piece) { return piece.optional; });

      return LineForm{indentOf(line), holeCount(line), loneHole(line), optional};
    }

    // The form the index worked out for a rule's template; one from elsewhere is worked out here, into `spare`.
    const TemplateForm& formFor(const Writer& writer, const Vector<Piece>& pieces, TemplateForm& spare) {
      const auto found = writer.index->forms.find(&pieces);

      if (found != writer.index->forms.end()) {
        return found->second;
      }

      spare = FormOfTemplate(*writer.index, pieces);

      return spare;
    }

  }  // namespace

  TemplateForm FormOfTemplate(const PageIndex& index, const Vector<Piece>& pieces) {
    const Flat flat = flatten(pieces);
    const size_t first = flat.text.find_first_not_of(' ');
    TemplateForm form{flat.text, flat.at, {}, pieces.empty() ? Vector<Vector<Piece>>{} : templateLines(pieces), {}};

    for (size_t i = 0; i < flat.holes.size(); i++) {
      form.holes.push_back(holeForm(index, flat, i));
    }

    for (const Vector<Piece>& line : form.lines) {
      form.lineForms.push_back(lineForm(line));
    }

    form.onlyOneHole = flat.holes.size() == 1 && rightTrimmed(flat.text.substr(first == String::npos ? 0 : first)) == String(1, kHole);
    form.castShaped = isCastShaped(flat);
    addLoosest(index, flat, form);

    return form;
  }

  Lines ExpandTemplate(Writer& writer, const Vector<Piece>& pieces, const Node& node, int required) {
    TemplateForm spare;
    const TemplateForm& form = formFor(writer, pieces, spare);
    const Filling filling{&form, isPassThrough(form, node), required};
    Lines lines;
    size_t next = 0;

    for (size_t i = 0; i < form.lines.size(); i++) {
      Lines made = templateLine(writer, form.lines[i], form.lineForms[i], node, filling, next, form.lines.size() > 1);
      lines.insert(lines.end(), make_move_iterator(made.begin()), make_move_iterator(made.end()));
    }

    return lines;
  }

  // The engine's own parts of the program (its support files, imports, what rules hoisted) are declarations.
  Lines WritePart(Writer& writer, const NodePart& part) {
    return spliced(writer, part, "", "", true);
  }

  // A prefix operator's text (`-x`, a negative literal, a cast) binds as a prefix operator does. It binds more loosely when an
  // operator outside its brackets does (`-{operand} | 0`).
  int LevelOf(Writer& writer, const Rule& rule, const Node& node) {
    const NodePart* value = PartNamed(node, "value");
    const bool negative = value != nullptr && value->kind == PartKind::Text && !value->text.empty() && value->text[0] == '-';
    TemplateForm spare;
    const TemplateForm& form = formFor(writer, rule.pieces, spare);

    if (isPassThrough(form, node)) {
      return writer.index->topLevel + 1;
    }

    const bool prefix = node.construct == "unary" || ((node.construct == "int" || node.construct == "float") && negative) || form.castShaped;
    const int loosest = loosestOf(writer, form, node);

    return prefix ? min(writer.index->topLevel, loosest) : loosest;
  }

  Lines Parenthesized(Lines lines) {
    if (lines.empty()) {
      return lines;
    }

    lines.front() = "(" + lines.front();
    lines.back() += ")";

    return lines;
  }

  String JoinLines(const Lines& lines) {
    String text;

    for (size_t i = 0; i < lines.size(); i++) {
      text += (i == 0 ? "" : "\n") + lines[i];
    }

    return text;
  }

}  // namespace polyir::translate
