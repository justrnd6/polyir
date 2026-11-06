# Debug and meta information

polyir exists to be the back half of other languages: a front end parses and checks its own source, hands polyir a
program, and polyir compiles it to LLVM (native, JIT, wasm), to Java, Kotlin, JavaScript, TypeScript, Swift, C++, Python,
Rust, Objective-C or C, or to a GPU kernel. Whatever a user of that front end then sees — a debugger's breakpoint, a stack
trace, a crash report, a profiler, an IDE's "go to definition", a tool reading the compiled module — must speak in the
front end's own terms: ITS files, ITS lines, ITS names. The generated polyir text is an implementation detail nobody
should have to look at. So a polyir module carries, next to the program, **a description of where everything came from
and what the author wants to say about it**: the source files, the position of every function and statement, the shape
of every struct, and `meta` notes. The program never reads it; debuggers, tools and front ends do.

There is **one description** of a program, independent of any target, and each output carries what it can.

## How the information gets in: three routes

A front end picks the route that matches how it talks to polyir. All three end in the same HIR fields, so everything
below (the JSON, DWARF, the wasm section, `--debug-json`) is the same whichever route was taken.

| Route | Positions | Source files | Meta |
|---|---|---|---|
| **polyir text** (`.poly` written by the front end, compiled by the `polyir` CLI) | the text's own lines and columns, re-mapped by a **source map** (`--source-map FILE`) to the front end's source | the `source` lines of the map (and `import`s) | `meta` declarations in the text |
| **HIR JSON** (what `--emit-hir-json` writes and `readHirJson` in libpolyir reads; how Nina's compiler hands programs to the Nina-written back ends) | `"at": [line, column]` on every expression, statement and function | `"origin": "file"` on each function | — (not carried in the JSON yet) |
| **libpolyir in-process** (PolyIRBuilder, the C API, or a `Module` built by hand) | `Span at` on every `Expr`, `Stmt` and `FuncDef` (line, column, end, byte offsets) | `FuncDef::origin` (`FunctionBuilder::setOrigin`); `CompileOptions::sourceFile` / `sourceDirectory` name the compile unit | — (`RawIr` fragments bring their own DWARF nodes, merged and renumbered) |

What each field is for:

- **`at` / `Span`** — the position a diagnostic or a debugger reports for the node. A statement's span becomes a DWARF line
  (`--debug`); an expression's span is used by diagnostics and by the source back ends (a closure is named after its position,
  `closure_12_5`, so two runs produce the same text). A span of all zeros means "no position" and is simply left out.
- **`origin`** — the source FILE a function was written in, independent of its position. The source back ends group by it:
  the C++ back end writes one unit per origin (`<Origin>.cpp`, with `project.hpp` for what every unit shares), so a
  front end with many files gets a build that recompiles only the unit of the file that changed. A function without an
  origin goes with the compiler-generated ones.
- **`meta`** — free-form `(target, key, value)` notes: a licence, a doc string, a stable identifier for a tool. They are
  about the module or about one function or struct; polyir never interprets them.
- **The description** (functions, structs, sources) — generated from the HIR itself; a front end does not write it.

With `--debug`, polyir puts a `Loc` marker (the `Builtin::Loc` pseudo-statement) in front of every statement that has a
span; the LLVM lowering turns markers into `!dbg` locations and DWARF, every other back end ignores them. A front end
building HIR directly may insert `Loc` markers itself, or just set the spans and let polyir do it.

## What a front end should hand over (checklist)

1. **A span on every statement**, and on every expression that can be the subject of an error or a breakpoint (calls,
   operators, literals the checker may reject). Lines and columns are 1-based and belong to the front end's source.
2. **An origin on every function**, the path the user would recognise (relative to the project), the same string for every
   function of that file.
3. **The source files** — through the source map's `source` lines (text route) or `CompileOptions::sourceFile` /
   `sourceDirectory` (library route), so the compile unit names a file that exists.
4. **Names that survive**: a function's HIR name is what a backtrace shows. A front end that mangles (overloads, generics,
   modules) should keep the user-facing part readable (`Shapes.area` rather than `_ZN6Shapes4areaE…`); `meta` can carry the
   original name when the mangled one must differ.
5. **`meta` for what the tool chain needs**: a stable id per declaration, the language and version that produced the module,
   a doc string — anything a later tool should find without re-parsing the front end's source.

| Output | Meta (`--meta`) | Debug (`--debug`) |
|---|---|---|
| LLVM IR (`--emit-llvm`) | named metadata `!polyir.meta`, `!polyir.function`; the JSON as the constant `@polyir.debug` | everything in `--meta`, plus DWARF: a compile unit, a file per source, a subprogram per function, a location for every statement |
| native code (`--out`, `--shared`, `--static-lib`) | the `@polyir.debug` constant is in the binary | DWARF in the object file (`dsymutil` / `llvm-dwarfdump` read it) |
| JIT (`--jit`) | accepted (carried in the module) | accepted |
| wasm (`--emit-wasm`) | a custom section named `polyir.meta` holding the JSON | the same, plus the DWARF custom sections clang writes (`.debug_info`, `.debug_line`, ...) |
| any other target | `--debug-json FILE` writes the same JSON | the same file |

`--debug` implies `--meta`. Without either flag a module is exactly what it was before.

## What the description holds (JSON, `"polyirDebug": 1`)

```json
{"polyirDebug":1,"producer":"polyir","source":"app.poly",
 "sources":["app.poly","app.nina"],
 "meta":[{"target":"","key":"license","value":"MIT"},
         {"target":"area","key":"doc","value":"Area of a square"}],
 "functions":[{"name":"area","source":"app.nina","line":40,"column":6,"params":["s"],"paramTypes":["Sq"],"returns":"i64","coroutine":false}],
 "structs":[{"name":"Sq","reference":false,
             "fields":[{"name":"side","type":"i64","mutable":false}],"interfaces":[]}]}
```

- **sources**: every file a position can name: the polyir file, the files it imports, and the files a source map names.
- **functions**: each function the program wrote (not the ones the compiler generates), where it starts, its parameter names and types, its return type.
- **structs**: fields with their types and whether they can change, whether a value is a reference, the interfaces implemented.
- **meta**: the `meta` entries below.

## `meta` declarations

```
meta "license" = "MIT"                    // about the module
meta area "doc" = "Area of a square"      // about the function or struct called area
```

They are ordinary top-level items (also in imported files). They have no effect on the program; they appear in the JSON and
as `!polyir.meta` metadata nodes `!{!"target", !"key", !"value"}`.

## Source maps: positions in the front end's own source

A front end that generates polyir text writes a second file next to it and passes `--source-map FILE`. Every position in the
description and in DWARF (function starts, statements) is then a position in the front end's source. The format is plain
text so any language can write it:

```
polyir-source-map 1
source app.nina
source lib/shapes.nina
map <polyir line> <source number> <line in that source> <column>
```

- `source` lines list the files; their numbers count from 0 in order.
- `map` lines are in ascending order of polyir line. An entry covers its own line and every following line up to the next entry.
  On the entry's own line the column is moved along (`column + polyir column - 1`); on following lines the entry's column is used.
- The map applies to the main file only (imported files keep their own positions).

## How it works

1. `meta` items and the program's functions and structs are gathered into one description (`src/DebugInfo.nina`).
2. With `--debug`, a marker (`Loc`) is put in front of each statement in the typed HIR. It carries the (mapped) line, column
   and source. This is target-independent; a GPU or other backend simply ignores it.
3. The LLVM lowering turns each marker into a comment line. `finish` then replaces the comments by `!dbg` locations on the
   instructions that follow, attaches `!dbg` to each `define`, and writes the DWARF metadata.
4. For wasm the JSON is appended as a `polyir.meta` custom section after linking.

## A complete example

`examples/frontend/` is a small front end (`tinyc.py`) for a toy language. `demo.sh` compiles a program in it, builds a native
program and a wasm module with `--debug --source-map`, and shows lldb stopping at `tiny.tl:3` with a backtrace in tiny's own
lines. Read its README first.

## Where the Nina front end stands

Nina's compiler hands polyir HIR (in-process through libpolyir and, for the Nina-written back ends, as HIR JSON). Today it
sets `origin` on every function (that is how a Nina project becomes one C++ unit per source file, and how the orchestrator
and the polyir back ends are split into units) and a span on each closure (for its stable name); it does not yet hand over
statement spans, so a Nina program compiled through `--debug` has subprograms and files but no line table in Nina's
lines, and the Nina `meta` (doc comments, decorator annotations) is not carried. The next step on this road: the lowering
(`ninac/codegen/polyir-lower`) copies each `IrStmt`/`IrExpr` position into `Span at`, so a Nina program debugs in `.nina`
lines exactly as `examples/frontend` debugs in `tiny.tl` lines; then doc comments and annotations as `meta`.

## Limits

- Local variables: the DWARF carries a `DILocalVariable` for each parameter and each `let`/`var`/`val` of integer, float,
  string, struct or array type (a debugger prints `(long) total = 0`, `(char *) word = ... "hello"`, `(Seg) s = { a = (x = 1, y = 2) ... }`).
  A value struct is the structure; a reference struct is a pointer to it. An array (`array`/`mutarray`) is a pointer to its
  header (`data`, `length`, `capacity`), so `p xs->data[0]` and `p *xs` work; a fixed array and a buffer are real DWARF
  arrays. Maps, sets, sparse containers, closures, variants and vectors are not described, and inside a struct or array they
  show as an opaque pointer. Nesting stops at three levels. A struct whose fields include a closure or variant is not
  shown. Struct layout is computed assuming LLVM's natural alignment. Compiler temporaries (names with `$`) are skipped. Each variable lives in a stack slot
  (a `var` already has one; an immutable value is spilled to one) bound with `llvm.dbg.declare`, so it is only exact at
  `-O0`.
- Closures, coroutines and the functions the compiler generates (comparison, hashing, `with`) have no subprogram.
- A statement mapped to a different file than its function keeps its line number but is attributed to the function's file.
