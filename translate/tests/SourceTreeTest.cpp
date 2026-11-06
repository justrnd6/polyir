// The translate library knows no AST (polyir/translate/Source.hpp). A made-up language keeps its programs in structs of its
// own, gives its canon in a `.canon` text, and answers the SourceTree questions over its structs. A small page translates
// it. A page for another source is refused, and so is a tree with a node of no form in the canon.
#include "polyir/translate/BuiltTree.hpp"
#include "polyir/translate/Page.hpp"

using namespace polyir;
using namespace polyir::translate;

namespace {

  int failures = 0;

  void expect(bool ok, const String& what) {
    if (ok) {
      return;
    }

    cerr << "FAIL " << what << "\n";
    failures++;
  }

  String firstOf(const Vector<String>& texts) {
    return texts.empty() ? "(none)" : texts[0];
  }

  // ── the made-up language: a program of lines, each saying some numbers ──

  struct SayLine {
    Vector<int> numbers;
  };

  struct SayProgram {
    Vector<SayLine> lines;
  };

  const char* const kSayCanon = R"canon(# Say
source: say

## kinds
block := lines
list := numbers

## statements
program(lines)
say(numbers)

## expressions
number(value: text)
)canon";

  const char* const kSayToText = R"page(# Say to Text
source: say
target: text
style: indent 2

## statements
program(lines) := {lines}
say(numbers) := say {numbers}

## expressions
number(value) := {value}
)page";

  // ── the SourceTree over the made-up structs ──

  // A node is what it is (the program, a line, a number), the line it is on, and its place in the line.
  const uint64_t kProgram = 0;
  const uint64_t kLine = 1;
  const uint64_t kNumber = 2;
  const uint64_t kNotAForm = 3;

  // The program being read and its canon: the functions below answer for them.
  const SayProgram* program = nullptr;
  const Schema* schema = nullptr;

  NodeRef refOf(uint64_t what, uint64_t line, uint64_t place) {
    return NodeRef{what << 40 | line << 20 | place};
  }

  uint64_t whatIs(NodeRef ref) {
    return ref.id >> 40;
  }

  uint64_t lineOf(NodeRef ref) {
    return (ref.id >> 20) & 0xfffff;
  }

  uint64_t placeOf(NodeRef ref) {
    return ref.id & 0xfffff;
  }

  ConstructId constructOf(NodeRef ref) {
    const char* const names[] = {"program", "say", "number", "picture"};

    return ConstructNamed(*schema, names[whatIs(ref)], false);
  }

  // The program has its lines, a line its numbers, and a number its value.
  bool hasPart(NodeRef ref, PartId part) {
    const char* const parts[] = {"lines", "numbers", "value", ""};

    return schema->parts[static_cast<size_t>(part)] == parts[whatIs(ref)];
  }

  String textOf(NodeRef ref, PartId) {
    return to_string(program->lines[lineOf(ref)].numbers[placeOf(ref)]);
  }

  size_t countOf(NodeRef ref, PartId) {
    return whatIs(ref) == kProgram ? program->lines.size() : program->lines[lineOf(ref)].numbers.size();
  }

  NodeRef itemOf(NodeRef ref, PartId, size_t i) {
    return whatIs(ref) == kProgram ? refOf(kLine, i, 0) : refOf(kNumber, lineOf(ref), i);
  }

  // Where a node came from, for a source that knows. Line i of the program came from line 10 × (i + 1) of said.txt, and the
  // program and its numbers have no span.
  SourceSpan spanOf(NodeRef ref) {
    return whatIs(ref) == kLine ? SourceSpan{"said.txt", static_cast<int>(lineOf(ref) + 1) * 10, 1, ""} : SourceSpan{};
  }

  // A tree whose program is a node of `what`.
  SourceTree treeOf(const SayProgram& said, const Schema& rules, uint64_t what) {
    program = &said;
    schema = &rules;
    SourceTree tree{&rules, refOf(what, 0, 0), constructOf, hasPart, textOf, countOf, itemOf};

    return tree;
  }

  const SayProgram kSaid{{SayLine{{1, 2, 3}}, SayLine{{4}}}};

  // ── the tests ──

  void translatedByItsPage() {
    const Schema rules = ReadSchema(kSayCanon, "Say.canon");
    const Target page{"text", "Text.translate", kSayToText};
    const Translation out = TranslateTree(treeOf(kSaid, rules, kProgram), page, {});

    expect(rules.problems.empty(), "Say.canon reads, got " + firstOf(rules.problems));
    expect(out.problems.empty(), "the program translates, got " + firstOf(out.problems));
    expect(out.text == "say 1, 2, 3\nsay 4\n", "each line says its numbers, got " + out.text);
    expect(out.source == "say" && out.target == "text", "the translation names its pair, got " + out.source + " → " + out.target);
  }

  void readThroughItsCanon() {
    const Schema rules = ReadSchema(kSayCanon, "Say.canon");
    const ReadTree read = ReadSourceTree(treeOf(kSaid, rules, kProgram));
    const NodePart* lines = PartNamed(read.root, "lines");

    expect(read.problems.empty() && read.root.construct == "program", "the tree reads as a program");
    expect(lines != nullptr && lines->nodes.size() == 2 && lines->nodes[0].construct == "say", "of two say lines");
    expect(PartNamed(lines->nodes[0], "numbers")->nodes.size() == 3, "the first says three numbers");
  }

  // A source that knows where its nodes came from says so through `spanOf`. With spans asked for, the page's `line` goes
  // before each say line, and each is mapped to its line of said.txt.
  void spansComeThroughSpanOf() {
    const Schema rules = ReadSchema(kSayCanon, "Say.canon");
    SourceTree tree = treeOf(kSaid, rules, kProgram);
    tree.spanOf = spanOf;

    Target page{"text", "Text.translate", String(kSayToText) + "\n## spans\nline := # {file} line {line}\n"};
    page.spans = true;
    page.spanLines = true;
    const Translation out = TranslateTree(tree, page, {});

    expect(out.text == "# said.txt line 10\nsay 1, 2, 3\n# said.txt line 20\nsay 4\n", "a line before each say, got " + out.text);
    expect(out.mappings.size() == 2 && out.mappings[1].line == 4 && out.mappings[1].at.line == 20, "the second say at line 4, from line 20");
  }

  void pageForAnotherSourceRefused() {
    const Schema rules = ReadSchema(kSayCanon, "Say.canon");
    const String polyirPage = String(kSayToText).replace(String(kSayToText).find("source: say"), 11, "source: polyir");
    const Translation out = TranslateTree(treeOf(kSaid, rules, kProgram), Target{"text", "Text.translate", polyirPage}, {});

    expect(out.text.empty() && out.problems.size() == 1, "nothing is written");
    expect(firstOf(out.problems).find("the page translates `polyir`, and the tree is a `say` tree") != String::npos,
      "the page's source must be the tree's, got " + firstOf(out.problems));
  }

  void nodeOfNoFormRefused() {
    const Schema rules = ReadSchema(kSayCanon, "Say.canon");
    const Translation out = TranslateTree(treeOf(kSaid, rules, kNotAForm), Target{"text", "Text.translate", kSayToText}, {});

    expect(out.text.empty(), "nothing is written");
    expect(firstOf(out.problems).find("of no form in say's canon") != String::npos, "a node of no form, got " + firstOf(out.problems));
  }

}  // namespace

int main() {
  translatedByItsPage();
  readThroughItsCanon();
  spansComeThroughSpanOf();
  pageForAnotherSourceRefused();
  nodeOfNoFormRefused();

  if (failures == 0) {
    cout << "SourceTreeTest ok\n";
  }

  return failures == 0 ? 0 : 1;
}
