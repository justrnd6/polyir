# core — libpolyir, the polyir compiler library

libpolyir is **polyir as a C++ library**: the typed intermediate representation Nina compiles through, the tools to make
it, and the compiler that turns it into LLVM IR (and so native code, through clang), `.pbc` bytes, and a C ABI. It is
C++20 with no dependency beyond the standard library, built as `libpolyir.dylib`/`.so` (and, for embedding,
`libpolyir_static.a`). Its neighbours in this repository: `../runtime` (libpolyir-vm, the VM that runs `.pbc`, and `poly`) and
`../targets` (each language's construct rules, built into libpolyir, and libpolyir-lang-targets, the GPU and wasm writers). This
directory also holds the polyir standard library (`stdlib/`), the example programs (`examples/`), the language reference
(`docs/language/`) and the test programs (`tests/programs/`).
`libpolyir-cli` (`RunPolyirCli`, `src/cli/Cli.cpp`) is the command line over all three, so it is a library of its own.

```
cmake -B build -G Ninja && ninja -C build && ctest --test-dir build     # from the repository root: one project for all the C++
```

| library | what it is | who links it |
|---|---|---|
| `libpolyir` (`polyir`, `polyir_static`) | HIR, builder, front end, LLVM compiler and native builds, `.pbc`, the C ABI | ninac (`polyir_static`: the lowering, the llvm and bytecode targets), `lib/polyir` and Nina programs (`link "polyir"`), the other polyir libraries |
| `libpolyir-cli` (`polyir-cli`, `polyir_cli_static`) | `RunPolyirCli`: every flag of `polyir-check`, with the VM (`--interp`) and the shaders (`--emit-msl` …) | `polyir-check`, the nina front door (`nina/orchestrator/cpp/cli/PolyirCliGlue`) |

## The goal

Nina has many targets (LLVM, the JVM, JavaScript, Python, Swift, C, GPUs …) and should not have to write each of them
from its own syntax tree. polyir is the one place they all meet: a small, fully typed IR — the **HIR** — that every
Nina program is lowered to once (`ninac/codegen/polyir-lower`). From there:

- **ship it**: compile the HIR to LLVM IR (and so to native code or wasm), to a portable `.pbc` file, or to another
  language's source by that language's construct rules (`docs/translate.md`; GPU shaders and the direct wasm writer are
  `../targets`);
- **run it without a toolchain**: the VM (`../runtime`) executes the HIR in-process, which is how `ninai` and `nina up` run Nina
  scripts and lifecycle phases on a machine with no JDK, no clang and no Node;
- **check it once**: the HIR is checked when it is made (the builder refuses ill-typed nodes), so every back end can
  trust what it gets.

The IR once had a second implementation written in Nina, kept in step with this one (identical HIR text, `.pbc` bytes and
errors). It is no longer part of the repository; the goldens it recorded (`tests/data/front_cases.txt`, `tests/data/negative`,
`../targets/tests/data/kernels`) are checked here still.

## How a program flows through it

```
            polyir text (.poly)          Nina source (.nina)            HIR JSON / .pbc
                   │                             │                             │
          front end (src/front)       ninac's polyir lowering        ReadHirJson / ReadPbc
                   │                  (PolyIRBuilder, src/builder)             │
                   └────────────────────────┬────┴─────────────────────────────┘
                                            ▼
                               Module — the checked HIR (include/polyir/Hir.hpp, Types.hpp)
          ┌──────────────┬──────────────┬───┴──────────┬──────────────┬───────────────┐
          ▼              ▼              ▼              ▼              ▼               ▼
     LLVM IR text   wasm binary     Metal / GLSL /   .pbc bytes    HIR text /     the VM: compile
     (emit/llvm)    (../targets/    SPIR-V           (pbc)         JSON (hir),    to instructions
          │          wasm)          (../targets/                   other          and run
     clang → native / wasm           gpu)                          languages      (../runtime/src/vm)
```

Everything in this directory is the left part and the middle: the HIR and the routes to LLVM, `.pbc` and text; the
wasm and GPU writers and the VM are the neighbouring libraries.

## Layout

| directory | what is in it |
|---|---|
| `include/polyir/` | the public API — everything another program includes (table below) |
| `src/hir/` | the HIR's data and its text forms: `Types.cpp` (type constructors and `TypeText`), `Hir.cpp` (node helpers, operator and builtin names), `HirPrint.cpp` (the printed notation), `HirJson.cpp`/`HirJsonWrite.cpp` (JSON in and out), `Json.cpp` (a small JSON reader) |
| `src/builder/` | `PolyIRBuilder`: making a `Module` node by node, with the checks that refuse a wrong one (`BuildError`) |
| `src/front/` | the polyir language front end in C++: lexer, parser, name resolution, the checker (`Check*.cpp`), generics, imports, interfaces, coroutines, kernel verification, and `HirBuild.cpp`, which writes the checked program as a `Module` |
| `src/emit/llvm/` | the LLVM IR back end: functions, labels, containers, maps, sums, closures, interfaces, `try`, coroutines, atomics, debug info (DWARF), block tidying, and the garbage collector native builds carry (`runtime/Collector.c`, frame records and safe points in `LlvmRoots.cpp`; [docs/memory.md](docs/memory.md)) |
| `src/pbc/` | `.pbc`: the HIR as a compact binary file (the format `ninac bytecode` writes too) |
| `src/cli/` | the `polyir` command line as a library function (`RunPolyirCli`, `Cli.cpp`, built as libpolyir-cli) and native builds through clang (`BuildExecutable`, `CliBuild.cpp`, in libpolyir) |
| `src/capi/` | the C ABI behind `polyir.h` (and `WriteContextOutput`, `polyir/CApiExtension.hpp`, through which libpolyir-lang-targets adds the kernel functions of `polyir_kernels.h`) |
| `tools/` | `polyir-check` (the command line over source and `.pbc`) and `polyir-hir` (over HIR JSON) |
| `tests/` | one test program per area (`*Test.cpp`), and `negative_cases.py` (the recorded answers to `data/negative`); `data/` holds goldens, negative cases and the GC programs; `programs/`: the polyir test programs (`*Tests.poly`, the checks written in polyir), `mapping/` (programs that exercise each target's mapping) and their `fixtures/diagnostics/` (programs with deliberate mistakes), run by `../runtime/tests/vm_vs_native.sh` (the tests of the libraries under review are in `../ideas/*/tests`) |
| `examples/` | `Gcd.cpp`: the smallest complete use of the builder; and the polyir example programs the front end, the VM, the native build and the targets are checked on (`*.poly`, `frontend/` — a toy front end, `gcd-project/`; see [examples/README.md](examples/README.md); the GPU and sound examples are in `../ideas`) |
| `stdlib/` | the polyir standard library, written in polyir ([stdlib/README.md](stdlib/README.md)): `Math`, `Gpu` and `Text`, all three built into the compiler (`sync-prelude.py` copies them into `src/front/PreludeText.cpp`). The domain libraries that used to sit here (waveforms, sound, filters, FFT, the GPU and audio host runtimes) are under review in `../ideas`; meta-build's host library is `../tools/metabuild/src/Host.poly` |
| `docs/` | the guides listed at the end; `language/`: the polyir language reference (the guide `polyir-guide.html`/`.pdf` built by `build.py` from `guide.html.in` and `snippets/`, `CAPABILITIES.md`, `hir-json.md`, `llvm-mapping.md`, `debug-and-meta.md`, `safety-checks.md`, `portability.md`, `why-polyir.md`, `vm-proposal.md`) |

## Entry points

### From C++ — `include/polyir/`

| header | what you call | used in this repository by |
|---|---|---|
| `Types.hpp` | `MakeInt(64)`, `MakeStruct("Pt")`, `MakeOptional(t)` …, `TypeText`, `SameType` | everything |
| `Hir.hpp` | the HIR itself: `Expr`, `Stmt`, `FuncDef`, `Module`; `BinOpName`, `BuiltinName` | everything |
| `Builder.hpp` | `PolyIRBuilder`, `DefineFunction(b, …)`, `Let(body, …)`, `Call(b, …)`, `Build(b)` | ninac's lowering (`ninac/codegen/polyir-lower`) |
| `Frontend.hpp` | `CheckSource(text)`, `CheckFile(path)` → `Module` + errors | `polyir-check` |
| `LlvmCompiler.hpp` | `CompileToLlvm(module, options)` → `CompileResult{ir, errors}` | ninac's llvm target (`ninac/codegen/llvm-polyir`) |
| `Pbc.hpp` | `WritePbc(module)`, `ReadPbc(bytes)` | ninac's bytecode target |
| `HirPrint.hpp` | `ModuleText`, `ExprText`, `StmtText` | tests, `polyir-check --hir` |
| `PolySource.hpp` | `WritePoly(module)`: the HIR as `.poly` source, and what it could not spell | `polyir-check --emit-poly`, `polyir-hir --poly`, nina-lite's `ninac --emit-poly` |
| `Translate.hpp` | `FindTarget(name, path)`, `Translate(module, target)`: the program in another language by its construct rules ([docs/translate.md](docs/translate.md)) | `polyir-check --emit-<lang>` |
| `HirJson.hpp` | `ReadHirJson(text)` | ninac's source back ends (`ninac/codegen/polyir-targets`) |
| `CApiExtension.hpp` | `WriteContextOutput(ctx, writer)`: for a library that adds C functions over a `polyir.h` context | libpolyir-lang-targets |
| `Cli.hpp` | `RunPolyirCli(args, out, err)` (libpolyir-cli), `BuildExecutable(ir, exe, release, …)` (libpolyir) | `polyir-check`, the nina front door |
| `Std.hpp` | the short names of the standard library (`String`, `Vector<T>`, `Map`, `move` …) the code uses | the library itself |

A minimal program — build gcd, compile it, or run it:

```cpp
PolyIRBuilder b;
auto& gcd = DefineFunction(b, "gcd", {{"a", MakeInt(64)}, {"b", MakeInt(64)}}, MakeInt(64));
Block& body = gcd.body;
WhileLoop(body, Ne(Local(body, "b"), IntConst(0)), [&](Block& loop) {
  Let(loop, "rest", Rem(Local(loop, "a"), Local(loop, "b")));
  SetLocal(loop, "a", Local(loop, "b"));
  SetLocal(loop, "b", Local(loop, "rest"));
});
Return(body, Local(body, "a"));
auto& entry = DefineFunction(b, "main", {}, MakeInt(64));
Return(entry.body, Call(b, "gcd", {IntConst(48), IntConst(18)}));
const Module module = Build(b);

CompileResult native = CompileToLlvm(module);   // native.ir: LLVM IR text for `clang -x ir`
VmOutcome ran = RunModule(module, VmOptions{}); // ran.result == "6", no toolchain involved (libpolyir-vm, ../runtime)
```

A mistake while building (an unknown name, an `i64` added to an `i32`) throws `BuildError`; `CompileToLlvm` never
throws and reports in `errors`; the VM reports a trap in `VmOutcome::trap` and a program it cannot bind as `declined`.

### From C and from Nina — `include/polyir/polyir.h`

The same builder and compiler behind about 140 C functions on plain `int64_t` handles, for any language that can call C.
A Nina program uses it with `import cpp "polyir/polyir.h"` (`examples/polyir-cpp-dylib` and `lib/polyir` at the top of
the repository):

```nina
import cpp "polyir/polyir.h"

@Entry
action main() {
    let ctx = polyir_context_new()
    let i64 = polyir_type_int(ctx, 64)
    let main = polyir_function_begin(ctx, "main", i64)
    polyir_ret(ctx, polyir_function_body(ctx, main), polyir_const_int(ctx, i64, 42))
    if (polyir_compile(ctx) == 1) { println(polyir_output(ctx)) }
}
```

The C ABI is snake_case on purpose (its callers are other languages). `tests/DylibTest.cpp` loads the library with
`dlopen` and calls it by name.

### From the command line — `tools/`

```
polyir-check F.poly --emit-hir | --emit-hir-json | --emit-poly | --emit-llvm | --check | --analyze | --ast | --bench N
polyir-check F.poly --emit-msl | --emit-glsl | --emit-spirv
polyir-check F.poly --emit-<lang> [--target-rules PATH]                      by targets/<lang>/<Lang>.translate (docs/translate.md): c, cpp, java, js, kotlin, objc, python, rust, swift, ts
polyir-check --targets | --new-target NAME | --update-target NAME            the construct-rule pages
polyir-check F.poly [--debug] [--release] --out EXE | --run [-- args…]      native, through clang
polyir-check F.poly|F.pbc --interp [--entry NAME] [-- args…]                 on the VM, no toolchain
polyir-check F.poly|F.pbc --vm-listing                                       the VM's instructions as text
polyir-hir   F.json [--llvm | --poly | --msl | --glsl | --spirv]             HIR JSON from the Nina front end (or any other)
```

`polyir-check` is a thin wrapper over `RunPolyirCli`, the same function the nina front door calls.

## The bytecode

`.pbc` — the file `polyir-check F.poly --emit-pbc` and `ninac bytecode` write — is the HIR serialized (`src/pbc`,
`WritePbc`/`ReadPbc`). The VM that runs it is `../runtime`
(libpolyir-vm): it compiles the HIR to a flat stack-machine bytecode at load and interprets it. How both look, byte by
byte and instruction by instruction, is [../runtime/docs/bytecode.md](../runtime/docs/bytecode.md).

## Checks

- `ctest --test-dir build`: one doctest program per area — the builder (`BuilderTourTest`, `HirTest`), LLVM programs
  compiled, assembled and run (`CompileTest`, `ContainerTest`), the C ABI (`CApi*Test`, `DylibTest`), the front end
  (`FrontParseTest`, `FrontCheckTest`), `.pbc` (`PbcTest`, `HirJsonWriteTest`), the native collector (`NativeGcTest`,
  against the VM), debug info and the command line (`CliTest`, which also runs `examples/modules.poly` and checks the module,
  `internal`, `if` value and generic misuse messages), nested structs, defaults, annotations and `@Builder` (`BuilderTest`), and every
  example written back as .poly and read again (`RoundTripTest`), the construct rules (`TranslateTest`, `RuleFeaturesTest`,
  `TranslateCoverageTest`, and `TargetGoldenTest`: every target's translation of the corpus against its golden); the VM's
  tests are `../runtime/tests`, the kernels' and wasm's `../targets/tests`, all in the same ctest.
- `../ci.sh [build dir]` is the whole gate: configure, build, ctest, the negative programs (`tests/negative_cases.py`) and
  style (the C++ must print nothing). `../runtime/tests/vm_vs_native.sh` runs every polyir example and test program
  (core's, and those of `../ideas`) on the VM and as a native build and compares them.

## Style

`../style.nina.properties`, enforced by `ninastyle core` (run from the repository root): records hold fields only and
behaviour is free functions taking the record first; functions of at most 16 code lines through named helpers; 2-space
indentation with namespace bodies indented; no namespace-qualified names — `Std.hpp` introduces the standard ones
once. Code reads as short paragraphs (nina's docs/style.md "Reading rules"): every `if`, `else` and loop body is
braced, an `if` chain, a loop and a `switch` have a blank line before and after them, and the final `return` has one
before it. In `src/emit/llvm/LlvmCollector.cpp` the rules cover the hand-written C++; the IR between its GENERATED
markers sits in a raw string that `ninastyle` never edits, and `runtime/gen_collector_ir.sh` rewrites only that part.

## What is not here yet

- **Front end**: the C++ checker covers the core language; `docs/frontend.md` lists the areas it does not cover yet (a
  front end may hand those over as HIR JSON, which `polyir-hir` reads).
- **LLVM**: the debug-only builtins (`Loc`, `Var`) do not compile; coroutines use `llvm.coro.*` and so need clang/llc.

## Further reading

- [docs/PolyIRBuilder.md](docs/PolyIRBuilder.md) — the builder, as a guided tour (every program in it runs in `tests/BuilderTourTest.cpp`)
- [../runtime/docs/bytecode.md](../runtime/docs/bytecode.md) — `.pbc` and the VM's instruction set
- [docs/llvm.md](docs/llvm.md) — how each HIR construct becomes LLVM IR, the output's shape, positions and debug info
- [docs/memory.md](docs/memory.md) — the garbage collector of native builds (the VM's is [../runtime/docs/memory.md](../runtime/docs/memory.md))
- [docs/translate.md](docs/translate.md) — the construct rules: a language's page, and the engine that applies it
- [../targets/gpu/README.md](../targets/gpu/README.md) — Metal, GLSL and SPIR-V output
- [docs/frontend.md](docs/frontend.md) — the C++ front end and what it covers
- [docs/language/](docs/language/why-polyir.md) — the language reference (the guide, CAPABILITIES.md, HIR JSON, LLVM mapping)
- [nina/docs/polyir-project.md](../../nina/docs/polyir-project.md) — polyir in the Nina project as a whole
