# runtime — running `.pbc`

The runtime runs polyir bytecode: a `.pbc` file (the HIR serialized; `core/src/pbc`, `polyir-check F.poly --emit-pbc`,
`ninac bytecode`) or any checked `Module`, in process and with no toolchain — no JDK, no clang, no Node. It is how
`polyir-check --interp` and `poly` run a program, and, in the nina checkout, how `ninai` and `nina up` run Nina scripts
and lifecycle phases and how `nina --vm` and `target bytecode` projects run.

| directory | what is in it | builds |
|---|---|---|
| `include/polyir/Vm.hpp` | the public API: `RunModule(module, options)`, `RunBytecode(bytes, options)`, `RunPbcFile(path, options)`, `BytecodeListing(module)`, and plain-typed doors for C++17 hosts (`RunPbcEntry`, `RunPbcBytesEntry`, `RunPbcBytesOutcome`) | — |
| `src/vm/` | the VM: `Compile.cpp` (HIR → instructions), `Run.cpp` (the interpreter loop), `Ops.cpp` (arithmetic, conversions, text), `Builtins.cpp`, `VmValues.*` (values and heap), `Collector.cpp` (the garbage collector), `Preflight.cpp` (is every extern bound?), the hosts it binds — `NinaRuntime.cpp` (the `nina_*` runtime pieces Nina programs call), `ProcessHost.cpp`, `LifecycleHost.cpp`, `Host.cpp` (`import native` libraries) — and `Listing.cpp` (the instructions as text) | `libpolyir-vm` (and `polyir_vm_static`) |
| `tools/` | `Poly.cpp`, the `poly` command: a `.pbc` file's entry run on the VM (`poly F.pbc [--entry NAME] [--args "WORDS"]`), or meta-build (`poly up`, `poly build java`, …), whose polyir source `MetaBuildText.cpp` holds (written by `tools/metabuild/sync-embed.py`) | `poly` |
| `tests/` | `VmTest.cpp` (the instructions, on programs built with PolyIRBuilder), `NinaRuntimeTest.cpp` (the `nina_*` bindings), `vm_vs_native.sh` (the VM against the native build) | ctest |
| `docs/` | [bytecode.md](docs/bytecode.md) — `.pbc` byte by byte and the VM's instruction set; [memory.md](docs/memory.md) — the VM's collector | — |

libpolyir-vm depends on libpolyir (`../core`: the HIR, `.pbc`). It is built by the CMake project at the root of the
repository:

```sh
cmake -B build -G Ninja && ninja -C build && ctest --test-dir build    # from the repository root
```

## Using it

```cpp
#include <polyir/Vm.hpp>

VmOutcome ran = RunModule(module, VmOptions{});   // ran.result == "6", no toolchain involved
VmOutcome file = RunPbcFile("program.pbc", VmOptions{"main", {}, cout});
```

The VM compiles the HIR to a flat stack-machine bytecode at load (milliseconds), checks that every extern has a
binding, then interprets it. A trap is reported in `VmOutcome::trap`, a program it cannot bind as `declined`. GPU
kernels (`@Kernel`) are not run on it by design — such a program is declined and built for a native target instead.
The text of an `i128`/`u128` is the one documented gap.

## How it is checked

- `VmTest` and `NinaRuntimeTest` (ctest), and core's `PbcTest` (`.pbc` byte compatibility with the Nina implementation).
- `tests/vm_vs_native.sh <build dir>` (from any folder: each program runs in its own, where it finds its fixtures): every `.poly` of
  `core/examples` and `core/tests/programs`, and of `ideas/audio/examples` and `ideas/*/tests` (the libraries under review), through
  `polyir-check --interp` and as a native LLVM build (an entry other than `main` through a
  small wrapper `main`); stdout and exit status must agree, and the VM must not decline a program the native build runs.
- nina's `ninac/codegen/llvm-polyir/tests/targets_via_polyir.sh`: the `bytecode` column runs each Nina program's
  `.pbc` on the VM and compares it with the llvm route; `nina/tests/RunConformanceTest.cpp` (`bytecode` rows) and
  `LifecycleCliTest.cpp` (`nina --vm`, a `target bytecode` project) run the Nina front door end to end.
