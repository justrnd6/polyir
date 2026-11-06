# Translating a program to another language

polyir writes a checked program in another language by that language's **construct rules**: one `.translate` page per
target, `targets/<lang>/<Lang>.translate`. A rule is `construct(parts) := template`: on the left, a construct polyir has and
the parts it brings; on the right, the target's text, with each part as a hole `{part}`. Nothing is generated in between,
and polyir only writes the text: building or running it is the target toolchain's job.

The engine is its own library, `libpolyir-translate` (`../../translate/`), and it knows no AST. A **source** language hands it
two things:
* its **canon**: every construct its trees hold, in every form, in a `.canon` file (see **The canon** below);
* its **tree**, through `SourceTree` (`include/polyir/translate/Source.hpp`), answered over the source's own AST.

polyir is one source (`src/translate/`): it makes its checked HIR into a tree of the constructs of `targets/Polyir.canon`. A page
is for one source and one target, and says so at its head (`source: polyir`, `target: js`); every translation and every line
of `--targets` names the pair (`polyir → js`). Pages, styles and `TranslateTree` are in `include/polyir/translate/Page.hpp`.

```
polyir-check F.poly --emit-js                       the program in JavaScript, by the built-in page
polyir-check F.poly --emit-go --target-rules DIR    by DIR/go/Go.translate (or a page's path), read from disk
polyir-check --targets                              each built-in page and its pair: rules written, todo, unimplemented, ...
polyir-check --new-target Go [--source S]           writes targets/go/Go.translate with every rule `todo` (S: polyir, or a .canon)
polyir-check --update-target Go                     adds a `todo` rule for each form the page lacks
```

From C++ (`include/polyir/Translate.hpp`): `FindTarget(name, rulesPath)`, `Translate(module, target)`, `PolyirSchema()`,
`NewTargetText(name, schema)`, `UpdatedTargetText(target)`, `MissingConstructs(target)`, `TargetProblems(target)`,
`CountRules(target)`, `TargetExtension(target)`. Another source calls `TranslateTree(tree, target, problems)`.

The built-in targets are JavaScript (`targets/js/Js.translate`) and TypeScript (`targets/ts/Ts.translate`, which starts from
the JavaScript page: see **A base page**). Each page's opening comment says how it writes polyir's values: a struct is a class,
an `i64` a BigInt kept in 64 bits by the `i64` helper, a narrower integer a Number kept in range by its bits, a map keyed by
records a `ValueMap`, a coroutine a `Coroutine` over a `function*`. `targets/poly/Poly.translate` writes polyir back as
`.poly` (`source: polyir`, `target: poly`), the way WritePoly spells it, so a translation checks again into the same program.

## A page

```
# Js
source: polyir
target: js
style: indent 2, braces same-line

## declarations
program(support, items) :=
  {support}
  {items}
func(name, params, result, body) := function {name}({params}) {body}
param(name, type)                := {name}

## statements
var(name, type, value)           := let {name} = {value};
assign(target, value)            := {target} = {value};
foreach(name, source, body)      := for (const {name} of {source}) {body}
return(value)                    := return {value};

## expressions
binary(left, op, right)          := {left} {op} {right}
name(name)                       := {name}
int(value)                       := {value}
```

`func sum(xs: array<i64>) -> i64 { var total: i64 = 0  for (x in xs) { total = total + x * 2 }  return total }` becomes

```
function sum(xs) {
  let total = 0;
  for (const x of xs) {
    total = total + x * 2;
  }
  return total;
}
```

* `# Js` names the target; `style:` sets the layout. A `## section` runs from its heading to the next one.
* One rule per line. A longer template goes on the lines indented under the rule, which ends at the first unindented line.
* A line starting `//` is a comment.
* A rule's value is a template, `todo` (not looked at yet), `unimplemented "why"` (understood, not written yet),
  `unsupported "why"` (the language cannot say it), or `lower` (write the construct as the HIR has it: see below).
* `source:` names the language whose constructs the page writes, and `target:` the language it writes them in. A page with
  `base:` takes its base's source when it names none; a base for another source is a problem.

The sections of rules are those of the page's source (polyir's: `declarations`, `statements`, `flow`, `expressions`,
`builtins` and `types`); `type arguments` holds type rules for a type written inside `<...>` (Java: `i64 := Long`);
`operators`, `reserved`, `imports` and `support` hold settings (below).

## The canon

A source's **canon** is the set of forms every page for it must answer: every construct its trees can hold, in every form it
comes in, in its `.canon` file. polyir's is `targets/Polyir.canon`, beside the pages that write it; another source has its own
(nina-lite's `NinaLite.canon`). Every page for a source has a rule for every form of its canon, or says why it has none
(`todo`, `unimplemented "why"`, `unsupported "why"`): `--new-target` writes them all, `MissingConstructs` lists the ones a page
lacks.

```
# Polyir
source: polyir
typed: expressions, builtins          the forms of these sections also have their value's `type`
declarations: items, methods          the parts whose items are declarations, set apart by blank lines

## kinds                              a part's kind by its name: any other part is a value
block := body then else items statements
list := params args fields values

## families                           words a pattern may use for several types (`left: int`)
int := i8 i16 i32 i64 u8 u16 u32 u64
number := int float double

## statements                         any other section lists forms, one per line
if(cond, then)
  facts: usedAfter                    what the construct may also have
  lowered                             it also has a lower form (a rule may say `:= lower`)
if(cond, then, else)

## blocks                             how a block is laid out, and a function's body
block(statements, of)
body(statements, result)
  facts: globals, outer, throws, expression

## types                              the type forms
array(elem)

## fact nodes                         the nodes a fact holds (`captures` holds `capture`), which need no rule
capture(name, type, value)
```

A page read from disk finds its source's `.canon` in its own folder or the one above (`targets/Polyir.canon` for
`targets/js/Js.translate`), else among the canons built into the library. A source's **coverage table** names each form
once, as produced by its tree, `todo`, `unimplemented` or `unsupported` (`include/polyir/translate/Coverage.hpp`;
polyir's is `src/translate/Coverage.cpp`).

## Constructs and their forms

`targets/Polyir.canon` lists every construct, in every form it comes in; `--new-target` writes them all. A form is
one set of parts: `if(cond, then)`, `if(cond, then, else)`, and `if(cond, then, elseIf)` for an else that is only another
`if`. The constructs are the `.poly` forms, found again in the HIR where the front end lowered them (`src/hir/Lift.cpp`,
shared with `--emit-poly`): a method in its struct, `for (x in xs)` and `for (k in m)` as `foreach`, an `if` value as
`choose(cond, then, else)`, a list literal as `list(values)`, text with holes as `interp(pieces)`. A `match` or `switch` the
checker made a `Select` of is `select(value, arms)`, or `select(value, arms, else)` with a default. Each arm is
`arm(keys, body)` for exact keys, or `caseArm(case, body)` / `caseArm(case, bound, body)` for a sum's case: `case` is `Some`,
`None`, `Ok`, `Err` or a variant's number, and `bound` names the payload it binds.

What fills a part depends on its kind:

| kind | parts | filled by |
|---|---|---|
| name | `name`, `callee`, `state` | the name, a reserved word renamed (`class_`), a path joined by the style's `paths` |
| text | `op`, `number`, `case`, a literal's `value` | the text; `{op}` is spelled as `## operators` says |
| value | `cond`, `left`, `value`, `object` … | the expression's own rule, in parentheses when needed |
| block | `body`, `then`, `items`, `methods` … | each statement or declaration by its rule, one per line |
| list | `params`, `args`, `fields`, `values`, `indices` … | each item by its rule, joined by `, `, or one per line |
| statement | `elseIf` | the statement's rule, written in place (`} else if (...) {`) |
| type | `type`, `result`, `typeParams`, `typeArgs` | the type's rule from `## types` |
| fact | `isKey`, `main`, `captures` … (below) | information a rule may use: `struct(name, fields, isKey)` |

The rules are the walker: each hole is written by its own construct's rule, down to names and numbers, so a target never
writes a traversal.

**Facts** are what a rule may ask for beyond the parts; a rule that names one applies only when the construct has it, and a hole
may write a fact the rule's head does not name (`{file}::{name}`). `Polyir.canon` lists them:

| construct | facts |
|---|---|
| program | `main` (typed with main's result: `main: int`) or `library`; `name` (the file's name as a class: `Check`, `BiquadFile` beside `struct Biquad`); `exports` (a library's names); `imports` and `hoisted` (below) |
| struct | `isKey` (a map's key or a set's element), `isMutable` (a `var` field), `sharesThis` (a method hands `this` on), `internal`, `annotations`; `traits`: `trait(name, methods)` (with its `owner`) per interface it implements, holding the methods that implement it (Rust's `impl Shape for ...`) |
| func, method, static, coro, kernel | `internal`, `annotations`, `globals` (the globals it assigns), `throws` (it can throw, through any call); a method `overrides` (it implements an interface's); a method or static `owner` (its struct's whole path) |
| signature | `throws`; `owner` (its interface); `implementers`: `implementer(name, method, args)` per struct implementing it, its arguments `p0, p1` (C dispatches by them) |
| closure | `name` (a fresh name), `globals`, `outer` (the outer variables it assigns), `captures`: `capture(name, type, value)` per variable it reads from around it |
| name, this | `global`; `boxed` (a closure shares it and it changes anywhere); `captured` (the closure or coroutine being made reads it from around it) |
| var, let | `boxed`; `guarded` (its function has a `try`: C keeps it `volatile`) |
| call, methodcall | `throws`; a methodcall `owner` |
| return | `inCoroutine` |
| for | `usedAfter` (a counter read after the loop, which .poly keeps) |
| copy | `isMutable` |
| use | `program` (the imported file has its own `main`) |
| funcref | `own` (its own name), `holder` (its module or file, as the code here names it), `file` (this file's name): `Holder::own` |

A type has facts too: a closure's and a function's `arity`, and a struct's `isInterface`. Every expression has a `type` part,
its checked type, which an `any` role can write: `any.ctype(value, type) := {type}`.

A few parts are there for the targets that need them. A struct and a function always have `typeParams`, empty when they are not
generic. A read `index(object, index, type)` has the type of what it reads (a map read gives that type's zero for a missing
key), and `store(object, indices, value)` writes a cell of a grid.

**A value is written in the type of its place.** The HIR keeps a whole-number literal an `i64` where its place holds a `u32`
or a `double`, and a kernel adds an `i64` counter to an `i32`. The nodes give each value the type its place holds: a
variable's, a parameter's, a field's, an element's, a `return`'s, the type an arithmetic operator works in, or a comparison's
other operand. A literal is written as a literal of that type (`4000000000` as a `u32`, `2` as `2.0`); any other number becomes
a `coerce`. `array<i64>().with(1).with(2)` is the list it makes, `list(values)`.

## Choosing a rule

A rule **applies** to a construct that has every part the rule names, when the rule names every part of the construct that is
not a type or a fact. A target whose language writes no types leaves those parts out: `param(name) := {name}`. Of the rules
that apply, the one naming the most parts wins, then the one with the most **part patterns**, then the later one on the page:

```
binary(left, op, right)              := {left} {op} {right}
binary(left: string, op: ==, right)  := {left}.equals({right})
int(value: i64)                      := {value}n
```

A pattern on a value or a type is matched against its checked type: `i64`, `bool`, `string`, `array`, `mutmap`,
`struct`, a struct's name, or `int` (any whole number), `number`, `list`. A pattern on a text part is matched against its
text (`op: ==`), and on a name part against the name (`extern(name: sinf, params, result)`). A pattern of several words
matches any of them (`binary(left: i8 i16, op: + - *, right)`), and `empty` matches a part with no items
(`struct(name, fields: empty, methods, nested, statics)`), `one` a part with one item. A pattern word may also be a fact of the
part's node (`capture(name, type, value: boxed)`), its construct when that names no type (`assign(target: get, value)`), or a
literal's text (`bool(value: true)`).

## Roles

A hole can ask for its part **in a role**: `{fields.param}` writes each field by the rule `field.param`, so a struct can write its
fields twice, as a constructor's parameters and as the lines that keep them:

```
struct(name, fields, methods, nested, statics) :=
  class {name} {
    constructor({fields.param}) {
      {fields.init}
    }
  }
field.param(name, type)     := {name}
field.init(name, type)      := this.{name} = {name};
```

A construct with no rule of its own in the role is written by the role's `any` rule, which takes the construct as its one
part `value`, else by its own rule. JavaScript indexes a list by a Number: `index(object, index, type) :=
{object}[{index.number}]`, with `any.number(value) := Number({value})` and `int.number(value) := {value}` (a literal is one
already). A type's role works the same way: `{type.zero}` writes a type's empty value (`i64.zero := 0n`,
`mutmap.zero(key, value) := new Map()`).

## Layout

* **A hole alone on its line** takes its part's items one per line, at that line's indentation. Declarations taking several
  lines are set apart by a blank line: the items of a part the source's `declarations:` line names, and the support, imports
  and hoisted text the engine writes.
* **A block hole in a line** is written by the page's `block` rule for it (below), else it opens a block as the style says:
  `{`, the statements one step in, `}` on a line of its own (`braces same-line`, or `braces next-line`); or `:` and indented
  lines (`blocks colon`, with `empty pass` for an empty block). The rest of the template's line follows the block's end:
  `if ({cond}) {then} else {else}` writes `} else {`.
* **A list hole in a line** joins its items with `, `. An empty list takes a `, ` beside it away:
  `def {name}(self, {params})` writes `def f(self)`.
* **An empty list between `<` and `>`** takes them away too: `class {name}<{typeParams}>` writes `class Box<T>` and `class Point`.
* **`{part?}`**: the template's line is written only when the part has items: `namespace {name} {nested?}`.
* **`{part|sep}`**: the part's items joined by `sep` in the line, instead of `, ` (`{pieces|}` by nothing, `{tests| && }`), even
  when the hole is alone on its line.
* `{name}` is a hole when `name` is a word (`{name.role}` in a role); any other brace is text, so `${{value}}` writes `${` and
  the value and `}`.
* A line of a multi-line template that its holes left blank is left out. At the end, blank lines are kept to one, and none
  stays just inside a block.

**Blocks as rules.** `block(statements, of)` is a form: every page has one, and most have only the general one,
`block(statements, of) := style`, which lays every block out as the style line says (`--new-target` writes it so). `style`
is the value of a `block` rule alone. A rule for some blocks names them in `of`, the construct and its part (`for.body`,
`if.then`, `body.statements`). Each must be one of the source's blocks, a block part of one of its code forms
(`BlockContexts`), or the page has a problem. `statements` holds the block's own statement nodes, so a pattern sees them:
`empty`, `one`, or a fact of the one node.

```
block(statements, of: for.body) :=
  do
    {statements}
  end
block(statements: one, of: if.then) :=
  then
    {statements}
block(statements, of: body.statements) :=
  "use strict";
  {statements}
```

The rule's first line goes on the line the block is in, the rest at that line's indentation, and the text after the block
follows its last line. A block rule also applies where a hole alone on its line writes a block's statements, as
`body(statements, result) := {statements}` does. `{statements}` written in a line of a `block` rule is laid out by the style.
A `block` rule names only `statements` and `of`.

**Bodies.** A function's, a method's and a closure's `body` block holds one node, `body(statements, result)`, so a page
writes what a body needs before its statements once, for every kind of function. Its facts come from the source: `globals`
and `outer` (the names it gives new values), `throws`, and `expression` (one `return` of a value). Python's page writes
its `global` and `nonlocal` lines this way:

```
body(statements, result)            := {statements}
body(statements, result, globals)   :=
  global {globals}
  {statements}
```

A rule for the function sees its body's facts too: `func(name, params, result, body: expression)` is chosen for a function
whose body is one `return`.

**Parentheses** are added only where an operand binds more loosely than the operator beside its hole. How tightly each
operator binds comes from `## operators`:

```
## operators
&& := and
|| := or
levels: lambda, if else, or, and, not, == != < <= > >= is, |, ^, &, << >>, + -, * / %
```

`apart: == <` lists operators that do not chain: an operand using one beside another is bracketed (Python's `(a < b) == c`).
`levels:` lists the target's operators from loosest to tightest (by default C's, with `?` loosest). An operand's own
binding is the loosest operator outside the brackets of its rule's template, so `({left} {op} {right}) | 0` binds as `|`;
an operand before `.`, `[` or `(` must be one piece, and a prefix operator's operand binds above every infix one.

## The style line

`style: indent 2, braces same-line, names keep, paths ., generics kept`

| setting | values |
|---|---|
| `indent N` | spaces per block level |
| `braces same-line` / `braces next-line` / `blocks colon` | how a block is laid out |
| `empty TEXT` | what an empty `:` block holds (`empty pass`) |
| `names keep` / `snake` / `camel` | the case of names that start in lower case |
| `paths .` / `paths ::` | what joins a path |
| `qualify relative` / `qualify full` | `Builder` inside Point, or `Point.Builder` everywhere |
| `generics kept` / `generics copies` | `Box<i64>`, or the copies polyir made (`Box_i64`) for a language without generics |
| `locals unique` | a local that would hide another is written with a name of its own (`c2`), as Java needs |
| `modules flat` | a declaration is declared by its whole path too (`Geometry_Point`), for a language with no namespaces (C) |
| `imported qualified` | another file's declaration is named through that file (`Check.offInt`), as Java needs |
| `extension js` | the file extension of a program |
| `library hpp` | the extension of a file with no `main` of its own, when it differs (a C++ header) |

## Structure and generics are carried

Modules and nested structs keep their nesting. `module Geometry { struct Point }` is `module(name, items)` holding
`struct(...)`; a struct's `nested` part holds the structs declared in it (an `@Builder`'s `Builder`), and `statics` its
static functions. A name is written the way the source wrote it from where the code sits: `Point` inside `Geometry`,
`Geometry.Point` outside it.

A generic that works for every type is written once, as declared: `struct(name, typeParams, ...)` with
`typeParams` the `param(name)` types. Its uses are `instance(name, args)` (`Box<Long>` in Java), and a call or literal of
one of its copies has `typeArgs`. A generic that does not work for every type (`value + 1` on a `T`) is written as its copies
on every target.

## What a template cannot say

| need | rule feature |
|---|---|
| text written before the statement that uses a rule (Python's `def` for a closure of several statements) | a `before:` line under the rule; the closure's fact `name` is a fresh name |
| number widths (JS `\| 0` for i32, BigInt for i64) | part patterns: `binary(left: i32, op, right) := ({left} {op} {right}) \| 0` |
| runtime helpers | `uses: equals` under a rule, and `## support` `equals := runtime/Equals.js`: the file is written once, at the program's `{support}` hole (below) |
| a line a part needs at the top (`#include <cmath>`) | `uses: cmath` and `## imports` `cmath := #include <cmath>` (below) |
| a declaration written apart from the code (C's function for a closure) | `hoist:` lines under the rule: written once, where the program writes `{hoisted}` |
| a line whose hole is empty | `{part?}` drops the line; an empty list takes the `, ` beside it away, the `<` and `>` around it, and a `[]` after a name |
| text a target's literal cannot hold as it is (`${` in a template literal) | `escape: ${ as \${` lines under the rule: each replaces text in the text parts the rule writes; `\n` and `\t` on the left stand for a line break and a tab |
| labels and `goto` | the flow constructs below |
| host calls and types | `hostcall(binding, args)`, the binding's `$0`, `$1` its arguments; `host(name, binding, args)` a host type. A page's name is the word its bindings use (`js`, `java`, `python`, `kotlin`, `swift`, `cpp`, `rust`, `objc`, `c`) |
| a struct used as a map key | the fact `isKey`: `struct(name, fields, methods, nested, statics, isKey)` |
| a construct the language has no form of | `lower`: the construct as the HIR has it |

**`lower`** writes a construct found in the HIR as the HIR has it: `foreach` over a list as the front end's loop over its
indices, `for` with counters as `var`s and a `while`, `lambda` as a `closure`, `interp` as `+`, `freeze`/`thaw` as `copy`,
`store` as `assign`, `coerce` (a width the front end fixed, or a value given its place's type) as a `cast`, an `else if` as an
`else` holding the `if`. A `choose` (an `if` value) is lowered with
its statement: the `var` and the `if` that fill it. A `select` is lowered to the `if` chain it means (Hir.hpp `SelectAsIfs`),
each arm's test its keys' `==` joined by `||`, or the sum case's `isSome`, `isOk` or `isCase`. A page writes its own switch
with a pattern on the value's type (`select(value: int bool string, arms)`), keeping `:= lower` for the others.

**Labels and gotos** become a state machine every target can write. The function declares `var label: i64 = 0` and each label
parameter; then `machine(state, cases)` holds one `case(state, number, body)` per block, and a `goto` sets the label's
parameters (through `next_` variables when one argument reads another parameter) and is `jump(state, number)`. `branch` is
an `if`, `switch` a chain of `else if`s. A block that runs off its end jumps to the next one, and the last one returns, so every
case ends with a jump or a return. A variable one block declares and another reads is declared with the label parameters, and
set where it was declared, since each case is a block of its own. The state is named `label`, a `.poly` keyword, so no program names one like it.

## Imports and support files, by use

A file imports only what its constructs use. `## imports` lists each import as `name := line`, and `## support` each runtime
file as `name := runtime/File.ext`. A rule names the ones it needs with `uses:`, and so may a support file or an import,
on an indented line under it:

```
## imports
cmath := #include <cmath>
literals := using namespace std::string_literals;
  uses: string

## support
core := runtime/Core.hpp
  uses: memory, unordered_map, vector
```

The program's fact `imports` holds the imports used, in the page's order, and the page writes it where they go (`{imports}`).
Its `{support}` part holds the support files used, each after what it uses.

## One .poly file is one file of the target

A translation writes the checked file's own declarations. Each declaration knows the imported file it came from (`origin` in the
HIR, in its JSON and in `.pbc`). The checked file's own declarations, and the library's, have none. A file that imports
another names that file's declarations through `use(name, path, names)`: the file's name, the path from here to it (without
`.poly`), and the names its code takes. `import { offInt } from "./Check.js";` in JavaScript, `from Check import off_int` in
Python, `#include "Check.hpp"` in C++. Java names them through the file's class instead (`imported qualified`), and Kotlin and
Swift need nothing, as one package or module holds every file.

A library function (one with no file of its own) that an imported file uses is written by that file and taken from it. A file
with no `main` of its own is a library: the program has the fact `library`, and `exports` its names that are not `internal`.
Its extension is the page's `library` one. Externs are written in each file that calls them.

The support files are written into each file that uses them. C++ guards each one, so a file and the headers it includes hold it
once. A Kotlin or Swift support function is private to its file. A support type (Swift's `MutableList`, Kotlin's `Variant`)
is declared by each file that uses it, so two such files of one Swift module or Kotlin package declare it twice.

## A base page

`base: Js` under a page's heading starts the page with every rule of the JavaScript page; a rule of the page itself replaces
the base's rule alike, since of two alike the later one is used. The style is the page's own. Its operators, reserved words
and support files add to the base's, and its support files replace the base's of the same name. A page read from disk finds
its base beside its own folder (`targets/js` beside `targets/ts`), a built-in page among the built-in ones. A page's host
bindings are those of its own name, then its base's: TypeScript uses the `js "..."` bindings.

## What stops a translation

A translation that reaches a `todo`, an `unimplemented` or an `unsupported` rule fails, writes no text, and lists every such
construct the program uses with the rule's `file:line` (in the page's order) and the reason; the command line names the pair
on each (`error: polyir → js: Js.translate:143: ...`). A construct no rule applies to is written by its lower form when it has
one, and is a problem otherwise. A page for another source than the tree's is refused before anything is written, and so is a
tree node of no form in its source's canon.

`TargetProblems` checks a page itself by its source's canon: a page with no `source:` or `target:`, a source with no canon
to be found, an unknown construct or section, a part a construct does not have, a hole that is not one of the rule's
parts, `lower` on a construct with no lower form. `MissingConstructs` lists the forms no rule covers (a rule with a pattern
covers only some of a form's constructs).

## Spans and source maps

A translation can say where each statement came from, when its source knows. It is off unless asked for, and asking changes
nothing else in the text.

* **A source gives spans.** A tree node has a `SourceSpan` (`file`, `line`, `column`, and `code`, the line's text when the
  source gives it). A built tree sets `node.at`; a `SourceTree` answers `spanOf(node)`. polyir gives one to every statement and
  function, from the HIR's `Span` (core/src/translate, `AddSpan`); nina-lite gives its `.nina` places, with each line's code.
* **A translation asks for them** with `Target.spans`. Each statement and function with a span is then marked where it is
  written. When the text is done, the marks are read out into `Translation.mappings`: each statement's written line and column,
  and its span. `Target.spanLines` also writes the page's `line` lines. `--spans` asks for both; `--source-map` alone keeps
  the spans for the map and writes no lines, so the text is the one written without spans.
* **The code.** A span may hold its line's code: nina-lite gives it, and a `//# from` line gives it after the place (below).
  When a span has none, the translation asks `Target.codeOf(file, line)`; the command line reads the line from the file, as
  named or beside the program.
* **A page says what to write**, in `## spans`:

  ```
  ## spans
  line := //# from {file}:{line}:{column} {code}\n#line {line} "{file}"   before each statement from another source line
  map := //# sourceMappingURL={map}           the line naming the source map, after the text (--source-map)
  ```

  Its holes are `{file}`, `{line}`, `{column}` and `{code}`. A `\n` in a value starts another line, and a line holding
  `{code?}` is left out when the span has no code (`{code}` writes nothing there instead). Every built-in page writes the
  same comment the `.poly` holds, in its own syntax: `//# from Hops.nina:7:9 tick += 1` (Python's `# from …`), the place and
  the code. C and C++ write `#line` under it, so their compilers and debuggers name the source's lines. JavaScript (and
  TypeScript, from its base) names its map. A base's span lines come with it, and the page's own replace them.
* **A source map.** `SourceMapText(translation, "race.js")` writes the mappings as a Source Map (version 3), and
  `SourceMapLink(translation, "race.js.map")` the page's `map` line. `polyir-check race.poly --emit-js --source-map` writes
  `race.js.map` beside the translation (the `--out` file, else the program's name with the target's extension) and ends the
  text with the map's line.

**Through a chain of sources.** A `.poly` text may say where its lines came from: `//# from Hops.nina:12:5 tick += 1` says the
line after it is line 12 of Hops.nina (from column 5), whose code is `tick += 1`. The lines after it count on, as C's `#line`
does, with no code. To any other reader it is a comment. The front end reads them into `Module::fromLines`, the imported files'
too (each with the file it is in), and polyir's spans map through them. So nina-lite writes `.poly` with them
(`ninac Hops.nina --to poly --spans`, by its page's `line := //# from {file}:{line}:{column} {code}`), and every translation of
that `.poly` maps to the Nina source and writes the Nina code: the source map's sources are `Hops.nina`, C's `#line` lines name
it, and each page's comment is the Nina line. polyir's own poly page writes them too, so a program translated to `.poly` and
on again keeps its first source's places and code.

**Native debugging maps through them too.** `polyir-check Hops.poly --debug` places every function, statement and variable at
the line its `//# from` line names (`core/src/emit/llvm/LlvmDebug.cpp`, `MapThrough`). Each source file has its own `DIFile`,
found from the checked file's directory; a function's subprogram is in the file its first line came from, and a line from
another file is scoped to a `DILexicalBlockFile`. So lldb stops at `Hops.nina:7`, lists the Nina code, and steps by Nina
lines. A function from an imported file is placed in that file's own lines.

## Where the pages live

Each `targets/<lang>/<Lang>.translate`, with the files under its `runtime/` folder, is built into libpolyir:
`cmake/EmbedTargets.cmake` writes them as text into a generated `TargetTexts.cpp`, and adding a target is adding its folder.

**Compiled pages.** Each page, and `targets/Polyir.canon`, is also compiled ahead by `polyir-translatec`
(`../../translate/tools/TranslateC.cpp`) into C++ that makes its rules (`CompiledPage<Lang>()`, `CompiledSchemaPolyir()`), so a
built-in target never reads its text. The page is checked by its source's canon first. A problem in it, or a form it
has no rule for, stops the build with its `file:line`:

```
polyir-translatec targets/js/Js.translate --canon targets/Polyir.canon --file targets/js/Js.translate --function CompiledPageJs --out Js.cpp
polyir-translatec targets/Polyir.canon --file targets/Polyir.canon --function CompiledSchemaPolyir --out Polyir.cpp
```

Another project compiles its own pages with the CMake function `polyir_translate_page(<sources> <page> FUNCTION <name>
[SHOWN <path>] [CANON <source.canon>] [DEPENDS <files>])` after `add_subdirectory()`ing polyir. A page read from disk
(`--target-rules`) is read once and kept for the rest of the process, as is its index (its rules grouped, its templates
worked out).

**How fast.** `build/TranslateBench`, run from the repository's root, checks every program of the corpus and writes it by
every built-in page (0.79 s for 160 files × 10 pages), and writes the largest one (ContainerTests.poly, 881 lines) to
JavaScript (2.5 ms, its tree 0.9 ms). The program is written once, with a mark where its support files, imports and hoisted
text go.
`--target-rules PATH`, or the folder `POLYIR_TARGETS` names, reads pages from disk instead, to try a change without
rebuilding.

## Tests

* `tests/TranslateTest.cpp`: a small page written in the test: rule choice by parts and by types, recursion, precedence,
  `blocks colon`, `before:`, support files, `todo` and `unsupported`, page problems, new and updated pages, `lower`, modules and
  nested structs, generics, labels, and the command line.
* `tests/RuleFeaturesTest.cpp`: roles, patterns of several words, `empty`, `{part?}`, empty angle brackets, `escape:`, a base
  page and its host bindings, imports by use, one file per .poly file, the type of `main`, literals in the type of their place,
  closures' facts, boxed variables, unique locals, `one`, `block(statements, of)` rules (where a block is in a line, and where
  its statements are spliced), a body's facts, and the problems of a `block` rule.
* `tests/TranslateCoverageTest.cpp`: every built-in page has a rule for every construct, and with a page whose every rule is
  written, every program of the corpus (examples, test programs, `ideas/`) translates with no problem.
* `tests/SourceCoverageTest.cpp`: `Polyir.canon` reads with no problem, polyir's coverage table names each form of its canon once,
  and every node polyir makes of the corpus is a form the table says it produces.
* `tests/PolyRoundTripTest.cpp`: every example translated to `.poly` by the poly page and checked again is the same program;
  what the page cannot spell yet, host parts, a program importing a file that does not come back the same (a file imports only
  what its own code uses), and the known gaps are counted apart.
* `tests/TranslateSpansTest.cpp`: spans off by default; a page's `line` before each statement; the mappings; `//# from` lines
  read and followed; the code from a `//# from` line and from `codeOf`, `{code?}` left out; mappings without lines; a Source
  Map of two lines; the `map` line, from a base page too; `## spans` problems; `--spans` and `--source-map`.
* `tests/PageCompileTest.cpp`: each built-in page's compiled rules are those its text reads as, polyir's compiled canon
  is `targets/Polyir.canon`, and a page that lacks a form or has a problem is refused.
* `../../translate/tests/`: the library alone. `SchemaTest` reads a `.canon` (forms, kinds, facts, families, problems at their
  lines); `SourceTreeTest` translates a made-up language through a `SourceTree` over its own structs, and refuses a page for
  another source and a node of no form.
* `tests/TargetGoldenTest.cpp`: each built-in target's translation of every file of the corpus (examples, the library's
  `core/stdlib`, test programs, `ideas/`) against its golden, `targets/<lang>/tests/<the file's path>.<ext>` (a library's
  extension for a file with no `main`), or `.problems` for what stops it (a C function JavaScript cannot call).
  `TargetGoldenTest --update` writes them again; a changed golden is reviewed by reading it.
