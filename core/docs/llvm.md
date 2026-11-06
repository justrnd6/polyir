# libpolyir: how the HIR compiles to LLVM IR

The LLVM back end (`src/emit/llvm`, `include/polyir/LlvmCompiler.hpp`): `CompileToLlvm(module, options)` turns a checked
`Module` into LLVM IR text that `clang -x ir` assembles. Moved here from the README.

## How polyir compiles to LLVM here

| polyir | LLVM |
|---|---|
| `i1`..`i64`, `u8`..`u64`; `f32`, `f64` | `i1`..`i64`; `float`, `double` |
| string, struct, pointer | `ptr`; a string is a constant, a struct a `malloc`ed heap object shared by reference |
| `+ - *` | `add`/`sub`/`mul` (they wrap) |
| signed `/` `%` | `sdiv`/`srem`, with a divisor of -1 handled so the smallest number wraps instead of trapping; unsigned `udiv`/`urem` |
| `<<` `>>` | the count taken modulo the width, then `shl` / `ashr` (signed) / `lshr` (unsigned) |
| `&&` `\|\|` | short-circuit, with a `phi` |
| conversions | `sext` `zext` `trunc` `sitofp` `uitofp` `fptosi` `fptoui` `fpext` `fptrunc`; to `i1` is `!= 0` |
| a variable | an `alloca` (compile with `-O1`, or run `opt -mem2reg`, to make registers of them) |
| `label`/`goto`/`branch`/`switch` | a basic block per label whose parameters are `phi`s fed by the jumps (a parameter that is assigned or captured stays an `alloca` the jumps store to); `br`, `switch` |
| function `main` | `polyir_main`, plus `i32 @main()` that returns its result as the exit status |

## The HIR and what compiles

The whole typed HIR is modelled, built and printed here; `tests/HirTest.cpp` builds the programs the Nina checker tests use and compares the printed text with the lines copied from `lang/tests/src/Main.nina`. The LLVM compiler covers most of it:

| polyir | LLVM |
|---|---|
| `mutarray<T>` / `array<T>` | a heap header of three cells (length, capacity, elements); `push` grows with `realloc`; an index outside the list stops the program (`llvm.trap`) |
| `T[N]`, `buffer`, `grid`, `tensor` | a zeroed block of the elements, indexed in row-major order with every index checked |
| `vec<T,N>` | an LLVM vector; `shuffle` is `shufflevector` |
| `optional`, `either`, `variant` | a heap block of two cells: the case number, then the value (`optional`: some = 0, none = 1; `either`: ok = 0, err = 1); reading the wrong case stops the program |
| `map`/`mutmap`/`set`/`mutset` | open addressing, doubling at three quarters full; keys hashed and compared by bits, text or fields (`LlvmMaps.cpp`) |
| closure | a block of the body's address and an environment; a name a closure captures lives in a heap cell the closure and the code around it share, so assignments are seen on both sides |
| func value | the function's address; a call through it passes the arguments unchanged |
| interface | a block of the object and a constant table of its methods (`<Struct>_<method>`, in the interface's order); `ifaceCall` goes through the slot |
| `try` / `throw` | no unwinding tables: a throw stores its number in a global and sets a flag, each call of a program function is followed by a look at it, and the caller jumps to its nearest `catch` or returns early; a program with no `try`/`throw` pays nothing; an uncaught throw traps |
| atomics | `load atomic`, `store atomic`, `atomicrmw`, `cmpxchg`, `fence` |
| strings | `strEq` (`strcmp`), `concat`, `len` (`strlen`), `hash` (djb2, as in the Nina version) |
| `===` | pointer comparison |

Memory: every object comes from the garbage collector (`gc_allocate`, src/emit/llvm/runtime/Collector.c), which reclaims what the program can no longer reach; each function that works with references keeps them in a frame record the collector walks, and a function that allocates has a safe point (`gc_poll`) at its start and at every loop head. The collector marks and sweeps in short slices at those safe points (incremental, so no pause grows with the data the program keeps), and every store of a reference into an object calls the write barrier (`gc_write_barrier`) first. An explicit `Free` is only a hint. How it works, what it costs and how it is checked: [memory.md](memory.md), Parts 2 and 3. `CompileOptions::collectGarbage = false` (or a wasm target triple, or `polyir --no-gc`) gives the old behaviour: plain `malloc`, and `Free` frees (a list, map or set its tables and block, a sum its block, a closure its block and its environment).

## Output

The IR reads like clang's `-O0` output (named values, role-named blocks, no compiler names) and can carry DWARF debug info (`--debug --source f.poly`); basic blocks are tidied (`LlvmBlocks.cpp`). See `core/docs/language/llvm-mapping.md`; `lang/targets/java-mapping/llvm-check.sh` checks the checked-in `.ll` files.

## Positions, origins and debug information

A front end that uses the library owes its users positions in ITS source, never in anything polyir generated. The HIR has
the fields for it and the library does the rest:

- `Span at` on every `Expr`, `Stmt` and `FuncDef` (1-based line and column, end position, byte offsets; all zero = no
  position). Set it on the nodes you build (the JSON reader fills it from `"at": [line, column]`). With
  `CompileOptions::debugInfo` the LLVM compiler writes DWARF from them (`LlvmDebug.cpp`): a subprogram per function,
  a line per statement, local variables; `sourceFile` / `sourceDirectory` name the compile unit.
- `Module::fromLines`: a `.poly` text another front end wrote says where its lines came from (`//# from Hops.nina:7:9`, read
  by the front end). The DWARF maps every position through them: each source file its own `DIFile`, a subprogram in the
  file its function's first line came from, a line from another file in a `DILexicalBlockFile`. A debugger then names the
  first source's lines (core/docs/translate.md, "Spans and source maps"). A function with an `origin` is placed in that file.
- `FuncDef::origin` (`fn.definition.origin = file`): the source file of the function. The source back ends group by it
  (one C++ unit per origin), and a tool reading the HIR knows which file a function belongs to without a position.
- `Builtin::Loc`: the statement marker `--debug` inserts ahead of each statement; a front end building HIR itself may set
  spans only and leave the markers to polyir.
- `RawIr` fragments (`AddLlvm`) carry their own debug nodes (`metadata`, `namedMetadata`); the module renumbers and merges them.
- `meta` notes (`meta "key" = "value"` in polyir text) are not yet a `Module` field: a front end on the library that wants
  them writes `--debug-json`-style descriptions itself for now.

The whole story — the three routes positions take, the JSON description, DWARF, the wasm section, a checklist for front
ends and where the Nina front end stands — is [docs/language/debug-and-meta.md](language/debug-and-meta.md);
`core/examples/frontend` is a complete front end debugged in its own source lines.
