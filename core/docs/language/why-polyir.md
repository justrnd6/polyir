# Why polyir: the argument

polyir is the half of a compiler that every language needs and no language wants to write: the part after the front end
has understood the program. This page makes the case for it — what it offers, why that is worth having, what it lets a
language do that a language cannot easily do alone, how it maps a high-level language down to a low-level target and a
low-level artifact back up, and, candidly, where it still falls short. The mechanics are in the README and the per-target
`docs/*-mapping.md`; this is the why.

## 1. What polyir offers

**One program, one meaning, many outputs.** A front end hands polyir a program once — as `.poly` text, as HIR JSON, or by
building the typed HIR in-process with libpolyir — and gets, from that one description:

- native code, a JIT'd function and WebAssembly through LLVM;
- Java, Kotlin, JavaScript, TypeScript, Swift, C++, Python, Rust, Objective-C and C as **source the target's own tools
  build**, written by back ends that are themselves ordinary programs (in Nina), not templates;
- Metal, Vulkan GLSL and SPIR-V for the kernel subset, so the same computation runs on a GPU;
- a debugger that stops in the FRONT END's file and line (DWARF from spans, source maps for generated text, a JSON
  description for every other target), and a `meta` channel for whatever a tool chain needs to know later.

**A typed HIR that already knows the hard things.** The HIR is not three-address code with types bolted on. It carries the
constructs that high-level languages are made of and that are painful to re-derive per target: closures with captures,
coroutines (`yield`/`resume`/`close`), sum types (`variant`, `either`), interfaces with dynamic dispatch, monomorphized
generics, exceptions (`try`/`catch`/`cleanup`), atomics, SIMD vectors, value-versus-reference structs, immutable and
mutable collections with structural `==`, hashing and `with`, and a memory model with **automatic release without a
garbage collector** (a pass proves what nothing else can see and frees it). A front end that lowers to these inherits
their implementation on every target.

**A written contract with the host.** `extern func`, `extern type`, `extern interface` and type bridges are polyir
declarations; `import java "…"` and its siblings are commands that write them. So "call the platform" is part of the IR,
not an escape hatch per target — a front end binds its standard library once and the back ends spell it per language.

**Two implementations that check each other.** The reference compiler is written in Nina; the C++ library (`runtime`)
is its port and must print byte-identical HIR for every program (`front_sweep.sh`). A harness (`targets_via_polyir.sh`)
compiles every test program through all ten source back ends and compares their output with the LLVM route. The IR's
meaning is therefore not what one implementation happens to do; it is what two implementations and eleven back ends agree
on.

## 2. Why that is worth having

A language is a front end plus N back ends plus a runtime plus debugging support plus interop, and only the front end
is the language. Everything else is the same work for every language, done badly N times because nobody has the budget
to do it well once. The usual escapes are to target one platform (and be a JVM language, or a JavaScript language), to
transpile to C (and lose the type system, the debugger and the platform's libraries), or to target LLVM directly (and
re-invent strings, collections, closures, exceptions and coroutines in IR).

polyir's claim is that the layer between "what the program means" and "what the target wants" can be shared, provided
it is typed enough to keep the high-level structure and portable enough that the structure can be spelled in any target.
The Nina compiler is the proof: when its six legacy generators (Java, Kotlin, C++, JavaScript, TypeScript, Python)
were deleted on 2026-10-20, nothing was lost — every target now goes through one lowering (`ninac/codegen/
polyir-lower`) and the polyir back ends, the compiler bootstraps through them, and the same corpus produces the same
output on every language. Where the lowering was wrong, it was wrong once, and fixed once.

## 3. What it lets a language do

Because the HIR carries advanced constructs, a front end does not have to choose between expressive features and many
targets. Nina's experience, feature by feature:

| Nina feature | What it becomes in polyir | What the back ends then do |
|---|---|---|
| `Result<T, E>` with `fail`, `else`, auto-propagation | `either<T, E>`, `EitherIsOk`/`UnwrapOk` | a tagged struct in C, a sealed class in Kotlin, a tuple in Python — the same checks |
| `Optional<T>`, `if (opt)` narrowing | `variant` with a none case | nullable where the target has it, a box where it does not |
| `match` on payload enums, list patterns | `VariantTag`/`VariantUnwrap` + structured branches | `switch`, `if` chains, pattern matching where the target has it (Swift, Rust, Kotlin `when`) |
| lambdas, bound methods, `f of g` | closures with captures | real closures on every target; boxing decided per target |
| generators, `Observable`, `Task`/`await` | coroutines and a struct-based observable model | `llvm.coro.*` natively; virtual threads / generators / async in the source languages |
| interfaces, generic dispatch | `interface` + vtables, monomorphized `struct<T>` | interfaces natively; erased where needed |
| `with { … }` copies, `===` identity | `With`, `Identical`, structural `==` and `Hash` | one definition of equality per type, not per language |
| `@Kernel` computations | the kernel subset | Metal / GLSL / SPIR-V, same numbers as the CPU route (checked) |
| `import java "…"` | host types, members and bridges | `java.util.ArrayList` where it is Java, a bridge elsewhere |

The power is in composition: a Nina program that uses coroutines inside a closure that returns a `Result` of an
interface value debugs in Nina lines on macOS natively, runs as JavaScript in a browser, and compiles as C for a
microcontroller, with one lowering and no per-target runtime written by the language author.

## 4. Mapping down and mapping up

**Down (high → low).** The lowering is a contract, not a translation: the front end must express its constructs in HIR
terms (sums, closures, containers, host calls) and declare what it needs from the host (`extern`, host types). polyir then
owes a spelling on each target. The per-target mapping documents (`docs/java-mapping.md` and the others) say, construct
by construct, what that spelling is, what it costs, and what is declined. A decline is a named, early error ("runtime
function X is not supported yet in rust"), never a silent fallback — a target that cannot do something says so at the
point of use. Unstructured control flow (labels with arguments) is restructured for targets without `goto` (the
relooper in the Java back end); manual `Free` is a hint garbage-collected targets ignore; raw pointers and SIMD are
declined where the target has none.

**Up (low → high).** `polyir-lift` reads LLVM IR text or a WebAssembly binary and recovers a program — functions, locals,
loops and conditionals, structs, calls — as an IR the same back ends can write as Java, C++, Kotlin, Python, JavaScript or
TypeScript. A module polyir itself compiled carries its `polyir.meta` description (names, parameter names, struct fields,
interfaces, positions), which is exactly what machine code loses; the lifter can read it back, so a round trip
`emit → lift` keeps the names and shapes a decompiler would have to guess.

**A worked case: Java to native, with polyir in the middle.** Imagine wanting ordinary Java to compile to a native
executable — no JVM, no JIT warm-up, a binary a C program could link. Today that means GraalVM Native Image, a closed
pipeline. With polyir the job splits into a front end that only has to understand Java, and a back end that already exists:

```
Foo.java ──javac-like front end──▶ polyir HIR ──polyir──▶ LLVM IR ──clang──▶ native binary (or wasm, or Swift/Rust/C source)
            classes, generics,        structs + interfaces + vtables,           DWARF says Foo.java:12
            exceptions, lambdas       either/variant, closures, try/catch,
            String, ArrayList         string, array/mutarray, extern (JNI)
```

What the front end maps, and to what: a class with fields becomes a reference `struct` (one `var` field is enough to
make it a reference) and its methods become struct methods; `interface` and `implements` become polyir `interface`
and `: I`, so `invokeinterface` is a vtable call the LLVM back end already writes; `extends` flattens into the struct
(the superclass's fields first, overridden methods rebound in the vtable); generics are monomorphized per use like
`struct Box<T>` (the front end erases where Java erases); `try`/`catch`/`finally` are `try`/`catch`/`cleanup`;
lambdas are closures with captured locals; `String` is `string`, `ArrayList`/`HashMap` are `mutarray`/`mutmap`
(or, kept as host types through `extern type`, the JDK's own classes when the output is Java again); `native` methods
are `extern func`; `synchronized` and `volatile` are atomics and a lock the front end declares. Positions go in as
spans, so the native binary debugs in `Foo.java` lines and a crash prints Java method names.

What polyir contributes that the front end does not write: the object layout and every operation on it on each
target, structural equality and hashing, the release analysis (a Java object that never escapes is freed without
a collector — the escape analysis a JIT does at run time, done once at compile time), exceptions on LLVM, interfaces
and dispatch, closures, DWARF, wasm, and — because the HIR is the same — the same Java program written back as C, Rust,
Swift or Python, which no Java tool chain offers.

What the gap is for THIS case, specifically: Java assumes a garbage collector and polyir's release analysis frees only
what provably does not escape (cycles and shared long-lived graphs would need a collector the front end brings or a
reference-counting scheme polyir does not have yet); threads, monitors and the memory model are not in the IR (atomics
are); reflection, class loading and `instanceof` on arbitrary hierarchies need runtime type information polyir does
not keep beyond interface tables; and the JDK itself is the real work — either bound as host types (which only helps
when the output is Java) or re-implemented over polyir's collections and strings. None of that is polyir being the
wrong intermediary; it is the list of what a Java front end would ask the IR to add, in the order it would ask.


**The same exercise for other languages.** Each would map most of itself onto what the HIR has, and each would ask
for something specific:

| Front end | Maps naturally to polyir | Would ask the IR for |
|---|---|---|
| Kotlin | classes and `data class` → reference structs with structural `==`/hash and `copy` (`with`); sealed classes and `when` → `variant` + tags; `suspend` functions and `Flow` → coroutines; nullable `T?` → a `variant` with none; extension functions → ordinary functions; lambdas with receivers → closures | coroutine dispatchers and structured concurrency (the IR has coroutines, not a scheduler); `inline`/`reified` need the front end to monomorphize; delegation (`by`) is sugar the front end expands; the same GC and JDK questions as Java |
| Swift | value structs and enums with payloads → value structs and `variant`; protocols → `interface`; `throws` → `either`/exceptions; optionals → `variant`; closures → closures; `async`/`await` → coroutines | ARC semantics (copy-on-write arrays, `weak`/`unowned`) against polyir's release analysis; actors and the executor model; generic protocols with associated types need monomorphization by the front end |
| TypeScript / JavaScript | classes → reference structs, interfaces → `interface`, union types → `variant`, `async` → coroutines, closures → closures, `Map`/arrays → `mutmap`/`mutarray`, numbers → `double` | a dynamic object model (`any`, prototypes, property bags, `typeof`) has no HIR type &mdash; the front end needs a boxed dynamic value and a runtime; the number model (doubles, 53-bit integers) must be emulated on integer targets; the event loop and promises are a scheduler the IR does not have |
| Python | functions, classes (as reference structs with a fixed field set), lists/dicts/sets → containers, generators → coroutines, exceptions → `try`/`catch` | everything dynamic: duck typing, `getattr`, dynamic dispatch by name, big integers, the global interpreter model &mdash; a Python front end is a static subset (what Cython or mypyc compile) plus a boxed value for the rest; polyir has no big integer or dynamic value type yet |
| Rust | structs, enums → `variant`, traits → `interface` (static dispatch monomorphized by the front end), `Result`/`Option` → `either`/`variant`, closures → closures, `Box`/`Vec`/`HashMap` → references/containers | ownership and borrowing are checked by the front end and then DROPPED: polyir's release analysis frees what does not escape, but `Drop` order, moves and lifetimes are not expressible, so destructors with side effects need explicit calls; no `unsafe` pointer model beyond `ptr<T>` |
| C | functions, structs, pointers → `ptr<T>`, arrays → `T[N]`, `goto` → labels; this is the closest fit and the direction the lifter already goes (compiled C → LLVM IR → lifted) | unions, bit fields, variadic functions, `setjmp`, inline assembly and the preprocessor are outside the IR; integer promotion rules and undefined behaviour must be chosen by the front end since polyir does not define them either |

The pattern is the same every time: types, control flow, sums, closures, coroutines, exceptions and collections are
already there; what each language brings that the IR lacks is its RUNTIME model (a collector, a scheduler, a dynamic
value) and its UNDEFINED corners (numeric rules, string encoding, destructor order). Those are the items in the gap list,
which is why the list is ordered as it is.

**The mirror case: C, compiled to LLVM IR, lifted into Java.** Take a C library — say a checksum and a small parser —
compiled by clang to LLVM IR. `polyir-lift` reads the `.ll` text (no libLLVM, no clang at lift time), and the Java back
end writes a class:

```
crc.c ──clang -O0 -emit-llvm──▶ crc.ll ──polyir-lift──▶ IR ──Java back end──▶ Crc.java   (or Kotlin, C++, Python, JS, TS)
         functions, locals,          allocas → locals, GEP patterns → fields,     static methods on long/double,
         loops, structs, calls       dominators + natural loops → while/if,        structs as classes, arrays as arrays,
                                     extern calls → declared methods               names from polyir.meta when present
```

What the lift recovers: every `alloca` becomes a local, the control-flow graph is restructured into `while`, `if` and
`break`/`continue` by dominator and natural-loop analysis (so the Java reads as a loop, not as a state machine over
basic blocks), `getelementptr` chains over a known struct type become field access, calls stay calls, and a function the
IR only declares becomes a method the Java side must provide. When the IR was produced by polyir itself, the `polyir.meta`
section gives back the function and parameter names, the struct fields and their types, `var` fields and interfaces —
the lifted Java has the original names instead of `%3` and `field_2`.

What Java then gains: C code it could only reach through JNI becomes ordinary Java it can read, step through, unit-test
and refactor; the same lift writes Kotlin for an Android project or TypeScript for a browser; and the lifted program,
re-entering polyir as a front end's input, compiles back down to native or wasm — a round trip that keeps the
program's shape.

The gap for this direction: the lifter is reliable on `-O0`-shaped IR (one `alloca` per local); optimized IR writes loops
with phi nodes that come back as guard flags and are not yet reshaped; raw memory has no Java spelling — a `char*`
string, `malloc`/`free`, pointer arithmetic over a byte buffer become a byte array plus an index, which is correct but
not idiomatic; function pointers become an interface the lifter has to invent; unions, bit fields, `setjmp` and inline
assembly stop it; and without `polyir.meta` every name is a guess. The lifted subset and a canonical round-trip form
(`lift(emit(p)) == normalize(p)`) are the two definitions the lifter still owes.

**Why both directions matter.** Down lets a new language start with eleven targets. Up lets existing compiled code —
a C library, a wasm module, LLVM IR from any compiler — enter the same world: lifted once, it becomes source in any of the
back ends' languages, and a front end can treat it as a module it imports rather than an opaque binary it links. The two
together make polyir a place code can move THROUGH, not only a place it goes to: Nina to C, C to Java, Java's own classes
(through `import java`) into a program that then compiles to Swift.

## 5. The gap, candidly

polyir is a working system and an incomplete IR. What is missing, in the order it hurts:

1. **The target contract is in code, not written down.** What a back end must provide (memory, strings, pointers,
   exceptions, coroutines, atomics, control flow) is implicit in the lowering and the mapping docs; `docs/portability.md`
   proposes a capability table the compiler reads so a program is rejected up front by capability name. Not done.
2. **The runtime object model lives in the LLVM lowering.** The source back ends bind collection operations to their own
   libraries (that is why they work), but the byte layout of list and map headers that the LLVM route uses is not part of
   the IR's specification; a third kind of back end (a C runtime of its own, a different allocator) has to copy it.
3. **Undefined numeric and string semantics.** Integer overflow, division by zero, out-of-range shifts and indexing are
   whatever the target does; strings are UTF-8 bytes natively and the host's strings elsewhere, so non-ASCII `==`,
   hashing and invalid bytes differ per target (the new file-system helpers show it: Swift decodes lossily, Objective-C
   answers "", C keeps the bytes).
4. **A flat namespace.** `import` merges files; front ends with modules mangle names themselves, and separate compilation
   and linking of polyir modules is not a thing yet.
5. **The lifter recovers `-O0`-shaped code.** Loops written with phis (optimized IR), closures, strings and coroutines come
   back as pointers and calls; the lifted subset and a canonical round-trip form are not defined.
6. **Debug information is one-way for most front ends.** The text route and the HIR carry positions, but the C++ library
   has no `meta` field and no builder-side `at(span)` convenience, and Nina itself does not yet hand statement spans over,
   so a Nina program does not debug in `.nina` lines.
7. **Per-target holes a user still meets.** From the Nina matrix: `import` of host classes on languages other than Java,
   `Dynamic<T>`, 128-bit integers on JVM/Apple/C, `Array<T>` indexing, `Bits<N>` slicing, error record literals, text
   range patterns, the host types Bit/BigInt/Decimal/Buffer; Kotlin's file helpers are JVM-only; the LLVM runtime text is
   regenerated on one host and bakes its ABI; single-language natives (JIT, GPU, wasm, React) rightly refuse other targets.
8. **Exceptions and coroutines are native-first.** Both compile on LLVM; the source back ends carry them where the language
   has them (Java virtual threads, generators) and decline elsewhere; wasm has no exception handling yet.

None of these is a flaw in the idea; each is a piece of specification or a bounded piece of work, and `docs/portability.md`
orders them. The idea — one typed, high-level, portable IR that front ends target and back ends read, with debugging and
host interop as first-class parts of it — is what the rest of the Nina project is now built on.
