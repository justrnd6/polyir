# Memory: how polyir reclaims what a program no longer uses

**Status: all three parts have landed.** Part 1 (the VM, `../runtime/src/vm/Collector.cpp`, 2026-10-30) and Part 2 (native builds,
`src/emit/llvm/runtime/Collector.c`, 2026-10-30; its own page allocator since 2026-10-31) make a program that keeps a bounded amount of
data run in bounded memory however long it runs. Part 3 (2026-10-31) makes both collectors **incremental**, the default: a collection is
spread over short slices between which the program runs, so the longest pause is set by a work budget, not by how much data the program
keeps (a game loop over a million live objects: 15.6 ms pauses stopping the world, 49 µs incremental). Since 2026-10-31 the strings ninac's C text runtime returns to a
native Nina program are the collector's objects too (Part 2, "Strings the Nina runtime makes"). Each section says what exists, how
it is checked and what it costs.

The goal, in one sentence: **a polyir program that keeps a bounded amount of data alive uses a bounded amount of memory,
however long it runs, on the VM and as a native build alike.**

## The model: tracing garbage collection

polyir uses a **tracing collector** (mark and sweep): from time to time, find every object the program can still reach,
and reclaim everything else. It was chosen over reference counting because:

- it needs no work on every assignment (reference counting adds an increment and a decrement to each store);
- it reclaims cycles (two objects pointing at each other) with no extra machinery;
- the VM already knows exactly where its live values are, so a precise collector is simple to write there.

Its cost is a pause while it runs, which grows with the data the program keeps. For servers and watchers that is fine; for games it
is not, which is why Part 3 spreads every collection over short slices (an **incremental** collector).

Explicit `Free`/`FreeBox` stay valid HIR. With a collector they are only hints: the object is reclaimed at the next
collection that finds it unreachable, never earlier, so a program that frees something it still uses cannot corrupt
memory.

## Words used here

- **object** — anything with identity that lives in the heap: a struct, a list, a map, a set, a closure, a box (the
  cell behind a `var` or a captured name), a coroutine.
- **reference** — a value that points at an object. On the VM it is the object's index in the heap; natively it is the object's address.
- **root** — a place the program can read a reference from without going through another object: a local slot, the
  operand stack, a global.
- **reachable** — an object a root refers to, or one a reachable object refers to.
- **safe point** — a moment when every live reference is in a root, so the collector can see all of them.
- **cycle** — one whole collection: marking everything reachable, then sweeping the rest. Stopping the world, a cycle is one pause;
  incremental (Part 3), it is many **slices**, each a short pause.
- **white, grey, black** — the colours of tri-colour marking (Part 3): not reached yet; reached but not looked inside yet; reached and
  looked inside.

## Part 1: the VM (landed)

The VM's collector is [../../runtime/docs/memory.md](../../runtime/docs/memory.md) (libpolyir-vm, `runtime/src/vm/Collector.cpp`).

## Part 2: native builds (landed)

A native build behaves like the VM: a program that holds flat memory on one holds flat memory on the other. The model is the same
mark and sweep; what differs is how the collector finds the objects and the roots, because native code keeps references in machine
words, in registers and in its own stack frames, and a word could just as well be a number.

There are two halves, and each is written to be read on its own:

- **the collector**, `src/emit/llvm/runtime/Collector.c`: C, read top to bottom like the VM's `Collector.cpp` (a long opening comment,
  then `gc_poll`, the story of a safe point, then a cycle's steps in order — beginning it, a slice of marking, the end of the marking, a
  slice of sweeping — each defined below it, then the write barrier, the sets of addresses, the pages, the objects, allocation, the
  shadow stack, the pauses and starting). `runtime/gen_collector_ir.sh` compiles it with `clang -O1` and writes its IR (and the C library functions it calls) into
  `src/emit/llvm/LlvmCollector.cpp`, so every module that allocates gets it as text, the way ninac's text runtime (`NinaText.c`) is
  embedded: a program needs nothing extra at link time, and the IR still says what it calls (`gc_allocate`, `gc_poll`, ...). The
  functions and globals the program shares with the collector are `linkonce_odr`, so modules built apart and linked together share one
  collector; its helpers are `internal`.
- **what the compiler does for it**, `src/emit/llvm/LlvmRoots.cpp` (frame records and safe points) and the allocation sites in the other
  `Llvm*.cpp` files.

### Objects

Every heap block the compiled code makes comes from `gc_allocate(bytes, layout)` or `gc_allocate_zeroed(count, size, layout)`; a list's
elements grow with `gc_resize` (realloc). That is every malloc, calloc and realloc the emitter used to write:

| what | where | layout |
|---|---|---|
| a struct (a literal, a copy) | `StructLit`, `CopyStruct` | references when any field holds one |
| a list header (3 words) | `ListHeader` | references (its third word is the elements) |
| a list's or block's elements | `ListHeader`, `DataBlock`, `CopyBlock`; grown by `Reallocate` (`gc_resize`) | the element type's |
| a map's header and keys, values, states tables | `map_new`, `map_grow` (IR text in `LlvmMaps.cpp`) | references; the states table data |
| a sparse's header | `EmptySparse`, `CopySparse` | references |
| an optional, either or variant (2 words), a coroutine step's variant | `SumValue`, `CoroutineVariant` | references |
| a closure (2 words) and its environment | `ClosureBlock`, `EnvironmentOf` | references |
| a captured variable's cell | `AllocateCell` | the variable type's |
| an interface value (2 words) | `InterfaceBox` | references |
| a coroutine's frame, yield slot and result block | `CoroutineFrame`, `YieldSlot`, `CoroutineRamp` | references; the slot the yielded type's |
| a string built at run time | `StringConcat`, `str`, `slice`, a character's text (`LlvmText.cpp`) | data |

A string constant is a global, not an object; strings built at run time are objects with the data layout, so they are collected like
the rest (no separate scheme). So are the strings ninac's C text runtime returns to a Nina program (`NinaText.c`: number to text, `trim`,
`slice`, case, a line read, a file's text, ...): it allocates them with `gc_allocate` too (below, "Strings the Nina runtime makes"). Strings
a host library makes (a `char*` from C) are not objects: the collector never follows or frees them.

**The layout** is one of two: *data* (no references inside: a string, a block of numbers, a struct of numbers) or *references* (any
8-byte word inside may be one: every word is looked up). It is chosen from the type at each allocation. A references object can keep
something alive by a word that only happens to equal an object's address (an integer map key, a list's unused capacity holding an old
element): that object lives a little longer, nothing is ever freed early. Word-exact layouts (which words of a struct are references)
were left out on purpose: they need LLVM's struct offsets in the compiler, and a wrong offset would free a live object, while a wrong
guess of the coarse layout only costs a look-up.

### Where objects live: pages of one size

The collector has its own allocator, built for what the compiled code allocates: a great many small objects (a struct, a list header,
an optional or a closure is 16 to 48 bytes) and a few large ones (a long list's elements, a big map's tables). It replaced a first
version in which every object was a malloc block with a 16-byte header and its address went into a hash set of all objects; that cost a
hash-set insert per allocation, a `free()` per object reclaimed, and some 27 bytes per live object (the table below has the numbers).

**Pages.** A small object (at most `GC_LARGEST_SMALL`, 2 KiB) lives in a **page** of `GC_PAGE_BYTES`, 64 KiB, and every page holds
objects of *one size* (its **size class**) and *one layout*. A page starts with a header (`GcPage`):

| field | what it is |
|---|---|
| `slotBytes`, `slotCount` | the size class, and how many slots of that size fit after the header |
| `layout` | data or references, for every object of the page |
| `allocated[64]` | a bitmap, one bit per slot: set while the slot holds an object |
| `marked[64]` | a bitmap, one bit per slot: set once the current collection has reached the object |
| `used`, `words`, `cursor` | how many slots are in use, how many bitmap words cover the slots, the first word that may have a free slot |
| `next` | the next page of the same class and layout (or the next spare) |

The slots follow the header (1,072 bytes, 1.6% of the page; Part 3 added `swept`, the cycle whose sweep last passed the page): slot *i*
starts at `page + 1072 + i × slotBytes`, and a page of 16-byte slots holds 4,029 of them. The bitmaps have room for 4,096 bits, the most
16-byte slots a page could hold, so the header is the same for every class.

Decisions, and why:

- **64 KiB pages, aligned to 64 KiB.** The page of any address is the address with its low 16 bits cleared, so finding where a word
  points needs no search through objects. 64 KiB makes the header a small part of the page, and keeps what an almost empty class costs
  small (there are 48 classes, 24 sizes × 2 layouts; one page each is 3 MiB at worst, and a program uses a handful). Pages come from
  `posix_memalign`: the collector's IR is compiled once and runs on every system, and `mmap`'s flags differ between systems.
- **The size classes**: 16, 32, 48, 64, 80, 96, 112, 128, then about a quarter apart up to 2048 (160, 192, 224, 256, 320, 384, 448, 512,
  640, 768, 896, 1024, 1280, 1536, 1792, 2048). All are multiples of 16, so every object is 16-byte aligned, as malloc's blocks were (LLVM's
  vector loads assume it), and a request wastes less than a quarter of its slot. 24-byte classes would suit a list header better, but
  would break that alignment. A table built at `gc_start` gives the class of a request of up to 16 × *k* bytes by *k*, one load.
- **One layout per page.** The layout is the page's, not the object's: no per-object header at all, and a data page is never looked into.
- **Large objects** (more than 2 KiB) keep the old path: a malloc block with a header in front (size, layout, mark bit, and since Part 3
  the links of the **large list**, newest first, which the sweep walks a few at a time: 32 bytes), its address in the **large set** (a hash
  set). They are rare, a page would hold too few of them for the rounding to pay, and a list's elements grow by `realloc`, which can move a
  large block without copying.

**Is this word a reference?** Exactly when it is the start of an object the collector made and still holds:

1. it is not 0, it is 16-byte aligned, and it lies between the lowest and the highest address the collector holds (one comparison each,
   which turns most numbers away);
2. its page (`word & ~0xFFFF`) is in the **page set**, a hash set of page addresses (hundreds of entries for a 20 MB heap, so it stays
   in the cache, where the old object set had one entry per object), and the word falls exactly on the start of a slot whose
   `allocated` bit is set;
3. or, when its page is not one of the collector's, it is in the large set.

Anything else is left alone, never followed and never freed: numbers, string constants, raw `ptr<T>` values, memory C made, a free slot's
address, and **an address into the middle of an object**. Interior pointers are not references, as with the object set before (so the
compiler's side did not change): the compiler keeps the start of every object it still needs in a slot of its frame record or inside an
object, and accepting interior addresses would make any number that falls anywhere inside a block keep it alive, and cost a search for
the start.

**Allocating** (`newSmall`): the class's current page; from the page's `cursor`, the first bitmap word that is not all ones, its
lowest clear bit (`ctz`), set; the slot's address. No hashing and no header to write. When the page is full the class moves on along its
list of pages to the next one with room, and only when there is none takes a new page (a spare first). A references object is zeroed (the
whole slot), so the words the program has not written yet (a list's spare capacity, the slot past what was asked for) keep nothing alive;
a data object is not, as malloc's blocks were not. `gc_resize` keeps a small object where it is while the new size fits its slot, and
otherwise moves it to a new one and frees the old slot at once (as `realloc` frees the old block); `gc_release` clears the slot's
`allocated` bit and moves the page's cursor back to it.

**Sweeping** (`sweepThePage`): for every 64 slots, `freed = allocated & ~marked` (counted with `popcount` for the report), then
`allocated = marked` and `marked = 0`. That is the whole sweep of a page (Part 3 sweeps a few pages per slice): no `free()` per object, no list of survivors, and the marks are clean for
the next collection. A page left with no objects leaves its class and becomes a **spare**, ready for any class. A spare that sat unused
through a whole round between two collections (the fewest spares there were at any time in the round) goes back to the C library: a
program that makes garbage at a steady rate reuses the same pages round after round (no thrashing between `free` and `posix_memalign`),
and one whose data shrinks gives its memory back one round later. (A first version kept as many spares as the next round could fill;
it still freed and took back a few pages each round, and the C library's bookkeeping for those nearly doubled the resident memory of `churn`, 9.7 MB to 15–20 MB.)
Large objects: the sweep walks the large list; an unmarked one is freed (and leaves the set and the list), a marked one is unmarked.

**The worklist** holds one entry per object still to trace (a grey object, Part 3): its address, plus a tag in the low bit (free, as
objects are 16-byte aligned) when it is in a page, so the trace finds how many words to read (the page's size class, or the large object's
header) without looking anything up, and the word to go on from (a large object is traced in chunks of 4,096 words). A data object is
marked but never put on the worklist.

**What an object costs now**: its size rounded up to its class, plus 2 bits in its page's header and the header's share (1,072 bytes per
page: a quarter of a byte per 16-byte object); a large object costs a 32-byte header and its entry in the large set. While a collection
runs, the worklist holds the grey objects; since the chunks of a large object go back on it under what they pushed, a list of a million
structs no longer puts them all on it at once (`scan` holds 25 MB, 4 MB less than before Part 3).

**Before `gc_start`, or without it** (a library a host calls), nothing changes: objects are plain malloc blocks, and `gc_resize`,
`gc_release` and `gc_free_hint` work on them with realloc and free. A block the collector did not make is never in a page (pages come
only from its own `posix_memalign` calls), so it is never mistaken for one.

**Built for an incremental collector.** The mark bits are where Part 3's write barrier finds them from an address alone (mask to the page,
divide by the size class; a large object's header), and the explicit worklist is what lets marking stop after a budget and go on later.

### Roots: the frame record (a shadow stack)

Native code keeps references in registers and stack slots the collector cannot see, so each function that works with references keeps
them in a **frame record** on its own stack, linked into a chain of the running functions' records:

```
%gc.frame = alloca { ptr, i64, [N x ptr] }      ; the caller's record, the count N, the N slots
call void @gc_push_frame(ptr %gc.frame, i64 N)  ; zero the slots, link: caller = gc_top, gc_top = this record
...
call void @gc_pop_frame(ptr %gc.frame)          ; before every `ret` (gc_top = caller)
ret ...
```

The slots are:

- **every variable whose value is a reference** (and every captured variable's cell pointer, and a label parameter that holds a
  reference, which keeps a slot instead of becoming a phi): the variable's own stack slot *is* a slot of the record
  (`%x = getelementptr ... %gc.frame, i32 0, i32 2, i64 k`), so storing to the variable keeps the record current at no extra cost;
- **one slot per expression that makes or reads a reference** (`KeepRoot`, called by `EmitExpr` for every expression whose value is a
  reference in a register, except a read of a plain variable, whose slot is already there): the value is stored as soon as it is made.
  This is what keeps a temporary alive while the rest of an expression runs: in `sumOf(make(1), make(2))` the first `Pt` is in no
  variable while `make(2)` runs and may collect. A slot holds its last value until it is written again or the function returns, so at
  worst an object lives a little longer.

Every `ret` (an ordinary return, the end of the body, the early return a throw takes to the caller, see `PassOnThrown`) is preceded by
the pop: `Terminate` adds it to any `ret`, so no way out is missed. A trap never returns and needs none. The record is linked on entry and
unlinked on the way out, so the chain is exactly the running functions, and `gc_top` is where marking starts (where a cycle's root scan
starts, Part 3).

A function that cannot reach a safe point at all (it does not allocate and calls nothing of the program's own: a getter, an arithmetic
helper) keeps no record: the decision is made when the function is finished (`FinishedLines`), and its slots become plain `alloca`s
again. So `fib` compiles exactly as before.

**Globals** that hold references are roots too: the entry point passes the collector a table of their addresses
(`@gc_globals`, `gc_start(ptr @gc_globals, i64 n)`). **Raw `ptr<T>` memory and host memory are not traced**, as on the VM: a
reference stored only there does not keep an object alive.

**Coroutines.** A coroutine's record lives in its frame (CoroSplit moves it there, as anything used across a suspend). It is filled in
once in the ramp (a plain store, before `llvm.coro.begin`, the way parameters are stored), linked each time the coroutine resumes and
unlinked before each suspend (`SuspendAt`), so it is in the chain exactly while the body runs. The frame itself is an object (layout
references, allocated by `gc_allocate` with `llvm.coro.size`) reached through the coroutine's result block, so while the coroutine is
suspended everything it keeps (its record's slots among the frame's words) is traced through the coroutine object. The ramp (from the
start to the first suspend) has no safe point, so the frame, the slot and the block, in registers there, need no record. A coroutine
nothing else refers to while it runs (`keeper().resume()`) is safe: the caller's slot for the `keeper()` expression holds it. Since Part 3
the resume passes the frame too, `gc_link_frame(ptr %gc.frame, ptr %hdl)`: a coroutine writes its frame without barriers, so the collector
looks at the whole frame when one resumes while it marks.

### Safe points

A safe point is a call of `gc_poll`, which does the collector's work when it is due (one comparison: `bytes > collectAt`; Part 3 says
what `collectAt` is during a cycle). A function that allocates has one at
the start of its body (after its parameters are in their slots) and one at the head of every loop (a `for` test, a label, which may be a
loop's head). **Allocation itself never collects**, as on the VM: `gc_allocate` only records the object, so the emitter's own sequences
(a list header, then its element block; a map header, then its tables) never meet a collection halfway, and nothing the emitter holds in
a register between two allocations needs a slot. Calls are where a collection can happen *for the caller* (inside the callee): a call of
one of the program's functions, a closure, an interface method, a coroutine's resume or close, an equality helper. That is why
temporaries are kept across calls, and why the order of a few operations changed so that nothing new is in a register across a call:
a struct literal works out its fields before it allocates its block, and `with` on a list or map works out the added element before it
makes the copy.

A collection can only become due after an allocation, and every function that allocates polls at its start and on every round of its
loops, so the program never runs long without a safe point once one is due (a loop that does not allocate itself but calls a function
that does polls in that function).

### When it runs, and the policy

- **Threshold.** The collector counts the bytes its objects take: a small object's whole slot (its size class), a large one's block and
  header. The first collection comes at 8 MiB (`GC_MINIMUM_THRESHOLD`); after each, the next comes once the objects hold more than twice
  what survived (what the sweep kept), and never past the ceiling. Bytes, not objects as on the VM: native objects range from a 16-byte box to a list block of
  megabytes.
- **Ceiling.** 4 GiB still reachable by default (`GC_DEFAULT_CEILING`), `POLYIR_GC_CEILING=BYTES` to change it. A collection that leaves
  more reachable stops the program as its traps do (what was printed is flushed, the message goes to standard error, `llvm.trap`):
  `out of memory: N bytes are still reachable (limit L)`. A failing malloc stops it with `out of memory: the system has no more to give`.
- **Stress mode.** `POLYIR_GC_STRESS=1` keeps the threshold at 0: a whole collection at every safe point. (Part 3 adds
  `POLYIR_GC_STRESS=incremental`.)
- **Report.** `POLYIR_GC_REPORT=1` prints at the end: the collections, the objects (and bytes) reclaimed, the peak, what is left, the
  pages (spares included) and large objects the collector holds, and (Part 3) the pauses: how many, the longest, the median, the 99th
  percentile, their sum, and the mode.
- **Free.** The program's `Free` is a hint once the collector runs (`gc_free_hint` does nothing): an object goes at the first
  collection that finds it unreachable, never earlier, so a program that frees something it still uses cannot corrupt memory (checked).
  The compiler's own frees stay immediate where nothing else can refer to the block: a map's old tables after it grew (`gc_release`).
- **Pages given back.** An empty page is kept as a spare; a spare that was not needed during a whole round between two collections goes
  back to the C library at the end of the next collection (see "Where objects live").
- **Who starts it.** Only the entry point the compiler writes for a program's `main` calls `gc_start`. A module without one (a library
  a host calls, a dylib, a `CompileFunctionToLlvm` fragment added to some other module) never collects, because its host may hold
  references the collector cannot see; its objects are plain malloc blocks and `Free` frees, as before.
- **Off.** `CompileOptions::collectGarbage = false` (and any wasm target triple; `polyir --no-gc`; ninac's wasm route) builds the old
  way: the same `gc_allocate` names over plain malloc, calloc, realloc and free, and no records or safe points. A wasm module is a
  library its host calls, so it never collects anyway.
- **Threads.** The collector is single-threaded, like polyir programs. A program that runs polyir code on several threads through a host
  library must build with the collector off.

### Strings the Nina runtime makes (2026-10-31)

ninac's llvm target lowers a Nina program to polyir and adds a small runtime written in C and LLVM IR for the services polyir has no
builder form for (`ninac/codegen/llvm-polyir`: `runtime/NinaText.c`, compiled to IR by `gen_runtime_text.sh` into
`PolyirRuntimeStrings.cpp`, and the number-to-text pieces of `PolyirRuntimeText.cpp`). Until this change every string it returned was a
malloc block nobody freed, so a native Nina server that turns numbers into text, trims, slices, splits or reads lines all day grew
without bound, even though the strings polyir itself builds (`+`, `join`'s concatenations) were collected. Now they are objects too.

**What the runtime allocates, and from where:**

| what | functions | memory |
|---|---|---|
| a string returned to the program | `nina_text_from_i64/u64/i128/u128/f64/f64_lit`, `trim`, `slice`, `char_at`, `upper`, `lower`, `from_code`, `nina_console_read_line`, `nina_fs_read_text`, `nina_fs_list`, `nina_fs_canonical`, `nina_fs_read_line` | `gc_allocate(bytes, 0)`: an object, data layout (a string holds no references); a line or a file that grows while it is read grows with `gc_resize` |
| a string constant (`true`, `false`) | `nina_text_from_bool` | a global, as before |
| scratch the runtime frees itself | a path's prefixes (`fs_make_parents`), `dir/name` while a tree is removed, a directory's names before they are joined, the 64 KiB copy buffer, the table of open streams | malloc and free: never reaches the program, never an object |
| host memory | `FILE*` handles, `DIR*` | the C library's |

Split, join and replace are Std actions written in Nina over these natives and polyir's own string operations, so they need nothing more.

**How the runtime knows whether the collector is on: it does not ask.** It always calls `gc_allocate`, and every module it goes into
defines that function: the collector's (`Collector.c`) when the module is built with the collector, which is plain malloc until a
program's `main` calls `gc_start` — so the same runtime in a library a host calls (`@Llvm`, a dylib, `compileActions`) gives malloc'd
strings, as before — or the plain allocator (malloc under the same name) in a module built without it (`collectGarbage = false`: ninac's
wasm route). For that, libpolyir now gives a module the collector's functions as soon as IR added to it (`Module::rawIr`) calls
`gc_allocate`, whether or not the module has a `main` and whether or not polyir's own code allocates (`NeedCollectorFor` in
`LlvmRoots.cpp`, called for every fragment by `RawFunctions`); a fragment that already carries their definitions (one this compiler wrote)
is left alone. A weak symbol or a flag set by `gc_start` was not needed: `gc_allocate` already falls back to malloc before the collector
starts, which is exactly the library case.

**Why it is safe:**

- *Exact starts.* Every string the runtime returns is the start of its block, because the collector follows only a word that is exactly an
  object's address. The one piece that returned an interior pointer (128-bit text, whose digits are written back to front into a 48-byte
  buffer) now moves the digits to the front of the string (`llvm.memmove`) and returns the block itself.
- *Roots.* The compiled caller treats the result like any string it made: it is in a register of reference type, so `KeepRoot` stores it
  in a slot of the caller's frame record at once, and from there it is stored in variables or objects (with the barrier, as any reference).
- *No safe point inside the runtime.* No runtime function calls back into the program or calls `gc_poll`, and allocating never collects,
  so a fresh string held only in a C local (`double_float_text`'s digits, a line being read) cannot be reclaimed under it. While marking,
  a new object is black; while sweeping, one made in a page not yet swept is kept: both as for the compiled code's objects.
- *No barrier needed.* The runtime never stores a reference into an object (strings hold none), and the strings it reads (its arguments)
  it only reads.
- *Host strings stay host memory.* A `char*` a C library returns (`import native`, an `llvm` fence calling one) lies in no page of the
  collector and not in its large set, so it is never followed and never freed; nor does the collector ever hand one to `free`.

**What it changed, measured** (`StringChurn`: each round `i.toString()`, `(i / 4.0).toString()`, a concatenation, `trim`, `split("-")`,
`join("+")`, `replace`, `toUpperCase`, `slice`, `toLowerCase`, all dropped; built with `ninac llvm F.nina > F.ll && clang F.ll`, -O0,
`/usr/bin/time -l`):

| rounds | before: maximum resident | after: maximum resident | after: time |
|---|---|---|---|
| 200,000 | 93 MB | 10.4 MB | 0.64 s |
| 2,000,000 | 897 MB | 10.4 MB | 5.09 s (5.62 s before) |

Under `POLYIR_GC_STRESS=1` (919,999 collections for 20,000 rounds) and `POLYIR_GC_STRESS=incremental` it prints the same total, peaking
at 784 and 2,960 bytes of objects. `ninac/codegen/llvm-polyir/tests/StringMemoryTest.cpp` (ctest `nina_codegen_llvm_polyir_string_memory`)
keeps checking it: ten times the rounds (1,000,000 against 100,000, with 128-bit text too) must stay within 16 MB of the smaller run and
under 64 MB, and 3,000 rounds must print the total Python computes for them as they are and under both stress modes.

### What it costs

Measured on the programs below (Apple M-series, `polyir-check --release` or a debug build, best of seven; "off" is `--no-gc`; `churn` is
tests/data/gc/Churn.poly, the others are a few lines each, written for the measurement as the table says). "Object set" is the first version
(a malloc block with a 16-byte header per object and a hash set of all objects); "pages" is the allocator described above:

| program | what it does | object set, -O2 | pages, -O2 | off, -O2 | object set, -O0 | pages, -O0 | off, -O0 |
|---|---|---|---|---|---|---|---|
| `fib(27)` | calls, no references | 0.00 s, 1.3 MB | the same | the same | the same | the same | the same (the IR is identical) |
| `churn` | Churn.poly, 1,000,000 rounds | 0.539 s, 14.7 MB | 0.103 s, 9.7 MB | 0.044 s, 369 MB | 0.611 s, 14.7 MB | 0.271 s, 9.7 MB | 0.236 s, 676 MB |
| `trees` | 40 binary trees of depth 14 made and counted, one of depth 16 kept | 0.212 s, 76 MB | 0.041 s, 19 MB | 0.041 s, 68 MB | 0.312 s, 76 MB | 0.112 s, 19 MB | 0.041 s, 68 MB |
| `scan` | 1,000,000 structs kept in a list, read 30 times | 0.126 s, 92 MB | 0.101 s, 30 MB | 0.094 s, 26 MB | 0.252 s, 92 MB | 0.212 s, 30 MB | 0.163 s, 26 MB |
| `text` | 300,000 strings joined and sliced | 0.053 s, 20 MB | 0.017 s, 10 MB | 0.023 s, 34 MB | 0.063 s, 20 MB | 0.035 s, 10 MB | 0.028 s, 34 MB |

The collector's cost in time at -O2, over the build without it: `churn` +1,125% with the object set, +134% with pages; `trees` +417%,
now none measurable; `scan` +34%, now +7%; `text` +130%, now 26% *faster* than the build that never frees (it reuses the same few pages
instead of asking the system for new memory). In memory, `scan` keeps a million 16-byte structs alive: with the object set that took
(92 − 26) MB / 1,000,000 ≈ 65 bytes more per live object than a build that never frees (16 of header, malloc rounding the 32-byte block,
the set, and the survivors and worklist of a collection); with pages it takes about 3 bytes more (a quarter of a byte of page header, the
rest the worklist while a collection runs), and the slot itself is exactly the 16 bytes malloc gave.

(At -O2 a build without the collector lets LLVM drop a malloc whose block is never used, which is why `churn` off is so fast there;
the collector's `gc_allocate` cannot be dropped.)

What is left: per allocation, a few loads and a bit search, and zeroing a references object's slot; per collection, two bitmap words per
64 slots of every page; per reference-valued expression a store, and the record's push and pop in functions that keep references. A
program that keeps a million small objects alive now takes about what a build that never frees takes (the `scan` row); one that makes
garbage takes a fraction (`churn`, `trees`, `text`). A function with no references (`fib`) is compiled exactly as before.

### How it is checked

- `tests/NativeGcTest.cpp` builds the programs in `tests/data/gc` with clang and runs them with the collector's switches, measuring the
  child's maximum resident memory with `wait4`:
  - `Churn.poly` (each round a struct, two lists, a map, a closure, an optional and a string, all dropped): 1,000,000 rounds give the
    closed-form total, with 83 collections reclaiming 16,813,476 objects, in 9 MB resident, the same as 100,000 rounds (without the
    collector the 1,000,000-round build holds 708 MB, debug build; 369 MB at -O2); with the object set it was 107 collections and 14 MB
    (the threshold counted 16-byte headers then);
  - `Cycles.poly`: 200,000 pairs of nodes that refer to each other are reclaimed (524,290 objects in its one collection);
  - `Generators.poly`: 300,000 generators made and closed half way: 2,621,430 objects (frames, slots, blocks) reclaimed, 9 MB resident;
  - `Roots.poly` under `POLYIR_GC_STRESS=1` (520 collections), at -O0 and -O2, prints what the VM prints: a temporary held while the rest
    of the expression runs, a struct whose fields are calls, two closures sharing a cell, a closure that outlives its maker, interface
    values in a list, a coroutine keeping a list in its frame across yields and one only the resume keeps, a throw out of functions that
    hold references, strings joined in a loop (with `KeepRoot` disabled, the same run crashes: the check sees a missing root);
  - a global holding a list is a root (built with PolyIRBuilder: the source language's globals hold numbers only); `Free` is a hint;
    `KeepAll.poly` stops at a 4,096-byte ceiling with the message; a build without the collector has no safe points and gives the same
    answer.
- The whole of `ctest` (21 tests) passes as it is and under `POLYIR_GC_STRESS=1`.
- `../runtime/tests/vm_vs_native.sh`: the same 412 OK, 3 DIFF, 1 DECL, 11 SKIP as before the collector (after Step 0's fixes, below), row for row, with and without `POLYIR_GC_STRESS=1`.
- ninac's llvm target lowers Nina programs through this compiler, so the Nina corpus runs on the collector too: the bytecode column of `targets_via_polyir.sh` is unchanged (OK 87, DIFF 0, FAIL 0, SKIP 2,
  before and after), and every corpus program built by ninac's llvm route gives its `.expected` output (or, without one, the same as
  without stress) with and without `POLYIR_GC_STRESS=1`: 87 programs, none differ, the 2 the lowering refuses skipped. ninac's wasm route
  builds with the collector off. (Strings made by ninac's C text runtime (`NinaText.c`) were host memory then, never collected; since
  2026-10-31 they are objects too: "Strings the Nina runtime makes".)
- `lang/targets/java-mapping/llvm-check.sh`: the checked-in `.ll` files were regenerated (they now carry the collector's IR,
  and Flow/Numbers were already stale from the division-by-zero trap); the "nothing mentions the compiler" check ignores the
  collector's `POLYIR_GC_*` switch names.
- When the page allocator replaced the object set (2026-10-31) all of the above was run again: `ctest` 21/21, as it is and under
  `POLYIR_GC_STRESS=1`; `vm_vs_native.sh` the same 412/3/1/11 rows, row for row, with and without stress (run from `lang/tests`, where the
  WAV fixtures are: from a folder without them, `WavTests` reads past a 4-byte host `calloc` and its answer depends on what the C library
  put next to it, so two of its rows can differ under stress); the Nina corpus table of `targets_via_polyir.sh` (all eleven columns, OK
  791, DIFF 0, FAIL 0, SKIP 13; bytecode OK 87, SKIP 2) cell for cell the same as before the change; ninac's llvm route gives the same
  output as `.expected` (or as without stress) for all 87 programs it builds, with and without stress; ninac (8/8) and nina (5/5) tests
  pass and the orchestrator snapshot is still a fixed point. Of the java-mapping `.ll` files the five that allocate were regenerated:
  only the embedded collector's IR and its declarations (`posix_memalign`, `llvm.ctpop`, `llvm.cttz`, `llvm.memcpy`) changed.

### Native fixes found on the way (Step 0)

`../runtime/tests/vm_vs_native.sh` had 19 DIFF rows before this work; the native bugs behind them were fixed first so the comparison could check
the collector: a `let`/`var` with a declared type now brings its starting value to that type (`var level: float = 1.0` stored a double
in a float slot: 15 rows); an either/variant case brings its payload to the case's type (`eitherOk<double, i64>(3)`); a list as a map
key is hashed and compared by its elements, as on the VM (`map<array<i64>, string>` crashed); the harness strips only the VM's value,
not a whole last line (`io.poly`). Left: `StreamTests:checkSoundStreamPlaysTheSound` (a deliberate VM refusal) and two `SoundTests`
filters that differ in the last float digit (the VM works f32 arithmetic in double precision and rounds only when storing; native code
rounds every operation).

## Part 3: bounded pauses — the incremental collector (landed)

A stop-the-world collection marks everything reachable before the program may go on, so its pause grows with the data the program keeps:
a game that keeps a world of a million entities stops for 15.6 ms at every native collection (and 63 ms on the VM), and a frame lasts
16.7 ms. Part 3 spreads each collection over many short **slices** between which the program runs, on both routes, with one design (the
same steps, the same barrier, the same colours, the same pacing), so a program behaves alike on the VM and natively. Its pauses are now
bounded by a budget of work, not by the amount of live data: the same game pauses at most 49 µs natively (99% of its pauses under 24 µs),
in about the same total time. The model of Parts 1 and 2 (mark and sweep, the roots, the safe points) is unchanged; only *when* the work
is done changed.

### Tri-colour marking

Marking is described with three colours:

- **white**: not reached yet. When marking is over, every white object is garbage.
- **grey**: reached (its mark bit is set), but its own references not looked at yet. The grey objects are the worklist.
- **black**: reached, and every reference inside it looked at.

Marking starts by making the roots' objects grey, then repeats one step — take a grey object, make every white object it refers to grey,
and it is black — until nothing is grey. Parts 1 and 2 already worked this way (the worklist is the grey set); the step is the unit an
incremental collector can stop after. A **slice** takes grey objects until it has done its budget of work, then returns to the program;
the next slice goes on where it stopped. A big object is taken in chunks (up to the budget, and at most 4,096 values or words at a time),
so one long list cannot make one long slice; what is left of it goes back on the worklist *under* the grey objects its chunk just pushed,
so those are traced first and the worklist stays short.

### Why a write barrier is needed

Between two slices the program runs, and it moves references around. Marking goes wrong in exactly one way: the program takes the only
reference to a white object W out of an object marking has not looked into yet (a grey or white one), by overwriting it, and keeps W only
somewhere marking has already looked — a black object, or a root (a frame's slot), which marking looked at when the cycle began. Then
nothing left for marking to look at leads to W: it stays white and is freed while the program still uses it. (That the program can
only reach W again through such places is what makes the bug quiet: it shows up later, as a wrong answer or a crash, in whatever object
reuses W's memory.)

A **write barrier** is a small piece of code run at every store of a reference into an object, which tells the collector about the store
while it marks. There are two classic kinds:

- **Dijkstra's (incremental update)**: grey the reference being *stored*. A black object can then never point at a white one. But stores
  into roots (local variables, the frame records, globals) have no barrier — there are far too many of them — so W hidden in a root is
  still lost, and the roots must be looked at **again at the end of marking**; whatever that finds must be marked before the cycle can end.
  That final pause grows with the stack and with what the program reached meanwhile.
- **Yuasa's (snapshot at the beginning)**: grey the reference about to be *overwritten*. Then nothing that was in the heap when the cycle
  began can disappear before marking has seen it, so **everything reachable at the beginning of the cycle is marked** (the snapshot); with
  the roots looked at once, at the very beginning, and objects made during the cycle made black, nothing reachable can be missed.

**Both routes use the snapshot (Yuasa) barrier.** The reasons: the roots are looked at once, when the cycle begins, and never again, so no
pause at the end; a cycle ends the moment nothing is grey, inside an ordinary slice; and the barrier needs the place being written, not the
value (natively: the address of the word about to be overwritten, which the compiler always has; on the VM: the value about to be
overwritten). The price is **floating garbage**: what was reachable when the cycle began survives the cycle even if the program drops it
meanwhile (it goes at the next one), as does everything made during the cycle.

### The roots: one scan at the start of the cycle

A cycle begins with its **root scan**, in one pause: every running frame (VM: `Machine::runningFrames`' slots and operand stacks; native:
the frame records from `gc_top` down), the globals, and the VM's pinned values. That pause is as long as the stack is deep and the globals
are many — not as long as the heap is big — and it is the only work of a cycle that is not cut into slices (the first slice follows it in
the same pause). The roots are never looked at again in that cycle: with the snapshot barrier they need not be (a reference the program
writes into a root afterwards came from the heap, where the barrier keeps it, or from a new object, which is black).

One kind of object is written like a stack and not through barriers: a **coroutine's frame**. While a coroutine runs, its frame is written
by the coroutine itself (VM: its `Frame` slots and stack; native: its frame record and what LLVM keeps across suspends, all inside the
frame object). So when a coroutine **resumes while marking runs**, every reference in its frame is greyed first — what the barrier would
have done for each of those writes (VM: `ResumeCoroutine`/`CloseCoroutine`; native: `gc_link_frame(record, frame)`, which the compiler
now passes the frame handle). It costs a look at one frame per resume, and only during marking.

### Objects made during a cycle

- **While marking**, a new object is **black**: it is marked at once, and marking never looks inside it. It holds nothing from before the
  cycle; whatever is stored into it later is either from the snapshot (and marked through it) or new (black too).
- **While sweeping**, a new object is marked if the sweep has not reached its place yet (VM: its cell is at or past the sweep's cursor;
  native: its page's `swept` is behind the collector's `cycle`), so the sweep keeps it; otherwise it is white, ready for the next cycle.
  A large object made while sweeping is white: it goes at the newest end of the large list, which the sweep has already passed.

### Sweeping in slices

The sweep is incremental too, a cursor walking forward a slice at a time: on the VM over the cells (`Marking::sweepAt` up to the
`sweepEnd` the heap had when marking ended), natively over the pages of each size class (`GcSweep`: the class, the last page kept), then
the large list from newest to oldest. A page the sweep has not reached keeps its marks, which still tell its live objects from its
garbage; a page that ends up empty becomes a spare; spares unused for a whole round go back to the C library when the sweep ends.

### When a cycle ends, and the next begins

Marking ends inside the slice that finds nothing grey (in the incremental stress mode the marks are checked first, below). The sweep
starts at once. When the sweep has passed everything, the cycle is over: what the sweep kept is what survived, the next threshold is twice
that (as before), and the ceiling is checked against it.

### Pacing: when the slices come

Every safe point still asks one question, one comparison: is the heap past `collectAt`? Between cycles `collectAt` is the threshold, and
a cycle begins there. During a cycle, it is where the program's allocations since the last slice have earned the next one:

| | native (`Collector.c`) | VM (`Collector.cpp`) |
|---|---|---|
| unit of work | a word looked at (or a bitmap word swept) | a value looked at (or a cell swept) |
| a slice | at most `POLYIR_GC_SLICE` units, default 10,000 (`GC_DEFAULT_SLICE`) | at most `VmOptions::collectorSlice` (or `POLYIR_GC_SLICE`), default 10,000 (`DEFAULT_COLLECTOR_SLICE`) |
| what allocation earns | one word per byte allocated (`GC_PACE_WORDS_PER_BYTE`) | 32 units per object made (`PACE`) |
| next slice due | after `slice` more bytes | after `slice / 32` more objects |

Why those rates. A cycle begins at the threshold T, twice what survived the last one, so at most half the heap is reachable. Natively that
is at most T/16 words, so at one word per byte the marking is done before the program has allocated T/16 more bytes: the heap peaks a
little above T, near where a stop-the-world collector's would, and little floating garbage is made. (A first try at a quarter word per byte
let the game's heap reach twice that.) On the VM a reachable object costs a few values to look at and a cell to sweep, and 32 units per
object made keeps the growth during a cycle to some tenth of T. Should a program allocate faster than that anyway (the heap half as big
again as when the cycle began), a slice comes at every safe point until the cycle is over: each one still within the budget, but the
program then makes little progress between them, so the collector catches up. If an incremental cycle lets the heap pass the ceiling, the
cycle is finished at once (one long pause rather than running out of memory).

### Where the barriers are

Natively, the compiler calls `gc_write_barrier(ptr word)` before the store (one load of `@gc_marking` and a branch when marking is not
running; LLVM inlines it at -O2), and the collector itself shades the words of a block it moves or gives back while marking:

| store | where |
|---|---|
| an assignment to a field, an element (list, buffer, grid, map's cell, sparse's cell) or a captured variable's cell | `Assign` → `HeapStore` (LlvmCompiler.cpp, LlvmRoots.cpp) |
| a `for` step or a label's argument into a captured variable's cell | `SlotStore` (`LoopStep`, LlvmLabels.cpp `HandOne`) |
| a push: the slot past the length can still hold an element a `truncate` left there | `AppendElement` (LlvmLists.cpp) |
| a coroutine's yield slot, at every `yield` | `YieldStatement`, `CoroutineYielded` (LlvmCoroutine.cpp) |
| a map's or set's keys and values moved and cleared by `remove` | `map_remove` (IR text in LlvmMaps.cpp) |
| a list's elements moved by growth, a map's tables replaced by growth | the collector: `gc_resize`, `gc_release` shade the old block first |
| a coroutine's frame, written by the coroutine | the collector: `gc_link_frame` shades it on resume |

Needing none: stores into objects made just before (a struct literal's fields, a list literal, a list header, a copy, `with`, `+` on lists,
`map_new`/`map_copy`/`map_grow`'s new tables, sum values, closures and their environments, interface values, a coroutine's result block) —
nothing was there to lose; stores into stack slots, frame records and globals — roots, looked at when the cycle began; stores of numbers
(the word overwritten held a number of the same type: a slot's type never changes); raw `ptr<T>` and host memory — never traced. A module
built without the collector gets a `gc_write_barrier` that does nothing (the map helpers call it either way).

On the VM, `WriteBarrier(heap, overwritten)` (VmValues.hpp: one comparison of the phase when marking is not running) greys the value
about to be overwritten, and `WriteBarrierAll(heap, object)` every value of an object about to be moved or removed in bulk:

| store | where |
|---|---|
| `SetField` | Run.cpp `setField` |
| `SetIndex` on a list's element or a map's existing entry | Builtins.cpp `writeElement`, `writeEntry` |
| a box's cell (`StoreBoxed`, `StoreGlobal`, `StoreBound` into a cell) | VmValues.cpp `BoxSet`, Run.cpp `storeBound` |
| `truncate` (each element it drops) | Builtins.cpp `truncated` |
| `remove` from a map or set (the erase moves every entry after it) | Builtins.cpp `removedEntry`, `removedMember` (`WriteBarrierAll`) |
| a coroutine's frame, before it resumes or closes | Run.cpp `ResumeCoroutine`, `CloseCoroutine` (`WriteBarrierAll`) |

Needing none: `push`, a set's `add` and a sparse's writes only append (nothing is overwritten); copies, `with`, closures and new boxes are
new objects (black); frames, globals and pins are roots. The C++ code of the host layer and the `nina_*` runtime only makes new objects.

### Modes and switches (both routes)

| | native (environment, read at `gc_start`) | VM |
|---|---|---|
| incremental (the default) | — | `VmOptions::incrementalCollector = true` |
| stop-the-world: each cycle in one pause | `POLYIR_GC_MODE=stop` | `incrementalCollector = false`, or `POLYIR_GC_MODE=stop` |
| the budget of one slice | `POLYIR_GC_SLICE=N` | `collectorSlice`, or `POLYIR_GC_SLICE=N` |
| stress: a whole stop-the-world collection at every safe point | `POLYIR_GC_STRESS=1` | `collectEverySafePoint`, or `POLYIR_VM_STRESS_GC=1` |
| incremental stress (below) | `POLYIR_GC_STRESS=incremental` | `verifyIncremental`, or `POLYIR_VM_STRESS_GC=incremental` |
| the report | `POLYIR_GC_REPORT=1` (standard error, at exit) | `VmOutcome`'s fields; `polyir --interp` with `POLYIR_GC_REPORT=1` |

### The incremental stress mode, and how a missing barrier is caught

The stop-the-world stress mode cannot see a missing barrier (it never lets the program run in the middle of a cycle). The **incremental
stress mode** makes the barrier's window as wide as it can be: a slice of one unit (one word, one value, one page or cell of sweep) at
every safe point, and a new cycle as soon as the last one ends, so the program runs in the middle of a cycle all the time and big objects
are always half traced. And at the end of every marking it **checks the marks**: a walk of everything the program can reach now, made
apart from the marking (with a set of its own of the objects it has seen, so it walks into black objects too and finds a white object
behind one), must find no unmarked object — by the snapshot argument, everything reachable now is marked. One it does find is reported
before the sweep could free it: the native program stops with `gc verify: N reachable objects were left unmarked by the incremental
marking (a write barrier is missing)`, the VM run ends with the same words as a trap. (Natively the walk takes any word that equals an
object's address for a reference, as the marking does; an integer stored after the cycle began that equals the address of a garbage object
would be a false alarm, which no check has seen.)

`tests/data/gc/Barriers.poly` is written to be caught: each of its checks makes its objects, lets a new cycle begin, then takes the only
reference to a list out of an object the collector may not have looked into yet and keeps it only in variables and in objects made
afterwards. With the barrier calls left out of the compiler (`HeapStore` writing a plain store) or out of the VM (`WriteBarrier` doing
nothing), the incremental stress mode stops the elements, fields, captured cells and truncate-then-push checks every time, at -O0 and -O2
and on the VM (`gc verify: 1 to 18 reachable objects ...`); with them, every check passes. The yield-slot and coroutine-frame checks pass
too, but they did not fail when their barrier was left out (the program keeps copies the collector finds), so for those two the mode
shows only that nothing breaks.

### What it costs

Apple M-series; native programs built with `polyir-check` (`--release` is -O2), best of 7 (the game: best of 3; the VM game: one run);
"before" is the collector of Part 2 as it was before this work, "stop" and "incremental" are the new one in its two modes.

**The game loop** (`tests/data/gc/Game.poly` with a world of 1,000,000 entities and 10,000 frames, each making 2,000 particles and 250
lists and replacing 16 entities):

| | before | stop | incremental |
|---|---|---|---|
| native -O2: total time | 0.339 s | 0.365 s | 0.340 s |
| native -O2: longest pause | (not measured) | 15.6 ms | **0.049 ms** |
| native -O2: p99 / median pause | | 15.6 ms / 13.1 ms | 0.024 ms / 0.017 ms |
| native -O2: pauses, sum of pauses | | 16, 186 ms | 9,868, 157 ms |
| native -O2: peak heap, resident | 115 MB, 129 MB | 115 MB, 114 MB | 142 MB, 140 MB |
| native -O0: total, longest, p99 | 0.993 s | 1.072 s, 41.4 ms, 41.4 ms | 1.011 s, 0.094 ms, 0.072 ms |
| VM: total time | 10.46 s | 10.65 s | 10.32 s |
| VM: longest pause | (not measured) | 63.4 ms | 5.1 ms |
| VM: p99 / median pause | | 63.4 ms / 56.0 ms | 0.22 ms / 0.067 ms |
| VM: pauses, sum of pauses | | 35, 1.67 s | 18,158, 1.48 s |
| VM: peak heap (objects), resident | 1.36 GB | 2,054,451, 1.36 GB | 2,531,567, 1.64 GB |

The VM's longest incremental pause is not the collector's work: timing the slices showed it is one cell whose emptying (`Obj{}`, the C++
allocator freeing that object's vectors) took 7 ms by itself, inside a sweep slice; every other slice stays near the budget. The VM keeps
its objects as C++ vectors, so what the C library's `free` does is outside the budget (the native collector has its own allocator, and no
such outlier).

**Throughput** (the programs of Part 2, and on the VM the ones of Part 1):

| program | before | stop | incremental |
|---|---|---|---|
| `churn` -O2 / -O0 | 0.099 s / 0.263 s, 9.7 MB | 0.104 s / 0.283 s, 9.7 MB | 0.104 s / 0.283 s, 9.7 MB |
| `trees` -O2 / -O0 | 0.037 s / 0.108 s, 18.9 MB | 0.043 s / 0.127 s, 18.9 MB | 0.039 s / 0.111 s, 23.8 MB |
| `scan` -O2 / -O0 | 0.098 s / 0.204 s, 29.5 MB | 0.098 s / 0.194 s, 25.4 MB | 0.099 s / 0.199 s, 25.4 MB |
| `text` -O2 / -O0 | 0.015 s / 0.033 s, 9.6 MB | 0.016 s / 0.036 s, 9.6 MB | 0.016 s / 0.036 s, 9.7 MB |
| VM: 3-million-iteration label loop | 0.501 s | 0.498 s | 0.503 s |
| VM: `fib(27)` | 0.150 s | 0.153 s | 0.152 s |
| VM: 1,000,000 rounds of a struct, two lists, a map | 1.054 s | 1.082 s | 1.083 s |
| VM: `churn`, `trees`, `scan`, `text` | 1.817, 1.369, 10.53, 0.222 s | 1.845, 1.421, 10.50, 0.224 s | 1.843, 1.430, 10.56, 0.223 s |

Incremental against stop-the-world: from 1% slower to 10% faster (`trees` natively: 9% faster); against the collector before this work:
at most 8% slower (`trees` -O2 +5%, `churn` +5%, `text` +7% at -O2, a millisecond), and the VM within 4%. Where the time goes: a
comparison per allocation (the new object's colour), the barrier's load and branch per reference stored, the pauses' clock, and marking
that can stop (the budget's checks, an entry of two words). The stop-the-world mode pays the same per-allocation and per-store costs
without the incremental mode's lower heap traffic, which is why it is the slowest of the three on `trees` (+16% natively). In memory, the
incremental mode's heap peaks a little higher (floating garbage: `trees` 24 MB instead of 19, the game 142 MB instead of 115 natively and
2.53 instead of 2.05 million VM objects).

### The default, and why

**Incremental is the default on both routes.** Everything passes in the incremental stress mode (ctest, `vm_vs_native.sh`, the Nina
corpus, below), the barriers it relies on are shown to be caught when missing, and its throughput is within the target: no benchmark is
more than 8% slower than the collector before this work, it is never slower than the new stop-the-world mode by more than 1%, and on the
game loop it is as fast as before while its longest pause falls from 15.6 ms to 49 µs natively (63 ms to 0.22 ms at the 99th percentile on
the VM). Stop-the-world stays one switch away (`POLYIR_GC_MODE=stop`, `VmOptions::incrementalCollector = false`) for a program that wants
the smallest heap and does not care how long one pause is.

### What is still not bounded by the budget

- The **root scan** at the start of a cycle: as long as the stack is deep (a recursion 20,000 calls deep has 20,000 frame records to look at).
- Work the program itself does in proportion: shading a block the collector moves or gives back while marking (a big map's old tables when
  it grows, a long list's elements when they move), and the frame of a coroutine resumed while marking — each as long as the copy the program
  is making anyway, or the frame.
- On the VM, freeing a dead object's vectors is the C++ allocator's work (the 5 ms outlier above), and growing the heap's vector of cells
  copies every cell (a hiccup of the program's own allocation, not of the collector, which the stop-the-world collector has too).
- Memory: floating garbage makes the heap peak higher than stopping the world does (above). The collector stays single-threaded.

### How it is checked

- `tests/NativeGcTest.cpp`, section 4: `Barriers.poly` under `POLYIR_GC_STRESS=incremental` at -O0 and -O2 prints what the VM prints, and
  the VM in its incremental stress mode prints the same; `Roots.poly` under the incremental stress mode at -O0 and -O2 and on the VM; map
  values taken out by `remove` (built with PolyIRBuilder: the source language has no `remove`) under the incremental stress mode, natively
  and on the VM; `Game.poly` (200,000 entities, 400 frames) stopping the world has one pause per collection, incremental has more than ten
  times as many pauses as collections, the longest under a quarter of the stop-the-world one, and both give the same answer.
- `../runtime/tests/VmTest.cpp`, section 12: stop-the-world has one pause per collection and the churn's peak stays at 65,537 objects; incremental
  slices its 61 cycles into 488 pauses (the longest some 0.2 ms) with a peak of 65,850 objects; churn, cycles and coroutines pass in the
  incremental stress mode (3,400,006 cycles for the churn).
- The whole of `ctest` (21 tests) passes as it is, with `POLYIR_GC_MODE=stop`, under `POLYIR_GC_STRESS=1 POLYIR_VM_STRESS_GC=1` and under
  `POLYIR_GC_STRESS=incremental POLYIR_VM_STRESS_GC=incremental`.
- `../runtime/tests/vm_vs_native.sh` (run from `lang/tests`, where the WAV fixtures are): the same 412 OK, 3 DIFF, 1 DECL, 11 SKIP as before, row for
  row, in all four modes (as it is, `POLYIR_GC_MODE=stop`, the two stress modes with both routes' switches set).
- The Nina corpus (`targets_via_polyir.sh`, whose reference column is ninac's llvm route on this collector, and whose bytecode column is the
  VM): all eleven columns OK 791, DIFF 0, FAIL 0, SKIP 13 (bytecode OK 87, SKIP 2), cell for cell the same table as before. Every corpus program ninac's llvm route builds gives its `.expected` output (or the plain run's) as it is, stopping the
  world, under `POLYIR_GC_STRESS=1` and under `POLYIR_GC_STRESS=incremental`: 87 programs the same in all four modes, none differ, the 2 the lowering refuses skipped. ninac (8/8) and nina (5/5) tests pass and the
  orchestrator snapshot is still a fixed point (`snapshot.sh --check`: unchanged).
- `lang/targets/java-mapping/llvm-check.sh`: the five `.ll` files that allocate were regenerated; what changed is the embedded collector's
  IR (and its declarations: `timespec_get`, `strcmp`, the renamed `%struct.gc_timespec`) and, in the program, the barrier calls (`map_remove`'s,
  and the method stores in `Methods.ll`); Flow and Numbers do not allocate and did not change.
- Found on the way: the collector's IR first named C's `struct timespec` `%struct.timespec`, which ninac's own text runtime (embedded in the
  same module) also defines, so every ninac llvm build with a string helper failed to assemble ("redefinition of type"); `filter_ir.py` now
  renames the C library's struct types `%struct.gc_*`, as it renames the string constants. It also drops the alias-scope declarations clang
  -O1 started to emit once the collector grew (their metadata is dropped already).
