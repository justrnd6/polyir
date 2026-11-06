// Spans in a translation (core/docs/translate.md, "Spans and source maps"): with spans asked for, each statement and function
// says where it came from. A page's `## spans` puts a `line` before each statement from another source line, with its code when
// there is some. The translation lists where it wrote each one, and those mappings become a Source Map. A `.poly` text's
// `//# from` lines name the source a front end wrote it from, and its code. Without spans asked for, nothing changes.
#include "polyir/Cli.hpp"
#include "polyir/Frontend.hpp"
#include "polyir/Translate.hpp"

using namespace polyir;
using namespace polyir::front;

namespace {

  int failures = 0;

  void expect(bool ok, const String& what) {
    if (ok) {
      return;
    }

    cerr << "FAIL " << what << "\n";
    failures++;
  }

  bool has(const String& text, const String& part) {
    return text.find(part) != String::npos;
  }

  String fileText(const String& path) {
    IFStream in(path);
    OStringStream text;
    text << in.rdbuf();

    return text.str();
  }

  // A page for small functions, whose `## spans` writes a comment naming each statement's line.
  const String kPage = R"page(# Small
source: polyir
target: small
style: indent 2, braces same-line

## declarations
program(support, items) :=
  {support}
  {items}
func(name, params, result, body) := function {name}({params}) {body}
param(name, type) := {name}

## statements
let(name, type, value) := const {name} = {value};
return(value) := return {value};

## expressions
name(name) := {name}
int(value) := {value}
binary(left, op, right) := {left} {op} {right}

## types
i64 := number

## blocks
block(statements, of) := style
body(statements, result) := {statements}

## spans
line := // from {file}:{line}
map := //# sourceMappingURL={map}
)page";

  const String kProgram = "func f(a: i64) -> i64 {\n  let b: i64 = a + 1\n  return b * 2\n}\n";

  // The page with another `line` in its `## spans`.
  String withLine(const String& line) {
    String page = kPage;
    const String old = "line := // from {file}:{line}";

    return page.replace(page.find(old), old.size(), "line := " + line);
  }

  // The program translated by `page`, keeping spans or not, and writing its `line` lines or not.
  Translation translatedBy(const String& page, const String& program, bool spans, bool lines) {
    const FrontResult checked = CheckSource(program);
    Target target{"small", "Small.translate", page};
    target.spans = spans;
    target.spanLines = lines;

    if (!Ok(checked)) {
      cerr << "the test's program does not check: " << checked.errors[0] << "\n";
    }

    return Translate(checked.module, target);
  }

  Translation translated(const String& program, bool spans) {
    return translatedBy(kPage, program, spans, spans);
  }

  // The mapping written at line `line`, or one with no span when there is none.
  SpanMapping mappingAt(const Translation& out, int line) {
    for (const SpanMapping& mapping : out.mappings) {
      if (mapping.line == line) {
        return mapping;
      }
    }

    return SpanMapping{};
  }

  // ── spans in the text ──

  // Without spans asked for, the page's `## spans` writes nothing, and there are no mappings.
  void nothingWithoutSpans() {
    const Translation out = translated(kProgram, false);

    expect(out.problems.empty() && !has(out.text, "// from"), "no span lines, got " + out.text);
    expect(out.mappings.empty() && out.mapLine.empty(), "no mappings and no map line");
  }

  // With spans, a statement from another source line has the page's `line` before it, at its indentation.
  void lineBeforeEachStatement() {
    const Translation out = translated(kProgram, true);

    expect(has(out.text, "// from :1\nfunction f(a) {\n  // from :2\n  const b = a + 1;\n  // from :3\n  return b * 2;\n}"),
      "a line before the function and each statement, got " + out.text);
  }

  // Each statement and the function is mapped: its written line and column, and the line it came from.
  void mappingsNameTheirLines() {
    const Translation out = translated(kProgram, true);
    const SpanMapping let = mappingAt(out, 4);
    const SpanMapping ret = mappingAt(out, 6);

    expect(out.mappings.size() == 3, "the function and two statements, got " + to_string(out.mappings.size()));
    expect(let.column == 2 && let.at.line == 2, "the let at written line 4, column 2, from line 2");
    expect(ret.column == 2 && ret.at.line == 3, "the return at written line 6, from line 3");
  }

  // A `//# from` line says the line after it came from another file: its statement maps there, and the ones after count on.
  void fromLinesNameTheFirstSource() {
    const String program = "//# from Hops.nina:12:5\nfunc f(a: i64) -> i64 {\n  let b: i64 = a + 1\n  return b * 2\n}\n";
    const Translation out = translated(program, true);

    expect(has(out.text, "// from Hops.nina:12\nfunction f(a) {\n  // from Hops.nina:13\n"), "Hops.nina's lines, got " + out.text);
    expect(mappingAt(out, 2).at.column == 5 && mappingAt(out, 2).at.file == "Hops.nina", "the column the `//# from` line gives");
  }

  // A `//# from` line is read whatever the place's file holds, even a colon (`C:/a.nina`); one it cannot read is left out.
  void fromLinesAreRead() {
    const FrontResult checked = CheckSource("//# from C:/src/a.nina:7\n//# from nowhere\nfunc f() -> i64 {\n  return 1\n}\n");
    const Vector<FromLine>& lines = checked.module.fromLines;

    expect(lines.size() == 1 && lines[0].file == "C:/src/a.nina" && lines[0].fromLine == 7, "the place with a colon in its file");
    expect(lines.size() == 1 && lines[0].line == 1 && lines[0].fromColumn == 0, "on line 1, with no column");
  }

  // ── the code ──

  const String kFromNina = "//# from Hops.nina:2:5 action f(a: Int) -> Int {\nfunc f(a: i64) -> i64 {\n"
                           "  //# from Hops.nina:3:5 let b = a + 1\n  let b: i64 = a + 1\n  return b * 2\n}\n";

  // A `//# from` line may give its line's code after the place: `{code?}` writes it, and a line with no code is left out.
  void codeFromTheFromLine() {
    const Translation out = translatedBy(withLine("// {code?}"), kFromNina, true, true);

    expect(has(out.text, "// action f(a: Int) -> Int {\nfunction f(a) {\n  // let b = a + 1\n  const b = a + 1;\n  return b * 2;"),
      "each statement after its Nina code, the return with none, got " + out.text);
  }

  String codeOfLine(const String&, int line) {
    return "line " + to_string(line) + "'s code";
  }

  // A span its source gave no code asks the target's `codeOf` (the command line reads the file).
  void codeFromTheTarget() {
    const FrontResult checked = CheckSource(kProgram);
    Target page{"small", "Small.translate", withLine("// {code?}")};
    page.spans = true;
    page.spanLines = true;
    page.codeOf = codeOfLine;

    const Translation out = Translate(checked.module, page);

    expect(has(out.text, "  // line 2's code\n  const b = a + 1;"), "the code the target gives, got " + out.text);
  }

  // Spans kept for a source map write no `line` lines: the text is the one written without spans.
  void mappingsWithoutLines() {
    const Translation out = translatedBy(kPage, kProgram, true, false);

    expect(out.text == translated(kProgram, false).text, "the same text, got " + out.text);
    expect(out.mappings.size() == 3, "and the mappings, got " + to_string(out.mappings.size()));
  }

  // ── source maps ──

  // Two mappings as Source Map segments: the first line's from the start, the second's against the first.
  void sourceMapOfTwoLines() {
    Translation out{"a\nb\n", {}};
    out.mappings = {SpanMapping{1, 0, SourceSpan{"x.poly", 2, 3, ""}}, SpanMapping{2, 2, SourceSpan{"x.poly", 3, 1, ""}}};
    const String map = SourceMapText(out, "x.js");

    expect(map == "{\"version\":3,\"file\":\"x.js\",\"sources\":[\"x.poly\"],\"names\":[],\"mappings\":\"AACE;EACF\"}\n", "the map, got " + map);
  }

  // The page's `map` line names the map, and a translation without one has none.
  void mapLineNamesTheMap() {
    const Translation out = translated(kProgram, true);

    expect(SourceMapLink(out, "f.small.map") == "//# sourceMappingURL=f.small.map", "the map line, got " + SourceMapLink(out, "f.small.map"));
    expect(SourceMapLink(Translation{"a\n", {}}, "f.map").empty(), "no map line when the page has none");
  }

  // TypeScript names its map as its base page, JavaScript, does.
  void baseGivesItsSpanLines() {
    Optional<Target> ts = FindTarget("ts", "");
    ts->spans = true;
    const Translation out = Translate(CheckSource(kProgram).module, *ts);

    expect(SourceMapLink(out, "f.ts.map") == "//# sourceMappingURL=f.ts.map", "the JavaScript page's map line, got " + out.mapLine);
  }

  // A `## spans` section holds `line` and `map`, and nothing else.
  void spansSectionHoldsTwoLines() {
    Target page{"small", "Small.translate", kPage + "shown := x\n"};
    page.findSchema = FindSchema;
    const Vector<String> problems = TargetProblems(page);

    expect(!problems.empty() && has(problems[0], "a `## spans` section holds `line :=` and `map :=`, and `shown` is neither"),
      "an unknown span line is a problem, got " + (problems.empty() ? "" : problems[0]));
  }

  // `--spans` writes each statement's place and code as a comment, the code read from the program's file.
  void commandLineWritesTheCode() {
    OFStream("spans_code.poly") << kProgram;
    OStringStream out;
    OStringStream err;
    const int status = RunPolyirCli({"spans_code.poly", "--emit-js", "--spans"}, out, err);

    expect(status == 0 && has(out.str(), "  //# from spans_code.poly:2:") && has(out.str(), " let b: i64 = a + 1\n  const b = i64(a + 1n);"),
      "the place and the code over the statement, got " + out.str());
  }

  // `--source-map` writes the map beside the translation, and the page's line naming it ends the translation.
  void commandLineWritesTheMap() {
    OFStream("spans_test.poly") << kProgram;
    OStringStream out;
    OStringStream err;
    const int status = RunPolyirCli({"spans_test.poly", "--emit-js", "--source-map", "--out", "spans_test.js"}, out, err);
    const String mapText = fileText("spans_test.js.map");

    expect(status == 0 && has(mapText, "\"sources\":[\"spans_test.poly\"]"), "the map names the program, got " + mapText + err.str());
  }

  void spansInTheText() {
    nothingWithoutSpans();
    lineBeforeEachStatement();
    mappingsNameTheirLines();
    fromLinesNameTheFirstSource();
    fromLinesAreRead();
  }

  void theCode() {
    codeFromTheFromLine();
    codeFromTheTarget();
    mappingsWithoutLines();
    commandLineWritesTheCode();
  }

  void sourceMaps() {
    sourceMapOfTwoLines();
    mapLineNamesTheMap();
    baseGivesItsSpanLines();
    spansSectionHoldsTwoLines();
    commandLineWritesTheMap();
  }

}  // namespace

int main() {
  spansInTheText();
  theCode();
  sourceMaps();

  if (failures == 0) {
    cout << "TranslateSpansTest ok\n";
  }

  return failures == 0 ? 0 : 1;
}
