// A `.translate` page as data (core/docs/translate.md): its head and style line, its rules `construct(parts) := template`, and
// the sections that are not rules (operators, reserved words, imports, support files). RulesRead.cpp reads a page into these
// structs; polyir-translatec compiles one into C++ that makes them (Compiled.hpp).
#pragma once

#include "polyir/translate/Schema.hpp"

namespace polyir::translate {

  // The style line: `style: indent 2, braces same-line, names keep, paths ., generics kept`.
  struct Style {
    int indent = 2;
    bool colon = false;     // `blocks colon`: a block is `:` and indented lines (Python)
    bool nextLine = false;  // `braces next-line`
    String empty;           // what an empty block holds (`empty pass`)
    String names = "keep";  // keep, snake or camel
    String paths = ".";     // what joins a path (`Geometry.Point`, `Geometry::Point`)
    bool fullNames = false; // `qualify full`: every declaration named by its whole path, even where its own name would do
    bool copies = false;    // `generics copies`: generics written as the copies made for each type (`Box_i64`)
    bool uniqueLocals = false;  // `locals unique`: a local that would hide another gets a name of its own (`c2`), as Java needs
    bool flatNames = false;  // `modules flat`: a declaration is declared by its whole path (`Geometry_Point`), for C
    bool qualifyImported = false;  // `imported qualified`: a declaration of an imported file is named through it (`Check.offInt`)
    String extension;       // the target's file extension, for tools that write files
    String library;         // `library hpp`: the extension of a file with no `main` of its own, when it differs
  };

  // What a rule's right side is: a template, `todo` (not looked at yet), `unimplemented "why"` (understood, not written yet),
  // `unsupported "why"` (the target cannot say it), `lower` (write the source's lower form of the construct), or `style` (a
  // block laid out as the style line says).
  enum class RuleValue { Template, Todo, Unimplemented, Unsupported, Lower, Style };

  // One part a rule names, with the pattern its value must match (`left: i32`), "" for any.
  struct RulePart {
    String name;
    String pattern;
    Vector<String> words{};  // the pattern's words: the part matches when one of them does
  };

  // A piece of a template: literal text, or a hole that a part fills (`{name}`), maybe in a role (`{fields.param}`).
  struct Piece {
    String text;
    String hole;
    String role;
    bool optional = false;  // `{nested?}`: the line is written only when the part has items
    String separator{};     // `{tests| && }`: what joins the part's items in the line, instead of `, `
    bool joined = false;    // a separator is given (it may be empty: `{pieces|}`)
  };

  struct Rule {
    String construct;
    String role;                  // `field.param(name)`: how a field is written where a hole asks for `param`; `any` is any construct
    bool isType = false;          // in `## types` or `## type arguments`
    bool isTypeArgument = false;  // in `## type arguments`: how a type is written inside `<...>`
    Vector<RulePart> parts;
    RuleValue value = RuleValue::Template;
    Vector<Piece> pieces;         // the template, its lines joined with "\n"
    String reason;                // why it is unimplemented or unsupported
    Vector<Piece> before;         // a `before:` template, written ahead of the statement that uses the rule
    Vector<Piece> hoist;          // a `hoist:` template, written once among the program's declarations (C's closure functions)
    String file;                  // the page the rule is on (a base page's rule keeps its own)
    Vector<String> uses;          // the support files it needs (`uses: equals`)
    Vector<Pair<String, String>> escapes;  // `escape: ` as \``: text replaced in the text parts it writes (a template literal's backtick)
    int line = 0;
  };

  // A `## section` of a page and the line it starts on.
  struct PageSection {
    String name;
    int line = 0;
  };

  struct TranslateRules {
    String name;    // the page's `# Js`
    String file;
    String source;  // `source: polyir`: the language whose constructs the rules write (a base page's, when the page names none)
    String target;  // `target: js`: the language they write them in
    String base;    // `base: Js`: the page whose rules come first, so one alike on this page replaces them
    Style style;
    Vector<Rule> rules;
    Map<String, String> operators;  // a .poly operator and the target's spelling (`&& := and`)
    Vector<Vector<String>> levels;  // how tightly operators bind, loosest first; empty means the default
    Set<String> apart;              // `apart: == <`: operators that do not chain, so an operand using one beside another is bracketed
    Set<String> reserved;           // names the target's language keeps for itself
    Map<String, String> support;    // a support file's name and its path under the page's folder
    Map<String, String> imports;    // `## imports`: an import's name and its line (`math := #include <math.h>`)
    Vector<String> importOrder;     // the imports in the order the page lists them, the order they are written in
    Map<String, Vector<String>> needs;  // what a support file or an import uses itself (its `uses:` line)
    Map<String, String> spans;      // `## spans`: `line` (before a statement whose source line changes) and `map` (names a source map)
    Vector<PageSection> sections;   // the page's own sections, in order
    Vector<String> problems;        // `file:line: what is wrong`
  };

  POLYIR_API TranslateRules ReadRules(const String& text, const String& file);

  POLYIR_API Vector<Piece> ReadTemplate(const String& text);  // `{part}` holes and literal text: any other brace is text

  POLYIR_API String RuleHead(const Rule& rule);  // `binary(left: i32, op, right)`

  POLYIR_API String RuleKey(const Rule& rule);  // the construct and the role a rule writes: `field.param`

  POLYIR_API bool IsSettingSection(const String& section);  // `## operators`, `## reserved`, `## imports`, `## support`: no rules there

}  // namespace polyir::translate
