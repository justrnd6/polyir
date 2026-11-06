# The C++ front end (parser and checker)

> The Nina implementation this document refers to (`lang/`, its CLI `polyir-lang` and its sweeps) is no longer part of
> this repository; the goldens it recorded are still checked here.

A port of the Nina front end in `lang` (`src/Polyir.intent` grammar, `src/*.nina` declaration passes, `src/hir/check/*.nina`) to C++, inside libpolyir, so `source text → Module → LLVM IR` is one library with no Nina in it. **The Nina implementation stays and is the oracle.** Both are kept.

## Goals

1. **Identical results.** For every program the Nina front end accepts, `polyir-check --hir f.poly` prints exactly what `polyir f.poly --emit-hir` prints (byte for byte; `tests/front_sweep.sh` compares them over `core/examples/**/*.poly` and the sources in `lang/tests/src/*.nina`). For every program it rejects, the C++ front end rejects it with the same messages (same text, same `line:column` prefix, same order).
2. **Fast.** The checker is the point of the port. Targets: a typical example (50–300 lines) parses and checks in a few milliseconds in-process (`polyir-check --bench N f.poly` reports parse and check time per run), and checking all of `core/examples` is at least 20× faster than running `polyir f.poly --emit-hir` on each file. How: one pass over the source with `string_view` tokens; an arena or flat vectors for AST nodes, no per-node `shared_ptr`; interned names (a 32-bit `Symbol`) and hash tables for declarations, scopes and the prelude (the Nina version searches lists); the prelude parsed and resolved once per process, not per program; no repeated copying of declaration lists; no exceptions on the normal path.
3. **Readable code, in the project's C++ style** (`style.nina.properties`: 2-space indent, functions at most 16 code lines, braced `if` and loop bodies set apart by blank lines, no `std::`-qualified names — `polyir/Std.hpp` has the aliases, add to it as needed — camelCase functions, PascalCase files and types, `k`-prefixed constants, public members first, private members end in `_`). `nina style runtime` must report nothing.

## Layout

```
core/include/polyir/Frontend.hpp     the public entry points (below)
core/src/front/                      namespace polyir::front
    Ast.hpp, Lexer.*, Parser*.cpp          the syntax tree and the parser (done)
    Session.hpp/.cpp                       what one check works on: Unit, Decls, Scope, Session            (below)
    Registry.hpp/.cpp                      the extension points: tables of handlers                         (below)
    Checker.hpp/.cpp                       the state a body is checked with, and the declarations of every Check*.cpp function
    Pipeline.hpp/.cpp                      the phases, front to back; CheckSource / CheckFile
    Resolve.cpp, ResolveHost.cpp           types and top-level items -> module declarations (HirResolve.nina, HirResolveHost.nina, StructMethods.nina)
    CheckBody.cpp                          function bodies (CheckBody.nina)
    CheckExpr.cpp, CheckNames.cpp,         expressions: operators and literals; names, calls and struct literals; .field / .method() / [index]
      CheckSuffix.cpp
    CheckStmt.cpp, CheckFlow.cpp           statements: let/var/if/for/return/assignment; labels, goto, branch, switch, try, throw, yield
    CheckOps/Types/Coerce/Equality/        the small rules: operator typing; kind mismatches; integer-width coercion; which comparisons are allowed;
      Fields/Interface/Host.cpp              which fields can change; interface values; calls on a host type
    CheckCollections/Builtins/             lists, maps and sets and their rules; the builtins that apply to any receiver; indexing and fresh storage
      Containers.cpp
    CheckDiagnose.cpp                      the problem nodes in the HIR -> `line:col: message`
    Imports.cpp, ImportMerge.cpp           Path.normalize, the duplicate-name check; resolving `import "x.poly"` / `import java` into units (ImportMerger)
    Prelude.cpp, PreludeText.cpp           the built-in Math, GPU, Text and Lists libraries, parsed once per process, pulled in by the retry rounds of Pipeline.cpp
    Strings.cpp                            text blocks and `${…}` rewritten before lexing (Strings.nina), the escapes of a literal decoded
    CheckLists/Strings/ForInList/          `[a, b]` list literals; str, slice, find, `s[i]`, truncate; `for (x in list)`; container fields left out of a
      StructDefaults.cpp                     construction (and `Name()`) start empty
    Generics.cpp, ExternTypes.cpp          monomorphization (an Instance + a Unit per `Box<i64>`); the host-binding / bridge / extern type problems
    Modules.cpp                            `module Name { ... }` and `internal`: which declaration a name means in the code's module, who may use it
    CheckIfValue.cpp                       `if (c) a else b` as a value: a hidden `var ifValue_L_C` and the `if` that fills it, hoisted before the statement
    ParserMatch.cpp, CheckMatch.cpp        `match` and the switch of blocks: arms (a pattern, a guard, a condition) parsed; each arm's pattern checked into
                                             tests and bound places, the arms lowered to a `Select` (every arm exact keys) or an `if` chain
    CheckGenericCall.cpp                   a call of `func f<T>(...)`: T taken from the arguments, the copy `f_i64` made (a Unit) the first time
    Interfaces, Coroutines .cpp            the phases: interface problems and coroutine restrictions
    CheckClosure.cpp, CheckSums.cpp        closure literals and their captures; variant / either / optional constructors and builtins, resume / done
    UnusedVarLint.cpp, Lint.hpp            the unused-`var` lint over the syntax tree (UnusedVarLint.nina): the `warning:` block
    Kernels.cpp, KernelVerify.cpp          the `kernel func` rules (KernelRoles / KernelVerify / Kernels.nina): `kernel check failed:` messages; no shader text
core/src/cli/Cli.cpp, CliBuild.cpp       RunPolyirCli: the Nina CLI's flags, output blocks and exit status; the native build and run
core/src/hir/HirJsonWrite.cpp            `WriteHirJson(Module)`: the HIR JSON, byte for byte what `polyir f.poly --emit-hir-json` prints (HirJson.nina)
core/tools/PolyirCheck.cpp           polyir-check: the Nina CLI's flags, output blocks and exit status
core/tests/front_sweep.sh            Nina vs C++ on every example, HIR text and error messages
core/tests/front_status.py           the same, grouped by the area of the language that is missing
core/tests/FrontCheckTest.cpp        the Nina checker's own test programs, with the results the Nina CLI gives (tests/data/front_cases.txt)
core/tests/extract_front_cases.py    makes that data file from lang/tests/src/*.nina, core/tests/programs and tests/data/handwritten.txt
core/tests/fuzz_front.py             random programs (and mutated examples) checked by both front ends
core/tests/cli_parity.py             polyir-check against the Nina CLI, stdout and exit status, per flag, over all examples (or files)
core/tests/fuzz_negative.py          broken variants of known programs (token/argument/line deletions, wrong types, duplicates, let->var, bad labels, kernel constructs) through both CLIs
core/tests/negative_cases.py         tests/data/negative/*.poly with the Nina CLI's recorded answers (ctest NegativeCases); `--update` records them again from Nina
core/tests/HirJsonWriteTest.cpp      WriteHirJson: write/read/write on every example, and equal to the Nina JSON when the Nina polyir is built
```

## Public API (`polyir/Frontend.hpp`)

```cpp
struct FrontResult { Module module; Vector<String> errors; String headline; Vector<String> warnings; int kernelCount; Vector<String> kernelProblems; bool ok() const; };
FrontResult CheckSource(const String& text);   // parse + check a source held in memory; errors are `line:col: message`
FrontResult CheckFile(const String& path);     // read, parse and check a file; errors carry its normalized path; `import` lines are resolved relative to the importing file
```
The `Module` is the same one the builder makes (`order` filled in, so `ModuleText` prints it exactly as the Nina `--emit-hir`), and `CompileToLlvm(result.module)` compiles it.
`errors` are the messages the Nina CLI prints under the line `headline` ("compile failed" for a syntax error, "check failed" for a mistake in the program, "no such file"); `CheckFile`
also runs the CLI's duplicate-name check and the lint: `warnings` are the unused-`var` messages (`file:line:col: 'x' is declared `var` but never reassigned in 'f' — use `let` instead`) of the file itself,
`kernelCount` / `kernelProblems` what the GPU rules say about its `kernel func`s. Neither makes `ok()` false; the CLI decides what to print.
`WriteHirJson(module)` (`polyir/HirJson.hpp`) is the Nina `--emit-hir-json` text (no final newline), floats as C99 hex floats.

### polyir-check, the Nina CLI's behaviour

`polyir-check f.poly [flag] [--strict]` prints what `polyir f.poly [flag] [--strict]` prints, on stdout, with the same exit status (0, or 1 for an error block, 2 for a usage error):

| flag (Nina / polyir-check) | prints |
|---|---|
| `--emit-hir` / `--hir` | the warnings, then the HIR text, one declaration per line |
| `--emit-hir-json` / `--json` (also `--emit-hir-json`) | the warnings, then `WriteHirJson` and a newline |
| `--analyze` | the warnings, then `ok` (declarations bound only to a host are accepted, as in Nina) |
| `--check` | the warnings, then `ok` once the LLVM compiler accepts the module; its errors are `compile failed:` |
| `--emit-llvm` / `--llvm` | the LLVM IR (libpolyir's, not Nina's text) |
| `--emit-msl`, `--emit-glsl`, `--emit-spirv` | only the rules: the warnings and `kernel check failed:` + messages (also `no `kernel func` found`); the shader text is not written (a note goes to stderr) |
| `--strict` | an unused `var` is `lint failed:` + the warnings, exit 1, before the program is checked |
| (none) | the warnings and any error block; nothing when the program is fine |
| `--ast`, `--bench N` | the syntax tree; the timing (C++ only) |

An error block is the headline, `:`, and each message indented two spaces: `no such file:` (the path), `compile failed:` (a syntax error, with the file), `lint failed:`, `check failed:` (the
program is wrong; `compile failed:` for `--check` and `--llvm`, which build; `kernel check failed:` for the kernel outputs), as `reportErrors` in `Polyircli.intent`. The warnings are printed first,
as `warning:` and the messages indented under it, whenever the file parses (never for a syntax error or a missing file), and only the file named on the command line is linted, not what it imports.

## Oracle workflow

* `lang/cli/build/out/polyir-lang f.poly --emit-hir` is the Nina result (rebuild: `cd lang/cli && /Users/justrnd/Documents/SourceCode/nina/nina/build/nina up . build`).
* The Nina front end's own tests (`lang/tests/src/*.nina`) hold many small sources with their expected HIR text and expected error messages: port them into C++ tests as well.
* `--emit-hir-json` + `polyir-hir` already round-trips all 115 examples to identical text, so a difference is in the new front end, not in the printer.

## The checker's architecture

A check is a `Session` (Session.hpp) that the phases of Pipeline.hpp work on, in this order: the declarations are resolved into the session's module and tables
(`ResolveDeclarations`), every function body is checked against them (`CheckBodies`), and the problem nodes the check left in the HIR are turned into messages
(`diagnose`). Nothing is thrown: a mistake in the program becomes a problem node (`unsupported(message, at)`, an `ExprKind::Unsupported`) where the thing
was, exactly as the Nina checker leaves an `HUnsupported`, and the diagnose step reports each one, in the Nina order, as `line:column: message` (with
`file:` in front when the program came from a file). The printed HIR is the one the Nina printer prints, so the HIR text is the test oracle.

**What is where**

| | |
|---|---|
| `Unit` | a run of top-level items of a parsed `Program` and the file they came from. A syntax node's ids are relative to its own Program, so the checker is told which unit a body is in (`Checker::enter`). A file with imports is several units (its items before the import, the imported file's units, the rest): the units are the merge. |
| `Decls` | structs, functions (free, extern, host members, and methods under their mangled name `Struct_method`), globals, interfaces and host types by name, in `SymbolMap`s (open-addressing tables keyed by the interner's 32-bit `Symbol`). The first declaration of a name wins, as in the Nina checker's front-to-back search. Struct types carry their name as text, so a lookup from a type interns the name (`Checker::symbolOf`). |
| `Scope` | the names a body can see, in one vector, newest last: a `let` is an append, leaving a block a truncate, a lookup a backward scan; the globals are looked up after it (`Checker::lookup`). A binding is a plain value, a `var` slot or a global, because they read differently. |
| `TypePool`, `IntType(n)` ... | the scalar types, `string` and `void` are shared by the process and a struct type is made once per name, so equal scalars and structs are the same object and `SameType` answers on the pointer. Derived types (`ptr<T>`, `array<T>`) are made where they are written and compared structurally. |
| `Registry` | tables of handlers, filled once per process (below). |
| `Session::module` | the output: `Module` with `order` filled in, so `ModuleText` prints the declarations in the order they were written. |

**Reading the code.** Each `Check*.cpp` is the port of the Nina `check/Check*.nina` of the same name, with the same helper names where there is one, so a
change to the Nina checker is a change to the same-named file here. Behaviour that looks like a quirk is the Nina behaviour and is kept: the oracle compares text.
The ones worth knowing: the loop variables of a `for` stay visible after the loop; a `&&` has the left operand's type; `true`/`false` have no position
(`0:0`); a character literal's value is the character after the quote, or the one its escape stands for; integer literals wrap modulo 2^64; a cast method
(`x.toI64()`) never looks at its arguments; a unary operator has its operand's position; a method that is not found on a struct reports `unknown function
<method>`; `diagnose` reports, per function, what is wrong in its statements first and its mistaken `return`s after, and for a `switch` only the
default's arguments; a `p: val T` parameter makes the body start with `let p = val p`; a `let` of a `mutarray`, `sparse` or `mutmap` is kept in a slot like a `var`.

### Adding to the checker

Every addition is a handler in a file of its own plus one line in `registerAll` (Registry.cpp).

* **A builtin method** (`x.len()`, `x.tag()`, `p.atomicLoad(...)`): a function `ExprPtr (MethodCall&)` that returns the HIR node, or null when the name is
  not a builtin for this receiver (then the next handler is asked, and last the call is an ordinary call of the function `name` with the receiver first).
  `call.receiver` and `call.args` are checked; `call.at` is the method name's position. A handler that answers null must leave `call.args` alone.
  ```cpp
  ExprPtr atomicLoad(MethodCall& call) {
    return builtinNode(Builtin::AtomicLoad, call.receiver, call.args, call.receiver->type->element(), call.at);
  }
  void registerAtomics(Registry& registry) { registry.addMethod("atomicLoad", kindBit(TypeKind::Pointer), atomicLoad); }
  ```
  `KindBit(TypeKind::X) | KindBit(TypeKind::Y)` or `kAnyKind` say which receivers it applies to. `AddFunctionForm("len")` makes `len(x)` mean `x.len()`;
  `AddFree("fence", handler)` is a builtin with no receiver. The builtins every receiver has (`len`, `count`, `hash`, `push`, `copy`, `with`, `has`, `add`,
  `freeze`, `thaw`) are written (CheckBuiltins.cpp); the Nina `CheckBuiltins.nina` `resultType` groups for specific receivers (grid, tensor, buffer,
  vec, variant, either, optional, pointer, coroutine) are the rows still to add.
* **An expression kind** (`vec[...]`, `mutarray<T>()`, `closure(...)`, `variant<...>(...)`, `Some<T>(x)`): `registry.addExpr(AstExprKind::VecLit, handler)` with
  `ExprPtr (Checker&, const AstExpr&)`. The handler reads the node's fields (Ast.hpp says which are set for each kind), checks sub-expressions with
  `CheckExpr(check, id)` (or `CheckExprExpecting` where the context has a type), and builds nodes with `HirBuild.hpp`. Until a kind has a handler it is
  `?expression form?`, as in Nina.
* **A statement kind**: `registry.addStmt(AstStmtKind::X, handler)` with `StmtPtr (Checker&, const AstStmt&)`. The handler binds and unbinds names itself
  (`check.scope.bind`, `mark()`, `reset(mark)`); `CheckBlock` already drops what a block bound.
* **A top-level declaration kind**: a row in `kItemResolvers` (Resolve.cpp), indexed by `ItemKind`, filling the module with `AddStruct`, `AddFunction`,
  `AddExtern`, ... (which also register the name for lookup).
* **A phase** (generics, imports, the library, interface and host rules): replace the stub of its file; `checkSession` in Pipeline.cpp is where each is called.
  A phase that adds source (the specializations of `Box<T>`, the library's functions) appends a `Unit` to `session.units`, parsed from text with the global interner,
  and keeps its `Program` alive for the process; a phase that merges files builds the units list in `CheckFile`.
* **A new HIR operation the printer or compiler lacks**: not the checker's business; `Hir.hpp` already has every case of the Nina HIR.

The checker works with `Interner::global()` (the one `ParseSource(text)` uses); the registry and the cast table are keyed by its symbols.

### Tests

* `tests/FrontCheckTest.cpp` (ctest): every polyir program the Nina tests use, with the result the Nina CLI gives for it. `tests/data/front_cases.txt` is made
  by `tests/extract_front_cases.py` (needs the Nina `polyir` built) from (1) every string the Nina test files (`lang/tests/src/*.nina`) build, as
  a literal or a `+` chain of literals, `let` locals and no-argument actions, that reads as a program, (2) `core/tests/programs/*.poly`, and (3) `tests/data/handwritten.txt`,
  programs written for the C++ checker (positive and negative; add to it and rerun the script). The expected result is the Nina CLI's own output, so the data
  is the oracle, not a transcription. Each case is tagged with the areas of the language it uses (`tests/front_areas.py` reads the text for them): `core`,
  `containers`, `generics`, `imports`, `host`, `prelude`, `closures`, `exceptions`, `coroutines`, `sums`, `interfaces`, `kernel`. A case runs when all its
  areas are in `kEnabledAreas` at the top of the test; **the work that completes an area adds it there** and its cases start to run. `FRONT_AREAS=all` runs everything,
  `FRONT_AREAS=core,containers` a chosen set, `FRONT_SUMMARY=1` prints the table by area, `FRONT_SHOW=1` the source of each failure. The Nina CLI's own crashes are not compared; lint warnings are part of the CLI output compared by `cli_parity.py` / `front_sweep.sh`.
* `tests/front_sweep.sh <build dir> [--strict | --update]`: the examples, file by file, Nina against C++ (stdout and exit status). While the port is unfinished it fails only for a REGRESSION (a program in `tests/data/front_sweep_matching.txt` that stopped matching) and prints NEW for one that now matches (`--update` rewrites the list); `--strict` fails on any difference. `tests/front_status.py <build dir>` runs the same and groups what differs by area.
* `tests/cli_parity.py <build dir> [--flag emit-hir|emit-hir-json|analyze|analyze+strict|check|emit-msl] [files]`: the two CLIs, whole stdout and exit status, per flag (over the examples by default).
* `tests/fuzz_negative.py <build dir> [--flags analyze|analyze+strict|emit-hir-json|emit-msl] [--count N] [--seed S] [--keep DIR]`: broken variants of known programs through both CLIs; differences are filed under the areas the mutant uses (`front_areas.py`), `core` ones fail the run, a Nina CLI abort is `nina-crash`.
* `tests/negative_cases.py` (ctest `NegativeCases`): `tests/data/negative/*.poly` -- lint, every error class, the kernel rules, one rule per file -- with the Nina CLI's recorded answers (`.expect`, made by `--update`).
* `tests/HirJsonWriteTest.cpp` (ctest): `WriteHirJson` writes, reads and writes every example to the same text, and, when the Nina polyir is built, equal to its `--emit-hir-json`.
* `tests/fuzz_front.py <build dir>`: random core programs with a share of deliberate mistakes, and with `--mutate` the examples with identifiers swapped; each is checked by both and
  the answers compared. About 7,000 programs had no difference in the core subset; the differences it reports on mutated examples are the host-binding checks and the library (areas still to port).
* `tests/FrontParseTest.cpp`, `tests/parse_sweep.sh`: the parser.

## Status

* **C++ only (2026-11-04): generics carried in the HIR.** Each generic is also read once with its type parameters as `Param` types
  (`Box_T`, `pick_T`: Generics.cpp `parameterUses`, CheckGenericCall.cpp `DeclareGenericFunctions`), so it is checked for every type, not
  only for the types a program uses.
  - An operator or a cast on a `T` is refused there (CheckTypes.cpp `AnyTypeProblem`). Diagnose leaves these copies out; their first
    problem becomes the generic's `notParametric` (Parametric.cpp `NoteParametric`). The program still runs, as its real copies are fine.
  - After the check the copies move to `Module::genericStructs` / `genericFunctions` (`SplitGenerics`). Every copy names its generic and
    its type arguments (`Generic` in Hir.hpp). LLVM, the VM and the HIR text see only the real copies, as before.
  - HIR JSON (`generic`, `isGeneric`, `HParam`) and `.pbc` section 3 carry them. `--emit-poly` writes a generic that works for every type
    once (`struct Box<T>`, `func pick<T>`, uses as `Box<i64>`), and any other as its copies. Tests: `GenericsTest`, `RoundTripTest`.
  - Known gap: a struct copy needed only inside a generic function's copy (`wrap(2.5).get()` when only `wrap<T>` returns `Box<T>`) is not
    made: `Box_double has no field value`. A `let d: Box<double>` anywhere makes it.

* **C++ only (2026-11-03): structs inside structs, `static func`, field defaults, annotations, `@Builder`, `fail(message)`.**
  - A nested struct and a static function are named through their struct (`Point.Step`, `Point.origin`). The parser reads them as items in
    the struct's path, and they keep `outer` in the HIR. Module members are named by their path too (`Geometry.Point`, no longer `__`).
  - `@Builder` is expanded in the parser (ParserBuilder.cpp): the nested `Builder` and `builder()` are made as .poly text and parsed in the
    struct's body. CheckBuilder.cpp refuses a chain that skips a required field; WritePoly writes `@Builder`, not the expansion.
  - Field defaults (FieldDefaults.cpp), annotations (Annotations.cpp) and `outer` travel through the HIR, its JSON (`fieldDefaults`,
    `annotations`, `outer`) and `.pbc` section 2. Tests: `BuilderTest`, and `RoundTripTest` (every example written as .poly and read back).
* **C++ only (2026-11-03): modules, `internal`, `if` values and generic functions.** `module Geometry { ... }` (declarations named
  `Geometry.Point`, its path; contextual words, so `module` and `internal` stay usable as names), `internal` on a function,
  struct, interface or method, `if (c) a else b` as an expression, and `func pick<T>(...)` with its types taken from the arguments. The Nina
  front end (`lang/src`) has none of them, so `front_sweep.sh` lists `examples/modules.poly` as a difference and `nina_sweep.sh` skips it;
  `CliTest` runs it (VM output, `--emit-poly` printing back the same) and checks the misuse messages of `tests/programs/fixtures/modules`.
  The HIR records each declaration's module and `internal` mark (HIR JSON keys `module`, `internal`; `.pbc` section 2).

* **Parser: done.** `src/front/Lexer.*`, `Parser*.cpp` and `Ast.hpp` (the tree, documented at the top of the header). `ParseSource(text)` gives `ParseResult{program, errors}`; the errors are the Nina parser's, word for word (`line:column: expected ... (found '...')`, `unexpected character 'c'`, the `darray`/`hashmap` hints). Checked by `tests/FrontParseTest.cpp` (115 broken sources with the Nina messages, all 122 examples, the AST shape), `tests/parse_sweep.sh <build dir>` (Nina vs C++ file by file) and a mutation fuzz of the examples against the Nina CLI (no differences in several thousand mutants). `polyir-check f.poly --ast`.
* **Checker, core: done** (phase 2a). `CheckSource` / `CheckFile`, `polyir-check f.poly --hir | --llvm | --bench N`. The core subset is: scalar types and operators (integer literal typing, `HCoerce`, casts,
  comparisons, logical operators), `let`/`var`/assignment, `if`/`else`, `for`, labels, `goto`, `branch`, `switch`, `try`/`throw`, `yield`, `return`, free functions and calls (forward
  and mutual recursion, function pointers, indirect calls), globals, extern functions, structs with fields, struct literals, field get/set, struct methods (`Struct_method`, `this` first),
  UFCS, plain strings, `val` copies, pointers and their indexing, interface values (boxing, calls through the method table), calls on host types, the duplicate-name check of `CheckFile`.
  In the Nina test programs: 100 of 100 `core` cases match, and all `exceptions`, `coroutines` and `kernel` cases that need nothing else; the remaining cases need the areas below.
* **Examples** (`tests/front_sweep.sh`): 31 of the 122 programs match exactly, and every program that uses only the core subset is among them (`tests/front_status.py`, which also takes
  the 18 programs of `core/tests/programs`: 32 of 140). The 108 that differ, by the area of the language they use that is not written yet -- a program counts for every area it uses, so
  the counts overlap; "alone" is how many are blocked by that area only (`front_status.py <build dir>` prints every file with the first difference; the test cases are counted by
  `FRONT_AREAS=all FRONT_SUMMARY=1 FrontCheckTest`):

| area to add | programs | first difference | where to start |
|---|---|---|---|
| **containers and builtins: done** | all 35 `containers` cases match, and the 13 programs blocked only by it (atomics, buffer, chars, collections, frontend/shapes, grid, Unsigned, methods, mutarray, mutmap, sparse, strings, tensor) match; the programs that still differ and use containers are blocked by another area (imports, prelude, sums) | -- | `CheckVec.cpp` (constructors `vec[..]`, `mutarray`, `array`, `buffer`, `sparse`, `map`, `mutmap`, `set`, `mutset`), `CheckShapes.cpp` (`rows`, `cols`, `dim`, `capacity`, `shuffle`, atomics, `fence`) |
| **imports: done** | `ImportMerge.cpp` (`ImportMerger`): the item list is cut at each `import`, the imported file's own units go between (depth first, each normalized path read and parsed once, which also ends a cycle); `{os}` in a path; `import not found: 'p'` at the import line; a parse error in an imported file is `file:line:col`. `import java` reads the existing `.polyir.d/java/<Class>.d.poly` (it does not run `javap`: a missing file is an error saying so); other languages: `import X is not available (only import java)`. All 70 programs match | none | |
| **prelude: done** | `Prelude.cpp`, `PreludeText.cpp` (the text of stdlib/Math.poly, Gpu.poly and Text.poly, as `PreludeText.nina` embeds it: `sync-prelude.py` writes both copies; Text.poly is copied as it is, with no `$~{`): the libraries are parsed the first time a program is missing a library function, never for a program that needs none; `LibraryUnits` answers only the needed functions and what they call (`needs`), as units placed in FRONT of the program's, and the round is dropped when they clash with something the program defines (`CheckDuplicates`) | none | |
| **closures, sums** (variant, either, optional) | DONE (CheckClosure.cpp, CheckSums.cpp): closure literals with `captures` as Nina lists them (every name the body mentions, first-mention order, repeats kept, one walk); `variant` / `eitherOk` / `eitherErr` / `Some` / `None` and their builtins (`tag`, `isCase`, `unwrapAt`, `isOk`, `isErr`, `unwrapOk`, `unwrapErr`, `isSome`, `isNone`, `unwrap`); programs left only wait on generics | none | |
| **generics: done** | `Generics.cpp`: discovery scans the declared types exactly as `Generics.nina` does (not expressions), dedupes by mangled name, keeps the uses whose arguments name no type parameter; each becomes an `Instance` (Session.hpp) and a Unit appended LAST that re-reads the generic struct / `extern type` with `Session::active` set, so `T` resolves to its argument (`Resolve.cpp` `namedType`, methods and `let` types alike). Specialized declarations carry no file (as in Nina, whose origins do not know `Box_i64`). The interface rules are asked again of the instance units (Interfaces.cpp: `Instance` keeps the use's type arguments, so `T` reads as its argument in the messages) | none | |
| **interfaces** | DONE (Interfaces.cpp): duplicate methods, fixed-array parameters, `unknown interface`, `is a struct, not an interface`, `does not implement`, `does not match`, extern-interface claims; the generic struct case is asked again after `monomorphize` | none | |
| **coroutines** | DONE: `resume` / `done` (CheckSums.cpp) and the syntax rules of `checkCoroRestrictions` (Coroutines.cpp: yield outside a coro, in a try-portion, in a closure; bare return) | none | |
| **host: done** | `ExternTypes.cpp`: every message of `ExternTypes.problems` (unknown / repeated / empty targets, missing binding, duplicate type names, repeated type parameters and members, bridge rules), labelled with the file of the first declaration of that name. Not written: `nativeProblems` (a native BUILD of a host-only declaration; the checker does not run it) | none | |
| **lint, kernels, CLI parity: done** | `UnusedVarLint.cpp` (the `warning:` block, `--strict`), `Kernels.cpp` / `KernelVerify.cpp` (every rule of `KernelVerify.nina`: signature types, launch shape, `barrier()`, atomics, writes, forbidden expressions and statements; `kernel check failed:`), `HirJsonWrite.cpp`, `polyir-check` with the Nina flags (table under *Public API*). `tests/front_sweep.sh` no longer strips warnings. | none | the shader text of `--emit-msl|glsl|spirv` is code generation and is not written here |

* **CLI parity and negative programs** (`tests/cli_parity.py`, `tests/fuzz_negative.py`, `tests/negative_cases.py`; Release build, the tree with all areas in). Whole stdout and exit status, Nina vs `polyir-check`:
  every example matches for `--emit-hir`, `--emit-hir-json`, `--analyze` and `--analyze --strict` (120 of 120 each; the 8 diagnostics fixtures, which hold the import and kernel-import errors, and the 18 `core/tests/programs` programs too),
  and `--emit-hir-json` is byte-identical (floats are C99 hex; this needed the Nina `String.toDouble` algorithm, which is not correctly rounded -- `6.2831853` is one bit from `strtod` -- ported into `ParseDouble`).
  `--check` matches on 118 of 120 examples and all 18 `core/tests/programs` programs. libpolyir's LLVM compiler gained structural `==` (LlvmEquality.cpp: `eq_<type>` functions added before compiling, as HirStructuralEquality.nina), `.copy()` of a reference struct and of a `sparse`, and `+` on lists; the Nina native-build check (`nativeProblems`, NativeProblems.cpp, `FrontResult::nativeProblems`) refuses host-only declarations in use. The 2 left (java-mapping/Closures, Sets) are Nina's own `uses a construct this compiler cannot lower yet`: libpolyir compiles them.
  The programs of `tests/data/negative` (`native_*` also run `--check`) (lint in methods / coroutines / closures / `try`, every error class, one kernel rule per construct) run in ctest `NegativeCases` against the Nina answers.
  The mutation differential (20,000 mutants of the examples and the Nina test programs: deleted tokens, arguments and lines, swapped identifiers, changed types, renamed functions and structs, `let`->`var`, number->string, operators, bad labels, forbidden constructs in a `kernel func`):
  `--analyze` 8,000: 7,975 agree (2,900 accepted, 5,075 rejected with identical text), 25 differ; `--analyze --strict` 4,000: 3,990 agree, 10 differ; `--emit-hir-json` 4,000: all agree; `--emit-msl` (kernel programs) 4,000: all agree.
  Left in the differential: only Nina aborts (`index N out of bounds (size M)` in the Nina checker, exit 134, no output: a UFCS call with too few arguments, a variant case out of range) where C++ reports an ordinary error: nothing to match. The 26 generics + interfaces differences are gone (Interfaces.cpp checks instance units).
* **Speed of the additions** (in-process, 120 examples, 493 KB): the lint takes 0.085 ms for all of them (0.7 us a program); parse 1.7 ms and check 3.4 ms in total. As processes, `polyir-check f.poly --analyze` over the 120 examples takes 0.24 s, the Nina `polyir f.poly --analyze` 4.8 s (20x).
* **Speed** (`polyir-check f.poly --bench N`, parse and check in-process, Release build on this machine): the 29 core examples take 0.02 ms each on average (0.63 ms for all of them together, 86 KB);
  the largest, a 27 KB declaration file, 0.18 ms; a synthetic 6,800-line core program (181 KB, 800 functions) parses in 0.87 ms and checks in 1.12 ms (91 MB/s) with HIR identical to the Nina CLI's, which takes
  1.1 s for it. The Nina CLI takes 11 ms on an average example, nearly all of it starting up, and `polyir-check f.poly --hir` as a process about 2 ms; in-process the C++ front end is three orders of magnitude faster.
