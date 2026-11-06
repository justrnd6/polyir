// The rule features a page uses where a plain template is not enough (docs/translate.md): roles, patterns of several words, the
// `empty` pattern, holes whose line needs items, escapes, a base page, imports by use, one file per .poly file, the type of the `main` fact, and literals written in the
// type of their place. Each test translates a few lines of .poly by a small page and looks at the text.
#include "polyir/Frontend.hpp"
#include "polyir/Translate.hpp"

using namespace polyir;
using namespace polyir::front;
using std::filesystem::create_directories;
using std::filesystem::temp_directory_path;

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

  // The rules every test's page starts with: enough for small functions.
  const String kStart = R"page(# Small
source: polyir
target: small
style: indent 2, braces same-line

## declarations
program(support, items) :=
  {support}
  {items}
func(name, params, result, body) := function {name}({params}) {body}
param(name, type) := {name}
field(name, type) := {name};

## statements
let(name, type, value) := const {name} = {value};
return(value) := return {value};
expr(value) := {value};

## expressions
name(name) := {name}
this := this
int(value) := {value}
string(value) := "{value}"
binary(left, op, right) := {left} {op} {right}
index(object, index) := {object}[{index}]
get(object, name) := {object}.{name}
new(name, values) := new {name}({values})
init(name, value) := {value}
call(callee, args) := {callee}({args})

## builtins
print(value) := console.log({value})

## types
i64 := number
u32 := number
string := string
void := void
struct(name) := {name}
array(elem) := Array

## blocks
block(statements, of) := style
body(statements, result) := {statements}
)page";

  Translation translated(const Target& target, const String& program) {
    const FrontResult checked = CheckSource(program);

    if (!Ok(checked)) {
      cerr << "the test's program does not check: " << checked.errors[0] << "\n";
    }

    return Translate(checked.module, target);
  }

  // The program translated by `page`, or its problems.
  String textOf(const String& page, const String& program) {
    const Translation out = translated(Target{"small", "Small.translate", page}, program);
    String problems;

    for (const String& problem : out.problems) {
      problems += problem + "\n";
    }

    return out.problems.empty() ? out.text : problems;
  }

  // ── roles ──

  // `{fields.param}` and `{fields.init}` write each field twice: as a constructor's parameter and as the line that keeps it.
  void rolesWriteAPartTwice() {
    const String page = kStart + R"page(
## declarations
struct(name, fields, methods, nested, statics) :=
  class {name} {
    constructor({fields.param}) {
      {fields.init}
    }
  }
field.param(name, type) := {name}
field.init(name, type) := this.{name} = {name};
)page";
    const String js = textOf(page, "struct Point(x: i64, y: i64)\n");

    expect(has(js, "class Point {\n  constructor(x, y) {\n    this.x = x;\n    this.y = y;\n  }\n}\n"), "a field as a parameter and as a line, got " + js);
  }

  // A role's `any` rule writes each construct that has no rule of its own in the role. An index is a Number, a literal as it is.
  void anyRuleWritesTheRest() {
    const String page = kStart + R"page(
## expressions
index(object, index) := {object}[{index.number}]
any.number(value) := Number({value})
int.number(value) := {value}
)page";
    const String js = textOf(page, "func f(xs: array<i64>, i: i64) -> i64 {\n  return xs[i] + xs[0]\n}\n");

    expect(has(js, "return xs[Number(i)] + xs[0];"), "a name through `any`, the literal by its own rule, got " + js);
  }

  // ── patterns ──

  // A pattern of several words matches any of them: `op: + -`.
  void patternOfSeveralWords() {
    const String page = kStart + "\n## expressions\nbinary(left, op: + -, right) := wrap({left} {op} {right})\n";
    const String js = textOf(page, "func f(a: i64, b: i64) -> i64 {\n  return a * (a + b) - b\n}\n");

    expect(has(js, "return wrap(a * wrap(a + b) - b);"), "+ and - wrapped, * not, got " + js);
  }

  // `empty` matches a part with no items: a struct with no fields is written without a constructor.
  void emptyPatternMatchesNoItems() {
    const String page = kStart + R"page(
## declarations
struct(name, fields, methods, nested, statics) := class {name} { constructor({fields.param}) {} }
struct(name, fields: empty, methods, nested, statics) := class {name} {}
field.param(name, type) := {name}
)page";
    const String js = textOf(page, "struct Point(x: i64)\nstruct Nothing {\n  func one() -> i64 {\n    return 1\n  }\n}\n");

    expect(has(js, "class Point { constructor(x) {} }") && has(js, "class Nothing {}"), "the empty rule for no fields, got " + js);
  }

  // ── layout ──

  // `{nested?}`: the line is written only when the part has items, so a struct with nothing nested gets no namespace.
  void optionalHoleDropsItsLine() {
    const String page = kStart + R"page(
## declarations
struct(name, fields, methods, nested, statics) :=
  class {name} {}
  namespace {name} {nested?}
)page";
    const String js = textOf(page, "struct Point(x: i64) {\n  struct Step(dx: i64)\n}\n");

    expect(has(js, "class Point {}\nnamespace Point {\n  class Step {}\n}"), "the namespace for Point, got " + js);
    expect(!has(js, "namespace Step"), "none for Step, which has nothing nested");
  }

  // An empty list between `<` and `>` takes them away: one rule writes `Box<T>` and `Point`.
  void emptyAnglesGo() {
    const String page = kStart + R"page(
## declarations
struct(name, typeParams, fields, methods, nested, statics) := class {name}<{typeParams}> {}
## types
param(name) := {name}
)page";
    const String js = textOf(page, "struct Box<T>(value: T)\nstruct Point(x: i64)\nfunc f(b: Box<i64>) -> i64 {\n  return 1\n}\n");

    expect(has(js, "class Box<T> {}") && has(js, "class Point {}"), "brackets only for the generic, got " + js);
  }

  // `escape:` under a rule replaces text in the text parts it writes: a backtick inside a template literal.
  void escapesInText() {
    const String page = kStart + R"page(
## expressions
interp(pieces) := `{pieces}`
text(value) := {value}
  escape: ` as \`
  escape: ${ as \${
hole(value) := ${{value}}
)page";
    const String js = textOf(page, "func f(x: i64) -> string {\n  return \"`a` ${x} \\${b}\"\n}\n");

    expect(has(js, "return `\\`a\\` ${x} \\${b}`;"), "the backtick and the text `${` escaped, got " + js);
  }

  // ── pages and facts ──

  // `base: Js` starts with the built-in JavaScript page; a rule of the page replaces the base's rule alike. A page named for no
  // host binding of its own uses its base's (`ts` uses `js` bindings).
  void basePageComesFirst() {
    const String page = "# Ts\nsource: polyir\ntarget: ts\nbase: Js\nstyle: indent 2, braces same-line, qualify full\n\n## builtins\nprint(value) := alert({value})\n";
    const String poly = "extern func abs(x: i64) -> i64 = js \"Math.abs($0)\"\nfunc main() -> void {\n  print(str(abs(2)))\n}\n";
    const Translation out = translated(Target{"ts", "Ts.translate", page}, poly);

    expect(out.problems.empty() && has(out.text, "function main() {\n  alert(String(Math.abs(2n)));\n}"), "the base's rules, the page's print, got " + out.text);
    expect(has(textOf("# Lost\nsource: polyir\ntarget: lost\nbase: Nowhere\n", ""), "there is no page `Nowhere`"), "a base that is not there is a problem");
  }

  // An import is written only when a rule or a support file that uses it is, in the order the page lists the imports. A support
  // file brings what its own `uses:` line names.
  void importsComeWithWhatUsesThem() {
    const String page = kStart + R"page(
## declarations
program(support, items) :=
  {imports}

  {support}

  {items}

## builtins
print(value) := log({value})
  uses: logger, first

## imports
first := import first;
second := import second;
unused := import unused;

## support
logger := runtime/Logger.js
  uses: second
)page";
    const Target target{"small", "Small.translate", page, {{"runtime/Logger.js", "function log(text) {}\n"}}};
    const String js = translated(target, "func main() -> void {\n  print(\"hi\")\n}\n").text;

    expect(has(js, "import first;\nimport second;\n\nfunction log(text) {}\n"), "the imports in the page's order, then the support, got " + js);
    expect(!has(js, "unused"), "an import nothing uses is not written, got " + js);
  }

  // One .poly file is one file of the target. A file importing another writes a `use` of it, not its declarations. A file with no
  // `main` of its own is a library, which shows its names (`exports`).
  void oneFileIsOneFile() {
    const String page = kStart + R"page(
## declarations
program(support, items, exports) :=
  {items}
  export {exports}
use(name, path, names) := import {names} from {path}
)page";
    const Path folder = temp_directory_path() / "polyir-one-file";
    create_directories(folder);
    OFStream((folder / "Lib.poly").string()) << "func twice(x: i64) -> i64 {\n  return x * 2\n}\n";
    OFStream((folder / "App.poly").string()) << "import \"Lib.poly\"\nfunc main() -> i64 {\n  return twice(3)\n}\n";

    const Target target{"small", "Small.translate", page};
    const String app = Translate(CheckFile((folder / "App.poly").string()).module, target).text;
    const String lib = Translate(CheckFile((folder / "Lib.poly").string()).module, target).text;

    expect(has(app, "import twice from Lib\n") && !has(app, "function twice"), "the program uses the library, got " + app);
    expect(has(lib, "function twice(x) {") && has(lib, "export twice"), "the library writes and shows its function, got " + lib);
  }

  // The fact `main` has main's result type: a rule can write a program whose main gives a number differently.
  void mainFactHasItsType() {
    const String page = kStart + R"page(
## declarations
program(support, items, main) :=
  {items}
  main();
program(support, items, main: int) :=
  {items}
  process.exitCode = main();
)page";

    expect(has(textOf(page, "func main() -> void {\n  print(\"x\")\n}\n"), "\nmain();"), "a void main is called");
    expect(has(textOf(page, "func main() -> i64 {\n  return 3\n}\n"), "process.exitCode = main();"), "a main that gives a number");
  }

  // A whole-number literal is written in the type of its place: `4000000000` held in a u32, and `2` beside one.
  void literalsTakeTheirPlacesType() {
    const String page = kStart + "\n## expressions\nint(value: i64) := {value}n\n";
    const String js = textOf(page, "func f() -> i64 {\n  let big: u32 = 4000000000\n  let half: u32 = big / 2\n  return 7\n}\n");

    expect(has(js, "const big = 4000000000;") && has(js, "const half = big / 2;"), "u32 literals are not i64 ones, got " + js);
    expect(has(js, "return 7n;"), "the result's literal is an i64");
  }

  // ── facts for closures and locals ──

  const String kCounter = "func f() -> i64 {\n  var count: i64 = 0\n  let inc: closure() -> i64 = closure() -> i64 {\n    count = count + 1\n"
                          "    return count\n  }\n  return inc()\n}\n";

  // A closure that gives an outer variable a new value has the fact `outer`; `before:` lines keep their indentation.
  void closureNamesWhatItChanges() {
    const String page = kStart + R"page(
## statements
var(name, type, value) := {name} = {value}
assign(target, value) := {target} = {value}
## expressions
invoke(callee, args) := {callee}({args})
closure(params, result, body, outer) := {name}
  before: def {name}({params}):
  before:     nonlocal {outer}
  before:     {body}
## types
closure(params, result) := Callable
)page";
    const String py = textOf(page, kCounter);

    expect(has(py, "  def closure1():\n      nonlocal count\n      count = count + 1\n      return count;\n"), "a def and its nonlocal line, got " + py);
  }

  // A variable a closure changes is `boxed`, where it is declared and wherever it is named; a function type has its `arity`.
  void boxedVariablesAndArity() {
    const String page = kStart + R"page(
## statements
let(name, type, value) := final {type} {name} = {value};
var(name, type, value, boxed) := final {type}[] {name} = {{value}};
assign(target, value) := {target} = {value};
## expressions
name(name, boxed) := {name}[0]
invoke(callee, args) := {callee}.call({args})
closure(params, result, body) := () -> {body}
## types
closure(params, result) := Fn{arity}<{result}>
)page";
    const String java = textOf(page, kCounter);

    expect(has(java, "final number[] count = {0};") && has(java, "count[0] = count[0] + 1;"), "count in a one-element array, got " + java);
    expect(has(java, "final Fn0<number> inc = () -> {"), "the closure's type is named for its arity");
  }

  // `locals unique` gives a local that would hide another a name of its own: here a `let` hiding a parameter.
  void uniqueLocalsDoNotHide() {
    const String page = "# Small\nsource: polyir\ntarget: small\nstyle: indent 2, braces same-line, locals unique\n" + kStart.substr(kStart.find("\n## declarations")) +
                        "\n## statements\nif(cond, then) := if ({cond}) {then}\n";
    const String js = textOf(page, "func f(c: i64) -> i64 {\n  if (c > 0) {\n    let c: i64 = 2\n    return c\n  }\n  return c\n}\n");

    expect(has(js, "const c2 = 2;\n    return c2;") && has(js, "  return c;\n"), "the inner c is c2, the parameter stays c, got " + js);
  }

  // `one` matches a part with exactly one item: a loop with one counter.
  void onePatternMatchesOneItem() {
    const String page = kStart + R"page(
## statements
for(counters, cond, steps, body) := several {counters}
for(counters: one, cond, steps, body) := for ({counters}; {cond}; {steps}) {body}
counter(name, type, value) := {name} = {value}
step(name, value) := {name} = {value}
)page";
    const String js = textOf(page, "func f() -> i64 {\n  for (i = 0, j = 1; i < 3; i = i + 1, j = j + 1) {\n  }\n  for (k = 0; k < 3; k = k + 1) {\n  }\n  return 0\n}\n");

    expect(has(js, "several i = 0, j = 1") && has(js, "for (k = 0; k < 3; k = k + 1) {}"), "one counter in its own rule, got " + js);
  }

  // ── blocks as rules ──

  // `block(statements, of)` lays out a block where a rule puts it in a line. Here it writes a loop's body as `do ... end` and
  // the one statement of an if's `then` on its own line. `body` writes an empty function's body, and any other block keeps
  // the style's braces.
  void blockRulesLayOutEachBlock() {
    const String page = kStart + R"page(
## statements
for(counters, cond, steps, body) := for ({counters}; {cond}; {steps}) {body}
counter(name, type, value) := let {name} = {value}
step(name, value) := {name} = {value}
var(name, type, value) := let {name} = {value};
assign(target, value) := {target} = {value};
if(cond, then) := if ({cond}) {then}
block(statements, of: for.body) :=
  do
    {statements}
  end
block(statements: one, of: if.then) :=
  then
    {statements}
body(statements: empty, result) := /* nothing */
)page";
    const String loop = "  var total: i64 = 0\n  for (i = 0; i < n; i = i + 1) {\n    total = total + i\n  }\n";
    const String poly = "func f(n: i64) -> i64 {\n" + loop + "  if (total > 2) {\n    return 1\n  }\n  return 0\n}\nfunc g() -> void {\n}\n";
    const String js = textOf(page, poly);

    expect(has(js, "  for (let i = 0; i < n; i = i + 1) do\n    total = total + i;\n  end\n"), "the loop's body by its rule, got " + js);
    expect(has(js, "  if (total > 2) then\n    return 1;\n"), "the if's one statement by its rule, got " + js);
    expect(has(js, "function g() {\n  /* nothing */\n}") && has(js, "function f(n) {\n"), "an empty body by its rule, a full one by braces, got " + js);
  }

  // A block rule applies where a rule writes a block's statements on lines of their own: here each body's statements.
  void blockRuleWhereStatementsAreSpliced() {
    const String page = kStart + "\n## blocks\nblock(statements, of: body.statements) :=\n  \"use strict\";\n  {statements}\n";
    const String js = textOf(page, "func f() -> i64 {\n  return 1\n}\n");

    expect(has(js, "function f() {\n  \"use strict\";\n  return 1;\n}"), "the line before the body's statements, got " + js);
  }

  const String kOneExpression = "func f() -> i64 {\n  return 1\n}\nfunc g() -> i64 {\n  let a: i64 = 2\n  return a\n}\n";

  // A body has the facts a function's first lines may need: here `expression`, a body that is one `return` of a value.
  void bodyHasItsFacts() {
    const String page = kStart + "\n## blocks\nbody(statements, result, expression) :=\n  // one expression\n  {statements}\n";
    const String js = textOf(page, kOneExpression);

    expect(has(js, "function f() {\n  // one expression\n  return 1;\n}"), "f's body is one expression, got " + js);
    expect(has(js, "function g() {\n  const a = 2;\n"), "g's is not");
  }

  // A function's rule sees its body's facts: `body: expression` is chosen for f, not for g.
  void functionSeesItsBodysFacts() {
    const String page = kStart + "\n## declarations\nfunc(name, params, result, body: expression) := const {name} = ({params}) => {body}\n";
    const String js = textOf(page, kOneExpression);

    expect(has(js, "const f = () => {\n  return 1;\n}"), "f by the expression rule, got " + js);
    expect(has(js, "function g() {\n"), "g by the plain one");
  }

  // The first problem a page has: "" when it has none.
  String firstProblem(const String& text) {
    Target page{"small", "Small.translate", text};
    page.findSchema = FindSchema;
    const Vector<String> problems = TargetProblems(page);

    return problems.empty() ? "" : problems[0];
  }

  // A `block` rule names `statements` and `of`, and nothing else; its `of` is one of the source's blocks.
  void blockRuleParts() {
    const String extra = firstProblem(kStart + "\n## blocks\nblock(statements, of, extra) := {statements}\n");
    const String unknown = firstProblem(kStart + "\n## blocks\nblock(statements, of: func.bodies) := {statements}\n");

    expect(has(extra, "`block` has no part `extra`"), "a block rule with another part is a problem, got " + extra);
    expect(has(unknown, "there is no block `func.bodies` among polyir's blocks"), "a block that is not there is a problem, got " + unknown);
  }

  // `:= style` lays a block out as the style line says; any other rule is written, not laid out.
  void styleIsOnlyForBlocks() {
    const String problem = firstProblem(kStart + "\n## blocks\nbody(statements, result) := style\n");

    expect(has(problem, "only a `block` rule is laid out by the style (`:= style`)"), "a body rule may not be `style`, got " + problem);
  }

  void templatesAndPatterns() {
    rolesWriteAPartTwice();
    anyRuleWritesTheRest();
    patternOfSeveralWords();
    emptyPatternMatchesNoItems();

    optionalHoleDropsItsLine();
    emptyAnglesGo();
    escapesInText();
  }

  void pagesFactsAndLocals() {
    basePageComesFirst();
    importsComeWithWhatUsesThem();
    oneFileIsOneFile();
    mainFactHasItsType();
    literalsTakeTheirPlacesType();

    closureNamesWhatItChanges();
    boxedVariablesAndArity();
    uniqueLocalsDoNotHide();
    onePatternMatchesOneItem();
  }

  void blocksAsRules() {
    blockRulesLayOutEachBlock();
    blockRuleWhereStatementsAreSpliced();
    bodyHasItsFacts();
    functionSeesItsBodysFacts();

    blockRuleParts();
    styleIsOnlyForBlocks();
  }

}  // namespace

int main() {
  templatesAndPatterns();
  pagesFactsAndLocals();
  blocksAsRules();

  if (failures == 0) {
    cout << "RuleFeaturesTest ok\n";
  }

  return failures == 0 ? 0 : 1;
}
