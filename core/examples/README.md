# polyir examples

Plain `.poly` source files — run them with `polyir-check` (`../../build`, from `cmake -B build -G Ninja && ninja -C build`
at the repository root). From this directory:

```
../../build/polyir-check structs.poly --check
../../build/polyir-check structs.poly --emit-llvm
../../build/polyir-check structs.poly --interp --entry distFromOrigin -- 3 4
../../build/polyir-check structs.poly --out /tmp/structs && /tmp/structs
```

| File | Feature | Try it |
| --- | --- | --- |
| `structs.poly` | SSA-pure structs (`insertvalue`/`extractvalue`) | `structs.poly --jit --entry distFromOrigin --args "3 4"` → `25.0` |
| `labels.poly` | explicit `label`/`goto`/`branch` (block arguments, not hand-written phi) | `labels.poly --jit --entry gcd --args "48 18"` → `6` |
| `control-flow.poly` | `if`/`else` sugar — `var` for ordinary reassignment on one or both branches, `let` for the no-mutation case | `control-flow.poly --jit --entry maxVal --args "3 7"` → `7` |
| `buffers.poly` | `ptr<T>` + `for` (real memory, an LLVM constraint) | `buffers.poly --check` / `buffers.poly --emit-llvm` (not `--jit`-able yet — see the file) |
| `arrays.poly` | fixed `T[N]` local array — `.len()`, and as a FUNCTION PARAMETER (decays to a plain `ptr`, same as C) | `arrays.poly --jit --entry callWithLocal` → `1004` |
| `operators.poly` | full C operator precedence | `operators.poly --jit --entry bitwisePrecedenceCheck` → `7` |
| `io.poly` | `extern func` — calling real C/libc functions (`putchar`/`getchar`) | `io.poly --out /tmp/io && /tmp/io` |
| `strings.poly` | UTF-8 strings (byte positions), interpolation, escapes, text blocks and `print` | `strings.poly --run --all` |
| `switch.poly` | LLVM's native `switch` instruction — no fallthrough, one required default | `switch.poly --jit --entry dayKind --args "3"` → `1` |
| `match.poly` | `match` as a value and `switch` with block arms — exact keys and alternatives, conditions, `Some`/`None`, `Ok`/`Err`, list patterns with `...rest`, guards; lowered to a switch or an `if` chain | `match.poly --interp` |
| `shapes.poly` + `geometry.poly` | `import "path.poly"` — multi-file programs, flat global namespace | `geometry.poly --jit --entry rectArea --args "3 4"` → `12` |
| `simd.poly` | `vec<Elem,N>` SIMD vectors — `vec[...]` literals, elementwise ops, `extractelement` reads; `vecMulSumViaVar` reduces one with a real `var` accumulator in a loop instead of a hand-unrolled sum | `simd.poly --jit --entry vecMulSumViaVar --args "2.0 3.0"` → `42.0` |
| `globals.poly` | `global name: Type = literal` — persists across calls (needs two `Jit.call*`s in ONE process to show; `--jit` alone only ever calls once) | `globals.poly --jit --entry bump` → `1` (call it again in the SAME process to see `2`) |
| `funcptr.poly` | `ptr<func(Types) -> Type>` — function pointers / indirect calls; a `var` holding one, reassigned conditionally and applied across a loop | `funcptr.poly --jit --entry pickAndApply --args "5 1"` → `6` |
| `collections.poly` | `array` / `map` / `set` (unchangeable; `with` gives a changed copy) and `mutarray` / `mutmap` / `mutset` (change in place), `freeze()` / `thaw()`, `has`, struct keys, `==` on collections | `collections.poly --jit --entry main` → `161295` |
| `mutarray.poly` | `mutarray<T>` growable arrays (a reference: the name holds a pointer, so a function handed a mutarray grows the caller's) — `push`/`len`, grown across a `for` loop's own iterations and conditionally inside an `if`, shared with a helper function, plus the lower-level `label`/`goto`/`branch` way with no loop statement at all | `mutarray.poly --jit --entry sumEvensBelow --args "10"` → `20` |
| `buffer.poly` | `buffer<T,S>` — a fixed-CAPACITY sibling of `T[N]`: a raw `ptr` (no struct, no runtime length), the full buffer `realloc`d once up front, index-only (`arr[i]=val`, no `push`/`.len()`); works as a function PARAMETER too | `array.poly --jit --entry fillToCapacity` → `100` |
| `grid.poly` | `grid<T,R,C>` fixed-size dense 2D array — `g[row,col]`/`g[row,col]=val` over a flat row-major `[R*C x T]` array | `grid.poly --jit --entry fillAndWeight --args "2"` → `328` |
| `tensor.poly` | `tensor<T,d1,...,dn>` — the N-dimensional generalization of `grid`, same flat row-major stride math | `tensor.poly --jit --entry weightedSum3D` → `5868` |
| `sparse.poly` | `sparse<T>` — a dynamically-sized sparse 2D structure (three parallel `mutarray`s, no fixed dims); append-only write, scan-based read, last write wins | `sparse.poly --jit --entry lastWriteWins` → `3` |
| `methods.poly` | Methods (UFCS) — `receiver.method(args)` desugars to `method(receiver, args...)`; `arr.push(x)` as a bare statement persists growth the same way `push(arr,x)` does | `methods.poly --jit --entry pushViaMethodSum` → `6` |
| `mutmap.poly` | `mutmap<K,V>` — real open addressing with linear probing, fixed capacity rounded up to a power of two; `K`/`V` can be `i64` or `string` (content equality via `strcmp`); `.len()`/`.count()`/`.capacity()` | `mutmap.poly --jit --entry stringKeyedWriteAndReadBack` → `1102` |
| `chars.poly` | `char` — a pure `i8` alias with real `'c'` literal syntax; usable as a `grid`/`mutarray` element type | `chars.poly --jit --entry darrayOfCharsRoundTrip` → `1` |
| `ml.poly` | A tiny 2-layer MLP (hand-chosen weights, no training) that solves XOR — `tensor<double,2,2>` weight matrix + genuinely 1-D `tensor<double,2>` bias/weight vectors, `relu`, a thresholded classifier | `ml.poly --jit --entry xorForward --args "1.0 0.0"` → `1.0` |
| `struct-methods.poly` | Struct methods — `func` declared INSIDE a `struct` body, called via an explicit receiver; `this` resolves to the receiver (an ordinary bare name, not a keyword); purely static, mangled-name dispatch, no vtables; a method calling another method on `this` | `struct-methods.poly --jit --entry doubledSumCheck --args "3 4"` → `14` |
| `shuffle.poly` | `v.shuffle(i0, i1, ...)` — a single-vector permute/duplicate over `vec<Elem,N>`, lowered to a real `shufflevector` | `shuffle.poly --jit --entry reverse4 --args "10 20 30"` → `43210` |
| `exceptions.poly` | `try { ... } catch (name) { ... }` / `throw expr` — emulated via `setjmp`/`longjmp`; a thrown `i64` reaches the nearest enclosing `try` without touching intermediate stack frames; nested `try`/`catch` with correct handler save/restore | `exceptions.poly --jit --entry nestedTryCheck --args "-1"` → `1000` |
| `closures.poly` | `closure(params) -> Type { ... }` — a lambda capturing its free variables BY REFERENCE, automatically (no capture list); mutating a captured `var` from inside the closure is visible to the creating function and vice versa; a closure can escape (be returned) past its creating function's own return | `closures.poly --jit --entry tripleOf --args "7"` → `21` |
| `variant.poly` | `variant<A,B,...>` — a tagged union over N cases, ONE universal `{tag,payload}` runtime shape; `either<Ok,Err>` is sugar over the same shape (case 0/1); `.isOk()`/`.unwrapOk()`/`.unwrapErr()`/`.tag()`/`.unwrapAt(i)`; a value is CAST (not bit-reinterpreted) to its case's declared type at construction; standalone `.toI64()`/`.toDouble()`/etc. casts on any numeric value | `variant.poly --jit --entry divideOrDefault --args "10 0"` → `99` |
| `builder.poly` | Structs inside structs (`Shape.Size`), `static func`, field defaults, annotations (`@Doc(text: ...)`) and `@Builder` (`Shape.builder().side(2).build()`, required fields checked) — C++ front end only | `polyir-check builder.poly --interp` → `shape 2 4 [red, big] square 3 9` |
| `modules.poly` | `module Geometry { ... }` (named `Geometry.Point` from outside), `internal` functions, structs and methods, `if (c) a else b` as a value, and generic functions (`func larger<T>`, a copy per set of types) — C++ front end only for now | `polyir-check modules.poly --interp` → `7 big 7 x first` |
| `generics.poly` | `struct Box<T>` — real generics via MONOMORPHIZATION (the opposite choice from `variant`'s type erasure): one concrete LLVM struct + specialized methods per distinct concrete type actually used (`Box<i64>` → real `%Box_i64 = type { i64 }`, zero boxing); two instantiations of the same struct coexisting with no name collision; a generic method doing real arithmetic on its own `T`-typed field | `generics.poly --jit --entry boxedBoth` → `102` |
| `atomics.poly` | Atomics on a plain `ptr<T>` — `.atomicLoad`/`.atomicStore`/`.atomicRmw`/`.cmpxchg` (the STRONG form — success is just `result == expected`) plus a standalone `fence(ordering)` builtin; ordering/op are compile-time string literals, alignment computed automatically from the element type | `atomics.poly --jit --entry cmpxchgSucceeds` → `10020` |
| `coroutines.poly` | `coro func` — real `llvm.coro.*` intrinsics; `yield expr` suspends and hands a value out; `.resume()` returns a `variant<T>` (tag 0 = yielded, tag 1 = done), `.done()` checks completion without resuming. **AOT-only** — `CoroSplit` only runs through `clang`'s own pipeline, so this one is NOT `--jit`-able (see the file) | `coroutines.poly --out /tmp/coroutines && /tmp/coroutines` → prints `0112358`, exits `20` |
| `yielded.poly` | `v.yielded()` — a producer's `yield` that answers whether the coroutine was abandoned (`co.close()`) while suspended there; the body then stops handing values out and finishes | `yielded.poly --out /tmp/yielded && /tmp/yielded` → exits `40` |

`sparse.poly` also gains single-dimension indexing (`sp[idx]`, sugar
for `sp[idx, 0]`) and `.len()`/`.count()`; `grid.poly`/`tensor.poly`
gain `.rows()`/`.cols()`/`.dim(i)`; `strings.poly` gains `hash(s)`/
`s.hash()` (the same djb2 helper `mutmap<string,V>` uses internally),
content equality with `==` / `!=` (identity is `===` / `!==`), and a
`mutarray<string>` demo.

`--jit` only dispatches scalar (`i64`/`double`, 0-3 params) entry points
in v1 — `buffers.poly`'s `sumBuf` takes a `ptr<double>`, so use the
default build/`--emit-llvm`/`--check` for it instead (or call it from
another polyir function).

`coroutines.poly` is `--jit`-incapable for a DIFFERENT reason, not
just signature shape: `--jit` hands raw `.ll` text straight to an
in-process LLVM JIT engine with no `clang`/`opt` step at all, and a
`coro func`'s mandatory `CoroSplit` legalization pass only ever runs
as part of `clang`'s own pipeline — `--check`/`--emit-llvm` still work
(they only verify/render the IR), but running it needs a real
`--out`/default build.

Related: `../README.md`, `../../README.md`.

## java-mapping/

`Tour.poly` (one program touching structs, interfaces, generics, collections, closures, `try`, `==`/`===`) and `Tour.java`, its
hand-written translation following `docs/java-mapping.md`. `run-check.sh` builds both and checks they return the same value (125).
