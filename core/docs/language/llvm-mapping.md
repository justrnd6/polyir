# polyir → LLVM IR

`runtime` (libpolyir) compiles the checked HIR to LLVM IR text. The output is written to read like IR a person (or clang at `-O0`) would write: values are named for what they are, blocks for what they do, there is no scaffolding that mentions the compiler, and a debugger can follow it back to the `.poly` source.

```
polyir f.poly --emit-hir-json > f.json          # the Nina front end checks the program
polyir-hir f.json --llvm > f.ll                  # libpolyir compiles it
polyir-hir f.json --llvm --debug --source f.poly > f.ll   # ... with debug info
```

`examples/java-mapping/*.ll` are the outputs for the example programs; `examples/java-mapping/llvm-check.sh` builds each program natively and from its `.ll` and checks both give the same result (`--update` rewrites the `.ll` files). The Nina implementation's own `polyir f.poly --emit-llvm` goes through the same block tidying (below).

## How the code reads

| in the output | what it is |
|---|---|
| `%total`, `%i.addr` | the stack slot of a variable, named for it; a parameter `n` arrives as `%n` and is kept in `%n.addr` |
| `%total.val`, `%n.val1` | a read of that variable |
| `%add`, `%sub1`, `%cmp`, `%conv`, `%call`, `%cond` | an operation, named for its kind; a repeat gets a number (`%add`, `%add1`, …), never one a variable already has |
| `if.then`, `for.body`, `to.done`, `try.end`, `catch` | blocks, named for their role; a label's parameters are `phi`s at the head of its block (`%i`, `%total`); `to.<label>` is a block that hands values to a label that cannot use phis (below) |
| `@program_main` and `@main` | the program's `main` and the one the system calls, which returns its result as the exit status |
| `@map_find_word`, `@hash_string`, `@eq_S_Point`, `@string_hash`, `@zero_cell`, `@thrown`, `@vtable_Dog_Animal` | the small runtime the program needs, written into the module only when it is used |

## Types and operations

| polyir | LLVM |
|---|---|
| `i1`..`i64`, `u8`..`u64`; `f32`, `f64` | `i1`..`i64`; `float`, `double` |
| string, struct, pointer | `ptr`; a string is a constant, a struct a `malloc`ed object shared by reference |
| `+ - *` | `add` / `sub` / `mul` (they wrap); operands of different widths are converted to the result's width first |
| `/` `%` | `sdiv`/`srem` (a divisor of -1 handled so the smallest number wraps), `udiv`/`urem` for unsigned |
| `<<` `>>` | the count taken modulo the width, then `shl` / `ashr` / `lshr` |
| `&&` `\|\|` | short-circuit with a `phi` |
| `==` on strings, structs without `var` fields, maps and sets | `strcmp`; field by field; entry by entry |
| `===` | pointer comparison |
| `mutarray`, `array` | a header (length, capacity, elements); `push` grows with `realloc`; an index out of range calls `llvm.trap` |
| `T[N]`, `buffer`, `grid`, `tensor` | a zeroed block, row-major, every index checked |
| `vec<T,N>` | an LLVM vector (`shuffle` is `shufflevector`) |
| `map`, `mutmap`, `set`, `mutset` | open addressing with linear probing, doubling at three quarters full; keys compared by bits, text or fields; a missing key reads as zero |
| `optional`, `either`, `variant` | a block holding the case number and the value; reading the wrong case traps |
| closure | a block of the body's address and its environment; a captured variable lives in a heap cell the closure and its creator share |
| interface | a block of the object and a constant table of method addresses |
| `try` / `throw` | a global flag checked after each call; no unwinding tables; a program with no `try` pays nothing |
| atomics | `load atomic`, `store atomic`, `atomicrmw`, `cmpxchg`, `fence` |
| sparse | a block of three list headers (row keys, column keys, values); a write appends, a read scans from the end (`sparse_find`) |
| coroutines | one `presplitcoroutine` function on `llvm.coro.*`; a call returns a block of the handle and the yield slot; `resume` is a variant |

## Basic blocks

Variables are stack slots, as in clang's `-O0` output; `opt -mem2reg` (or `clang -O1`) turns them into registers and phis. The parameters of a label (`label loop(i: i64, total: i64):`) are already SSA: each is a `phi` at the head of the label's block, with one entry per jump (`goto`, `branch`, `switch`, or running on into the label) naming the value and the block it jumps from, so `%total = phi i64 [ 0, %entry ], [ %add, %step ]`. A `branch` or `switch` names the label's block directly when its arguments are cheap and cannot trap (constants, variables, arithmetic other than `/` and `%`) and no two of its edges go to the same label; otherwise each edge goes through a block `to.<label>` that jumps on. A parameter the function assigns to, or a closure captures, keeps a stack slot (`%loop.i`) that every jump stores into. The control-flow graph is kept small before it is written:

* a block nothing jumps to is dropped;
* a block that only jumps on is skipped over (when its target starts with no `phi`);
* a block that is the only way into the next one, joined by an unconditional jump, is merged into it;
* a `phi` with a single entry is replaced by that value, and a dropped block's entries leave the phis it jumped to.

The Nina implementation's pass (`src/Blocks.nina`, applied to `--emit-llvm`, `--out`, `--jit`) does the same steps, so both produce the same shape of code.

## Debug info

With `--debug` (`CompileOptions.debugInfo`) the module carries DWARF 5 metadata: a compile unit for the `.poly` file, a subprogram per function, a `!dbg` location (line and column) on every instruction, and `llvm.dbg.declare` for every parameter and variable with its type. `clang -g` turns it into a debug build (llvm-check.sh assembles every example that way); a debugger reads the lines, columns and variable names from it. A variable a closure captures lives in a heap cell and is not described.
