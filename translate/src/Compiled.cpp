// A page's rules and a source's canon written as C++ that makes them (polyir/translate/Compiled.hpp), and what stops a page
// from being compiled. The C++ is plain data: each struct written with its fields named, in the order they are declared.
#include "polyir/translate/Compiled.hpp"

namespace polyir::translate {

  namespace {

    // ── C++ values ──

    String escapeOf(unsigned char c) {
      char octal[8];

      if (c == '\\' || c == '"') {
        return String("\\") + static_cast<char>(c);
      }

      if (c >= 0x20 && c < 0x7f) {
        return String(1, static_cast<char>(c));
      }

      snprintf(octal, sizeof octal, "\\%03o", c);

      return c == '\n' ? "\\n" : octal;
    }

    String quoted(const String& text) {
      String out = "\"";

      for (const char c : text) {
        out += escapeOf(static_cast<unsigned char>(c));
      }

      return out + "\"";
    }

    String flag(bool value) {
      return value ? "true" : "false";
    }

    // `{a, b}`: each item as `write` writes it.
    template <typename Items, typename Write>
    String listOf(const Items& items, Write write) {
      String out;

      for (const auto& item : items) {
        out += (out.empty() ? "" : ", ") + write(item);
      }

      return "{" + out + "}";
    }

    String strings(const Vector<String>& items) {
      return listOf(items, quoted);
    }

    String stringSet(const Set<String>& items) {
      return listOf(items, quoted);
    }

    String stringMap(const Map<String, String>& items) {
      return listOf(items, [](const Pair<const String, String>& item) { return "{" + quoted(item.first) + ", " + quoted(item.second) + "}"; });
    }

    String stringsMap(const Map<String, Vector<String>>& items) {
      return listOf(items, [](const Pair<const String, Vector<String>>& item) { return "{" + quoted(item.first) + ", " + strings(item.second) + "}"; });
    }

    String kindName(PartKind kind) {
      const char* const names[] = {"Name", "Text", "Value", "Block", "List", "Statement", "Type", "Fact"};

      return String("PartKind::") + names[static_cast<int>(kind)];
    }

    // ── a page's rules ──

    String valueName(RuleValue value) {
      const char* const names[] = {"Template", "Todo", "Unimplemented", "Unsupported", "Lower", "Style"};

      return String("RuleValue::") + names[static_cast<int>(value)];
    }

    String pieceText(const Piece& piece) {
      const String joined = piece.joined ? ", " + quoted(piece.separator) + ", true" : "";

      return "Piece{" + quoted(piece.text) + ", " + quoted(piece.hole) + ", " + quoted(piece.role) + ", " + flag(piece.optional) + joined + "}";
    }

    String pieces(const Vector<Piece>& items) {
      return listOf(items, pieceText);
    }

    String partText(const RulePart& part) {
      return "RulePart{" + quoted(part.name) + ", " + quoted(part.pattern) + ", " + strings(part.words) + "}";
    }

    String escapes(const Vector<Pair<String, String>>& items) {
      return listOf(items, [](const Pair<String, String>& item) { return "{" + quoted(item.first) + ", " + quoted(item.second) + "}"; });
    }

    String ruleText(const Rule& rule) {
      String text = "      Rule{.construct = " + quoted(rule.construct) + ", .role = " + quoted(rule.role) + ", .isType = " + flag(rule.isType);
      text += ", .isTypeArgument = " + flag(rule.isTypeArgument) + ",\n        .parts = " + listOf(rule.parts, partText);
      text += ", .value = " + valueName(rule.value) + ",\n        .pieces = " + pieces(rule.pieces) + ",\n        .reason = " + quoted(rule.reason);
      text += ", .before = " + pieces(rule.before) + ", .hoist = " + pieces(rule.hoist) + ", .file = " + quoted(rule.file);

      return text + ", .uses = " + strings(rule.uses) + ", .escapes = " + escapes(rule.escapes) + ", .line = " + to_string(rule.line) + "}";
    }

    String styleText(const Style& style) {
      String text = "Style{.indent = " + to_string(style.indent) + ", .colon = " + flag(style.colon) + ", .nextLine = " + flag(style.nextLine);
      text += ", .empty = " + quoted(style.empty) + ", .names = " + quoted(style.names) + ", .paths = " + quoted(style.paths);
      text += ", .fullNames = " + flag(style.fullNames) + ", .copies = " + flag(style.copies) + ", .uniqueLocals = " + flag(style.uniqueLocals);
      text += ", .flatNames = " + flag(style.flatNames) + ", .qualifyImported = " + flag(style.qualifyImported);

      return text + ", .extension = " + quoted(style.extension) + ", .library = " + quoted(style.library) + "}";
    }

    String sections(const Vector<PageSection>& items) {
      return listOf(items, [](const PageSection& section) { return "PageSection{" + quoted(section.name) + ", " + to_string(section.line) + "}"; });
    }

    String rulesList(const Vector<Rule>& rules) {
      String text;

      for (const Rule& rule : rules) {
        text += (text.empty() ? "" : ",\n") + ruleText(rule);
      }

      return "{\n" + text + "\n    }";
    }

    String rulesBody(const TranslateRules& rules) {
      String text = "    page.name = " + quoted(rules.name) + ";\n    page.file = " + quoted(rules.file) + ";\n";
      text += "    page.source = " + quoted(rules.source) + ";\n    page.target = " + quoted(rules.target) + ";\n";
      text += "    page.base = " + quoted(rules.base) + ";\n    page.style = " + styleText(rules.style) + ";\n";
      text += "    page.rules = " + rulesList(rules.rules) + ";\n    page.operators = " + stringMap(rules.operators) + ";\n";

      text += "    page.levels = " + listOf(rules.levels, strings) + ";\n    page.apart = " + stringSet(rules.apart) + ";\n";
      text += "    page.reserved = " + stringSet(rules.reserved) + ";\n    page.support = " + stringMap(rules.support) + ";\n";
      text += "    page.imports = " + stringMap(rules.imports) + ";\n    page.importOrder = " + strings(rules.importOrder) + ";\n";

      text += "    page.needs = " + stringsMap(rules.needs) + ";\n    page.spans = " + stringMap(rules.spans) + ";\n";

      return text + "    page.sections = " + sections(rules.sections) + ";\n";
    }

    // ── a schema ──

    String formText(const FormSpec& form) {
      const String parts = listOf(form.parts, [](const PartSpec& part) { return "PartSpec{" + quoted(part.name) + ", " + kindName(part.kind) + "}"; });

      return "FormSpec{" + quoted(form.section) + ", " + quoted(form.construct) + ", " + parts + ", " + flag(form.isType) + "}";
    }

    String formsList(const Vector<FormSpec>& forms) {
      String text;

      for (const FormSpec& form : forms) {
        text += (text.empty() ? "" : ",\n      ") + formText(form);
      }

      return "{\n      " + text + "}";
    }

    String partsOfText(const Vector<Vector<PartOf>>& partsOf) {
      return listOf(partsOf, [](const Vector<PartOf>& parts) {
        return listOf(parts, [](const PartOf& part) { return "PartOf{" + to_string(part.part) + ", " + kindName(part.kind) + "}"; });
      });
    }

    String idsText(const Map<String, int32_t>& ids) {
      return listOf(ids, [](const Pair<const String, int32_t>& id) { return "{" + quoted(id.first) + ", " + to_string(id.second) + "}"; });
    }

    String familiesText(const Map<String, Set<String>>& families) {
      return listOf(families, [](const Pair<const String, Set<String>>& family) { return "{" + quoted(family.first) + ", " + stringSet(family.second) + "}"; });
    }

    String schemaBody(const Schema& schema) {
      String text = "    schema.name = " + quoted(schema.name) + ";\n    schema.file = " + quoted(schema.file) + ";\n";
      text += "    schema.forms = " + formsList(schema.forms) + ";\n    schema.factNodes = " + formsList(schema.factNodes) + ";\n";
      text += "    schema.facts = " + stringsMap(schema.facts) + ";\n    schema.typeFacts = " + stringsMap(schema.typeFacts) + ";\n";
      text += "    schema.lowered = " + stringSet(schema.lowered) + ";\n    schema.families = " + familiesText(schema.families) + ";\n";

      text += "    schema.typed = " + stringSet(schema.typed) + ";\n    schema.constructs = " + strings(schema.constructs) + ";\n";
      text += "    schema.declarations = " + stringSet(schema.declarations) + ";\n";
      text += "    schema.isType = " + listOf(schema.isType, flag) + ";\n    schema.partsOf = " + partsOfText(schema.partsOf) + ";\n";
      text += "    schema.parts = " + strings(schema.parts) + ";\n    schema.codeIds = " + idsText(schema.codeIds) + ";\n";

      return text + "    schema.typeIds = " + idsText(schema.typeIds) + ";\n    schema.partIds = " + idsText(schema.partIds) + ";\n";
    }

    // Each line of the body two steps further in, as the function it is in.
    String indentedBody(const String& body) {
      String text;
      IStringStream lines(body);

      for (String line; getline(lines, line);) {
        text += (line.empty() ? "" : "  ") + line + "\n";
      }

      return text;
    }

    // The generated file: what made it, then a function that makes the value once and keeps it.
    String generated(const String& type, const String& name, const String& function, const String& from, const String& body) {
      String text = "// GENERATED by polyir-translatec from " + from + ": edit that file, not this one.\n";
      text += "#include \"polyir/translate/Rules.hpp\"\n\nnamespace polyir::translate {\n\n  namespace {\n\n";
      text += "    " + type + " made() {\n      " + type + " " + name + ";\n" + indentedBody(body) + "\n      return " + name + ";\n    }\n\n  }  // namespace\n\n";
      text += "  const " + type + "& " + function + "() {\n    static const " + type + " value = made();\n\n    return value;\n  }\n\n";

      return text + "}  // namespace polyir::translate\n";
    }

  }  // namespace

  Vector<String> CompileProblems(const Target& page) {
    Vector<String> problems = TargetProblems(page);

    for (const String& form : problems.empty() ? MissingConstructs(page) : Vector<String>{}) {
      problems.push_back(page.file + ": the form `" + form + "` has no rule (write one, or `todo`, `unimplemented \"why\"`, `unsupported \"why\"`)");
    }

    return problems;
  }

  String CompiledRulesText(const TranslateRules& rules, const String& function, const String& from) {
    return generated("TranslateRules", "page", function, from, rulesBody(rules));
  }

  String CompiledSchemaText(const Schema& schema, const String& function, const String& from) {
    return generated("Schema", "schema", function, from, schemaBody(schema));
  }

}  // namespace polyir::translate
