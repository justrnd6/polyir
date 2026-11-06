// Choosing a node's rule and writing it (Writer.hpp). A rule applies to a node that has every part it names, when it names every
// part of the node that is not a type or a fact; of those that apply, the one naming the most parts wins, then the one with the
// most part patterns (`left: i32`), then the later one. A node no rule writes is written by its lower form when it has one.
#include "Writer.hpp"

namespace polyir::translate {

  namespace {

    // ── part patterns ──

    // A node's own `type` part's node: an expression's type.
    const Node* ownType(const Node& node) {
      const NodePart* type = PartNamed(node, "type");

      return type != nullptr && type->nodes.size() == 1 ? &type->nodes[0] : nullptr;
    }

    // The type node a part's pattern is matched against: a type part's own node, or the type of its one value. A literal's
    // text (`int(value: i64)`) is matched against the type of the node the part is of.
    const Node* typeNodeOf(const Node& node, const NodePart& part) {
      if (part.nodes.size() == 1) {
        return part.nodes[0].isType ? &part.nodes[0] : ownType(part.nodes[0]);
      }

      return part.nodes.empty() && part.kind == PartKind::Text ? ownType(node) : nullptr;
    }

    // The type's own word, one of its source's families (`int`), or a struct's name.
    bool typeMatches(const Schema& schema, const String& word, const Node& type) {
      const Set<String>* family = FamilyNamed(schema, word);

      if (word == type.construct || (family != nullptr && family->count(type.construct) > 0)) {
        return true;
      }

      const NodePart* name = type.construct == "struct" || type.construct == "instance" ? PartNamed(type, "name") : nullptr;

      return name != nullptr && (word == name->text || word == OwnName(name->text));
    }

    bool namesAType(const Schema& schema, const String& word) {
      return FamilyNamed(schema, word) != nullptr || ConstructNamed(schema, word, true) != kNoId;
    }

    // A word may name a fact of the part's node (`type: isInterface`), or its construct when that names no type (`target: get`).
    bool nodeMatches(const Schema& schema, const String& word, const NodePart& part) {
      const NodePart* fact = part.nodes.size() == 1 ? PartNamed(part.nodes[0], word) : nullptr;
      const bool construct = part.nodes.size() == 1 && part.nodes[0].construct == word && !namesAType(schema, word);

      return (fact != nullptr && fact->kind == PartKind::Fact) || construct;
    }

    // A pattern word: `empty` for a part with no items, else the part's type or its text (a literal's too: `bool(value: true)`).
    bool wordMatches(const Schema& schema, const String& word, const Node& node, const NodePart& part) {
      if (word == "empty" || word == "one") {
        return word == "one" ? part.nodes.size() == 1 : part.nodes.empty() && part.text.empty();
      }

      if ((part.kind == PartKind::Text && part.text == word) || nodeMatches(schema, word, part)) {
        return true;
      }

      const Node* type = typeNodeOf(node, part);

      return type == nullptr ? part.text == word : typeMatches(schema, word, *type);
    }

    // A pattern of several words matches when one of them does (`op: + - *`).
    bool partMatches(const Schema& schema, const RulePart& rulePart, const Node& node, const NodePart& part) {
      const auto matches = [&](const String& word) { return wordMatches(schema, word, node, part); };

      return rulePart.words.empty() || any_of(rulePart.words.begin(), rulePart.words.end(), matches);
    }

    bool namesPart(const Rule& rule, const String& name) {
      return any_of(rule.parts.begin(), rule.parts.end(), [&](const RulePart& part) { return part.name == name; });
    }

    bool applies(const Schema& schema, const Rule& rule, const Node& node) {
      for (const RulePart& rulePart : rule.parts) {
        const NodePart* part = PartNamed(node, rulePart.name);

        if (part == nullptr || !partMatches(schema, rulePart, node, *part)) {
          return false;
        }
      }

      return all_of(node.parts.begin(), node.parts.end(), [&](const NodePart& part) { return MayLeaveOut(part.kind) || namesPart(rule, part.name); });
    }

    int patternCount(const Rule& rule) {
      return static_cast<int>(count_if(rule.parts.begin(), rule.parts.end(), [](const RulePart& part) { return !part.pattern.empty(); }));
    }

    // The most specific rule that applies; of two alike, the later on the page, so a page can replace a rule it took from another.
    const Rule* bestOf(const Schema& schema, const Vector<const Rule*>& rules, const Node& node) {
      const Rule* best = nullptr;

      for (const Rule* rule : rules) {
        const bool better = best == nullptr || rule->parts.size() > best->parts.size() ||
          (rule->parts.size() == best->parts.size() && patternCount(*rule) >= patternCount(*best));

        if (applies(schema, *rule, node) && better) {
          best = rule;
        }
      }

      return best;
    }

    const Rule* bestIn(const Schema& schema, const Map<String, Vector<const Rule*>>& rules, const String& key, const Node& node) {
      const auto found = rules.find(key);

      return found == rules.end() ? nullptr : bestOf(schema, found->second, node);
    }

    // ── writing ──

    // `if(cond, then, else)`: the node's parts as a rule would name them, for a problem.
    String headOf(const Node& node) {
      String parts;

      for (const NodePart& part : node.parts) {
        parts += MayLeaveOut(part.kind) ? "" : (parts.empty() ? "" : ", ") + part.name;
      }

      return node.parts.empty() || parts.empty() ? node.construct : node.construct + "(" + parts + ")";
    }

    String where(const Writer&, const Rule& rule) {
      return rule.file + ":" + to_string(rule.line) + ": ";
    }

    bool isUsed(const Writer& writer, const String& name) {
      return find(writer.supportUsed.begin(), writer.supportUsed.end(), name) != writer.supportUsed.end();
    }

    // A support file or an import a rule uses, after what it uses itself, so each is written after what it needs.
    void noteUse(Writer& writer, const Rule& rule, const String& name, int depth) {
      const TranslateRules& rules = *writer.rules;

      if (rules.support.count(name) == 0 && rules.imports.count(name) == 0) {
        return WriterProblem(writer, where(writer, rule) + "no `## support` file or `## imports` line is named `" + name + "`");
      }

      const auto needs = rules.needs.find(name);

      for (const String& need : depth > 16 || needs == rules.needs.end() ? Vector<String>{} : needs->second) {
        noteUse(writer, rule, need, depth + 1);
      }

      if (!isUsed(writer, name)) {
        writer.supportUsed.push_back(name);
      }
    }

    void noteUses(Writer& writer, const Rule& rule) {
      for (const String& name : rule.uses) {
        noteUse(writer, rule, name, 0);
      }
    }

    void noteBefore(Writer& writer, const Rule& rule, const Node& node) {
      if (rule.before.empty() || writer.before.empty()) {
        return;
      }

      const Lines lines = ExpandTemplate(writer, rule.before, node, 0);
      writer.before.back().insert(writer.before.back().end(), lines.begin(), lines.end());
    }

    Lines writeLowered(Writer& writer, const Node& node, int required) {
      if (node.lowered.size() == 1) {
        return WriteNode(writer, node.lowered[0], required);
      }

      Lines lines;

      for (const Node& lowered : node.lowered) {
        const Lines made = WriteStatement(writer, lowered);
        lines.insert(lines.end(), made.begin(), made.end());
      }

      return lines;
    }

    // A node whose rule is not written still has its parts written, so the problems name every construct the program uses.
    void writeParts(Writer& writer, const Node& node) {
      for (const NodePart& part : node.parts) {
        for (const Node& inner : part.nodes) {
          if (part.kind == PartKind::Block || part.kind == PartKind::Statement) {
            WriteStatement(writer, inner);
          } else {
            WriteNode(writer, inner, 0);
          }
        }
      }
    }

    Lines notWritten(Writer& writer, const Node& node, const String& problem) {
      WriterProblem(writer, problem);
      writeParts(writer, node);

      return {"?" + node.construct + "?"};
    }

    // A node no rule writes: its lower form; a `choose` asks for its statement's; anything else is a problem.
    Lines unwritten(Writer& writer, const Node& node, int required, const String& problem) {
      if (!node.lowered.empty()) {
        return writeLowered(writer, node, required);
      }

      if (node.construct != "choose") {
        return notWritten(writer, node, problem);
      }

      writer.lowerStatement = true;

      return {"?choose?"};
    }

    // The template laid out with the rule's escapes, which the rules of the nodes inside it do not share.
    Lines expanded(Writer& writer, const Rule& rule, const Node& node, int required) {
      const Vector<Pair<String, String>>* around = writer.escapes;
      writer.escapes = &rule.escapes;
      Lines lines = ExpandTemplate(writer, rule.pieces, node, required);
      writer.escapes = around;

      return lines;
    }

    // A `hoist:` template's text, kept once for the program's `hoisted` part.
    void noteHoist(Writer& writer, const Rule& rule, const Node& node) {
      const String text = rule.hoist.empty() ? "" : JoinLines(ExpandTemplate(writer, rule.hoist, node, 0));

      if (!text.empty() && find(writer.hoisted.begin(), writer.hoisted.end(), text) == writer.hoisted.end()) {
        writer.hoisted.push_back(text);
      }
    }

    Lines writeTemplate(Writer& writer, const Rule& rule, const Node& node, int required) {
      noteUses(writer, rule);
      noteBefore(writer, rule, node);
      noteHoist(writer, rule, node);
      Lines lines = expanded(writer, rule, node, required);

      if (LevelOf(writer, rule, node) < required) {
        return Parenthesized(move(lines));
      }

      return lines;
    }

    // Why a rule writes nothing: `if(cond, then)` is todo, is unimplemented (and why), or is unsupported (and why).
    String statusOf(const Rule& rule) {
      const String head = "`" + RuleHead(rule) + "` is ";

      if (rule.value == RuleValue::Todo) {
        return head + "todo";
      }

      return head + (rule.value == RuleValue::Unimplemented ? "unimplemented: " : "unsupported: ") + rule.reason;
    }

    Lines writeRule(Writer& writer, const Rule* rule, const Node& node, int required) {
      if (rule == nullptr) {
        return unwritten(writer, node, required, writer.rules->file + ": no rule for `" + headOf(node) + "`");
      }

      switch (rule->value) {
        case RuleValue::Template:
          return writeTemplate(writer, *rule, node, required);
        case RuleValue::Lower:
          return unwritten(writer, node, required, where(writer, *rule) + "`" + RuleHead(*rule) + "` has no lower form here");
        default:
          return notWritten(writer, node, where(writer, *rule) + statusOf(*rule));
      }
    }

    Lines writeWith(Writer& writer, const Node& node, int required, bool asArgument) {
      return writeRule(writer, ChooseRule(writer, node, asArgument), node, required);
    }

    // The lines a statement's rules put before it, then its own.
    Lines withBefore(Lines before, Lines lines) {
      if (before.empty()) {
        return lines;
      }

      before.insert(before.end(), make_move_iterator(lines.begin()), make_move_iterator(lines.end()));

      return before;
    }

    // ── a role's rule for any construct ──

    // A word of an `any` rule's `value` pattern, matched against the node as it would be in the part `value` of an `any` node.
    bool valueWordMatches(const Schema& schema, const String& word, const Node& value) {
      const NodePart* fact = PartNamed(value, word);
      const Node* type = value.isType ? &value : ownType(value);

      if (word == "empty" || word == "one") {
        return word == "one";
      }

      if ((fact != nullptr && fact->kind == PartKind::Fact) || (value.construct == word && !namesAType(schema, word))) {
        return true;
      }

      return type != nullptr && typeMatches(schema, word, *type);
    }

    bool anyPartMatches(const Schema& schema, const RulePart& part, const Node& node) {
      const NodePart* type = PartNamed(node, "type");
      const auto matches = [&](const String& word) { return valueWordMatches(schema, word, node); };

      if (part.name == "type") {
        return type != nullptr && partMatches(schema, part, node, *type);
      }

      return part.name == "value" && (part.words.empty() || any_of(part.words.begin(), part.words.end(), matches));
    }

    // An `any` rule applies to a node as it would to `any(value: node, type: its type)`, without that node being made.
    bool anyApplies(const Schema& schema, const Rule& rule, const Node& node) {
      const bool namesValue = any_of(rule.parts.begin(), rule.parts.end(), [](const RulePart& part) { return part.name == "value"; });

      return namesValue && all_of(rule.parts.begin(), rule.parts.end(), [&](const RulePart& part) { return anyPartMatches(schema, part, node); });
    }

    // The most specific `any` rule that applies, chosen as bestOf chooses.
    const Rule* bestAny(const Schema& schema, const Vector<const Rule*>& rules, const Node& node) {
      const Rule* best = nullptr;

      for (const Rule* rule : rules) {
        const bool better = best == nullptr || rule->parts.size() > best->parts.size() ||
          (rule->parts.size() == best->parts.size() && patternCount(*rule) >= patternCount(*best));

        if (anyApplies(schema, *rule, node) && better) {
          best = rule;
        }
      }

      return best;
    }

    // A rule with an empty template and nothing it uses, puts before or hoists: it writes nothing (`any.include(value) :=`).
    bool writesNothing(const Rule& rule) {
      const bool empty = rule.pieces.empty() && rule.before.empty() && rule.hoist.empty() && rule.uses.empty();

      return rule.value == RuleValue::Template && empty;
    }

    // The node as the part `value` of an `any` node, for a role's rule that takes any construct.
    Node anyNode(const Node& node) {
      Node any = MakeNode("any");
      any.isType = node.isType;
      AddNode(any, "value", PartKind::Value, node);

      if (const NodePart* type = PartNamed(node, "type")) {
        any.parts.push_back(*type);
      }

      return any;
    }

    // The node in `role`: its construct's rule for the role, else the role's rule for any construct, else its own rule.
    Lines writeInRole(Writer& writer, const Node& node, int required, bool asArgument, const String& role) {
      const Map<String, Vector<const Rule*>>& rules = node.isType ? writer.index->typeRules : writer.index->codeRules;

      if (role.empty()) {
        return writeWith(writer, node, required, asArgument);
      }

      if (const Rule* rule = bestIn(*writer.schema, rules, node.construct + "." + role, node)) {
        return writeRule(writer, rule, node, required);
      }

      const auto anyRules = rules.find("any." + role);
      const Rule* rule = anyRules == rules.end() ? nullptr : bestAny(*writer.schema, anyRules->second, node);

      if (rule == nullptr) {
        return writeWith(writer, node, required, asArgument);
      }

      return writesNothing(*rule) ? Lines{} : writeRule(writer, rule, anyNode(node), required);
    }

    // `camelCase` as `camel_case`, for a name that starts in lower case.
    String snakeCase(const String& name) {
      String out;

      for (const char c : name) {
        const bool upper = c >= 'A' && c <= 'Z';
        out += upper && !out.empty() ? "_" + String(1, static_cast<char>(c - 'A' + 'a')) : String(1, c);
      }

      return name.empty() || (name[0] >= 'A' && name[0] <= 'Z') ? name : out;
    }

    // `snake_case` as `snakeCase`.
    String camelCase(const String& name) {
      String out;

      for (size_t i = 0; i < name.size(); i++) {
        const bool joins = name[i] == '_' && i > 0 && i + 1 < name.size();
        out += joins ? String(1, static_cast<char>(toupper(static_cast<unsigned char>(name[++i])))) : String(1, name[i]);
      }

      return out;
    }

    String renamedPart(const Writer& writer, const String& part) {
      const String& names = writer.rules->style.names;
      const String cased = names == "snake" ? snakeCase(part) : names == "camel" ? camelCase(part) : part;

      return writer.rules->reserved.count(cased) > 0 ? cased + "_" : cased;
    }

  }  // namespace

  const Rule* ChooseRule(Writer& writer, const Node& node, bool asArgument) {
    if (asArgument && writer.index->argumentRules.count(node.construct) > 0) {
      if (const Rule* rule = bestOf(*writer.schema, writer.index->argumentRules.at(node.construct), node)) {
        return rule;
      }
    }

    return bestIn(*writer.schema, node.isType ? writer.index->typeRules : writer.index->codeRules, node.construct, node);
  }

  Node Verbatim(const String& text) {
    Node node = MakeNode("verbatim");
    AddText(node, "text", PartKind::Text, text);

    return node;
  }

  Lines WriteNode(Writer& writer, const Node& node, int required) {
    return WriteNodeAs(writer, node, required, "");
  }

  Lines WriteNodeAs(Writer& writer, const Node& node, int required, const String& role) {
    if (node.construct == "verbatim") {
      Lines lines;
      IStringStream text(node.parts[0].text);

      for (String line; getline(text, line);) {
        lines.push_back(line);
      }

      return lines;
    }

    return writeInRole(writer, node, required, false, role);
  }

  Lines WriteStatement(Writer& writer, const Node& node) {
    return WriteStatementAs(writer, node, "");
  }

  // A statement whose expression asked for the lower form is written again as its lower form, when it has one.
  Lines WriteStatementAs(Writer& writer, const Node& node, const String& role) {
    const bool around = writer.lowerStatement;
    writer.lowerStatement = false;
    writer.before.emplace_back();
    Lines lines = WriteNodeAs(writer, node, 0, role);

    if (writer.lowerStatement && !node.lowered.empty()) {
      writer.lowerStatement = false;
      writer.before.back().clear();
      lines = writeLowered(writer, node, 0);
    }

    Lines before = move(writer.before.back());
    writer.before.pop_back();
    writer.lowerStatement = writer.lowerStatement || around;

    return WithSpanMark(writer, node, withBefore(move(before), move(lines)));
  }

  Lines WriteType(Writer& writer, const Node& node, bool asArgument, const String& role) {
    return writeInRole(writer, node, 0, asArgument, role);
  }

  // Each step of the path renamed, joined by the style's `paths`.
  String RenamedName(const Writer& writer, const String& name) {
    String out;
    size_t from = 0;

    for (size_t dot = name.find('.'); dot != String::npos; dot = name.find('.', from)) {
      out += renamedPart(writer, name.substr(from, dot - from)) + writer.rules->style.paths;
      from = dot + 1;
    }

    return out + renamedPart(writer, name.substr(from));
  }

  void WriterProblem(Writer& writer, const String& message) {
    if (find(writer.problems.begin(), writer.problems.end(), message) == writer.problems.end()) {
      writer.problems.push_back(message);
    }
  }

}  // namespace polyir::translate
