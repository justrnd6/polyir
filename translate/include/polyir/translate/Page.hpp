// A target's page (core/docs/translate.md): its rules, `construct(parts) := template`, for the constructs of one source, and a
// source's tree written by them. translate knows no source: a source hands in its tree (Source.hpp) and gets the target's text
// back. A page names both languages (`source: polyir`, `target: js`), and has a rule, or a word for why it has none, for every
// a form of its source's canon.
//
//   const Translation out = TranslateTree(tree, page);
//   if (out.problems.empty()) { cout << out.text; }
#pragma once

#include "polyir/translate/Rules.hpp"
#include "polyir/translate/Source.hpp"
#include "polyir/translate/Tree.hpp"

namespace polyir::translate {

  // A target's rules: its name (`js`), where the page came from (for problems), the page, and its support files by their path
  // under the page's folder (`runtime/Equals.js`). What the page names is beside a page read from disk, else found by the
  // functions a program gives (the pages and canons it has built in): the page it starts from (`base: Js`), and its
  // source's canon (`source: polyir`).
  struct Target {
    String name;
    String file;
    String text;
    Map<String, String> files{};
    bool isBuiltIn = false;
    Function<Optional<Target>(const String&)> findBase{};
    Function<Optional<Schema>(const String&)> findSchema{};
    const TranslateRules* rules = nullptr;  // the page compiled ahead (polyir-translatec): used instead of reading its text
    bool spans = false;      // keep the tree's spans: where each statement was written (Translation.mappings)
    bool spanLines = false;  // and write the page's `line` lines before statements from another source line
    Function<String(const String&, int)> codeOf{};  // a file's line of text, for a span its source gave no code (optional)
  };

  // A place in the written text (its line from 1, its column from 0) where a statement or declaration with a span starts.
  struct SpanMapping {
    int line = 0;
    int column = 0;
    SourceSpan at;
  };

  // The file in the target's language, or the problems that stopped it (each `file:line: ...`, the text then empty), and the
  // extension of the file it is: the page's `library` extension for a file with no `main` of its own (`hpp`), else its `extension`.
  // It names the languages it is from and in (`polyir`, `js`).
  struct Translation {
    String text;
    Vector<String> problems;
    String extension{};
    String source{};
    String target{};
    Vector<SpanMapping> mappings{};  // with spans: where each statement and declaration that has one was written
    String mapLine{};                // with spans: the page's `map` line, its `{map}` to be filled (SourceMapLink)
  };

  // How many of a page's rules are written, still `todo`, `unimplemented`, `unsupported`, or written as their lower form.
  struct RuleCounts {
    int written = 0;
    int todo = 0;
    int unimplemented = 0;
    int unsupported = 0;
    int lowered = 0;
  };

  // What a source asks of a page before it builds its tree: the languages, the style, and the target's name then its base's
  // (`ts`, `js`), as host bindings name them.
  struct PageSettings {
    String source;
    String target;
    Style style;
    Vector<String> names;
  };

  // ── pages ──

  POLYIR_API Optional<Target> ReadPage(const String& path);  // a .translate page on disk and the files under its runtime/ folder

  POLYIR_API Optional<Target> PageInFolder(const String& folder, const String& name);  // <folder>/<name>/<Name>.translate

  POLYIR_API PageSettings PageSettingsOf(const Target& target);

  // The canon of the page's source: a `.canon` file beside the page (in its folder or the one above), else `findSchema`'s.
  POLYIR_API Optional<Schema> SchemaOf(const Target& target);

  POLYIR_API String NewTargetText(const String& name, const Schema& source);  // a page with a `todo` rule for each form of its canon

  POLYIR_API String UpdatedTargetText(const Target& target);  // the page with a `todo` rule added for each construct it lacks

  POLYIR_API Vector<String> MissingConstructs(const Target& target);  // the forms of its source's canon no rule of the page covers

  POLYIR_API Vector<String> TargetProblems(const Target& target);  // what is wrong in the page itself, by its source's canon

  POLYIR_API RuleCounts CountRules(const Target& target);

  POLYIR_API String TargetExtension(const Target& target);  // the extension of a program's file: its page's `extension`, else its name

  // ── translating ──

  // The program a source's tree holds, written by the target's page, which must be a page for the tree's source. `found` are
  // the problems the source met making the tree.
  POLYIR_API Translation TranslateTree(const SourceTree& tree, const Target& target, const Vector<String>& found);

  // A tree the source built itself (BuiltTree.hpp), fitted to its schema in place and written. It is what TranslateTree writes
  // of TreeOver(root), without reading it into a copy.
  POLYIR_API Translation TranslateBuilt(Node root, const Schema& schema, const Target& target, const Vector<String>& found);

  // ── spans (Spans.cpp) ──

  // The translation's mappings as a Source Map (version 3) for the written file `file` (`race.js`). Its sources are the files
  // the spans name, and it has a line of segments for each written line.
  POLYIR_API String SourceMapText(const Translation& translation, const String& file);

  // The page's `map` line naming the map file `mapName` (`//# sourceMappingURL=race.js.map`), "" when the page has none.
  POLYIR_API String SourceMapLink(const Translation& translation, const String& mapName);

}  // namespace polyir::translate
