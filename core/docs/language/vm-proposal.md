# Proposal: native PolyIR interpreter and PBC bytecode

> The Nina implementation this document refers to (`lang/`, its CLI `polyir-lang` and its sweeps) is no longer part of
> this repository; the goldens it recorded are still checked here.

Status: draft — superseded for the interpreter: the Nina-written VM described below (`lang/src/vm/`, `src/run/Interp.nina`)
was removed on 2026-10-28. The `interp` target is libpolyir's C++ bytecode VM (`runtime/src/vm`), reached from the
Nina-written commands through `runtime/nina/` (the checked HIR written as `.pbc` bytes, run in process). The `.pbc` format
and `src/pbc/` stay.

## Summary

Add two things to the native `polyir` toolchain, in order:

1. **An HIR interpreter** — run PolyIR (and, through `ninac`'s HIR JSON,
   Nina) programs directly, with no LLVM anywhere in the path:
   `polyir run file.poly --entry gcd --args "48 18"`.
2. **PBC** ("PolyIR ByteCode") — a compact binary serialization of checked
   HIR: PolyIR's `.pyc`/`.class` file. `polyir build f.poly -o f.pbc`, then
   `polyir run f.pbc` skips parsing and checking entirely.

This is deliberately native-first. An earlier, larger sketch (a portable VM
emitted into every target language) is deferred: the interpreter built here is
a strict subset of that design, so nothing is closed off (see "Future
directions").

## Gaps this fills

| Gap today | With this proposal |
|---|---|
| Running anything requires LLVM (`--jit` links libLLVM: heavy dependency, slow startup) | `polyir run` is a small static binary; millisecond startup; shebang scripts (`#!/usr/bin/env polyir run`) |
| 13+ source backends, no ground truth for what a program *should* output | the interpreter is the semantic oracle: differential tests check every backend (and both lifters) against it |
| No compile-time evaluation | the compiler can fold constants and evaluate global initializers by calling its own interpreter |
| `libpolyir` embedders must go through LLVM to execute anything | a `polyir_eval()`-style C ABI entry point: plugins, config-as-code, hot reload |
| Scripts re-parse and re-check on every invocation; distribution means shipping source | `.pbc`: checked once, cached like `.pyc`, shipped as one self-contained verified artifact |
| Debugging means reading generated C++/LLVM | deterministic traps with HIR-level stack traces |

## Why this is cheap: the pipeline already exists

Verified against the current toolchain with a simple `gcd.poly`:

- `--emit-hir` prints the checked, fully **typed** HIR
  (`(Rem:i64 a b)` — every node carries its `HType`).
- `--emit-hir-json` prints a complete, self-contained serialization of the
  checked program (1.8 KB for `gcd`), and `--from-hir-json` reads it back and
  feeds every backend. **PBC is a binary, interned sibling of this existing
  format** — `PbcWrite`/`PbcRead` are mechanical siblings of `HirJson*`,
  validated by round-tripping through `HirStructuralEquality`.
- Control flow is already bytecode-shaped: the surface language has no
  `while`/`for` statements — loops are `label loop(a: i64, b: i64):` /
  `goto loop(b, r)` / `branch cond ? t(...) : f(...)` (`HLabel`/`HGoto`/
  `HBranch` in `hir/HirNodes.nina`), i.e. basic blocks with block arguments.
  An interpreter needs a jump table per function, not CFG analysis.
- Container, atomic, variant, either, optional and coroutine operations are
  `HBuiltin` enum cases; the interpreter dispatches on enums exactly like
  every existing emitter.

So: `.poly` → parse/check → HIR → **serialize = `.pbc`**, and
**interpret = run**. Both arrows start from a structure the compiler already
produces and already knows how to serialize.

## Deliverables

### Deliverable 0: Nina `using` ergonomics (directory-aware `using`)

Prerequisite quality-of-life work in `ninac`, motivated directly by this
project: the VM/PBC code lands as ~10 sibling files in `lang/src/vm/` and
`lang/src/pbc/`, and under today's rules each file must `using`-introduce
every sibling symbol it touches with a bare module name that tells a reader
nothing about *where* the symbol lives (existing `lang/src` files carry 20+
such `using` lines).

Current state (verified):

- `using Module.*` **already exists** (`usingDecl: USING qualifiedName
  (DOT STAR | AS identifier)? (FROM StringLiteral)?` in
  `ninac/ast/grammar/NinaParser.g4`) and is used throughout `lang/src`
  (`using Polyir.*`, `using PolyirCompiler.*`).
- `using X from "..."` exists with **provenance** meaning: the string is a
  dependency/origin label (`using Gfx from "gfx@2.1"`).
- The std is already **ambient**: std modules and prelude names need no
  `using` (seeded as a sealed bottom scope in `StdLibrary.cpp`). Nothing in
  this deliverable touches std behavior.
- What does *not* exist: any implicit scope between files of the same
  directory, and any directory-qualified `using`.

The readability problem: a bare `using PolyirCompiler` gives a plain-text
reader (diffs, code review, grep — anywhere ninad isn't running) no hint of
where the module lives: this directory? a sibling? a dependency? JS/TS import
paths demonstrate the fix — the introduction line should double as a map.

Proposed model:

```text
std                    ambient (unchanged)
same directory         ambient — siblings are one unit, no line needed (new)
other directories      using * from "./hir"            all exposed members (new)
                       using HirNodes from "./hir"     one module, path shown (new)
                       using gpu.PolyirKernelFloat     dotted shorthand: lowercase
                                                         leading segment = relative
                                                         directory (new)
project libs / deps    using Gfx from "gfx@2.1"        provenance (unchanged)
                       bare `using Module`             = NOT local: a project/lib
                                                         module (meaning sharpened)
```

Disambiguation is by string shape, not new grammar: a `from` string starting
with `./` or `../` is a **directory path** (relative to the file's own
directory); any other string keeps the existing **provenance** meaning. The
existing `usingDecl` rule already parses every form above.

The dotted shorthand `using gpu.PolyirKernelFloat` rides on the same
`qualifiedName` rule and is disambiguated by Nina's own naming convention:
modules are PascalCase, directories lowercase — a lowercase leading segment is
a relative directory (equivalent to `using PolyirKernelFloat from "./gpu"`),
a PascalCase one is a module path, exactly as today. Multi-level nesting
composes (`using hir.check.PolyirChecks`).

Rationale:

- *Same-directory ambience*: directories are already logical units in the
  build machinery (directory modules); within one, the `using` gate is
  mechanical noise ninad's quick-fix inserts anyway. Precedent: Go packages,
  Java package-scope.
- *Directory-qualified `using` for everything else local*: the source file
  becomes self-documenting — every cross-directory name carries its location,
  readable without an IDE. The gate (explicit dependency graph) is not
  weakened; it is strengthened with location information.
- *Bare `using Module` narrows to non-local*: reading `using Foo` then means
  "external to this tree", restoring signal to the shortest form.

Resolution rules: same-directory ambient names sit in a scope *above* std and
*below* the file's explicit `using`s; an explicit `using`/alias always wins;
an ambiguity between two same-directory symbols of the same name is already
`sema.duplicate` today (one FQN space), so no new conflict class appears. A
path `using` that names a module not actually found in that directory is an
error (`sema.using-path`), keeping the paths honest as documentation.

Migration: existing bare `using` lines that name local modules keep working
(warning tier at first), with a ninad quick-fix that rewrites them to the
path-qualified form or deletes them when same-directory ambience covers them.

**Style rule — hoist repeated member qualification into `using`.** Nina
already supports member-level introduction (`using Module.member` brings one
member bare, and `using Module.member as name` renames it) — but bodies full
of `PolyirKernelFloat.fixed(v)` remain common because nothing nudges toward
it. Add a `nina style` rule — **implemented as `hoist_member_using = N`** in
`style.nina.properties` (opt-in; the value is the call-site threshold, `3` is
a good default): when a file qualifies the same module member at N or more
call sites, the first use is reported with a suggestion to hoist it to the
top of the file:

```nina
using PolyirKernelFloat.fixed              // member, bare
using PolyirKernelFloat.fixed as fixedRounding  // member, renamed for clarity
```

so bodies read `fixed(v)` / `fixedRounding(v)` and the file's dependencies
are summarized in its `using` block instead of scattered through expressions.
The rule is advisory (style tier, not sema), suppressible per line, and ninad
offers the hoist as a quick-fix that rewrites all call sites.

Touch points: AST builder (`UsingDecl::fromSpec` gains a path/provenance
discriminant), sema (`SymbolTable::introducedIn`, `Resolver::resolveHead`,
`checkUsingProvenance`), no IR change (`using` resolves away before codegen);
ninad's `UsingScope`/completion updated to offer path-qualified inserts and
stop suggesting same-directory ones.

**Status**: implemented in ninac — same-directory ambience, `using X from
"./dir"` / `from "../dir"`, the dotted sugar (`using hir.HType`), dir
wildcards, honesty checks, and a *sibling-reach fallback*: a `./dir` spec
that misses under the file's own directory matches any source directory
whose path ends with the segments, so `using hir.HType` works from
`src/vm` (docs/modules-and-namespaces.md). Piloted across `src/pbc`,
`src/run`, and `src/vm`, which also moved to a class/builder shape:
`PbcOut`/`PbcIn` (byte builder with chained `put`s, byte walker),
`PbcWriter`/`PbcDecoder` (per-program encoder/decoder over them), and
`VmHeap` as a class (`m.heap.alloc(...)`) — wire bytes unchanged, verified
by the C++ `PbcTest` cross-check and the differential sweep.

### New code (Nina, inside `lang/`)

```text
lang/src/vm/
  VmValue.nina   runtime value: I64/U64 (slot type gives width + signedness),
                 F32/F64, Bool, Str, Ref (heap object), FuncRef, Closure,
                 Coro, optional-absence
  VmFrame.nina   call frames: typed local slots, label jump table; goto/branch
                 write block arguments into the target label's parameter
                 slots then jump (the same phi-edge model as the LLVM backend)
  VmHeap.nina    structs, containers (darray/map/set/sparse/grid/tensor/
                 buffer), closure capture boxes — implemented with Nina
                 containers, so memory management is inherited;
                 Free/FreeBox/Release follow the same rules the backends use
  VmMachine.nina the machine (program, heap, globals): `machine(decls)
                 .run(entry, args)`; calls by name and through values;
                 `evalThen`/`evalAllThen` continue a flow with its value(s)
  VmExecutor.nina  statements: labels/goto/branch/switch, loops, try/throw
                 (including empty-catch-name cleanup semantics)
  VmEvaluator.nina expressions; VmAssigner.nina assignment targets;
  VmBuiltins.nina  HBuiltin dispatch, interface vtable calls (IfaceBox/IfaceCall)
  VmCoro.nina    native green frames for CoroResume/CoroDone/CoroYield/
                 CoroClose — no per-target coroutine lowering involved
lang/src/pbc/
  PbcFormat.nina section ids, node tags, version constants
  PbcWrite.nina  checked HIR -> .pbc bytes
  PbcRead.nina   .pbc bytes  -> checked HIR
```

### CLI (`lang/cli`, `tools/poly`)

As implemented (flags beside `--jit`, no subcommands), plus a dedicated
runtime command:

```sh
polyir FILE.poly --interp --entry NAME [--args "…"]   # parse+check+interpret
polyir FILE.poly --emit-pbc                           # write FILE.pbc (self-verified round trip)
polyir FILE.pbc  --interp --entry NAME [--args "…"]   # load+interpret, no parsing or checking
polyir FILE.pbc  --emit-hir-json | --emit-java | …    # a .pbc feeds the source outputs too
polyir --from-hir-json F.json --interp | --emit-pbc   # HIR JSON runs in the VM / becomes bytecode too

poly FILE.pbc [--entry NAME] [--args "…"]             # the runtime: run bytecode in the VM —
                                                      # polyir's `python` to .pyc, `java` to .class
```

`poly` (`tools/poly`) is a small separate binary linking the same compiled
library; it carries none of the compiler's flags. Transparently caching a
`.pbc` beside the source (mtime check, Python-style) is deferred.

### `libpolyir` (`cpp/`, later phase)

C ABI additions mirroring the Nina implementation: load a module (from HIR
built with `PolyIRBuilder`, HIR JSON, or `.pbc` bytes), call a function, read
the result — no LLVM linked.

## PBC format (v1 — implemented)

Implemented in `lang/src/pbc/` (`PbcFormat.nina`, `PbcWrite.nina`,
`PbcRead.nina`). Simpler than the original sketch: one section, no interning
— the section framing leaves room to add interned tables later without a
version bump.

```text
magic      "PBC\1"            (bytes 80 66 67 1)
u32        version            (raw little-endian; 1)
section*   (id: u8, byteLength: varint, payload)

  1 DECLS      count, then every HDecl pre-order
```

- All integers (counts, indices, values, bit widths, span fields) are
  **zigzag LEB128** varints; tag bytes are raw single bytes.
- Strings are a code-point count followed by one varint per code point.
- Floats are the C99 hex text `PolyirHirJson.floatText` writes (bit-exact,
  the same spelling HIR JSON uses).
- Operators (`HBinOp`, `HUnOp`, `HBuiltin`) are indices into the frozen
  orders of `PolyirHirOps.allBinOps()` / `[Neg, LogNot, BitNot]` /
  `PolyirHirOps.allBuiltins()`.
- Spans carry line and column only (the same fidelity as HIR JSON).
- Bodies are a pre-order encoding of `HStmt`/`HExpr`: one tag byte per node
  (tags frozen at v1), then the node's type, span and fields.

Versioning: tag values and section ids are frozen at v1; readers skip unknown
sections and refuse other versions. Because PBC serializes *checked* HIR, it
is closer to `.class` (verified, typed) than `.pyc` (cached parse).

Losslessness: every `--emit-pbc` decodes its own bytes back and compares the
HIR JSON of both before the file is written — each write is a round-trip
test. A `.pbc` can still be emitted to any of the source backends
(`--emit-hir-json`, `--emit-java`, …), exactly like `--from-hir-json`.

## Execution semantics

- **Traps are defined and deterministic**: `OptionUnwrap` on absence, bad
  variant unwrap, division by zero, index out of bounds (honoring
  `--unchecked`, per `docs/safety-checks.md`). Determinism is what makes the
  interpreter usable as an oracle.
- **Unsigned arithmetic**: slot types carry `HUInt`; division, remainder,
  shift, comparison and widening follow the unsigned rules, as `HirTypes.nina`
  specifies.
- **Host declarations** (`HHostType`/`HHostMember`): the native interpreter
  binds only what a registry provides; unbound host references trap with a
  clear message at first use.
- **GPU kernels** (`HKernel`, `HMat`): carried opaquely, trap if called.
  Out of scope for interpretation in v1.

## Roadmap

0. **`using` ergonomics** — ✅ DONE. Directory-aware `using` in `ninac`
   (Deliverable 0): same-directory ambience plus path-qualified
   `using * from "./dir"`,
   so the VM/PBC sibling files are written without `using` preambles and
   cross-directory introductions show their location.
1. **Interpreter as oracle** — ✅ DONE. `VmInterp` tree-walks checked HIR
   in-process; spelled `polyir FILE.poly --interp [--entry NAME] [--args "…"]`
   (a flag beside `--jit`, not a `run` subcommand). Differential-tested:
   every zero-arg `func` in `examples/` runs under `--interp` and `--jit`
   and the outputs agree (66 functions), except documented v1 limits —
   `malloc`-based examples (atomics), coroutines (interp traps cleanly;
   note the JIT itself crashes on them), `getchar`, host types, and
   non-scalar returns (the JIT prints a raw pointer; the interpreter prints
   `<object #N>`). Floats are carried at double precision, so f32 libm
   externs (`powf`/`sinf`/…) can differ in the last bits.
   *Value lands here; everything after is additive.*
2. **PBC** — ✅ DONE. `PbcFormat`/`PbcWrite`/`PbcRead` in `lang/src/pbc/`;
   spelled `polyir FILE.poly --emit-pbc` (writes `FILE.pbc`, self-verifying:
   the bytes are decoded back and the HIR JSON compared before the file is
   written) and `polyir FILE.pbc --interp|--analyze|--emit-*` (a `.pbc`
   given as the input file loads as the checked program). Differential
   sweep: all 39 examples emit, and all 77 zero-arg entries print the same
   from `.pbc --interp` as from `.poly --interp`. v1 limits: one DECLS
   section (no interning yet), spans are line/column only. The full `Int`
   range round-trips (the varint is built from exact `%`/`>>` groups — `/`
   on Int divides as Double and would lose the low bits of a large value).
   `.pbc` caching on plain runs is deferred.
   A dedicated runtime command exists too: **`poly FILE.pbc [--entry NAME]
   [--args "…"]`** (`tools/poly`), a small binary that only loads and
   interprets bytecode — the `Interp`/`Pbc` bridges live in `src/run/` so
   the compiler CLI and the runtime share them.
3. **`libpolyir` embedding** — ◐ STARTED. The PBC format is in `cpp/`:
   `writePbc(Module)` / `readPbc(bytes)` (`polyir/Pbc.hpp`, `src/Pbc.cpp`),
   byte-compatible with the Nina implementation — `tests/PbcTest.cpp`
   round-trips every example the C++ front end accepts and reads the Nina
   CLI's `--emit-pbc` bytes, comparing the HIR JSON of both. Still to come:
   C ABI execution entry points (load a `.pbc`, call a function, read the
   result — no LLVM linked).
4. **Nina scripting** — `ninac` already emits HIR JSON; teach the pipeline to
   `run` a Nina program end-to-end without LLVM.

## Future directions (explicitly deferred)

- **Portable VM**: emit the interpreter itself through the source backends so
  Java/Python/JS/etc. each get a native PolyIR runtime executing the same
  `.pbc`. The interpreter core (values, frames, dispatch, coroutines, traps)
  built here *is* that VM's core; upgrading later means restricting it to the
  backend-portable Nina subset and adding per-target host registries. Deferred
  until a proven need — WASM covers portable execution today.
- **Performance tier**: flattened register bytecode in a new section (same
  container), inline caches for `IfaceCall`, threaded dispatch. The
  interpreter is the startup/portability tier; `--emit-llvm`/`--release`
  remains the speed tier.

## Risks

- **Interpreter speed** (~10–50× slower than compiled): acceptable for
  scripts, tests, const-eval and embedding; compiled paths are unaffected.
- **Format versioning**: mitigated by frozen v1 tags, skippable sections, and
  round-trip tests in CI.
- **Semantic drift** between interpreter and backends: inverted into a
  feature — the differential test suite makes any drift a test failure.
