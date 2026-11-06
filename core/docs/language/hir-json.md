# HIR JSON: the back ends on HIR made elsewhere

`polyir f.poly --emit-hir-json` writes the checked program as JSON (`src/hir/HirJson.nina`). `polyir --from-hir-json` reads it back (`src/hir/HirJsonRead.nina`, the exact inverse) and hands the declarations to the source back ends, so that a program that makes HIR (ninac, libpolyir) gets its Java, Kotlin, JavaScript, Swift, C++, Python, Rust, Objective-C or C from the same code that writes them for a `.poly` file.

```
polyir f.poly --emit-hir-json | polyir --from-hir-json - --name F --emit-java      # standard input
polyir --from-hir-json f.json --emit-kotlin --package app.demo                      # a file
polyir --from-hir-json f.json --emit-hir                                            # the checked-HIR text
polyir --from-hir-json f.json --emit-hir-json                                       # the same JSON again
```

Every `--emit-java/kotlin/js/swift/cpp/python/rust/objc/c`, `--emit-hir` and `--emit-hir-json` works on it, with the same text as for the `.poly` file (`examples/java-mapping/json-check.sh` checks that, byte for byte, for every example and every output). No source file is given.

## The input

Either the array of declarations, or an object that also names the program:

```json
{"name": "Tour", "decls": [ {"k": "HStructDef", ...}, {"k": "HFuncDef", ...} ]}
```

What the output is called (the Java class, the Kotlin object, the C++ namespace...) is the first of: `--name X`, the object's `"name"`, the JSON file's name (`tour.json` gives `Tour`), `Main` (standard input). A name that is not an identifier gives `Program`, as for a `.poly` file.

The nodes are those of `HirJson.nina`: every node is an object whose `k` names its case and whose other keys are the case's fields; an expression ends with `ty` and `at`, a statement with `at`; `at` is `[line, column]` and may be left out (0:0); a floating point value is a string in C99 hexadecimal (`"0x1.8p3"`, `"-0x1p-1074"`, `"nan"`, `"inf"`, `"-inf"`; read exactly, with powers of two only); whole numbers are JSON numbers up to 64 bits (`-9223372036854775808` to `9223372036854775807`); `null` is the missing `init` of a `HVarDecl`. `null` as the `value` of an `HReturn` is a return that gives nothing back (read as a value of type `void`, written back as such); the back ends write it as a bare `return`. In a coroutine function an `HReturn` ends the coroutine (as falling off its end does); its value, if any, is only evaluated: the LLVM compiler takes the final suspend and the source back ends write the bare `return` of their generator bodies (checked on Java, JavaScript, Swift, C++, Python, Objective-C and C). Keys may come in any order and unknown keys are ignored. A problem is reported once, as `HIR JSON line L, column C: <node kind>: <what is wrong>`.

## What a producer has to supply, and what is not in the JSON

The back ends need only the declarations (`HStructDef`, `HFuncDef`, `HExternDef`, `HGlobalDef`, `HIfaceDef`, and for host languages `HHostType`, `HBridge`, `HHostMember` with their `bindings`). Everything else the source back ends used from the checker is either in those or not needed:

| used by | where it comes from with `--from-hir-json` |
|---|---|
| the class / object / module name | `--name`, the JSON `"name"`, the file name (above) |
| host types, bridges, `import java/kotlin/...` bindings | `HHostType`, `HBridge`, `HHostMember` declarations in the JSON; verified for the host-type, bridge and import examples |
| positions in the output (closure names such as `closure_12_5`, `--debug`-free line comments) | `at` = `[line, column]`; the end position and offsets of a source span are not written (they are read as the start; only the LLVM temporaries use them) |
| interface vtable layout | `HIfaceDef` (method order is the slot order) |
| the Kotlin package | `--package` |
| which functions are GPU kernels, and how each parameter reaches the GPU (`--emit-msl`, `--emit-glsl`) | the `stage` and `roles` of the function's object (below) |

Not available with `--from-hir-json` (refused with the reason), because they need the polyir parse tree and not just the HIR:

* `--smap` and `--debug-json` (they map the output back to the `.poly` source: functions, files and imports of the tree);
* `--emit-llvm`, `--emit-wasm`, `--jit`, `--out`, `--shared`, `--static-lib`, `--check`: the Nina LLVM lowering reads the declarations of the tree. LLVM from HIR is `polyir-hir f.json --llvm` (`runtime`, see llvm-mapping.md).

## The source file of a function

A function written in a known source file carries one more key on its `HFuncDef` object, written after `isCoroutine` and only when it is set
(libpolyir's `FuncDef::origin`; a program without it reads and writes as it always did):

```json
{"k": "HFuncDef", "name": "twice", "paramNames": ["n"], ..., "isCoroutine": false, "origin": "Maths", "at": [1, 1]}
```

The Nina implementation reads it as the mark `HDecl.HOrigin(name, file)` right after the function (as a kernel's `stage` and `roles` become
`HKernel`), and writes the mark back as the key. `PolyirCpp.emitUnits` (`--emit-cpp --units`) groups the C++ output by it: one `<file>.cpp`
per origin, `project.hpp` for what every unit shares, `project_std.cpp` for the functions without one.

## Generics

A program with generics carries them twice. The code is still the copies made for each set of types (`Box_i64`, `pick_i64`): LLVM, the VM
and the HIR text follow those. Each copy also says which generic it was made from, and the generics as declared follow the code. Both are
libpolyir's own (`Module::genericStructs`, `genericFunctions` and `Generic` in Hir.hpp); a program without generics reads and writes as it
always did.

* A copy, and each of its methods, has the optional key `"generic"`. It holds the generic's name, what each type parameter stands for,
  no parameters and no reason:

  ```json
  {"k": "HStructDef", "name": "Box_i64", ..., "generic": {"of": "Box", "arguments": [{"k": "HInt", "bits": 64}], "params": [], "notParametric": ""}}
  ```

* A generic as declared is an `HStructDef` or `HFuncDef` marked `"isGeneric": true`, after all the code. Its types are `{"k": "HParam",
  "name": "T"}`, and its `"generic"` lists the parameters. `notParametric` says why its body does not work for every type
  (`"57:23: T can be any type, and `+` does not work on every type"`); it is `""` when it does:

  ```json
  {"k": "HStructDef", "name": "Box_T", "fieldNames": ["value"], "fieldTypes": [{"k": "HParam", "name": "T"}], ...,
   "generic": {"of": "Box", "arguments": [{"k": "HParam", "name": "T"}], "params": ["T"], "notParametric": ""}, "isGeneric": true}
  ```

`.pbc` keeps the same in its section 3, which other readers skip. `--emit-poly` writes a generic that works for every type once
(`struct Box<T>`) and its uses as `Box<i64>`; it writes any other generic as its copies.

## GPU kernels

A function the GPU runs carries two more keys on its `HFuncDef` object, written only for a kernel (a program without kernels reads and writes as it always did):

```json
{"k": "HFuncDef", "name": "scale", "paramNames": ["x", "k"], ..., "isCoroutine": false, "stage": "compute", "roles": [0, 1], "at": [4, 14]}
```

`stage` is `"compute"` (one thread per element: the Metal and Vulkan kernels), `"vertex"` or `"fragment"` (the graphics stages: desktop OpenGL GLSL), or `"reduce"` (the combine of a reduction, `(f32, f32) -> f32`: the GPU folds a whole array into one number in one launch of one workgroup). `roles` has one number per parameter:

| role | | for |
|---|---|---|
| 0 | element | compute: the thread's own element of an input array |
| 1 | uniform | one value every thread sees |
| 2 | buffer | a whole array every thread may read anywhere (`ptr<T>`) |
| 3 | index | the thread's element number (two of them: a grid, column then row) |
| 4 | writable | a whole array every thread may read and write |
| 5 | workgroup | memory the threads of one workgroup share |
| 6 | attribute | vertex: an input per vertex (`in`) |
| 7 | varying | fragment: the value of the vertex stage's output of the same name (`in`) |
| 8 | output | vertex or fragment: a value the stage writes (`out`) |
| 9 | position | vertex: the clip-space position (`gl_Position`; never declared) |

The types a kernel adds are `{"k": "HVec", "elem": ..., "count": 2..4}` (already there) and `{"k": "HMat", "elem": {"k": "HFloat", "bits": 32}, "size": 4}`, a square matrix that a kernel multiplies by a vector or another matrix; only the GPU back ends write it, and the LLVM back ends refuse it. A kernel calls the C library `float` functions (`sqrtf`, `sinf`, `fminf`...) and the vector functions `dot`, `cross`, `normalize` and `length`, declared `extern`; the GPU back ends write them as the shading language's own.

`polyir --from-hir-json f.json --emit-msl` (and `--emit-glsl`, `--emit-spirv`) work on such a JSON: libpolyir's `polyir-hir f.json --msl`, `--glsl` and `--spirv` write the same Metal, GLSL and SPIR-V. ninac makes such a module (and could write this JSON of it) for `@Kernel("gpu")`, `@Vertex` and `@Fragment`; the Metal and GLSL it embeds are libpolyir's.

## Coroutine builtins for producers

Besides `resume` and `done`, a `HCallBuiltin` may name `yielded` (receiver: the value to hand out; an `i1` that says whether the coroutine was abandoned while suspended there, after which the body cleans up and returns without yielding again) and `close` (receiver: the coroutine; void). `close` on a coroutine suspended at a `yielded` resumes it once with that answer; on one suspended at a plain `HYield`, or never resumed, it only drops it. In `.poly` source `co.close()` and `v.yielded()` are available (`yielded` is for producers: ninac writes it for a `resource { }` open across an `emit`; `examples/yielded.poly`). The Nina LLVM lowering supports `yielded` as the condition of an `if` or `while`.

## `HTry` with an empty name

`{"k": "HTry", "try": [...], "name": "", "catch": [...]}` is a *cleanup*: the `catch` statements run when anything is thrown out of `try` (a polyir `throw`, or, in a source language, an exception the host raised), and the throw then goes on to the enclosing handler; a normal exit of `try` does not run them. The C++ and Nina LLVM compilers re-throw the saved number after the catch body; Java, Kotlin, JavaScript, Python and C++ write `catch (Throwable ...) { ...; throw ...; }` / `except BaseException: ...; raise`; Swift, Rust, Objective-C and C (which have no host exceptions) write just the body.
