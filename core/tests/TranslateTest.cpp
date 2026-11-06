// Translating a program by a target's construct rules (polyir/Translate.hpp, docs/translate.md). Each test writes a few lines of
// .poly and a small page of rules, translates the one by the other, and looks at the text it wrote or the problems it found.
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

  // A page for a small JavaScript-like target, with the rules the tests' programs need.
  const String kTiny = R"page(# Tiny
source: polyir
target: tiny
style: indent 2, braces same-line

## declarations
program(support, items) :=
  {support}
  {items}
module(name, items) := namespace {name} {items}
struct(name, fields, methods, nested, statics) :=
  class {name} {
    {fields}
    {statics}
    {methods}
    {nested}
  }
struct(name, typeParams, fields, methods, nested, statics) :=
  class {name}<{typeParams}> {
    {fields}
    {statics}
    {methods}
    {nested}
  }
field(name, type) := {name};
method(name, params, result, body) := {name}({params}) {body}
static(name, params, result, body) := static {name}({params}) {body}
func(name, params, result, body) := function {name}({params}) {body}
func(name, typeParams, params, result, body) := function {name}<{typeParams}>({params}) {body}
param(name, type) := {name}

## statements
let(name, type, value) := const {name}: {type} = {value};
var(name, type, value) := let {name} = {value};
var(name, type) := let {name};
assign(target, value) := {target} = {value};
expr(value) := {value};
return(value) := return {value};
return := return;
if(cond, then) := if ({cond}) {then}
if(cond, then, else) := if ({cond}) {then} else {else}
if(cond, then, elseIf) := if ({cond}) {then} else {elseIf}
while(cond, body) := while ({cond}) {body}
foreach(name, source, body) := for (const {name} of {source}) {body}

## flow
machine(state, cases) := for (;;) switch ({state}) {cases}
case(state, number, body) := case {number}: {body}
jump(state, number) := {state} = {number}; continue;

## expressions
int(value) := {value}
string(value) := "{value}"
bool(value) := {value}
name(name) := {name}
this := this
binary(left, op, right) := {left} {op} {right}
unary(op, operand) := {op}{operand}
call(callee, args) := {callee}({args})
call(callee, typeArgs, args) := {callee}<{typeArgs}>({args})
methodcall(object, name, args) := {object}.{name}({args})
get(object, name) := {object}.{name}
index(object, index) := {object}[{index}]
new(name, values) := new {name}({values})
new(name, typeArgs, values) := new {name}<{typeArgs}>({values})
init(name, value) := {value}
list(values) := [{values}]
closure(params, result, body) := ({params}) => {body}
some(type, value) := {value}
none(type) := null

## builtins
print(value) := console.log({value})
len(object) := {object}.length
str(value) := String({value})
isNone(object) := {object} === null
unwrap(object) := {object}
fail(value) := panic({value})

## types
i64 := number
bool := boolean
string := string
double := number
void := void
struct(name) := {name}
instance(name, args) := {name}<{args}>
param(name) := {name}
array(elem) := Array<{elem}>
optional(elem) := {elem} | null
closure(params, result) := ({params}) => {result}

## blocks
body(statements, result) := {statements}
)page";

  // A page made in the test, which finds polyir's canon as a built-in page does: `go` is Go.translate.
  Target madePage(const String& name, const String& text) {
    Target page{name, String(1, static_cast<char>(toupper(name[0]))) + name.substr(1) + ".translate", text};
    page.findSchema = FindSchema;

    return page;
  }

  // The line a rule added after `page` is on.
  int lineAfter(const String& page, int added) {
    return static_cast<int>(count(page.begin(), page.end(), '\n')) + added;
  }

  // The program translated by `page`.
  Translation translated(const String& page, const String& program) {
    const FrontResult checked = CheckSource(program);

    if (!Ok(checked)) {
      cerr << "the test's program does not check: " << checked.errors[0] << "\n";
    }

    return Translate(checked.module, Target{"tiny", "Tiny.translate", page});
  }

  String textOf(const String& page, const String& program) {
    const Translation out = translated(page, program);

    for (const String& problem : out.problems) {
      cerr << "  problem: " << problem << "\n";
    }

    return out.text;
  }

  // ── rules and holes ──

  // A struct becomes a class with one line per field.
  void structBecomesClass() {
    const String js = textOf(kTiny, "struct Point(x: i64, y: i64)\n");

    expect(has(js, "class Point {\n  x;\n  y;\n}\n"), "a struct is a class with a line per field, got " + js);
  }

  // Of two rules for `if`, the one whose parts the construct has is used.
  void ruleChosenByParts() {
    const String poly = "func f(a: i64) -> i64 {\n  if (a > 1) {\n    return 1\n  }\n  if (a > 2) {\n    print(\"x\")\n  } else {\n    print(\"y\")\n  }\n  return 0\n}\n";
    const String js = textOf(kTiny, poly);

    expect(has(js, "  if (a > 1) {\n    return 1;\n  }\n"), "an if without an else, got " + js);
    expect(has(js, "  } else {\n    console.log(\"y\");\n  }\n"), "an if with an else");
  }

  // A rule can ask for a part's type: text equality is `.equals`, number equality `==`.
  void ruleChosenByPartTypes() {
    const String page = kTiny + "\n## expressions\nbinary(left: string, op: ==, right) := {left}.equals({right})\n";
    const String js = textOf(page, "func f(a: string, n: i64) -> bool {\n  return a == \"x\" && n == 2\n}\n");

    expect(has(js, "return a.equals(\"x\") && n == 2;"), "the string rule for strings, the plain one for numbers, got " + js);
  }

  // Every hole is filled by its own construct's rule, down to names and numbers.
  void holesFilledByTheirRules() {
    const String js = textOf(kTiny, "func sum(xs: array<i64>) -> i64 {\n  var total: i64 = 0\n  for (x in xs) {\n    total = total + x * 2\n  }\n  return total\n}\n");

    expect(has(js, "function sum(xs) {\n  let total = 0;\n  for (const x of xs) {\n    total = total + x * 2;\n  }\n  return total;\n}"), "the plan's example, got " + js);
  }

  // Parentheses only where an inner operator binds more loosely than the one outside it.
  void parenthesesByPrecedence() {
    const String js = textOf(kTiny, "func f(a: i64, b: i64, c: i64) -> i64 {\n  return (a + b) * c + a * b - (b - c)\n}\n");

    expect(has(js, "return (a + b) * c + a * b - (b - c);"), "parentheses kept where they are needed and only there, got " + js);
  }

  // `blocks colon` writes a block as `:` and indented lines, and an empty one with what `empty` says.
  void colonBlocks() {
    const String page = "# Py\nsource: polyir\ntarget: py\nstyle: indent 4, blocks colon, empty pass\n\n## declarations\nprogram(support, items) := {items}\n"
                        "func(name, params, result, body) := def {name}({params}){body}\nparam(name, type) := {name}\n\n"
                        "## statements\nif(cond, then) := if {cond}{then}\nreturn := return\nreturn(value) := return {value}\n\n"
                        "## expressions\nname(name) := {name}\nint(value) := {value}\nbinary(left, op, right) := {left} {op} {right}\n\n"
                        "## blocks\nbody(statements, result) := {statements}\n";
    const String py = textOf(page, "func f(a: i64) -> i64 {\n  if (a > 1) {\n    return a\n  }\n  return 0\n}\nfunc g() -> void {\n}\n");

    expect(has(py, "def f(a):\n    if a > 1:\n        return a\n    return 0\n"), "colon blocks, got " + py);
    expect(has(py, "def g():\n    pass\n"), "an empty block holds `pass`");
  }

  // A `before:` line under a rule is written ahead of the statement that uses the rule.
  void beforeLines() {
    const String page = kTiny + "\n## expressions\nclosure(params, result, body, name) := {name}\n  before: function {name}({params}) {body}\n"
                                "invoke(callee, args) := {callee}({args})\n";
    const String js = textOf(page, "func f() -> i64 {\n  var n: i64 = 1\n  let add: closure(i64) -> i64 = closure(k: i64) -> i64 {\n    n = n + k\n    return n\n  }\n  return add(2)\n}\n");

    expect(has(js, "  function closure1(k) {\n    n = n + k;\n    return n;\n  }\n  const add: (number) => number = closure1;\n"), "the closure's function comes first, got " + js);
  }

  // A support file a rule uses is written once, at the program's `{support}` hole.
  void supportWrittenOnce() {
    const String page = kTiny + "\n## builtins\nhash(value) := polyHash({value})\n  uses: hashing\n\n## support\nhashing := runtime/Hash.js\n";
    const Module module = CheckSource("func f(a: string, b: string) -> i64 {\n  return hash(a) + hash(b)\n}\n").module;
    const Translation out = Translate(module, Target{"tiny", "Tiny.translate", page, {{"runtime/Hash.js", "function polyHash(v) { return 7; }\n"}}});

    expect(out.problems.empty() && out.text.find("function polyHash") == 0, "the support file starts the program, got " + out.text);
    expect(out.text.find("function polyHash", 1) == String::npos, "and is written once");
  }

  // ── what stops a translation ──

  // A rule still `todo` fails the translation, and every such rule the program reaches is listed with its line.
  void todoFailsTheTranslation() {
    const String page = kTiny + "\n## expressions\nint(value) := todo\nbinary(left, op, right) := todo\n";
    const Translation out = translated(page, "func f() -> i64 {\n  return 1 + 2\n}\n");

    const String line = "Tiny.translate:" + to_string(lineAfter(kTiny, 3)) + ": `int(value)` is todo";

    expect(out.text.empty(), "nothing is written, got " + out.text);
    expect(out.problems.size() == 2 && has(out.problems[0], line), "the int rule is listed with its line, got " + (out.problems.empty() ? "" : out.problems[0]));
    expect(out.problems.size() == 2 && has(out.problems[1], "`binary(left, op, right)` is todo"), "and so is the binary rule");
  }

  // `unsupported "why"` fails it too, and says why.
  void unsupportedSaysWhy() {
    const String page = kTiny + "\n## statements\nthrow(value) := unsupported \"Tiny has no exceptions\"\n";
    const Translation out = translated(page, "func f() -> i64 {\n  throw 3\n}\n");

    expect(out.problems.size() == 1 && has(out.problems[0], "is unsupported: Tiny has no exceptions"), "the reason is given");
  }

  // `unimplemented "why"` (understood, not written yet) fails it too, says why, and is counted apart from `todo`.
  void unimplementedSaysWhy() {
    const String page = kTiny + "\n## statements\nthrow(value) := unimplemented \"needs the error type first\"\n";
    const Translation out = translated(page, "func f() -> i64 {\n  throw 3\n}\n");

    expect(out.problems.size() == 1 && has(out.problems[0], "`throw(value)` is unimplemented: needs the error type first"), "the reason is given");
    expect(CountRules(madePage("tiny", page)).unimplemented == 1, "the rule is counted as unimplemented");
  }

  // A page names its source and its target, and its translations name both.
  void pageNamesItsPair() {
    const Translation out = translated(kTiny, "func f() -> i64 {\n  return 1\n}\n");
    const Vector<String> noTarget = TargetProblems(madePage("tiny", "# Tiny\nsource: polyir\n"));
    const Vector<String> noSource = TargetProblems(madePage("tiny", "# Tiny\ntarget: tiny\n"));
    const Vector<String> otherSource = TargetProblems(madePage("tiny", "# Tiny\nsource: nina-lite\ntarget: tiny\n"));

    expect(out.source == "polyir" && out.target == "tiny", "the translation names polyir and tiny, got " + out.source + " → " + out.target);
    expect(noTarget.size() == 1 && has(noTarget[0], "Tiny.translate: the page names no `target:`"), "a page names its target");
    expect(noSource.size() == 1 && has(noSource[0], "Tiny.translate: the page names no `source:`"), "a page names its source");
    expect(otherSource.size() == 1 && has(otherSource[0], "no canon for the source `nina-lite`"), "its source has a canon");
  }

  // A section a page's source does not have is a problem at its line.
  void unknownSectionIsAProblem() {
    const Vector<String> problems = TargetProblems(madePage("tiny", "# Tiny\nsource: polyir\ntarget: tiny\n\n## shapes\n"));

    expect(problems.size() == 1 && has(problems[0], "Tiny.translate:5: there is no section `shapes` in polyir's canon"),
      "the section is named, got " + (problems.empty() ? "" : problems[0]));
  }

  // A rule naming a part its construct does not have is a problem on the page, at its line.
  void unknownPartIsAProblem() {
    const Vector<String> problems = TargetProblems(madePage("tiny", kTiny + "\n## statements\nif(cond, then, otherwise) := x\n"));
    const String line = "Tiny.translate:" + to_string(lineAfter(kTiny, 3)) + ": `if` has no part `otherwise`";

    expect(problems.size() == 1 && has(problems[0], line), "the unknown part is named, got " + (problems.empty() ? "" : problems[0]));
  }

  // ── pages ──

  // A new page lists every construct, each `todo` but blocks, laid out by its style: nothing is missing, nothing else written.
  void newTargetListsEveryConstruct() {
    const Target page = madePage("go", NewTargetText("Go", PolyirSchema()));
    const RuleCounts counts = CountRules(page);

    expect(MissingConstructs(page).empty() && TargetProblems(page).empty(), "a new page covers every construct");
    expect(counts.written == 1 && counts.todo > 150, "every rule is todo but the block's, got " + to_string(counts.todo));
    expect(has(page.text, "block(statements, of)  ") && has(page.text, ":= style\n"), "blocks are laid out by the style");
    expect(has(page.text, "if(cond, then, elseIf)") && has(page.text, "instance(name, args)"), "forms and types are there");
  }

  // Updating a page adds a `todo` for each construct it lacks (blocks by the style) and keeps what it has.
  void updateAddsWhatIsMissing() {
    const Target page = madePage("go", "# Go\nsource: polyir\ntarget: go\n\n## expressions\nint(value) := {value}\n");
    const Target updated = madePage("go", UpdatedTargetText(page));

    expect(MissingConstructs(updated).empty(), "nothing is missing after the update");
    expect(has(updated.text, "int(value) := {value}\n") && CountRules(updated).written == 2, "the written rule is kept, beside the block's");
  }

  // A `lower` rule writes the construct as the HIR has it: here the loop over a list's indices.
  void lowerWritesTheHirForm() {
    const String page = kTiny + "\n## statements\nforeach(name, source, body) := lower\n";
    const String js = textOf(page, "func f(xs: array<i64>) -> void {\n  for (x in xs) {\n    print(str(x))\n  }\n}\n");

    expect(has(js, "  const forList_2_8: Array<number> = xs;\n") && has(js, "  while (forIndex_2_8 < forList_2_8.length) {\n"), "a while over the indices, got " + js);
    expect(!has(js, "for (const"), "and no for-of");
  }

  // ── structure carried ──

  // A module stays a block, and a struct declared in a struct stays in it: `Builder` inside Shape, `Shape.Builder` outside.
  void structureIsCarried() {
    const String poly = "module Geometry {\n  struct Point(x: i64)\n}\n@Builder\nstruct Shape(side: i64)\n"
                        "func main() -> void {\n  let p: Geometry.Point = Geometry.Point(x: 1)\n  let s: Shape = Shape.builder().side(2).build()\n  print(str(p.x + s.side))\n}\n";
    const String js = textOf(kTiny, poly);

    expect(has(js, "namespace Geometry {\n  class Point {\n    x;\n  }\n}\n"), "the module is a namespace around its struct, got " + js);
    expect(has(js, "  static builder() {\n    return new Builder(") && has(js, "  class Builder {\n"), "the builder is inside Shape");
    expect(has(js, "const p: Geometry.Point = new Geometry.Point(1);") && has(js, "Shape.builder().side(2).build()"), "named by path outside");
  }

  // A generic that works for every type is written once, and its uses as `Box<i64>`; `generics copies` writes the copies.
  void genericsAreCarried() {
    const String poly = "struct Box<T>(value: T)\nfunc main() -> void {\n  let b: Box<i64> = Box(value: 1)\n  print(str(b.value))\n}\n";
    const String once = textOf(kTiny, poly);
    const String copies = textOf("# Tiny\nsource: polyir\ntarget: tiny\nstyle: generics copies\n" + kTiny.substr(kTiny.find("\n## declarations")), poly);

    expect(has(once, "class Box<T> {\n  value;\n}") && has(once, "const b: Box<number> = new Box<number>(1);"), "Box<T> once, got " + once);
    expect(has(copies, "class Box_i64 {") && has(copies, "const b: Box_i64 = new Box_i64(1);"), "the copy, got " + copies);
  }

  // Labels and gotos become a machine: each block a case, each goto a jump.
  void labelsBecomeAMachine() {
    const String poly = "func gcd(a0: i64, b0: i64) -> i64 {\n  goto loop(a0, b0)\nlabel loop(a: i64, b: i64):\n  branch b == 0 ? done(a) : body(a, b)\n"
                        "label body(a: i64, b: i64):\n  let r: i64 = a % b\n  goto loop(b, r)\nlabel done(result: i64):\n  return result\n}\n";
    const String js = textOf(kTiny, poly);

    expect(has(js, "  for (;;) switch (label) {\n    case 0: {\n      a = a0;\n      b = b0;\n      label = 1; continue;\n    }\n"), "block 0, got " + js);
    expect(has(js, "      a = b;\n      b = r;\n      label = 1; continue;\n"), "a goto sets the label's parameters, then jumps");
  }

  // A variable one block declares and another reads is declared before the machine, as each case is a block of its own.
  void labelsShareAVariable() {
    const String poly = "func f(n: i64) -> i64 {\n  let x: i64 = n * 2\n  goto done()\nlabel done():\n  return x\n}\n";
    const String js = textOf(kTiny, poly);

    expect(has(js, "  let label = 0;\n  let x;\n") && has(js, "    case 0: {\n      x = n * 2;\n"), "x before the machine, set in block 0, got " + js);
  }

  // .poly keeps a loop's counter after the loop: the `for` then has the fact `usedAfter`.
  void counterUsedAfterItsLoop() {
    const String page = kTiny + "\n## statements\nfor(counters, cond, steps, body) := for (let {counters}; {cond}; {steps}) {body}\n"
                                "for(counters, cond, steps, body, usedAfter) := let {counters};\ncounter(name, type, value) := {name} = {value}\n"
                                "step(name, value) := {name} = {value}\n";
    const String js = textOf(page, "func f(n: i64) -> i64 {\n  for (i = 0; i < n; i = i + 1) {\n  }\n  for (j = 0; j < n; j = j + 1) {\n  }\n  return j\n}\n");

    expect(has(js, "for (let i = 0; i < n; i = i + 1) {}") && has(js, "let j = 0;"), "only j is used after its loop, got " + js);
  }

  // A host type and a host function are written by the target's own binding, and are not declared again.
  void hostBindings() {
    const String poly = "extern type ArrayList<T> = js \"List\"\nextern func newList() -> ArrayList<i64> = js \"makeList()\"\n"
                        "extern func abs(x: i64) -> i64 = js \"Math.abs($0)\"\nfunc main() -> void {\n  let xs: ArrayList<i64> = newList()\n  print(str(abs(0 - 3)))\n}\n";
    const String page = kTiny + "\n## expressions\nhostcall(binding, args) := {binding}\n\n## types\nhost(name, binding, args) := {binding}<{args}>\n";
    const Translation out = Translate(CheckSource(poly).module, Target{"js", "Tiny.translate", page});

    expect(has(out.text, "const xs: List<number> = makeList();"), "the host type and call by their bindings, got " + out.text);
    expect(has(out.text, "console.log(String(Math.abs(0 - 3)));"), "the binding's $0 is the argument");
  }

  // ── the command line ──

  // `--emit-<lang> --target-rules PATH` writes the program by the page at PATH.
  void commandLineEmits() {
    OFStream("translate_test.poly") << "func main() -> void {\n  print(\"hi\")\n}\n";
    OFStream("translate_test.translate") << kTiny;
    OStringStream out;
    OStringStream err;
    const int status = RunPolyirCli({"translate_test.poly", "--emit-tiny", "--target-rules", "translate_test.translate"}, out, err);

    expect(status == 0 && out.str() == "function main() {\n  console.log(\"hi\");\n}\n", "the program in Tiny, got " + out.str() + err.str());
  }

  void rulesAndHoles() {
    structBecomesClass();
    ruleChosenByParts();
    ruleChosenByPartTypes();
    holesFilledByTheirRules();

    parenthesesByPrecedence();
    colonBlocks();
    beforeLines();
    supportWrittenOnce();
  }

  void pagesAndProblems() {
    todoFailsTheTranslation();
    unsupportedSaysWhy();
    unimplementedSaysWhy();

    pageNamesItsPair();
    unknownSectionIsAProblem();
    unknownPartIsAProblem();

    newTargetListsEveryConstruct();
    updateAddsWhatIsMissing();
    lowerWritesTheHirForm();
  }

}  // namespace

int main() {
  rulesAndHoles();
  pagesAndProblems();

  structureIsCarried();
  genericsAreCarried();
  labelsBecomeAMachine();
  labelsShareAVariable();

  counterUsedAfterItsLoop();
  hostBindings();
  commandLineEmits();

  if (failures == 0) {
    cout << "TranslateTest ok\n";
  }

  return failures == 0 ? 0 : 1;
}
