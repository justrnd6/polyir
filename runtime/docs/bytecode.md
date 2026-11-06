# Bytecode: the `.pbc` file and the VM's instructions

"Bytecode" means two different things in polyir, and keeping them apart explains most of the design:

| | **`.pbc`** — the portable file | **the VM's instructions** — what actually runs |
|---|---|---|
| what it is | the checked HIR `Module`, serialized | a flat stack machine per function, compiled from that `Module` |
| where it lives | on disk, in a pipe, in memory as bytes | only in memory, inside one VM run |
| written by | `ninac bytecode F.nina`, `polyir F.poly --emit-pbc`, `WritePbc(module)` | `CompileProgram(module)` (`src/vm/Compile.cpp`), at load |
| read by | `ReadPbc(bytes)` (`../core/src/pbc/Pbc.cpp`), the Nina `polyir`, `ninai F.pbc` | the interpreter loop (`src/vm/Run.cpp`) |
| stable? | yes — format v1 is frozen, byte-compatible with the Nina implementation | no — an implementation detail, free to change between builds |
| see it as text | `polyir-check F.poly --hir` (the HIR it carries) | `polyir-check F.poly\|F.pbc --vm-listing` |

So a `.pbc` is never "executed" directly: `ninai F.pbc` reads it back into a `Module`, the VM compiles that module to
its instructions in a few milliseconds, checks it can bind everything (the preflight), and runs.

```
 F.nina ──ninac──►  Module  ──WritePbc──►  F.pbc  ──ReadPbc──►  Module  ──CompileProgram──►  Program  ──run──►  output
 F.poly ──check──►  (HIR)                  (file)               (HIR)                        (instructions)
```

One program in all three forms — `Add.nina`:

```nina
action add(a: Int, b: Int) -> Int = a + b

@Entry
action main() {
    println(add(2, 3))
}
```

## 1. The `.pbc` file

### Container

```
offset  bytes            meaning
0       50 42 43 01      magic "PBC\1"
4       01 00 00 00      format version, u32 little-endian: 1
8       01               section id 1: the declarations (v1 has exactly one section)
9       96 13            the section's byte length, varint: 1227
11      …                the section's payload (1227 bytes)
```

`ninac bytecode Add.nina > add.pbc` writes 1238 bytes; the first ones are

```
00000000: 5042 4301 0100 0000 0196 132a 011c a601  PBC........*....
00000010: e801 c801 be01 be01 aa01 dc01 e601 ca01  ................
```

### Encoding

- **Every integer** is a zigzag-encoded LEB128 varint: `n ≥ 0` is written as `2n`, `n < 0` as `-2n - 1`, then 7 bits per
  byte, low bits first, the top bit set on every byte but the last. `96 13` is `0x16 + 0x13·128 = 2454`, zigzag → `1227`.
- **A string** is its count of code points (a varint), then one varint per code point — not UTF-8 bytes. In the dump,
  `1c` = 28 → 14 code points, then `a6 01` = 166 → 83 = `S`, `e8 01` → 116 = `t`, `c8 01` → 100 = `d`: the first
  declaration is a std helper whose name starts `Std…`.
- **A float** is the exact C99 hexadecimal text (`0x1.8p+1` for 3.0), the same spelling HIR JSON uses, so it round-trips
  bit for bit.
- **A record** is a tag (a varint) and its fields in a fixed order. The declarations section is a count (`2a` = 42 → 21
  declarations here: `main`, `add` and the std pieces they reach), then one tagged declaration record each.

### What the records carry

The payload is the HIR, record for record: types (`i64`, `struct Pt`, `array<i64>`, `closure(i64)->i64`, …),
expressions (constants, locals, `Binary`, `Call`, `FieldGet`, `ClosureLit`, …), statements (`Let`, `If`, `Label`,
`Goto`, `Branch`, `Try`, `Yield`, …) and declarations (functions, structs, interfaces, externs, globals, host types,
kernels, origins). The tag numbers are the order of the HIR cases in the Nina implementation
(`lang/src/pbc/PbcWrite.nina`: `ty`, `expr`, `stmt`); `src/pbc/Pbc.cpp` keeps them in three tag-ordered lists
(`kTypeKinds`, `kExprKinds`, `kStmtKinds`) that both its writer and reader use. Kernel and origin records attach to the
function they name rather than being declarations of their own.

Because it is the HIR, the readable form of a `.pbc` is the HIR text. For `core/examples/labels.poly`:

```
$ polyir-check labels.poly --hir
func gcd(a0:i64,b0:i64)->i64 {goto loop(a0,b0); label loop(a:i64,b:i64):; branch (Eq:i1 b 0) ? done(a) : body(a,b); label body(a:i64,b:i64):; let r:i64 = (Rem:i64 a b); goto loop(b,r); label done(result:i64):; return result}
```

`tests/PbcTest.cpp` checks that `WritePbc` and the Nina writer produce the same bytes for every example, and that
`ReadPbc(WritePbc(m))` gives `m` back.

## 2. The VM's instructions

### The machine

- **One `Code` per function** (closure bodies are functions too, listed after the declared ones): its instructions, the
  number of parameters, captures and slots, its `try` handlers and its `switch` tables.
- **Slots** are the function's numbered locals. Parameters are slots `0..p-1`; a closure body's captures come first.
- **Boxes**: a parameter, a `var` and a loop counter live in a heap box (the slot holds the box), so a closure that
  captures the name shares it with the code around it. A `let`, a label parameter and a caught code are plain values.
- **An operand stack** per frame. Every statement leaves it empty; an expression leaves exactly its value.
- **A program** also has a constant pool, a table of *shapes* (one per distinct type: kind, width, signedness — what
  `fit`, `convert` and the operators need), struct layouts with their default fields, externs, interfaces with their
  method order, and globals with one small initializer function each.
- **Values** are scalars carried inline — integers in 128 bits, always in canonical form for their declared width
  (signed sign-extended, unsigned zero-extended), so `I128`/`U128` are exact — or references into the heap (structs,
  containers, closures, boxes, coroutines). A string carries an identity stamp (a counter, not a heap cell), because
  `===` compares identities. A mark-and-sweep collector reclaims what the program can no longer reach, between two
  instructions ([memory.md](memory.md), `src/vm/Collector.cpp`).

### Instruction format

An instruction is an opcode and up to three 32-bit immediates `a`, `b`, `c`. What they mean depends on the opcode; the
listing spells them out (`call add/2`, not `call 1 2`).

### The instruction set

Stack effects read *before → after*; the top of the stack is on the right.

| mnemonic | opcode | immediates | stack effect / meaning |
|---|---|---|---|
| **constants and slots** | | | |
| `pushConst` | `PushConst` | a: constant | → value |
| `pushString` | `PushString` | a: constant (a string) | → string with a fresh identity |
| `pushVoid` | `PushVoid` | | → void |
| `pushNone` | `PushNone` | | → none |
| `load` | `Load` | a: slot | → the slot's value |
| `loadBoxed` | `LoadBoxed` | a: slot holding a box | → the box's value |
| `store` | `Store` | a: slot | value → (into the slot) |
| `storeBoxed` | `StoreBoxed` | a: slot holding a box | value → (into the box) |
| `boxSlot` | `BoxSlot` | a: slot | the slot's value moves into a new box; the slot now holds the box |
| `loadBound` | `LoadBound` | a: slot, b: name, c: 1 if boxed | as `load`/`loadBoxed`, but traps `'NAME' is not bound` on the unbound marker |
| `storeBound` | `StoreBound` | a: slot, b: name, c: 1 if boxed | as `store`/`storeBoxed`, with the same check |
| `loadGlobal` | `LoadGlobal` | a: global | → the global's value |
| `storeGlobal` | `StoreGlobal` | a: global | value → |
| `pop` | `Pop` | | value → |
| `dup` | `Dup` | | v → v v |
| `fit` | `Fit` | a: shape | v → v fitted to the slot type it is stored in (width, signedness, kind) |
| `convert` | `Convert` | a: shape | v → v cast or coerced |
| **operators** | | | |
| `binary` | `Binary` | a: operator, b: operand shape, c: result shape | left right → result |
| `unary` | `Unary` | a: operator, b: operand shape, c: result shape | v → result |
| **jumps** | | | |
| `jump` | `Jump` | a: pc | goes to pc |
| `jumpIfFalse` | `JumpIfFalse` | a: pc | condition → (goes to pc when false) |
| `jumpIfTrue` | `JumpIfTrue` | a: pc | condition → (goes to pc when true) |
| `switch` | `Switch` | a: table | subject → (goes to the table's pc for it, or its default) |
| **calls** | | | |
| `call` | `Call` | a: function, b: argument count | args → result (void for a void function) |
| `callExtern` | `CallExtern` | a: extern, b: argument count | args → result (a `nina_*` runtime piece, a host, or a native library) |
| `callIndirect` | `CallIndirect` | b: argument count | callee args → result (a function or closure value) |
| `callInterface` | `CallInterface` | a: interface, b: argument count | receiver slot args → result (the receiver's struct picks the method) |
| `makeClosure` | `MakeClosure` | a: closure body, b: capture count | captures → closure |
| `funcRef` | `FuncRef` | a: function | → function value |
| `externRef` | `ExternRef` | a: extern | → function value that calls the extern |
| `return` | `Return` | | result → (returns it) |
| `returnVoid` | `ReturnVoid` | | returns |
| **objects** | | | |
| `newStruct` | `NewStruct` | a: struct | → struct of its defaults |
| `setField` | `SetField` | a: field | struct value → struct |
| `getField` | `GetField` | a: field | struct → field |
| `setIndex` | `SetIndex` | a: index count, b: base shape | container indices… value → |
| `getIndex` | `GetIndex` | a: index count, b: base shape, c: element shape | container indices… → element |
| `newArray` | `NewArray` | a: element count | elements… → array |
| `newContainer` | `NewContainer` | a: shape, b: size count | sizes… → container (map, set, grid, buffer, …) |
| `variant` | `Variant` | a: case | v → variant of that case |
| `either` | `Either` | a: 1 ok, 0 err | v → either |
| `builtin` | `Builtin` | a: builtin, b: argument count, c: shape | receiver args… → result (`len`, `push`, `isSome`, …: the 42 HIR builtins) |
| **exceptions and coroutines** | | | |
| `throw` | `Throw` | | code → (unwinds to the nearest handler) |
| `yield` | `Yield` | | value → (suspends; `resume` gets case 0 of it) |
| `yielded` | `Yielded` | | value → answer (suspends; on resume, whether the coroutine was closed) |
| `trap` | `Trap` | a: constant (the message) | stops the program with the message |
| `halt` | `Halt` | | stops |

### How HIR becomes instructions

- **`let`/`var`/assignment**: the value, `fit` to the slot type, `store` (or `storeBoxed`). An assignment evaluates its
  value first.
- **Labels and `goto`**: labels are function-wide; each label parameter has its own slot. `goto L(x, y)` evaluates the
  arguments, stores them into L's parameter slots and `jump`s. Code that *falls through* into a label stores the
  unbound marker instead (`pushConst unbound`), so reading such a parameter traps as an unbound name would — that is
  why label parameters are read with `loadBound`.
- **`branch c ? A(..) : B(..)`**: the condition, `jumpIfFalse` to B's argument stores, then the A side.
- **`try`**: no instructions of its own — a handler entry (`try @from..@to -> @handler into sN`) says which pcs it
  covers, where the catch starts and which slot receives the thrown code. A `throw` looks the current pc up there
  (and up the calling frames); a trap is not catchable and stops the program.
- **A struct literal** evaluates its fields into hidden slots first, then `newStruct` and one `setField` each.
- **A coroutine** is a frame kept alive between resumes: `yield` suspends it with case 0 of the value; reaching the end
  gives case 1 (done).
- **Every function** ends with `returnVoid`, even one that always returns: it is what a fall-off-the-end reaches.

### Before anything runs: the preflight

`src/vm/Preflight.cpp` checks every extern the program declares has a binding: a `nina_*` runtime piece
(`NinaRuntime.cpp`), a Process host function (`ProcessHost.cpp`), the lifecycle step (`LifecycleHost.cpp`, given by the
embedding host), or a symbol of an `import native` library loaded with `dlopen` (`Host.cpp`). If one is missing the run
is **declined** (status 125, `'NAME' has no binding on the VM`) before a single instruction executes — so the caller
(ninai, the nina front door) can safely run the program another way.

### Reading a listing

`polyir-check F.poly --vm-listing` and `polyir-check F.pbc --vm-listing` print the instructions; the library function
is `BytecodeListing(module)` (`include/polyir/Vm.hpp`, `src/vm/Listing.cpp`). Each function is a header —
`(params P, captures C, slots S, coroutine)` — then one line per instruction: the pc, the mnemonic (its opcode's name in camelCase: `PushConst` is `pushConst`), the operands by name
(functions, externs, structs, types, constants), then its handlers and tables.

`Add.nina` (from its `.pbc`):

```
$ ninac bytecode Add.nina > add.pbc && polyir-check add.pbc --vm-listing
function main  (params 0, slots 0)
  0000  pushConst     2
  0001  pushConst     3
  0002  call          add/2
  0003  callExtern    nina_text_from_i64/1
  0004  callExtern    nina_println_text/1
  0005  pop
  0006  returnVoid

function add  (params 2, slots 2)
  0000  boxSlot       s0
  0001  boxSlot       s1
  0002  loadBoxed     s0
  0003  loadBoxed     s1
  0004  binary        Add i64 -> i64
  0005  return
  0006  returnVoid
```

`add`'s parameters are boxed on entry (the convention for parameters); `println(add(2, 3))` is the call, the runtime's
integer-to-text piece and its print piece.

`gcd` from `core/examples/labels.poly` — labels with parameters:

```
func gcd(a0: i64, b0: i64) -> i64 {
  goto loop(a0, b0)
label loop(a: i64, b: i64):
  branch b == 0 ? done(a) : body(a, b)
label body(a: i64, b: i64):
  let r: i64 = a % b
  goto loop(b, r)
label done(result: i64):
  return result
}
```

```
function gcd  (params 2, slots 8)
  0000  boxSlot       s0                ; a0, b0 are parameters: boxed
  0001  boxSlot       s1
  0002  loadBoxed     s0                ; goto loop(a0, b0): the arguments ...
  0003  loadBoxed     s1
  0004  store         s3                ; ... into loop's parameter slots (b = s3, a = s2)
  0005  store         s2
  0006  jump          @11
  0007  pushConst     unbound           ; falling into `loop` instead binds nothing
  0008  store         s2
  0009  pushConst     unbound
  0010  store         s3
  0011  loadBound     s3  ; b           ; loop: branch b == 0 ? done(a) : body(a, b)
  0012  pushConst     0
  0013  binary        Eq i64 -> i1
  0014  jumpIfFalse   @18
  0015  loadBound     s2  ; a           ; done(a)
  0016  store         s6
  0017  jump          @39
  0018  loadBound     s2  ; a           ; body(a, b)
  0019  loadBound     s3  ; b
  0020  store         s5
  0021  store         s4
  0022  jump          @27
  ...
  0027  loadBound     s4  ; a           ; body: let r = a % b
  0028  loadBound     s5  ; b
  0029  binary        Rem i64 -> i64
  0030  fit           i64
  0031  store         s7
  0032  loadBound     s5  ; b           ; goto loop(b, r)
  0033  load          s7
  0034  store         s3
  0035  store         s2
  0036  jump          @11
  ...
  0039  loadBound     s6  ; result      ; done: return result
  0040  return
  0041  returnVoid
```

(The comments after the operands are added here for the reader; the tool prints the `; name` of a bound slot only.)

A closure, from `closures.poly` (`count` is a `var` the closure captures, so it lives in a box both share):

```
function counterTwice  (params 0, slots 4)
  0000  pushConst     0
  0001  fit           i64
  0002  store         s0
  0003  boxSlot       s0
  0004  load          s0
  0005  load          s0
  0006  load          s0
  0007  makeClosure   counterTwice$closure4 captures 3
  0008  fit           closure()->i64
  0009  store         s1
  0010  load          s1
  0011  callIndirect  callee/0
  ...
```

A `try`, from `exceptions.poly`:

```
function safeDouble  (params 1, slots 3)
  ...
  try @5..@9 -> @10 into s2
```

### Speed and checks

A loop iteration costs about 115 ns (a 3-million-iteration label loop runs in 0.35 s). The VM is checked by
`tests/VmTest.cpp` and `tests/NinaRuntimeTest.cpp`, by `tests/vm_vs_native.sh` (every polyir example run on the VM and as
a native LLVM build, stdout and exit status compared), and by the `bytecode` column of
`ninac/codegen/llvm-polyir/tests/targets_via_polyir.sh` (every Nina corpus program as `.pbc` on the VM against the llvm
route).
