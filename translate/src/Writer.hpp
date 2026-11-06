// Writing nodes by a page's rules (Apply.cpp chooses each node's rule, Layout.cpp lays its template out as lines). The Writer is
// the context the rules read: the page, the target's support files, what the nodes written so far needed, and the problems.
#pragma once

#include "polyir/translate/Tree.hpp"
#include "Pages.hpp"

namespace polyir::translate {

  using Lines = Vector<String>;

  // A hole of a template, worked out once: its part, its role, and what of how tightly its operand must bind is known from
  // the template alone. A level next to the hole is `kNodeOperator` when it is the node's own `{op}`, -1 when there is none.
  struct HoleForm {
    String name;
    String role;
    bool inAngles = false;    // inside `<...>`: a type there is a type argument (`List<{elem}>`)
    String separator;         // `{tests| && }`: what joins a list's items, when `joined`
    bool joined = false;
    int fixed = -1;           // its requirement when the template alone says it (quoted, before a `.`, after a prefix), else -1
    bool afterPrefix = false; // `-{x}`, `(int) {x}`: binds above every infix operator
    int before = -1;          // the operator just before it, as a level
    int after = -1;           // the operator just after it, as a level (one more for an operator that does not chain)
  };

  constexpr int kNodeOperator = -2;  // a level that is the node's own operator's (`{left} {op} {right}`)

  // A line of a template, worked out once: its indentation, how many holes it has, its one hole when nothing else but spaces
  // is on it (a hole that may take its part's items one per line), and whether a hole of it asks for items (`{nested?}`).
  struct LineForm {
    String indent;
    size_t holes = 0;
    size_t lone = String::npos;  // that hole's place among the line's pieces
    bool optional = false;
  };

  // A template worked out once, when its page is indexed (Layout.cpp): as one string with a mark per hole, cut at its line
  // breaks, and how tightly its text binds where the template alone says.
  struct TemplateForm {
    String text;                  // the template, each hole one mark
    Vector<size_t> at;            // where each hole is in `text`
    Vector<HoleForm> holes;
    Vector<Vector<Piece>> lines;  // the template cut at its line breaks
    Vector<LineForm> lineForms;   // each of them worked out
    bool onlyOneHole = false;     // the template is one hole and nothing else: it passes a value through
    bool castShaped = false;      // `(int) {value}`: a bracketed type, then its operand
    int loosest = 0;              // the loosest operator outside its brackets and quotes, as a level
    bool opOutside = false;       // the node's own `{op}` is outside its brackets too
  };

  // A page made ready to write by, once (Index.cpp): its rules with its base's, grouped by what they write, its operators'
  // levels, each template worked out, and what is wrong in it by its source's canon.
  struct PageIndex {
    Page page;
    Map<String, Vector<const Rule*>> codeRules;      // by construct and role, in page order
    Map<String, Vector<const Rule*>> typeRules;
    Map<String, Vector<const Rule*>> argumentRules;  // `## type arguments`
    Map<String, int> levels;                         // each operator's level, loosest 0
    int topLevel = 0;                                // above every operator: a prefix operator's operand
    Map<const Vector<Piece>*, TemplateForm> forms;   // each rule's template, `before:` and `hoist:`
    Set<String> ownBlocks;                           // the blocks (`func.body`) a `block` rule of the page's own may lay out
    bool everyBlockOwn = false;                      // a `block` rule of its own names no block: it may lay out any
    Vector<String> problems;
  };

  // What one translation writes with: its page's index, the target's support files, what the nodes written so far needed, and
  // the problems.
  struct Writer {
    const PageIndex* index = nullptr;
    const TranslateRules* rules = nullptr;           // the index's page's rules
    const Target* target = nullptr;
    const Schema* schema = nullptr;                  // the canon of the page's source
    Vector<String> supportUsed;                      // the support files the rules used, in the order first used
    Vector<Lines> before;                            // for each statement being written, the lines its rules put before it
    Vector<String> hoisted;                          // what `hoist:` lines wrote, each once, in the order written
    bool lowerStatement = false;                     // an expression asked for its statement's lower form
    const Vector<Pair<String, String>>* escapes = nullptr;  // the escapes of the rule being written
    Vector<SourceSpan> spans;                        // with spans: each written statement's span, its place named by a mark
    Vector<String> problems;
  };

  // ── Index.cpp ──

  // The page indexed by its source's canon: made once for each page's text, then kept.
  SharedPtr<const PageIndex> IndexOf(const Target& target, const Schema& schema);

  Writer MakeWriter(const PageIndex& index, const Target& target, const Schema& schema);

  // ── Apply.cpp ──

  Node Verbatim(const String& text);  // text written as it is, line by line (a support file, a statement already written)

  const Rule* ChooseRule(Writer& writer, const Node& node, bool asArgument);

  Lines WriteNode(Writer& writer, const Node& node, int required);  // a node's text; in parentheses when it binds looser than `required`

  // A node in a role (`{fields.param}`): by its construct's rule for the role, else the role's `any` rule, else its own rule.
  Lines WriteNodeAs(Writer& writer, const Node& node, int required, const String& role);

  Lines WriteStatement(Writer& writer, const Node& node);  // with the lines its rules put before it

  Lines WriteStatementAs(Writer& writer, const Node& node, const String& role);

  Lines WriteType(Writer& writer, const Node& node, bool asArgument, const String& role);

  String RenamedName(const Writer& writer, const String& name);  // away from reserved words, in the style's case, joined by its paths

  void WriterProblem(Writer& writer, const String& message);

  // ── Spans.cpp ──

  // With spans asked for, the statement's first filled line starts with a mark naming the node's span, read out later.
  Lines WithSpanMark(Writer& writer, const Node& node, Lines lines);

  // The finished text with its marks read out, each into a mapping. The page's `line` lines go before a statement whose source
  // line is not the one before's.
  Translation SpansReadOut(const Writer& writer, const String& text);

  // ── Layout.cpp ──

  TemplateForm FormOfTemplate(const PageIndex& index, const Vector<Piece>& pieces);  // with the index's operator levels

  Lines ExpandTemplate(Writer& writer, const Vector<Piece>& pieces, const Node& node, int required);

  Lines WritePart(Writer& writer, const NodePart& part);  // a part's items one per line, as a hole alone on its line takes them

  int LevelOf(Writer& writer, const Rule& rule, const Node& node);  // how tightly the node's text binds

  Lines Parenthesized(Lines lines);

  String JoinLines(const Lines& lines);

}  // namespace polyir::translate
