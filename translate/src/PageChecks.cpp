// What a page lacks and what is wrong in it, by its source's canon (Schema.hpp). A page names its source and its target. It
// has a rule, or a word for why it has none, for every form of its source, and its rules name only what those forms have.
// Also a new page with every rule `todo`, a page with a `todo` rule added for each form it lacks, and a page's counts.
#include "Pages.hpp"

namespace polyir::translate {

  namespace {

    // A page being checked: its rules (with its base's), its source's canon, and what is wrong so far.
    struct Check {
      const TranslateRules& rules;
      const Schema& schema;
      Vector<String> problems;
    };

    // Whether `rule` writes every construct of `form`. It names only parts the form has (a typed form's `type` too), and it
    // names every part that is not a type or a fact.
    bool covers(const Schema& schema, const Rule& rule, const FormSpec& form) {
      const bool same = rule.construct == form.construct && rule.role.empty() && rule.isType == form.isType && !rule.isTypeArgument;
      const bool anyValue = all_of(rule.parts.begin(), rule.parts.end(), [](const RulePart& part) { return part.pattern.empty(); });
      const bool hasEach = all_of(rule.parts.begin(), rule.parts.end(), [&](const RulePart& part) {
        return FindPart(form, part.name) != nullptr || (IsTyped(schema, form) && part.name == "type");
      });

      if (!same || !anyValue || !hasEach) {
        return false;
      }

      return all_of(form.parts.begin(), form.parts.end(), [&](const PartSpec& part) {
        return MayLeaveOut(part.kind) || any_of(rule.parts.begin(), rule.parts.end(), [&](const RulePart& named) { return named.name == part.name; });
      });
    }

    bool isCovered(const Schema& schema, const TranslateRules& rules, const FormSpec& form) {
      return any_of(rules.rules.begin(), rules.rules.end(), [&](const Rule& rule) { return covers(schema, rule, form); });
    }

    String ruleWhere(const Rule& rule) {
      return rule.file + ":" + to_string(rule.line) + ": ";
    }

    // ── what is wrong in a page ──

    // A hole that is not one of the rule's parts. A hole may write a fact of the construct the head does not name: naming one
    // there asks for it.
    void holeProblems(Check& check, const Rule& rule) {
      const Vector<String> facts = FactsOf(check.schema, rule.construct, rule.isType);

      for (const Vector<Piece>* pieces : {&rule.pieces, &rule.before, &rule.hoist}) {
        for (const Piece& piece : *pieces) {
          const bool fact = find(facts.begin(), facts.end(), piece.hole) != facts.end();
          const bool named = piece.hole.empty() || fact || any_of(rule.parts.begin(), rule.parts.end(), [&](const RulePart& part) { return part.name == piece.hole; });

          if (!named) {
            check.problems.push_back(ruleWhere(rule) + "`{" + piece.hole + "}` is not one of the rule's parts");
          }
        }
      }
    }

    // A part the rule names that no form of its construct has; a typed form also has its `type`.
    void partProblems(Check& check, const Rule& rule, const Vector<const FormSpec*>& forms) {
      const bool typed = !forms.empty() && IsTyped(check.schema, *forms[0]);
      const Vector<String> facts = FactsOf(check.schema, rule.construct, rule.isType);

      for (const RulePart& part : rule.parts) {
        const bool inForm = any_of(forms.begin(), forms.end(), [&](const FormSpec* form) { return FindPart(*form, part.name) != nullptr; });
        const bool known = inForm || (typed && part.name == "type") || find(facts.begin(), facts.end(), part.name) != facts.end();

        if (!known) {
          check.problems.push_back(ruleWhere(rule) + "`" + rule.construct + "` has no part `" + part.name + "`");
        }
      }

      holeProblems(check, rule);
    }

    // `any.number(value)`: a role's rule for any construct, which it takes as its one part `value`.
    void anyProblems(Check& check, const Rule& rule) {
      const bool onlyValue = all_of(rule.parts.begin(), rule.parts.end(), [](const RulePart& part) { return part.name == "value" || part.name == "type"; });

      if (rule.role.empty()) {
        check.problems.push_back(ruleWhere(rule) + "`any` writes a role, as in `any.number(value)`");
      } else if (!onlyValue) {
        check.problems.push_back(ruleWhere(rule) + "an `any` rule has the parts `value` and `type`");
      }

      holeProblems(check, rule);
    }

    // `block(statements, of: func.body)`: how a block is laid out where a construct's rule puts it in a line.
    void blockProblems(Check& check, const Rule& rule) {
      const auto named = [&](const String& name) { return any_of(rule.parts.begin(), rule.parts.end(), [&](const RulePart& part) { return part.name == name; }); };
      const bool onlyThose = all_of(rule.parts.begin(), rule.parts.end(), [](const RulePart& part) { return part.name == "statements" || part.name == "of"; });

      if (!onlyThose || !named("statements") || !named("of")) {
        check.problems.push_back(ruleWhere(rule) + "a `block` rule has the parts `statements` and `of`, as in `block(statements, of: func.body)`");
      }

      holeProblems(check, rule);
    }

    // Each word of a block rule's `of:` names a place the source holds a block (`func.body`, `if.then`).
    void contextProblems(Check& check, const Rule& rule) {
      const Vector<String> contexts = BlockContexts(check.schema);

      for (const RulePart& part : rule.parts) {
        for (const String& word : part.name == "of" ? part.words : Vector<String>{}) {
          const bool known = find(contexts.begin(), contexts.end(), word) != contexts.end();
          known ? void() : check.problems.push_back(ruleWhere(rule) + "there is no block `" + word + "` among " + check.schema.name + "'s blocks");
        }
      }
    }

    void ruleProblems(Check& check, const Rule& rule) {
      const Vector<const FormSpec*> forms = FormsOf(check.schema, rule.construct, rule.isType);
      const String among = rule.isType ? " among " + check.schema.name + "'s types" : " in " + check.schema.name + "'s canon";

      // A source whose canon has no `block` still lets a page lay its blocks out by `block` rules.
      if (rule.construct == "any" || (rule.construct == "block" && forms.empty())) {
        return rule.construct == "any" ? anyProblems(check, rule) : blockProblems(check, rule);
      }

      if (rule.value == RuleValue::Style && rule.construct != "block") {
        check.problems.push_back(ruleWhere(rule) + "only a `block` rule is laid out by the style (`:= style`)");
      }

      rule.construct == "block" ? contextProblems(check, rule) : void();

      if (forms.empty()) {
        return check.problems.push_back(ruleWhere(rule) + "there is no construct `" + rule.construct + "`" + among);
      }

      if (rule.value == RuleValue::Lower && !HasLowerForm(check.schema, rule.construct)) {
        check.problems.push_back(ruleWhere(rule) + "`" + rule.construct + "` has no lower form");
      }

      partProblems(check, rule, forms);
    }

    // A section of rules is one of the source's, or `## type arguments`.
    bool isSectionOf(const Schema& schema, const String& section) {
      const bool forms = any_of(schema.forms.begin(), schema.forms.end(), [&](const FormSpec& form) { return form.section == section; });

      return forms || section == "type arguments" || IsSettingSection(section);
    }

    // The page names its source and its target, and its sections are its source's.
    void headProblems(Check& check) {
      const TranslateRules& rules = check.rules;

      if (rules.target.empty()) {
        check.problems.push_back(rules.file + ": the page names no `target:`, the language it writes " + rules.source + "'s constructs in");
      }

      for (const PageSection& section : rules.sections) {
        if (!isSectionOf(check.schema, section.name)) {
          check.problems.push_back(rules.file + ":" + to_string(section.line) + ": there is no section `" + section.name + "` in " +
            check.schema.name + "'s canon");
        }
      }
    }

    // ── a new page, and the rules a page lacks ──

    // `name(parts) := todo` lines, the `:=` of each section lined up.
    // A block is laid out by the style until a page says otherwise; every other rule starts `todo`.
    String todoLines(const Vector<const FormSpec*>& forms) {
      size_t width = 0;

      for (const FormSpec* form : forms) {
        width = max(width, FormHead(*form).size());
      }

      String text;

      for (const FormSpec* form : forms) {
        const String head = FormHead(*form);
        text += head + String(width - head.size() + 2, ' ') + (form->construct == "block" ? ":= style\n" : ":= todo\n");
      }

      return text;
    }

    // The schema's sections in order, each with its forms that `wanted` keeps.
    Vector<Pair<String, Vector<const FormSpec*>>> sectionsOf(const Schema& schema, const Function<bool(const FormSpec&)>& wanted) {
      Vector<Pair<String, Vector<const FormSpec*>>> sections;

      for (const FormSpec& form : schema.forms) {
        if (sections.empty() || sections.back().first != form.section) {
          sections.push_back({form.section, {}});
        }

        if (wanted(form)) {
          sections.back().second.push_back(&form);
        }
      }

      return sections;
    }

    // `// struct may also name the facts isKey, isMutable`.
    String factsLine(const String& construct, const Vector<String>& facts) {
      String names;

      for (const String& fact : facts) {
        names += (names.empty() ? "" : ", ") + fact;
      }

      return "// " + construct + " may also name the facts " + names + "\n";
    }

    // The comment a section of a new page starts with: the facts its constructs may also name.
    String factsComment(const Schema& schema, const Vector<const FormSpec*>& forms) {
      String text;
      Set<String> said;

      for (const FormSpec* form : forms) {
        const Vector<String> facts = FactsOf(schema, form->construct, form->isType);

        if (!facts.empty() && said.insert(form->construct).second) {
          text += factsLine(form->construct, facts);
        }
      }

      return text;
    }

    const char* const kNewPageEnd =
      "## type arguments\n"
      "// How a type is written inside <...>, when it differs from ## types (Java: i64 := Long).\n\n"
      "## operators\n"
      "// A source operator and the target's spelling (&& := and), and levels: from loosest to tightest.\n\n"
      "## reserved\n"
      "// The target's own words: a name that is one of them gets a _ after it.\n\n"
      "## imports\n"
      "// name := an import line (math := #include <math.h>), written once at the program's {imports} when a rule says uses: name.\n\n"
      "## support\n"
      "// name := runtime/File.ext, written once at the program's {support} when a rule says uses: name; an indented uses: line\n"
      "// under it names what it needs.\n\n"
      "## spans\n"
      "// With spans asked for: line := what goes before a statement from another source line ({file} {line} {column} {code}),\n"
      "// and map := the line naming a source map ({map}).\n";

    // Where lines go at the end of the section whose heading is at `at`: after its last rule, before the blank lines that end it.
    size_t sectionEnd(const String& text, size_t at) {
      const size_t next = text.find("\n## ", at + 1);
      size_t end = next == String::npos ? text.size() : next + 1;

      while (end >= 2 && text[end - 1] == '\n' && text[end - 2] == '\n') {
        end--;
      }

      return end;
    }

    // The page with `todo` lines for `missing` added at the end of their sections (a section the page lacks is added).
    String withMissing(const String& page, const Vector<Pair<String, Vector<const FormSpec*>>>& missing) {
      String text = page.empty() || page.back() == '\n' ? page : page + "\n";

      for (const auto& [section, forms] : missing) {
        const size_t at = text.find("## " + section + "\n");

        if (forms.empty()) {
          continue;
        }

        if (at == String::npos) {
          text += "\n## " + section + "\n" + todoLines(forms);
        } else {
          text.insert(sectionEnd(text, at), todoLines(forms));
        }
      }

      return text;
    }

    String lowerCase(String text) {
      for (char& c : text) {
        c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
      }

      return text;
    }

  }  // namespace

  Vector<String> PageProblems(const TranslateRules& rules, const Schema& schema) {
    Check check{rules, schema, rules.problems};
    check.problems.insert(check.problems.end(), schema.problems.begin(), schema.problems.end());
    headProblems(check);

    for (const Rule& rule : rules.rules) {
      ruleProblems(check, rule);
    }

    return check.problems;
  }

  // A page with no `source:`, or a source with no canon to be found, has nothing to be checked by.
  Vector<String> TargetProblems(const Target& target) {
    const SharedPtr<const Page> page = PageOf(target);
    const TranslateRules& rules = page->rules;
    const Optional<Schema> schema = rules.source.empty() ? nullopt : SchemaFor(target, rules.source);
    Vector<String> problems = rules.problems;

    if (rules.source.empty()) {
      problems.push_back(rules.file + ": the page names no `source:`, the language whose constructs it writes");
    } else if (!schema) {
      problems.push_back(rules.file + ": there is no canon for the source `" + rules.source + "` (a .canon file naming it)");
    }

    return schema ? PageProblems(rules, *schema) : problems;
  }

  Vector<String> MissingConstructs(const Target& target) {
    const SharedPtr<const Page> page = PageOf(target);
    const TranslateRules& rules = page->rules;
    const Optional<Schema> schema = SchemaFor(target, rules.source);
    Vector<String> missing;

    for (const FormSpec& form : schema ? schema->forms : Vector<FormSpec>{}) {
      if (!isCovered(*schema, rules, form)) {
        missing.push_back(FormHead(form) + (form.isType ? " (a type)" : ""));
      }
    }

    return missing;
  }

  String NewTargetText(const String& name, const Schema& source) {
    String text = "# " + name + "\nsource: " + source.name + "\ntarget: " + lowerCase(name) + "\n";
    text += "style: indent 2, braces same-line, names keep, paths ., generics kept\n";

    for (const auto& [section, forms] : sectionsOf(source, [](const FormSpec&) { return true; })) {
      text += "\n## " + section + "\n" + factsComment(source, forms) + todoLines(forms);
    }

    return text + "\n" + kNewPageEnd;
  }

  String UpdatedTargetText(const Target& target) {
    const SharedPtr<const Page> page = PageOf(target);
    const TranslateRules& rules = page->rules;
    const Optional<Schema> schema = SchemaFor(target, rules.source);

    if (!schema) {
      return target.text;
    }

    return withMissing(target.text, sectionsOf(*schema, [&](const FormSpec& form) { return !isCovered(*schema, rules, form); }));
  }

  RuleCounts CountRules(const Target& target) {
    RuleCounts counts;

    const SharedPtr<const Page> page = PageOf(target);

    for (const Rule& rule : page->rules.rules) {
      counts.written += rule.value == RuleValue::Template || rule.value == RuleValue::Style ? 1 : 0;
      counts.todo += rule.value == RuleValue::Todo ? 1 : 0;
      counts.unimplemented += rule.value == RuleValue::Unimplemented ? 1 : 0;
      counts.unsupported += rule.value == RuleValue::Unsupported ? 1 : 0;
      counts.lowered += rule.value == RuleValue::Lower ? 1 : 0;
    }

    return counts;
  }

}  // namespace polyir::translate
