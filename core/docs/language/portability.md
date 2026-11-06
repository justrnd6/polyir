# Making polyir portable, in both directions

This is a candid assessment of where polyir stands as an intermediate representation that front ends target and back ends
read, what to decide next, and what to change. "Down" means polyir → a target; "up" means a target → polyir.

## 1. What polyir is today

```
 polyir text ──parse──▶ AST ──check──▶ typed HIR ──(passes)──▶ HIR ──lower──▶ LLVM text ──clang──▶ native / wasm
                                          │                            └────▶ MSL / GLSL / SPIR-V (kernels only)
                                          └── debug & meta (target-neutral JSON, DWARF for LLVM)
```

- The **typed HIR** (`hir/HirNodes.nina`, `HirTypes.nina`) is the one neutral layer. Everything target-independent happens on it:
  checking, structural equality and hashing, `with` on maps, the memory-release analysis, debug markers.
- **LLVM is a back end**, not the definition of the language, but it is the only complete one. Wasm goes through it; the GPU
  back ends read the HIR directly for the kernel subset.
- **polyir text is the only serialization.** The HIR exists only in memory.

## 2. Down: polyir to a target

### What a target must provide (the contract today, unwritten)

| Need | Where it is implicit now | Targets that struggle |
|---|---|---|
| Raw memory: `realloc`/`calloc`/`free`/`memcpy`/`strlen`/`strcmp` | called by name from lowering | JVM, JS (no raw memory) |
| Runtime object layout: list header `{data, length, capacity}`, map header `{keys, values, states, slots, count}` at fixed byte offsets | written in `Containers.nina` and every `Lower*.nina` | any target with its own collections |
| Strings as NUL-terminated byte pointers | everywhere | JVM, JS, anything with real strings |
| Pointer arithmetic and `ptr<T>` | the type system | JVM, JS |
| Exceptions by setjmp/longjmp | `LowerTry` | wasm without exception handling, JVM, JS |
| Coroutines by `llvm.coro.*` | `LowerCoro` | everything but LLVM |
| Unstructured control flow (`goto`, labels with arguments) | HIR statements | JVM, JS, wasm text (need restructuring) |
| Atomics with LLVM memory-order names | `atomicRmw(..., "seq_cst")` | targets with other models |
| C-ABI externals | `extern func` | wasm (imports), JVM (JNI), JS |
| Manual freeing | the release pass inserts `Free` | garbage-collected targets (should ignore it) |

### Recommendations

1. **Write the target contract down as a capability table** and make it data the compiler reads: manual memory or GC; raw
   pointers yes/no; exceptions kind; coroutines yes/no; unstructured control flow yes/no; SIMD; atomics; int widths. A
   program that needs a capability the target lacks is rejected up front with the capability's name, instead of failing deep in
   lowering. (The GPU path already does this for kernels.)
2. **Make the runtime object model part of the IR, not of the LLVM lowering.** Today a back end has to re-implement the byte
   offsets of every header. Either (a) specify the model (a section of the spec with the offsets), or better (b) lower
   collections to **abstract operations** (`ListPush`, `MapGet`, `SetHas`, ...) that already exist as HIR builtins, and let each
   target bind them to its own library (Java `ArrayList`, JS `Map`, a C runtime). Option (b) is what makes a JVM/JS target
   realistic, and the new `array`/`map`/`set` split maps one-to-one onto `List.copyOf`/`Map.copyOf`/`Set.copyOf`.
3. **Treat `Free` as a hint.** Keep the release analysis, but mark its output as optional so GC targets drop it. Likewise `val`
   and `.copy()` are semantic (keep), while boxing for closures is a representation choice (target decides).
4. **Offer a structured control-flow normal form** (`while`/`if`/`break` only) as a pass for targets that cannot take labels. A
   relooper over the HIR labels is a bounded piece of work and would unlock wasm text, JS and the JVM.
5. **Define numeric behaviour.** Today integer overflow, division by zero, over-wide shifts, out-of-range indexing and NaN
   comparison are whatever LLVM does. Pick: wrap or trap for integers, trap for indexing, IEEE for floats (already chosen for
   `==`), and state it. A target that cannot match (JS numbers are doubles) then knows what to emulate.
6. **Unsigned integers (done: `u8`, `u16`, `u32`, `u64`).** Before, there was no `u8`/`u32`/`u64`; `char` is a signed `i8`. Front ends for most languages need them,
   and wasm/LLVM/JVM all differ in how they spell the operations.
7. **Strings need a model.** Bytes (UTF-8, as now), UTF-16 code units, or abstract text with explicit encode/decode. Without
   one, a front end cannot promise anything about non-ASCII text, and string `==` / hashing differ per target.
8. **Replace the flat namespace with modules.** `import` merges files into one namespace and a clash is an error. Front ends need
   qualified names and mangling that survives separate compilation; wasm and native both want to link modules, not one program.
9. **Host imports instead of `extern func`.** Say what the module needs from its host (`import "env" "name" (sig)`), so wasm
   imports, JVM natives and JS globals all bind to the same declaration.

### Decisions only you can make (feedback requested)

1. Which targets next, in order? (JVM, JS, a C backend, wasm without clang?) The answer sets whether recommendation 2 is urgent.
2. Is the interchange format **polyir text**, or do you want a versioned **binary/JSON IR** that front ends emit directly? A
   HIR → text printer that round-trips is the minimum; JSON is easier for non-Nina front ends.
3. Managed or manual memory as the default semantic: should a program be *correct* without any `Free`, with the release pass
   purely an optimization? (I recommend yes.)
4. Integer semantics: wrap or trap on overflow? Checked indexing on by default?
5. Unsigned integers: now, or only via library functions over signed ones?
6. How much of generics (functions, not only structs) must the IR express versus a front end monomorphizing before it emits?
7. Are exceptions part of the IR (portable semantics) or only a feature of native back ends?
8. Is the `kernel` subset a separate profile of the IR (with its own verifier), as it effectively is now?

## 3. Up: a target to polyir

`polyir-lift` reads LLVM IR or wasm and produces Nina IR. For polyir to be a good *destination* of a lift:

1. **Produce polyir text, not only Nina IR.** The lifter's output should be a `.poly` program (or HIR), so the lifted code goes
   through the same checker, release analysis and back ends. That needs a faithful **HIR → polyir printer** (the current
   `HirPrint` is a debug dump); it doubles as the serialization recommended above.
2. **Use the meta section as the lift's type information.** A module compiled by polyir now carries `polyir.meta` (JSON). The
   lifter can read it to recover function names and parameter names, struct fields with types, `var` fields and implemented
   interfaces, which are exactly what machine code loses. To make the round trip lossless the description should also carry
   **function signatures** (parameter and return types) and **local variable names**, the layout of collection headers
   (so `ptr` + offsets can be turned back into `mutarray`), and the language version. (Signatures are the next addition to the
   JSON; parameter names are in already.)
3. **Define the "lifted" subset.** Raw LLVM has no `string`, `mutarray`, closures or coroutines, only pointers and calls to
   `realloc`. The lifter should say what it recovers (scalars, structs, calls, loops) and what it leaves as `ptr<i8>` plus
   `extern func`, rather than guessing. Today it also mishandles loops written with phis (as optimized IR is), so it works best
   on `-O0` shaped input.
4. **A canonical form for round trips.** `lift(emit(p))` should equal `p` after normalization (shorthand structs expanded,
   `val` and `with` explicit). Writing that normal form down gives a test: emit, lift, emit again, compare.

## 4. A good IR, in one list

Complete semantics written down (including undefined cases); a capability table per target; abstract collection operations
bound per target; a stable serialization with a version; structure-preserving debug information (done: `--debug`, source
maps, JSON); a verifier for each profile (full, kernel, no-GC, structured); and round-trip tests in both directions. polyir
already has the typed HIR, the memory analysis, structural equality, the collection model and the debug path; what is missing is
mostly specification and the three structural items: abstract runtime operations, structured control flow and a module system.
