# polyir: everything it can do

A catalogue of every capability, with the flag or syntax that reaches it and where it is described. Status is `done` (built and
tested), `native` (needs a native target, not the JIT) or `limit` (works with a stated restriction). The README is the full
description; this page is the index. The argument for polyir — what it offers, why, and the gap — is [docs/why-polyir.md](why-polyir.md).

## 1. The language

| Capability | Syntax / flag | Status |
|---|---|---|
| Functions, typed parameters, one return type | `func f(a: i64) -> i64 { ... }` | done |
| `let` (fixed name), `var` (movable name), `val` (fixed name over a copy) | `let a: T = e`, `let a = e` (the type is the value's), `var`, `val` | done |
| Control flow | `if`/`else`, `for (init; cond; step)`, `switch`, labels with block arguments (`label`, `goto`, `branch`) | done |
| Exceptions | `try { } catch (e) { }`, `throw` (setjmp/longjmp; wasm exceptions under `--run --target wasm`) | done |
| Closures | `closure(x: i64) -> i64 { ... }`, captured by reference | done |
| Coroutines | `coro func`, `yield`, `.resume()`, `.done()`, `.close()`, `v.yielded()` (a yield that answers whether the coroutine was abandoned there) | done: native, jit, wasm, interp and every source target |
| Loops over a coroutine | `for (x in gen) { ... }`: resumes `gen` each turn, closes it on `return` | done |
| Structs | `struct P { x: i64 }`, `struct P(x: i64, var y: i64);` | done |
| Struct methods | functions inside the struct body, called `p.m()` | done |
| Generics | `struct Box<T>` (monomorphized) | limit: structs only |
| Interfaces | `interface`, `struct S : I`, dynamic dispatch through tables | limit: no `==` on interface values |
| Imports | `import "file.poly"` (one flat namespace) | done |
| Externals / C calls | `extern func` | done |
| Globals | `global x: i64 = 0` | limit: scalars |
| Meta declarations | `meta "k" = "v"`, `meta name "k" = "v"` | done |

## 2. Types

| Group | Types |
|---|---|
| Numbers | `i1`, `i8`, `i16`, `i32`, `i64`, `float`, `double`, `char` (an `i8`) |
| Pointers and vectors | `ptr<T>`, `vec<T,N>` (SIMD), `T[N]` (stack array) |
| Strings | `string` (NUL-terminated bytes), `+` joins, `==` compares characters |
| Collections | `array`/`mutarray`, `map`/`mutmap`, `set`/`mutset`, `buffer<T,S>`, `sparse`, `grid`, `tensor` |
| Sums | `variant<A,B,...>`, `either<Ok,Err>` |
| Functions | `func(...) -> T` (pointer), `closure(...) -> T`, `coro<T>` |
| User | structs (value or reference), interfaces |

## 3. Operators and meaning

`+ - * / %`, bit operators, shifts, comparisons, `&&`/`||`; **`==`/`!=` compare what values hold** (structural, any order for
maps and sets) and **`===`/`!==` ask for the same object**; `val x` copies; `x.copy()`, `x.freeze()`, `x.thaw()`; `x.with(...)`,
`x.has(k)`, `s.add(x)`; casts `x.toI64()` etc.

## 4. Memory

| Capability | Behaviour |
|---|---|
| Value vs reference | numbers, `vec`, immutable structs are values; structs with a `var` field, collections, closures, interface values are references |
| Automatic release | private containers, structs, strings, closure boxes, variant payloads, interface copies, coroutines, results of calls, owned slots, temporaries are freed when nothing else can see them |
| Not released | shared objects (returned, stored, captured, kept by a callee), string literals, functions with labels/jumps |
| Allocators | libc `realloc`/`calloc`/`free` natively; a built-in free-list allocator on wasm |

## 5. Outputs

| Output | Flag | Notes |
|---|---|---|
| LLVM IR text | `--emit-llvm` | the common currency |
| Native executable | default, `--out` | `clang` |
| Shared / static library | `--shared`, `--static-lib` | |
| JIT | `--jit --entry f --args "..."` | no GPU host calls |
| WebAssembly | `--emit-wasm` | clang + wasm-ld; allocator and string functions included |
| Run anywhere | `--run --target T` (interp, jit, native, wasm, java, kotlin, js, ts, python, swift, c, cpp, objc, rust), `--run --all` compares them | done; a missing tool says how to install it |
| Project file | `Polyfile` (`name`, `main`, `entry`, `args`, `target`, `targets`), `--project DIR` | done |
| Metal / Vulkan GLSL / SPIR-V | `--emit-msl`, `--emit-glsl`, `--emit-spirv` | the `kernel func`s only |
| Check only | `--check`, `--strict` | |
| Debug / meta | `--meta`, `--debug`, `--source-map F`, `--debug-json F` | see docs/debug-and-meta.md |

## 6. GPU kernels

`kernel func` with parameter roles `index`, `uniform`, `writable`, `workgroup`; 1-D and 2-D launches; atomics (integer, float,
min/max); workgroup memory and barriers; math library; the same kernel compiles to Metal, Vulkan GLSL and SPIR-V (written
directly). Host libraries `ideas/gpu/Metal.poly`, `ideas/gpu/Vulkan.poly` (under review, not the standard library) launch them.

## 7. Libraries written in polyir (`core/stdlib/`, `ideas/`)

The standard library (`core/stdlib/`): Math (`sign`, `fract`, `clamp`, `mix`), the CPU side of the GPU functions and Text,
all three built in (a prelude pulled in on demand). Under review in `ideas/`, not the standard library: waveforms, sound
buffers, filters (biquad, convolution, resampling, reverb), FFT (CPU and GPU), WAV reading, the Metal and Vulkan host
runtimes, streams and live audio (macOS, Linux, Windows).

## 8. Tooling

| Tool | What it does |
|---|---|
| `polyir` | the compiler CLI (all flags above) |
| `polyird` | a long-running daemon speaking a line protocol (header `<id>\t<path>\t<lineCount>`) for editors |
| `polyir-lift` | the reverse direction: LLVM IR / wasm → Nina IR (see docs/portability.md) |
| VS Code extension | grammar for `.poly`, an IR preview pane |
| Lints | unused `var`, with `--strict` to fail on them |
| Diagnostics | `file:line:col: message`, with the file of an imported function |
| Tests | `lang/tests`: `*Tests.poly` run on the JIT, diagnostics, release (`free` counts), debug, golden IR for every example, perf gate |

## 9. Limits to know about

Generic structs only (no generic functions); `==` is not yet defined on `sparse`/`grid`/`tensor`/interface values; `mutmap`/`mutset`
grow as they fill; `buffer`, `map` and `set` are reserved words; one flat namespace.
