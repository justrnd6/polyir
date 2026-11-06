# PolyIRBuilder

`PolyIRBuilder` builds a program in memory and `CompileToLlvm` turns it into LLVM IR text. You write the program the way you would write it in source: declare, call, loop, return. Every expression knows its type, and a mistake is reported on the line that makes it, so there is no separate "check" step.

```cpp
#include <polyir/Builder.hpp>
#include <polyir/LlvmCompiler.hpp>

using namespace polyir;

PolyIRBuilder b;
Return(DefineFunction(b, "main", {}, MakeInt(64)).body, Mul(IntConst(6), IntConst(7)));

CompileResult result = CompileToLlvm(Build(b));   // result.ir is LLVM IR; result.errors says why not
```

Pipe `result.ir` to `clang -x ir -` (or `llc`, `lli`, `opt`) and it runs; a function `main` returns the exit status. Link with `find_package(polyir REQUIRED)` and `target_link_libraries(app PRIVATE polyir::polyir)` after `cmake --install`.

Every program below is in `tests/BuilderTourTest.cpp`, built as shown and run in the test suite.

## The shape of it

* Types are made directly (`MakeInt(64)`, `MakeDynamicArray(t, true)`); so are expressions that need no context (`Add(x, y)`, `IntConst(1)`).
* The **builder** `b` is a plain struct that holds the module so far; the functions that read or add declarations take it first: `DefineFunction(b, …)`, `DefineStruct(b, …)`, `Call(b, "f", …)`, `FieldAccess(b, obj, "x")`, `Build(b)`.
* A **block** (a function's `body`, or the `Block&` a loop or `if` hands you) is the first argument of every statement: `Let(body, …)`, `var`, `set`, `ret`, `IfThen`, `WhileLoop`, and so on. `Local(body, "x")` names a variable that is in scope.
* Structs hold fields only; everything that does something is a free function in `namespace polyir`.
* Names are checked when you use them, types when you combine them.

## Loops and variables

```cpp
Var(body, "result", MakeInt(64), IntConst(1));
Var(body, "n", MakeInt(64), IntConst(5));
WhileLoop(body, Gt(Local(body, "n"), IntConst(1)), [&](Block& loop) {
  SetLocal(loop, "result", Mul(Local(loop, "result"), Local(loop, "n")));
  SetLocal(loop, "n", Sub(Local(loop, "n"), IntConst(1)));
});
Return(body, Local(body, "result"));       // 120
```

`let` is for a value that does not change, `var` for one that does. `ForRange(body, "i", from, to, ...)` counts, `IfThen` / `IfElse` branch.

## Functions

```cpp
auto& gcd = DefineFunction(b, "gcd", {{"a", MakeInt(64)}, {"b", MakeInt(64)}}, MakeInt(64));
IfThen(gcd.body, Eq(Local(gcd.body, "b"), IntConst(0)), [&](Block& then) { Return(then, Local(then, "a")); });
Return(gcd.body, Call(b, "gcd", {Local(gcd.body, "b"), Rem(Local(gcd.body, "a"), Local(gcd.body, "b"))}));
// ... elsewhere: Call(b, "gcd", {IntConst(48), IntConst(18)})   -> 6
```

A function can call itself, and `DeclareFunction(b, ...)` lets a call come before the definition. `DeclareExtern(b, "puts", {{"line", MakeString()}}, MakeInt(32))` names a C function to call.

## Structs

```cpp
DefineStruct(b, "Point", {{"x", MakeInt(64), false}, {"y", MakeInt(64), false}});
Let(body, "p", StructLit(b, "Point", {{"x", IntConst(3)}, {"y", IntConst(4)}}));
Return(body, Add(FieldAccess(b, Local(body, "p"), "x"), FieldAccess(b, Local(body, "p"), "y")));   // 7
```

A field marked `true` can be assigned with `SetField(body, object, "x", value)`.

## Lists, maps, sets

```cpp
Let(body, "xs", NewContainer(MakeDynamicArray(MakeInt(64), true)));
ForRange(body, "i", IntConst(0), IntConst(5), [&](Block& loop) { Eval(loop, Push(Local(loop, "xs"), Mul(Local(loop, "i"), Local(loop, "i")))); });
Return(body, Add(Len(Local(body, "xs")), Index(Local(body, "xs"), {IntConst(4)})));   // 5 + 16

Let(body, "ages", NewContainer(MakeHashmap(MakeString(), MakeInt(64), true), {IntConst(8)}));
StoreIndex(body, Local(body, "ages"), {StringConst("ann")}, IntConst(31));
Index(Local(body, "ages"), {StringConst("ann")})                                    // 31; a missing key reads as 0
```

Also `MakeSet(..., false)`, `b.setAdd`, `b.has`, `Truncate(xs, n)` (a mutarray keeps its first `n` elements; the result is the list), `Remove(m, k)` (takes a key out of a mutmap or mutset in place; an `i1`: it was there), grids, tensors, vectors (`MakeVec(f32, 4)`), and immutable `b.array` / `b.map` with `With(...)`. An index out of range stops the program (the pending output is flushed and a line on standard error says why).

## Optionals, eithers, variants

```cpp
Let(body, "found", Some(IntConst(40)));
Let(body, "missing", None(MakeInt(64)));
IfThen(body, IsNone(Local(body, "missing")), [&](Block& then) { Return(then, Add(Unwrap(Local(body, "found")), IntConst(2))); });   // 42
```

`Ok(type, v)` / `Err(type, v)` with `b.isOk`, `b.unwrapOk`, and `VariantLit(type, case, v)` with `b.variantTag` / `b.variantUnwrap` work the same way.

## Closures

```cpp
Var(body, "count", MakeInt(64), IntConst(0));
Let(body, "tick", ClosureLiteral(body, {}, MakeInt(64), [&](Block& inner) {
  SetLocal(inner, "count", Add(Local(inner, "count"), IntConst(1)));      // the closure and its creator share `count`
  Return(inner, Local(inner, "count"));
}));
Eval(body, CallIndirect(Local(body, "tick"), {}));
Return(body, Add(CallIndirect(Local(body, "tick"), {}), Local(body, "count")));   // 2 + 2
```

The builder works out what a closure captures. A function can also be a value: `FuncRef(b, "name")`.

## Exceptions

```cpp
TryCatch(body, [&](Block& attempt) { ThrowValue(attempt, IntConst(7)); },
              "code", [&](Block& handler) { SetLocal(handler, "result", Mul(Local(handler, "code"), IntConst(6))); });   // 42
```

`TryCleanup(body, cleanup)` runs `cleanup` when anything is thrown out of `body` and then lets the throw go on (a normal exit does not run it): a `try` with an empty caught name in the HIR. A source-language back end writes it as `catch (...) { cleanup; throw; }` and so also runs it for an exception the host raised (a library throwing). `ninac` uses it to close a `resource { }` handle.

## Globals

`DefineGlobal(b, name, type, init)` takes any initialiser. A constant starts the global as it is; anything else (a container, a call, a record) is stored as zero and a function the compiler writes (`polyir_init_globals`) gives it its value before `main` runs. The source back ends print the initialiser as written.

## WebAssembly without a toolchain

`polyir::WasmModule(module, {{"function", "exportName"}, ...})` (`polyir/Wasm.hpp`) writes a `.wasm` binary straight from the HIR for functions of `i64`, `f64` and `i1` values (locals, `if`, loops, calls between the module's functions). Anything else (text, records, lists, a call of a function the module does not define) is an `errors` entry, and `polyir --emit-wasm`'s route through LLVM, clang and `wasm-ld` is the way for those.

## Interfaces

```cpp
DefineStruct(b, "Square", {{"side", MakeInt(64), false}});
DefineInterface(b, "Shape", {{"area", {}, MakeInt(64)}});
Implement(b, "Shape", "Square");          // Square_area(self) is the method
// ...
Let(body, "shape", InterfaceBox(b, "Shape", StructLit(b, "Square", {{"side", IntConst(7)}})));
Return(body, InterfaceCall(b, Local(body, "shape"), "area", {}));  // 49
```

## Mistakes

```cpp
Add(IntConst(1), IntConst(1, MakeInt(32)));   // throws BuildError: "operator Add: i64 and i32 are different types"
Call(b, "nothing", {});                           // throws BuildError: "no function called 'nothing' is declared"
```

`CompileToLlvm` itself never throws; anything it cannot do ends up in `result.errors`.

## Building part of a program on its own

`SubBuilder(b)` gives a builder that starts with everything `b` has declared (structs, globals, externs, interfaces and the signatures of its functions, so it can call them) and collects functions of its own. `Adopt(b, sub)` brings them back. Make as many as you like, in any order: a library of helpers, one builder per source file, one per thread of work.

```cpp
PolyIRBuilder b;
DefineStruct(b, "Point", {{"x", MakeInt(64), false}, {"y", MakeInt(64), false}});

PolyIRBuilder helpers = SubBuilder(b);                      // knows Point
auto& norm = DefineFunction(helpers, "norm1", {{"p", MakeStruct("Point")}}, MakeInt(64));
Return(norm.body, Add(FieldAccess(helpers, Local(norm.body, "p"), "x"), FieldAccess(helpers, Local(norm.body, "p"), "y")));
Adopt(b, helpers);                                            // norm1 is now part of b

Return(DefineFunction(b, "main", {}, MakeInt(64)).body, Call(b, "norm1", {StructLit(b, "Point", {{"x", IntConst(30)}, {"y", IntConst(12)}})}));   // 42
```

A name defined in two sub-builders is reported when the second is adopted (`function same is already defined`).

## One function as LLVM text, added to another module

`CompileFunctionToLlvm(module, "name")` compiles a single function (and any closure bodies it needs) to text, and returns the module-level lines it relies on (type definitions, string constants, library declarations). `AddLlvm` puts that text into another builder's module as it is. Nothing is shared between the two builders, so this works across threads, processes or a cache on disk.

```cpp
PolyIRBuilder helper;
// ... define "safe_div" in helper ...
FunctionIr alone = CompileFunctionToLlvm(Build(helper), "safe_div");   // alone.text, alone.declarations

PolyIRBuilder b;
DeclareFunction(b, "safe_div", {{"a", MakeInt(64)}, {"b", MakeInt(64)}}, MakeInt(64));   // so b can call it
AddLlvm(b, alone.text, alone.declarations);   // with debug info: AddLlvm(b, alone.text, alone.declarations, alone.metadata, alone.namedMetadata)
Return(DefineFunction(b, "main", {}, MakeInt(64)).body, Call(b, "safe_div", {IntConst(84), IntConst(2)}));   // 42
```

Identical declarations from several fragments are written once, and a string constant is named for its text, so two fragments that use the same string share it. Functions the fragment calls must be defined in the final module; compiled with `options.debugInfo` the fragment carries its debug metadata (`metadata`, `namedMetadata`; pass them to `AddLlvm` too) and the module renumbers the `!N` references after its own, listing every compile unit in `!llvm.dbg.cu` and writing the module flags once.

## Where things came from

Every node the builder makes has a `Span at` (all zero until set) and every function an `origin`. A front end sets them so
that errors, DWARF and the generated sources speak in its own files and lines:

```cpp
FunctionBuilder& fn = DefineFunction(b, "area", {{"s", sq}}, MakeInt(64));
fn.definition.origin = "shapes.mylang";            // the file; the C++ back end writes one unit per origin
Block& body = fn.body;
Return(body, Mul(Local(body, "s"), Local(body, "s")));
// positions: the HIR JSON reader fills `at` from `"at": [line, column]`; a builder-based front end sets
// `at` on the nodes it keeps (the `Stmt`/`Expr` it holds pointers to) — a builder-side `at(span)` convenience is planned.
CompileOptions options;
options.debugInfo = true;                 // DWARF from the spans: break on shapes.mylang:3, backtrace in your lines
options.sourceFile = "shapes.mylang";
options.sourceDirectory = "/work/app";
```

See [language/debug-and-meta.md](language/debug-and-meta.md) for the routes, the JSON description and the
checklist of what a front end should hand over.

## Looking at the result

```cpp
CompileOptions options;
options.debugInfo = true;                  // lines, columns and variables, for a debugger
options.sourceFile = "program.src";
CompileResult result = CompileToLlvm(Build(b), options);
```

The IR is written to be read: named values (`%add`, `%total.val`), blocks named for their role, no runtime scaffolding unless the program needs it. `ModuleText(Build(b))` (`polyir/HirPrint.hpp`) prints the program itself in the HIR notation, which is handy when a test fails.

## From C, and from Nina

The same builder is available through the C ABI in `polyir/polyir.h` (`polyir_function_begin`, `polyir_binary`, `polyir_compile`, ...), which is what `import cpp "polyir/polyir.h"` in Nina uses; see `examples/polyir-cpp-dylib`.
