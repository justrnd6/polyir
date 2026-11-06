# PolyIR

PolyIR is a typed, portable, high-level intermediate representation (HIR) and its compiler toolchain, written in C++
with programs in its own language, `.poly`. It sits between language front ends and many back ends: a front end
writes `.poly` (or builds the HIR with `PolyIRBuilder`, or hands over HIR JSON), and PolyIR writes the program as
native code, as source in another language, or as bytecode its C++ VM runs. [Nina](../nina) is one such front end,
in a sibling checkout (`SourceCode/nina` beside `SourceCode/polyir`).

## Architecture

```text
PolyIR source (.poly) / PolyIRBuilder / HIR JSON
                    │
          core: the typed PolyIR HIR (the C++ front end and checker)
        ┌───────────┴─────────────────────┬───────────────────────────────┐
        ▼                                 ▼                               ▼
 core: LLVM IR → native          core + targets: another language   core: .pbc bytecode
 (clang), the C ABI              by its construct rules: C, C++,          │
                                 Java, Kotlin, JS, TS, Python,      runtime: the C++ VM
                                 Swift, Objective-C, Rust; Metal,   runs it in process
                                 GLSL, SPIR-V, wasm (C++ writers)
```

## Layout

| Directory | What it is | Builds |
|---|---|---|
| [`core/`](core/README.md) | The compiler library: the typed HIR, `PolyIRBuilder`, the C++ front end, the LLVM compiler (native builds through clang, with the native garbage collector), `.pbc`, the C ABI and polyir as a translate source; the standard library written in polyir (`stdlib/`: Math, Gpu and Text, built in), the example programs (`examples/`), the language reference (`docs/language/`), the test programs and fixtures (`tests/programs/`) | `libpolyir` (+ `polyir_static`), `libpolyir-cli` (`RunPolyirCli`, + `polyir_cli_static`), the `polyir-check` and `polyir-hir` tools |
| `translate/` | The translate library: a source language's tree written by a target's page of construct rules. It knows no AST; a source hands in its canon and its tree (`include/polyir/translate/Source.hpp`), and polyir is one source ([`core/docs/translate.md`](core/docs/translate.md)) | `libpolyir-translate` (+ `polyir_translate_static`) |
| [`runtime/`](runtime/README.md) | The bytecode VM that runs `.pbc` (`src/vm`, `include/polyir/Vm.hpp`) and the `poly` command (`tools/`) | `libpolyir-vm` (+ `polyir_vm_static`), `poly` |
| `targets/` | Each language's construct rules, `<lang>/<Lang>.translate`, with its runtime helpers and its goldens (`tests/`), built into libpolyir ([`core/docs/translate.md`](core/docs/translate.md)); and the C++ writers for GPUs and WebAssembly (`gpu/`, `wasm/`, [`gpu/README.md`](targets/gpu/README.md)) | `libpolyir-lang-targets` (+ `polyir_lang_targets_static`) |
| [`tools/`](tools/README.md) | `metabuild` (a Polyfile → CMake/Gradle/Cargo/npm/pip projects, written in polyir and built into `poly`) and `vscode` (the VS Code extension) | — |
| [`ideas/`](ideas/README.md) | Libraries under review, written in polyir (audio, GPU, …) | — |
| `CMakeLists.txt`, `cmake/`, `ci.sh` | One CMake project for all of it, and the gate a change passes | everything above |
| `style.nina.properties` | The C++ style of the repository, and the style of every `.poly` file, read by the style checker | — |

## Commands

| Command | Built from | What it does |
|---|---|---|
| `polyir-check` | `core/tools` | libpolyir's command line (RunPolyirCli): check, LLVM, native builds, the VM, shaders, `.pbc`, and every target language |
| `polyir-hir` | `core/tools` | HIR JSON printed, or compiled to LLVM IR or a shader |
| `poly` | `runtime/tools` | Runs a `.pbc` on the VM; `poly up` (and meta-build's other goals) runs meta-build |

```sh
polyir-check FILE.poly --check               # check it
polyir-check FILE.poly --emit-llvm           # LLVM IR
polyir-check FILE.poly --out app --run       # a native program, built through clang and run
polyir-check FILE.poly --interp              # run it on the VM
polyir-check FILE.poly --emit-pbc            # FILE.pbc; `poly FILE.pbc` runs it
polyir-check FILE.poly --emit-java           # the file in another language (--targets lists them)
```

A translation only writes the target's source text: one `.poly` file is one file of the target, and building or
running it is that language's own toolchain's job.

## Building

```sh
cmake -B build -G Ninja && ninja -C build && ctest --test-dir build   # libpolyir, libpolyir-vm, libpolyir-lang-targets, libpolyir-cli, the tools, the tests
./ci.sh                                                                # the gate: configure, build, ctest, the negative programs, the style check
```

Every library, tool and test lands in `build/` itself, so `LIBRARY_PATH=build` is all a project linking `polyir`,
`polyir-vm` or `polyir-cli` needs. Nina finds this checkout as `../polyir` next to it: its CMake builds take
`-DPOLYIR_DIR=<path>`, add this folder as a subdirectory with `POLYIR_STATIC_ONLY` and link the static archives.

```sh
# the style check of the C++ (rules in style.nina.properties, here at the root; the checker is built in the nina checkout)
../nina/ninastyle/build/ninastyle core/include core/src core/tools core/examples/Gcd.cpp core/tests/*.cpp core/tests/*.hpp core/tests/consumer \
  runtime targets/gpu targets/wasm targets/include targets/tests/*.cpp
```

## Using the C++ library

```cpp
#include <polyir/Builder.hpp>
#include <polyir/LlvmCompiler.hpp>

PolyIRBuilder b;
FunctionBuilder& gcd = DefineFunction(b, "gcd", {{"a", MakeInt(64)}, {"b", MakeInt(64)}}, MakeInt(64));
WhileLoop(gcd.body, Ne(Local(gcd.body, "b"), IntConst(0)), [&](Block& loop) {
  Let(loop, "rest", Rem(Local(loop, "a"), Local(loop, "b")));
  SetLocal(loop, "a", Local(loop, "b"));
  SetLocal(loop, "b", Local(loop, "rest"));
});
CompileToLlvm(Build(b));
```

The C++ is written as records and free functions: a `struct` holds fields only, and every behaviour is a function
that takes the record first (`DefineFunction(b, …)`, `Let(loop, …)`). The style checker enforces it (`cpp.fields_only`).

A C ABI is available via `core/include/polyir/polyir.h` for other languages (and, for GPU kernels,
`targets/include/polyir/polyir_kernels.h` in libpolyir-lang-targets). See [`core/README.md`](core/README.md) and
`core/docs/PolyIRBuilder.md`.

## Further documentation

- [`core/README.md`](core/README.md) — libpolyir, the C++ library
- [`core/docs/translate.md`](core/docs/translate.md) — the construct rules and the translate library
- [`runtime/README.md`](runtime/README.md) — the VM; `runtime/docs/bytecode.md` — `.pbc` and the instruction set
- [`tools/README.md`](tools/README.md) — metabuild and the VS Code extension
- `core/docs/language/why-polyir.md` — design rationale
- `core/docs/language/llvm-mapping.md` — HIR → LLVM mapping
- `core/docs/language/hir-json.md` — HIR JSON interchange format
- `core/docs/language/debug-and-meta.md` — debug info and metadata
- `core/docs/language/portability.md` — portability guarantees
