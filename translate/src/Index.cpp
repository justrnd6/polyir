// A page made ready to write by (Writer.hpp): its rules grouped by the construct and role they write, its operators' levels,
// each template worked out, and its problems by its source's canon. It is made once for each page's text, then kept.
#include "Writer.hpp"

namespace polyir::translate {

  namespace {

    // How tightly operators bind when a page does not say (`levels:`), loosest first: C's, with `?` for a conditional.
    const char* const kDefaultLevels[] = {"?", "||", "&&", "|", "^", "&", "== != === !==", "< <= > >=", "<< >>", "+ -", "* / %"};

    // The rules by the construct and role they write (`field.param`).
    void groupRules(PageIndex& index) {
      for (const Rule& rule : index.page.rules.rules) {
        Map<String, Vector<const Rule*>>& group = rule.isTypeArgument ? index.argumentRules : rule.isType ? index.typeRules : index.codeRules;
        group[RuleKey(rule)].push_back(&rule);
      }
    }

    // The levels, each source operator in the default ones spelled as the page spells it.
    Vector<Vector<String>> levelsOf(const TranslateRules& rules) {
      if (!rules.levels.empty()) {
        return rules.levels;
      }

      Vector<Vector<String>> levels;

      for (const char* level : kDefaultLevels) {
        IStringStream tokens(level);
        levels.emplace_back();

        for (String token; tokens >> token;) {
          const auto spelled = rules.operators.find(token);
          levels.back().push_back(spelled == rules.operators.end() ? token : spelled->second);
        }
      }

      return levels;
    }

    void readLevels(PageIndex& index) {
      const Vector<Vector<String>> levels = levelsOf(index.page.rules);

      for (size_t i = 0; i < levels.size(); i++) {
        for (const String& token : levels[i]) {
          index.levels[token] = static_cast<int>(i);
        }
      }

      index.topLevel = static_cast<int>(levels.size());
    }

    // The blocks a `block` rule other than `:= style` may be for: those its `of:` names, or every one when it names none.
    void addOwnBlocks(PageIndex& index) {
      const auto rules = index.codeRules.find("block");

      for (const Rule* rule : rules == index.codeRules.end() ? Vector<const Rule*>{} : rules->second) {
        const auto of = find_if(rule->parts.begin(), rule->parts.end(), [](const RulePart& part) { return part.name == "of"; });
        const bool any = of == rule->parts.end() || of->words.empty();

        if (rule->value != RuleValue::Style) {
          any ? void(index.everyBlockOwn = true) : index.ownBlocks.insert(of->words.begin(), of->words.end());
        }
      }
    }

    void addForms(PageIndex& index) {
      for (const Rule& rule : index.page.rules.rules) {
        for (const Vector<Piece>* pieces : {&rule.pieces, &rule.before, &rule.hoist}) {
          index.forms.emplace(pieces, FormOfTemplate(index, *pieces));
        }
      }
    }

    // The index is made where it is kept, so its rules' places (which the groups and forms point at) never move.
    SharedPtr<const PageIndex> indexed(const Target& target, const Schema& schema) {
      SharedPtr<PageIndex> index = make_shared<PageIndex>();
      index->page = *PageOf(target);
      groupRules(*index);
      readLevels(*index);

      addForms(*index);
      addOwnBlocks(*index);
      index->problems = PageProblems(index->page.rules, schema);

      return index;
    }

    // An index kept with the page's text and support files: a page of the same file with another text, or other files, is
    // indexed again.
    struct KeptIndex {
      String text;
      Map<String, String> files;
      SharedPtr<const PageIndex> index;
    };

  }  // namespace

  SharedPtr<const PageIndex> IndexOf(const Target& target, const Schema& schema) {
    static Mutex mutex;
    static Map<String, KeptIndex> kept;  // by the source and the page's file
    const LockGuard<Mutex> lock(mutex);
    KeptIndex& entry = kept[schema.name + "\n" + target.file];

    if (entry.index == nullptr || entry.text != target.text || entry.files != target.files) {
      entry = KeptIndex{target.text, target.files, indexed(target, schema)};
    }

    return entry.index;
  }

  Writer MakeWriter(const PageIndex& index, const Target& target, const Schema& schema) {
    Writer writer;
    writer.index = &index;
    writer.rules = &index.page.rules;
    writer.target = &target;
    writer.schema = &schema;

    return writer;
  }

}  // namespace polyir::translate
