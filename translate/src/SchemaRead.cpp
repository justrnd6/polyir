// Reads a `.canon` file (Schema.hpp): a source's canon. `source:` names the language, `typed:` the sections whose forms
// also have their value's `type`, and `declarations:` the parts that hold declarations. `## kinds` and `## type kinds` give a part's kind by its name. `## families` gives the words a
// pattern may use for several types. Every other `## section` lists forms, one per line: `## types` the type forms, `## fact
// nodes` the nodes a fact holds. An indented `facts:` line, or `lowered`, under a form is about its construct.
#include "polyir/translate/Schema.hpp"

namespace polyir::translate {

  namespace {

    // The canon as it is read: the section, the kinds read so far, and the construct indented lines are about.
    struct SchemaReader {
      Schema schema;
      String section;
      int line = 0;
      Map<String, PartKind> codeKinds;  // `## kinds`
      Map<String, PartKind> typeKinds;  // `## type kinds`
      String lastConstruct;             // the construct of the form just read, "" for none
      bool lastIsType = false;
    };

    struct KindName {
      const char* name;
      PartKind kind;
    };

    const KindName kKindNames[] = {{"name", PartKind::Name}, {"text", PartKind::Text}, {"value", PartKind::Value}, {"block", PartKind::Block},
      {"list", PartKind::List}, {"statement", PartKind::Statement}, {"type", PartKind::Type}, {"fact", PartKind::Fact}};

    Optional<PartKind> kindNamed(const String& name) {
      for (const KindName& row : kKindNames) {
        if (name == row.name) {
          return row.kind;
        }
      }

      return nullopt;
    }

    void problem(SchemaReader& reader, const String& message) {
      reader.schema.problems.push_back(reader.schema.file + ":" + to_string(reader.line) + ": " + message);
    }

    String trimmed(const String& text) {
      const size_t from = text.find_first_not_of(" \t");
      const size_t to = text.find_last_not_of(" \t\r");

      return from == String::npos ? "" : text.substr(from, to - from + 1);
    }

    bool startsWith(const String& text, const String& start) {
      return text.rfind(start, 0) == 0;
    }

    // The items between commas, each trimmed (`cond, then` -> `cond`, `then`).
    Vector<String> itemsOf(const String& text) {
      Vector<String> items;
      IStringStream in(text);

      for (String item; getline(in, item, ',');) {
        if (!trimmed(item).empty()) {
          items.push_back(trimmed(item));
        }
      }

      return items;
    }

    // The words of a line, apart by spaces or commas.
    Vector<String> wordsOf(const String& text) {
      Vector<String> words;

      for (const String& item : itemsOf(text)) {
        IStringStream in(item);

        for (String word; in >> word;) {
          words.push_back(word);
        }
      }

      return words;
    }

    bool isWord(const String& text) {
      const auto isWordChar = [](char c) { return isalnum(static_cast<unsigned char>(c)) || c == '_'; };

      return !text.empty() && isalpha(static_cast<unsigned char>(text[0])) && all_of(text.begin(), text.end(), isWordChar);
    }

    // ── kinds and families ──

    // `text := op number case`: each part named on the right has the kind on the left.
    void kindLine(SchemaReader& reader, const String& left, const String& right) {
      const Optional<PartKind> kind = kindNamed(left);
      Map<String, PartKind>& kinds = reader.section == "type kinds" ? reader.typeKinds : reader.codeKinds;

      if (!kind) {
        return problem(reader, "`" + left + "` is no kind of part: name, text, value, block, list, statement, type or fact");
      }

      for (const String& name : wordsOf(right)) {
        kinds[name] = *kind;
      }
    }

    // The types a word of a family stands for: those of a family named before, or the word itself.
    Set<String> typesOf(const Map<String, Set<String>>& families, const String& word, const String& family) {
      const auto named = families.find(word);

      return named == families.end() || word == family ? Set<String>{word} : named->second;
    }

    // `number := int float double`: a family's types, with the types of a family it names.
    void familyLine(SchemaReader& reader, const String& left, const String& right) {
      Set<String> types;

      for (const String& word : wordsOf(right)) {
        const Set<String> more = typesOf(reader.schema.families, word, left);
        types.insert(more.begin(), more.end());
      }

      reader.schema.families[left] = types;
    }

    void pairLine(SchemaReader& reader, const String& text) {
      const size_t mark = text.find(":=");

      if (mark == String::npos) {
        return problem(reader, "`" + text + "` is not a `name := words` line");
      }

      const String left = trimmed(text.substr(0, mark));
      const String right = trimmed(text.substr(mark + 2));

      return reader.section == "families" ? familyLine(reader, left, right) : kindLine(reader, left, right);
    }

    // ── forms ──

    // A part's kind from its name in `## kinds` (`## type kinds` for a type's part): else a value, or for a type a type.
    PartKind kindOf(const SchemaReader& reader, const String& name, bool isType) {
      const Map<String, PartKind>& kinds = isType ? reader.typeKinds : reader.codeKinds;
      const auto found = kinds.find(name);

      return found != kinds.end() ? found->second : isType ? PartKind::Type : PartKind::Value;
    }

    // `then: value` or `body`.
    PartSpec partOf(SchemaReader& reader, const String& text, bool isType) {
      const size_t colon = text.find(':');
      const String name = trimmed(text.substr(0, colon));
      const String given = colon == String::npos ? "" : trimmed(text.substr(colon + 1));
      const Optional<PartKind> kind = given.empty() ? nullopt : kindNamed(given);

      if (!given.empty() && !kind) {
        problem(reader, "`" + given + "` is no kind of part: name, text, value, block, list, statement, type or fact");
      }

      return PartSpec{name, kind ? *kind : kindOf(reader, name, isType)};
    }

    // `if(cond, then, else)`: a construct's name, then its parts in parentheses (none for `return`).
    void formLine(SchemaReader& reader, const String& text) {
      const size_t open = text.find('(');
      const bool closed = open == String::npos || text.back() == ')';
      FormSpec form{reader.section, trimmed(text.substr(0, open)), {}, reader.section == "types"};

      if (!closed || !isWord(form.construct)) {
        return problem(reader, "`" + text + "` is not a form, like `if(cond, then)`");
      }

      for (const String& part : itemsOf(open == String::npos ? "" : text.substr(open + 1, text.size() - open - 2))) {
        form.parts.push_back(partOf(reader, part, form.isType));
      }

      reader.lastConstruct = form.construct;
      reader.lastIsType = form.isType;
      (reader.section == "fact nodes" ? reader.schema.factNodes : reader.schema.forms).push_back(form);
    }

    // `facts: isKey, isMutable` or `lowered` under a form: about the form's construct.
    void underForm(SchemaReader& reader, const String& text) {
      if (reader.lastConstruct.empty()) {
        return problem(reader, "`" + text + "` is under no form");
      }

      if (text == "lowered" && !reader.lastIsType) {
        reader.schema.lowered.insert(reader.lastConstruct);
        return;
      }

      if (!startsWith(text, "facts:")) {
        return problem(reader, "`" + text + "` is neither `facts: ...` nor `lowered` (which only a code construct has)");
      }

      Vector<String>& facts = (reader.lastIsType ? reader.schema.typeFacts : reader.schema.facts)[reader.lastConstruct];
      const Vector<String> named = wordsOf(text.substr(6));
      facts.insert(facts.end(), named.begin(), named.end());
    }

    // ── one line ──

    // `source: polyir`, `typed: expressions, builtins` and `declarations: items, methods`, before the first section.
    void headLine(SchemaReader& reader, const String& text) {
      if (startsWith(text, "source:")) {
        reader.schema.name = trimmed(text.substr(7));
      } else if (startsWith(text, "typed:")) {
        const Vector<String> sections = itemsOf(text.substr(6));
        reader.schema.typed.insert(sections.begin(), sections.end());
      } else if (startsWith(text, "declarations:")) {
        const Vector<String> parts = itemsOf(text.substr(13));
        reader.schema.declarations.insert(parts.begin(), parts.end());
      } else {
        problem(reader, "`" + text + "` comes before any `## section`");
      }
    }

    void sectionLine(SchemaReader& reader, const String& text) {
      const bool pairs = reader.section == "kinds" || reader.section == "type kinds" || reader.section == "families";

      return pairs ? pairLine(reader, text) : formLine(reader, text);
    }

    void schemaLine(SchemaReader& reader, const String& line) {
      const String text = trimmed(line);

      if (text.empty() || startsWith(text, "//")) {
        return;
      }

      if (line[0] == ' ' || line[0] == '\t') {
        return underForm(reader, text);
      }

      if (startsWith(text, "## ")) {
        reader.section = trimmed(text.substr(3));
        reader.lastConstruct = "";
      } else if (text[0] != '#') {
        reader.section.empty() ? headLine(reader, text) : sectionLine(reader, text);
      }
    }

    // ── ids ──

    PartId partId(Schema& schema, const String& name) {
      const PartId found = PartIdOf(schema, name);

      if (found != kNoId) {
        return found;
      }

      schema.parts.push_back(name);
      schema.partIds[name] = static_cast<PartId>(schema.parts.size() - 1);

      return static_cast<PartId>(schema.parts.size() - 1);
    }

    ConstructId constructId(Schema& schema, const String& name, bool isType) {
      const ConstructId found = ConstructNamed(schema, name, isType);

      if (found != kNoId) {
        return found;
      }

      const ConstructId id = static_cast<ConstructId>(schema.constructs.size());
      schema.constructs.push_back(name);
      schema.isType.push_back(isType);
      schema.partsOf.emplace_back();

      (isType ? schema.typeIds : schema.codeIds)[name] = id;

      return id;
    }

    void addPart(Schema& schema, ConstructId construct, const String& name, PartKind kind) {
      const PartId part = partId(schema, name);
      Vector<PartOf>& parts = schema.partsOf[static_cast<size_t>(construct)];
      const bool known = any_of(parts.begin(), parts.end(), [&](const PartOf& each) { return each.part == part; });

      if (!known) {
        parts.push_back(PartOf{part, kind});
      }
    }

    // A form's parts, its construct's facts, a typed form's `type`, and the lower form of a construct that has one.
    void addIds(Schema& schema, const FormSpec& form) {
      const ConstructId construct = constructId(schema, form.construct, form.isType);

      for (const PartSpec& part : form.parts) {
        addPart(schema, construct, part.name, part.kind);
      }

      for (const String& fact : FactsOf(schema, form.construct, form.isType)) {
        addPart(schema, construct, fact, PartKind::Fact);
      }

      if (IsTyped(schema, form)) {
        addPart(schema, construct, "type", PartKind::Type);
      }

      if (!form.isType && HasLowerForm(schema, form.construct)) {
        addPart(schema, construct, kLoweredPart, PartKind::Block);
      }
    }

  }  // namespace

  Schema ReadSchema(const String& text, const String& file) {
    SchemaReader reader;
    reader.schema.file = file;
    IStringStream lines(text);

    for (String line; getline(lines, line);) {
      reader.line++;
      schemaLine(reader, line);
    }

    if (reader.schema.name.empty()) {
      reader.schema.problems.push_back(file + ": the canon names no `source:`, the language whose trees it describes");
    }

    for (const Vector<FormSpec>* forms : {&reader.schema.forms, &reader.schema.factNodes}) {
      for (const FormSpec& form : *forms) {
        addIds(reader.schema, form);
      }
    }

    return reader.schema;
  }

}  // namespace polyir::translate
