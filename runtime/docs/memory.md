# Memory: how the VM reclaims what a program no longer uses

The VM's collector (`src/vm/Collector.cpp`, libpolyir-vm). The model both collectors share, the words used, and the
native builds' collector (Parts 2 and 3: the code LLVM builds carry) are in
[../../core/docs/memory.md](../../core/docs/memory.md); this is Part 1 of that design.

## Part 1: the VM (landed)

The collector is one file, `src/vm/Collector.cpp`, written to be read top to bottom: an opening comment explains mark and
sweep, roots and safe points (and, since Part 3, slices and the write barrier), then `CollectGarbage` tells the story — the work
at a safe point: begin a cycle when one is due, then one slice of marking or sweeping (or the whole cycle at once, stopping the
world), then when the next work is due — each step defined right below it in that order. This part describes the model, which
Part 3 did not change; Part 3 says how the work is spread out.

### Where references live

| place | what it holds | how the collector finds it |
|---|---|---|
| a frame's slots and operand stack | locals, temporaries | every running frame is on `Machine::runningFrames`: `runCode` (Run.cpp) calls `EnterFrame` before its first instruction and `LeaveFrame` after its last, on every way out |
| globals | one box per global | `Machine::globals` |
| a suspended coroutine | its own frame | the coroutine object (reached like any other object) |
| inside an object | fields, elements, keys, values, a box's cell, a closure's captures | traced through the object, by its kind |
| inside a value | an optional's, variant's or either's payload (`Value::inner`) | traced through the value (a loop unwraps nested payloads) |
| a C++ local of the VM itself | a builtin that runs program code while it holds a value | the builtin **pins** the value (`PinValue`/`UnpinValue`, `Machine::pinned`) |

Every place C++ code runs program code was audited (each caller of `CallFunction`, `CallValue`, `CallNamed`, `RunFrame`,
`ResumeCoroutine`, `CloseCoroutine`):

- a call instruction, a closure call, an interface call (`Run.cpp`) and an interface builtin (`Builtins.cpp`, `callMethod`)
  move the arguments and the receiver into the callee's frame before it runs, so they are already in a root; a closure's
  record is copied out of its object first, so the closure object itself need not survive the call;
- the host layer (`Vm.cpp`) passes the entry's arguments the same way, and a global's initializer stores its value only
  after the initializer has returned;
- **coroutine resume and close** are the one place that needs a pin: the coroutine is popped off the operand stack as the
  builtin's receiver, so while its body runs nothing but the builtin holds it (`Resume(make())` keeps no other reference).
  `ResumeCoroutine`/`CloseCoroutine` now take the coroutine's *reference* (`const Value&`) instead of an `HeapObject&` into the
  heap — an `HeapObject&` dangles when the heap's vector grows during the call — hold the coroutine by its shared pointer, and pin
  the reference while the frame runs.

A value travelling back from a call (an `Outcome`) is never exposed to a collection: it is pushed on the caller's stack
before the caller's next instruction, which is the next safe point.

Strings need nothing: their text is reference-counted and freed by C++, and their identity (what `===` compares) is now a
counter (`Heap::nextStamp`, `NewStamp`), not a heap cell, so strings take no heap cells at all. `===` on strings still
compares identities (VmTest: `string identity`); the only visible change is that a printed `<object #N>` no longer skips
the ids strings used to take, and an id may be reused once its old object was collected.

### When it runs: safe points

The collector never runs inside an allocation. `Alloc` (VmValues.cpp) only counts (`Heap::objects`, the cells in use); the
dispatch loop (`runInstructions` in Run.cpp) asks `IsCollectionDue` — one comparison, `heap.objects > memory.collectAt` —
before every instruction, and collects there, the one moment every live reference is in a slot, on a stack, in a global or
in a pinned value.

### What a collection does

1. **Mark.** Start from the roots and mark each object reached. Marking an object only pushes it on an explicit
   **worklist**; tracing pops objects off it and marks what they refer to, by kind: a struct's, list's, set's, map's or
   sparse's `items` and `values`, a box's `cell`, a closure's `captured`, a coroutine's frame slots and stack. No recursion,
   so a long chain cannot overflow the C++ stack. The mark bits are `Marking::marked` (a byte per cell, kept with the heap,
   `Heap::marking`, and cleared by the sweep as it passes, so a cycle never starts by clearing a million of them).
2. **Sweep.** Every unmarked cell that is in use is emptied (`HeapObject{}`, so its vectors, text and coroutine go back to C++
   at once), its kind becomes `ObjectKind::Free`, and its index goes on `Heap::freeCells` (a deque, so freeing a million cells
   never copies the list of them).
3. **Reuse.** `Alloc` takes the most recently freed index before growing the heap. Cells never move: indices are
   references.

An index that is reused can never be confused with the old object: the old one was unreachable, so nothing still holds
its index. Explicit `Free`/`FreeBox` stay no-ops in the VM (hints): an object goes only when a collection finds it
unreachable.

### The policy, and the numbers chosen

- **Threshold.** After a collection the next comes once the heap holds more than `max(minimum, 2 × survivors)` objects,
  so a program that keeps a steady amount of data collects at a steady rate and its heap stays at about twice its live
  size. The **minimum is 65,536 objects** (`MINIMUM_THRESHOLD`): a heap cell is 224 bytes before what its lists hold, so
  that is some 15 MB — small enough that a long run stays flat, large enough that a collection's fixed cost is spread
  over many allocations. The threshold is never set past the ceiling.
- **Ceiling.** `VmOptions::heapCeiling`, by default **16,777,216 objects** (`DEFAULT_HEAP_CEILING`, 2^24, about 3.5 GB of
  cells): far beyond what a VM program keeps reachable (the corpus peaks in the thousands) and still short of what a
  laptop has. A collection that leaves more objects reachable stops the run with the trap
  `out of memory: N objects are still reachable (limit L)`. Garbage never counts against it, only survivors do; the heap
  passes the ceiling only by the one object whose allocation sets the deciding collection off. The call-depth guard
  (`stack overflow` at 20,000 nested calls) stays.
- **Stress mode.** `VmOptions::collectEverySafePoint`, or `POLYIR_VM_STRESS_GC=1` in the environment (read once, in
  `Vm.cpp`), keeps the threshold at 0: a whole collection between every two instructions, so a missing root shows up at once
  as a wrong answer. (Part 3 adds the incremental stress mode, `POLYIR_VM_STRESS_GC=incremental`.)

### What a host can see

`VmOutcome` carries the run's memory statistics: `collections`, `objectsReclaimed` and `peakObjects` — the most objects the
heap held at once (reachable or not yet collected; measured whenever collecting is due and at the end of the run) — and, since
Part 3, the pauses: `pauses`, `longestPauseNs`, `medianPauseNs`, `p99PauseNs`, `totalPauseNs`. `polyir --interp` prints them on
standard error with `POLYIR_GC_REPORT=1`, in the words a native build's report uses.

### How it is checked

- `tests/VmTest.cpp`, section 12: a loop allocating a struct, a list, a map and a closure each iteration for 1,000,000
  iterations gives the same total as the closed form and peaks at 65,537 objects (61 collections); 200,000 two-node
  cycles are reclaimed (peak 65,537), and run under a ceiling of 1,000 objects too (peak 1,001); a value held in `main`
  and a list in a coroutine's own frame survive collections forced inside the coroutine (stress mode), including a
  coroutine that only the resume's pin keeps alive; a program that keeps 5,000 structs in a list stops at a 1,000-object
  ceiling with the trap; `===` on strings still compares identities.
- The whole of `VmTest`, `NinaRuntimeTest` and `tests/vm_vs_native.sh` under `POLYIR_VM_STRESS_GC=1` give the same
  results as without it (vm_vs_native: the same 427 rows, row for row, as before the collector).
- The bytecode column of the corpus (`targets_via_polyir.sh`) is unchanged: OK 87, DIFF 0, FAIL 0, SKIP 2.
- A Nina program run by `ninai` that builds and drops records and lists for 30 s holds a flat maximum RSS (about 425 MB,
  most of it the compile; the same for 10,000 rounds and 250,000), where before the collector it grew without bound
  (2.2 GB at 10,000 rounds, 4.0 GB at 20,000).
- The corpus's bytecode column under `POLYIR_VM_STRESS_GC=1` is unchanged as well (OK 87, DIFF 0, FAIL 0, SKIP 2).
- Speed (best of five, `polyir-check --interp`): a 3-million-iteration label loop runs in 0.52 s, as before; a call-heavy
  `fib(27)` in 0.155 s (0.175 s before); an allocation-heavy loop (1,000,000 rounds of a struct, two lists, a map) in
  1.10 s (1.35 s before) with a maximum RSS of 52 MB (4.2 GB before): a reused cell is cheaper than a growing heap.

