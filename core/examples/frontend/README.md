# A front end on polyir, debugged all the way down

`tiny` is a small language (functions, `let`, assignment, `while`, `if`, `print`, 64-bit integers) with a complete compiler
in `tinyc.py` (about 200 lines of Python, no dependencies). It does not know LLVM. It writes **polyir text** and a **source
map**; polyir does the rest. `demo.sh` runs the whole chain and checks each step.

```
tiny.tl ──tinyc.py──▶ tiny.poly + tiny.map ──polyir --debug --source-map──▶ native program / wasm module
 (tiny source)         (generated text + where every line came from)         (DWARF line table says tiny.tl:LINE)
                                                                                         │
                                                                    lldb: break at tiny.tl:3, backtrace in tiny.tl
```

Run it: `examples/frontend/demo.sh` (add `--show` to print the generated files and the debugger session).

## What the debugger shows

```
(lldb) breakpoint set -f tiny.tl -l 3
Breakpoint 1: where = tiny`square at tiny.tl:3:15
(lldb) run
* thread #1, stop reason = breakpoint 1.1
    frame #0: tiny`square at tiny.tl:3:15
   2   	fn square(x) {
-> 3   	  return x * x
(lldb) frame variable
(long) x = 1
(lldb) bt
  * frame #0: tiny`square at tiny.tl:3:15
    frame #1: tiny`sumsq at tiny.tl:10:9
    frame #2: tiny`main at tiny.tl:16:3
```

`frame select 1` then `frame variable` prints `n`, `total` and `i` of `sumsq`; `p total` works too.

(`demo.sh` builds the debuggee in a temp directory: on the macOS used here lldb hangs when launching a binary that sits under `~/Documents`, though the same binary debugs fine from `/tmp`.)

The debugger never sees `tiny.poly`. Frame 1 is the `total = total + square(i)` line of the tiny program; frame 2 is `print sumsq(4)`.

## What the front end must give polyir

1. **A polyir program** (`tiny.poly`). The generated text is ordinary polyir: `func`, `let`/`var`, `for`, `if`, calls.
   `tinyc.py` shows the pieces a real front end needs: types for every value (`i64`), `let` for names never reassigned and `var`
   for the rest, loops as `for (w1 = 0; cond; w1 = w1 + 1)`, a runtime written in polyir (`printInt`), and `extern func` for the host (`putchar`).
2. **A source map** (`tiny.map`), one line per generated line:
   ```
   polyir-source-map 1
   source tiny.tl
   source <tiny runtime>
   map 9 0 10 5        # polyir line 9 is tiny.tl line 10, column 5
   ```
   Emit an entry for every generated line (a closing brace maps to the source's closing brace; generated runtime lines map to a
   second "source"). An entry also covers the lines after it until the next one, so no line may be left unmapped by accident.
3. Nothing else. No LLVM, no DWARF, no linker.

## What polyir does with them

| Step | Command | Result |
|---|---|---|
| native, debuggable | `polyir tiny.poly --debug --source-map tiny.map --out tiny` | a program and its `tiny.dSYM`; DWARF files named `tiny.tl` |
| LLVM IR | `... --emit-llvm` | `!DIFile(filename: "tiny.tl")`, a `DISubprogram` per function at its tiny line, a `DILocation` per statement |
| wasm | `... --emit-wasm --out tiny.wasm` | the same line table in the wasm DWARF sections, and the description in a custom section `polyir.meta` |
| any other target | `... --debug-json tiny.json --check` | functions, structs, notes, with tiny positions |

## What is proven and what is not yet

Proven by `demo.sh`: the program runs and prints 30 and 385; the LLVM IR, the JSON, the native DWARF and the wasm DWARF all
carry tiny.tl positions; lldb stops at a tiny line and walks the stack in tiny coordinates; the wasm module runs under
wasmtime with a host stub for `putchar`; `polyir.meta` is present in the wasm file.

Not yet: **variables.** A debugger can stop and show the stack but cannot print `total` or `i`, because polyir does not emit
`DILocalVariable` yet. Doing so needs each source name to survive lowering as a named slot or `dbg.value`, and a type
description per local; the front end would give names through the map or a `meta` note. Also not yet: stepping inside
generated runtime helpers shows `<tiny runtime>` lines, and a Windows/Linux debugger has not been tried (the DWARF is the
same; the dSYM step is macOS-specific).
