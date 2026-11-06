// The HIR model against the Nina implementation: programs built with PolyIRBuilder print as the same text the Nina checker's HIR prints (the expected
// lines are copied from lang/tests/src/Main.nina), plus the shapes only this side builds (closures, interfaces, coroutines) and the
// mistakes the builder refuses.
#include "polyir/Std.hpp"
#include "polyir/Builder.hpp"
#include "polyir/HirPrint.hpp"

#include <algorithm>
#include <iostream>
#include <sstream>

using namespace polyir;

namespace {

  int failures = 0;

  Vector<String> linesOf(const String& text) {
    Vector<String> out;
    IStringStream in(text);

    for (String line; getline(in, line);) {
      out.push_back(line);
    }

    return out;
  }

  // The declarations print in groups here (structs, externs, globals, functions), so the lines are compared without regard to order.
  Vector<String> sortedLines(const String& text) {
    Vector<String> out = linesOf(text);
    sort(out.begin(), out.end());

    return out;
  }

  void showMismatch(const String& name, const Vector<String>& got, const Vector<String>& expected) {
    failures++;
    cout << "FAIL " << name << "\n";

    for (const String& g : got) {
      cout << "  got      " << g << "\n";
    }

    for (const String& e : expected) {
      cout << "  expected " << e << "\n";
    }
  }

  void expectLines(const String& name, const Module& program, Vector<String> expected) {
    const Vector<String> got = sortedLines(ModuleText(program));
    sort(expected.begin(), expected.end());

    if (got == expected) {
      cout << "ok   " << name << "\n";
      return;
    }

    showMismatch(name, got, expected);
  }

  void expectText(const String& name, const String& got, const String& expected) {
    if (got == expected) {
      cout << "ok   " << name << "\n";
      return;
    }

    failures++;
    cout << "FAIL " << name << "\n  got      " << got << "\n  expected " << expected << "\n";
  }

  bool refuses(const Function<void()>& mistake) {
    try {
      mistake();
    } catch (const BuildError&) {
      return true;
    }

    return false;
  }

  void expectRefused(const String& name, const Function<void()>& mistake) {
    if (refuses(mistake)) {
      cout << "ok   " << name << " is refused\n";
      return;
    }

    failures++;
    cout << "FAIL " << name << " was accepted\n";
  }

  const Vector<String> kScalarsExpected = {
    "struct Pair{a:i64,b:i64}",
    "func Pair_sum(this:Pair)->i64 {return (Add:i64 this.#0 this.#1)}",
    "extern take32(v:i32)->i32",
    "global counter:i64",
    "func widen(x:i32,y:i64)->i64 {return (Add:i64 y (cast:i64 x))}",
    "func narrow(v:i64)->i32 {return take32:i32((coerce:i32 v))}",
    "func bump()->i64 {@counter = (Add:i64 @counter 1); return @counter}",
    "func total(n:i64)->i64 {var t:i64 = 0; for (i=0; (Lt:i1 i n); (Add:i64 i 1)) {t@ = (Add:i64 t@ i)}; if (Lt:i1 t@ 0) {return 0} else {return t@}}",
    "func mk()->Pair {return Pair{#1=2,#0=1}}",
    "func useMk()->i64 {let p:Pair = mk:Pair(); return (Add:i64 Pair_sum:i64(p) sq:i64(p.#0))}",
    "func sq(x:i64)->i64 {return (Mul:i64 (Neg:i64 x) x)}",
    "func boxed()->i64 {let b:Box_i64 = Box_i64{#0=42}; return b.#0}",
    "func later()->i64 {return ?unknown function missing?}",
    "struct Box_i64{value:i64}",
  };

  void scalarDeclarations(PolyIRBuilder& b) {
    DefineStruct(b, "Pair", {{"a", MakeInt(64), false}, {"b", MakeInt(64), false}});
    DefineStruct(b, "Box_i64", {{"value", MakeInt(64), false}});
    DeclareExtern(b, "take32", {{"v", MakeInt(32)}}, MakeInt(32));
    DefineGlobal(b, "counter", MakeInt(64), IntConst(0));
  }

  void pairSum(PolyIRBuilder& b) {
    auto& sum = DefineFunction(b, "Pair_sum", {{"this", MakeStruct("Pair")}}, MakeInt(64));
    Return(sum.body, Add(FieldAccess(b, Local(sum.body, "this"), "a"), FieldAccess(b, Local(sum.body, "this"), "b")));
  }

  void widenAndNarrow(PolyIRBuilder& b) {
    auto& widen = DefineFunction(b, "widen", {{"x", MakeInt(32)}, {"y", MakeInt(64)}}, MakeInt(64));
    Return(widen.body, Add(Local(widen.body, "y"), Cast(Local(widen.body, "x"), MakeInt(64))));
    auto& narrow = DefineFunction(b, "narrow", {{"v", MakeInt(64)}}, MakeInt(32));
    Return(narrow.body, Call(b, "take32", {Coerce(Local(narrow.body, "v"), MakeInt(32))}));
  }

  void bumpFunction(PolyIRBuilder& b) {
    auto& bump = DefineFunction(b, "bump", {}, MakeInt(64));
    Assign(bump.body, Global(b, "counter"), Add(Global(b, "counter"), IntConst(1)));
    Return(bump.body, Global(b, "counter"));
  }

  void totalFunction(PolyIRBuilder& b) {
    auto& total = DefineFunction(b, "total", {{"n", MakeInt(64)}}, MakeInt(64));
    Block& body = total.body;
    Var(body, "t", MakeInt(64), IntConst(0));
    ForRange(body, "i", IntConst(0), Local(body, "n"), [&](Block& loop) { SetLocal(loop, "t", Add(Local(loop, "t"), Local(loop, "i"))); });
    IfElse(body, Lt(Local(body, "t"), IntConst(0)), [&](Block& yes) { Return(yes, IntConst(0)); }, [&](Block& no) { Return(no, Local(no, "t")); });
  }

  void makeAndSquare(PolyIRBuilder& b) {
    auto& mk = DefineFunction(b, "mk", {}, MakeStruct("Pair"));
    Return(mk.body, StructLit(b, "Pair", {{"b", IntConst(2)}, {"a", IntConst(1)}}));
    auto& sq = DefineFunction(b, "sq", {{"x", MakeInt(64)}}, MakeInt(64));
    Return(sq.body, Mul(Neg(Local(sq.body, "x")), Local(sq.body, "x")));
  }

  void useMkFunction(PolyIRBuilder& b) {
    auto& useMk = DefineFunction(b, "useMk", {}, MakeInt(64));
    Let(useMk.body, "p", Call(b, "mk", {}));
    Return(useMk.body, Add(Call(b, "Pair_sum", {Local(useMk.body, "p")}), Call(b, "sq", {FieldAccess(b, Local(useMk.body, "p"), "a")})));
  }

  void boxedAndLater(PolyIRBuilder& b) {
    auto& boxed = DefineFunction(b, "boxed", {}, MakeInt(64));
    Let(boxed.body, "b", StructLit(b, "Box_i64", {{"value", IntConst(42)}}));
    Return(boxed.body, FieldAccess(b, Local(boxed.body, "b"), "value"));
    auto& later = DefineFunction(b, "later", {}, MakeInt(64));
    Return(later.body, Unsupported("unknown function missing", MakeInt(64)));
  }

  // The structs, the extern and the global, and the functions that work on plain numbers.
  void scalarFunctions(PolyIRBuilder& b) {
    scalarDeclarations(b);
    widenAndNarrow(b);
    bumpFunction(b);
    totalFunction(b);
  }

  // The functions that make or read structs: Pair's sum, `mk` and `useMk` (with the `sq` it calls), and `boxed`. `boxed` comes
  // with `later`, whose body is a call nothing defines.
  void structFunctions(PolyIRBuilder& b) {
    pairSum(b);
    makeAndSquare(b);
    useMkFunction(b);
    boxedAndLater(b);
  }

  void scalarsAndStructs() {
    PolyIRBuilder b;
    scalarFunctions(b);
    structFunctions(b);
    expectLines("scalars and structs", Build(b), kScalarsExpected);
  }

  const Vector<String> kFlowExpected = {
    "func gcd(a0:i64,b0:i64)->i64 {goto loop(a0,b0); label loop(a:i64,b:i64):; branch (Eq:i1 b 0) ? done(a) : body(a,b); label body(a:i64,b:i64):; let r:i64 = (Rem:i64 a b); goto loop(b,r); label done(result:i64):; return result}",
    "func pick(x:i64)->i64 {switch x {1->one(10), 2->one(20), default->none()}; label one(v:i64):; return v; label none():; return 0}",
    "func safe(x:i64)->i64 {var out:i64 = 0; try {throw x} catch (code) {out@ = (Add:i64 code 1)}; return out@}",
  };

  void gcdEntry(Block& gcdBody) {
    Jump(gcdBody, "loop", {Local(gcdBody, "a0"), Local(gcdBody, "b0")});
    Label(gcdBody, "loop", {{"a", MakeInt(64)}, {"b", MakeInt(64)}});
    Branch(gcdBody, Eq(Local(gcdBody, "b"), IntConst(0)), "done", {Local(gcdBody, "a")}, "body", {Local(gcdBody, "a"), Local(gcdBody, "b")});
  }

  void gcdStep(Block& gcdBody) {
    Label(gcdBody, "body", {{"a", MakeInt(64)}, {"b", MakeInt(64)}});
    Let(gcdBody, "r", Rem(Local(gcdBody, "a"), Local(gcdBody, "b")));
    Jump(gcdBody, "loop", {Local(gcdBody, "b"), Local(gcdBody, "r")});
  }

  void gcdExit(Block& gcdBody) {
    Label(gcdBody, "done", {{"result", MakeInt(64)}});
    Return(gcdBody, Local(gcdBody, "result"));
  }

  void gcdFunction(PolyIRBuilder& b) {
    auto& gcd = DefineFunction(b, "gcd", {{"a0", MakeInt(64)}, {"b0", MakeInt(64)}}, MakeInt(64));
    gcdEntry(gcd.body);
    gcdStep(gcd.body);
    gcdExit(gcd.body);
  }

  void pickFunction(PolyIRBuilder& b) {
    auto& pick = DefineFunction(b, "pick", {{"x", MakeInt(64)}}, MakeInt(64));
    Block& body = pick.body;
    SwitchOn(body, Local(body, "x"), {{1, "one"}, {2, "one"}}, "none", {{IntConst(10)}, {IntConst(20)}}, {});

    Label(body, "one", {{"v", MakeInt(64)}});
    Return(body, Local(body, "v"));

    Label(body, "none");
    Return(body, IntConst(0));
  }

  void safeFunction(PolyIRBuilder& b) {
    auto& safe = DefineFunction(b, "safe", {{"x", MakeInt(64)}}, MakeInt(64));
    Block& body = safe.body;
    Var(body, "out", MakeInt(64), IntConst(0));
    TryCatch(body, [&](Block& attempt) { ThrowValue(attempt, Local(attempt, "x")); },
               "code", [&](Block& handler) { SetLocal(handler, "out", Add(Local(handler, "code"), IntConst(1))); });
    Return(body, Local(body, "out"));
  }

  void flow() {
    PolyIRBuilder b;
    gcdFunction(b);
    pickFunction(b);
    safeFunction(b);
    expectLines("labels, switch and try", Build(b), kFlowExpected);
  }

  const Vector<String> kContainersExpected = {
    "extern malloc(size:i64)->ptr<i64>",
    "func vecs()->i64 {let v:vec<i64,4> = vec[10,20,30,40]; let r:vec<i64,4> = (shuffle v,3,2,1,0); return (Add:i64 r[0] v[1])}",
    "func dyn(a:i64)->i64 {var arr:mutarray<i64> = new mutarray<i64>(); arr@ = (push arr@,a); (push arr@,2); return (Add:i64 (len arr@) arr@[0])}",
    "func fixed()->i64 {let hist:i64[4] = new i64[4](); hist[1] = 20; return (Add:i64 hist[1] (len hist))}",
    "func shapes()->i64 {let g:grid<i64,3,4> = new grid<i64,3,4>(); g[0,1] = 5; let t:tensor<i64,2,3> = new tensor<i64,2,3>(); return (Add:i64 (Add:i64 (Mul:i64 (rows g) 100) (dim t,1)) g[0,1])}",
    "func maps()->i64 {var hm:mutmap<i64,i64> = new mutmap<i64,i64>(4); hm@[1] = 20; return (Add:i64 hm@[1] (capacity hm@))}",
    "func sums(which:i64)->i64 {var v:variant<i64,f64> = variant#0(10); let r:either<i64,i64> = ok(3); if (isErr r) {return (unwrapErr r)} else {}; return (Add:i64 (tag v@) (unwrapOk r))}",
    "func atoms()->i64 {let slot:ptr<i64> = malloc:ptr<i64>(8); let old:i64 = (atomicRmw slot,\"add\",1,\"seq_cst\"); (fence 0,\"acquire\"); return (Add:i64 old (atomicLoad slot,\"seq_cst\"))}",
    "func hashing()->i64 {if (Eq:i1 \"a\" \"b\") {return 0} else {}; return (hash \"a\")}",
  };

  void vecsFunction(PolyIRBuilder& b) {
    const TypePtr i64 = MakeInt(64);
    auto& vecs = DefineFunction(b, "vecs", {}, i64);
    Block& body = vecs.body;

    const TypePtr vec4 = MakeVec(i64, 4);
    Let(body, "v", VecLit(vec4, {IntConst(10), IntConst(20), IntConst(30), IntConst(40)}));
    Let(body, "r", BuiltinCall(Builtin::Shuffle, Local(body, "v"), {IntConst(3), IntConst(2), IntConst(1), IntConst(0)}, vec4));
    Return(body, Add(Index(Local(body, "r"), {IntConst(0)}), Index(Local(body, "v"), {IntConst(1)})));
  }

  void dynFunction(PolyIRBuilder& b) {
    const TypePtr i64 = MakeInt(64);
    auto& dyn = DefineFunction(b, "dyn", {{"a", i64}}, i64);
    Block& body = dyn.body;

    Var(body, "arr", MakeDynamicArray(i64, true), NewContainer(MakeDynamicArray(i64, true)));
    SetLocal(body, "arr", Push(Local(body, "arr"), Local(body, "a")));
    Eval(body, Push(Local(body, "arr"), IntConst(2)));
    Return(body, Add(Len(Local(body, "arr")), Index(Local(body, "arr"), {IntConst(0)})));
  }

  void fixedFunction(PolyIRBuilder& b) {
    auto& fixed = DefineFunction(b, "fixed", {}, MakeInt(64));
    Block& body = fixed.body;
    Let(body, "hist", NewContainer(MakeFixedArray(MakeInt(64), 4)));
    StoreIndex(body, Local(body, "hist"), {IntConst(1)}, IntConst(20));
    Return(body, Add(Index(Local(body, "hist"), {IntConst(1)}), Len(Local(body, "hist"))));
  }

  ExprPtr shapeTotal(Block& body) {
    const ExprPtr rows = BuiltinCall(Builtin::Rows, Local(body, "g"), {}, MakeInt(64));
    const ExprPtr dim = BuiltinCall(Builtin::Dim, Local(body, "t"), {IntConst(1)}, MakeInt(64));

    return Add(Add(Mul(rows, IntConst(100)), dim), Index(Local(body, "g"), {IntConst(0), IntConst(1)}));
  }

  void shapesFunction(PolyIRBuilder& b) {
    auto& shapes = DefineFunction(b, "shapes", {}, MakeInt(64));
    Block& body = shapes.body;

    Let(body, "g", NewContainer(MakeGrid(MakeInt(64), 3, 4)));
    StoreIndex(body, Local(body, "g"), {IntConst(0), IntConst(1)}, IntConst(5));
    Let(body, "t", NewContainer(MakeTensor(MakeInt(64), {2, 3})));
    Return(body, shapeTotal(body));
  }

  void mapsFunction(PolyIRBuilder& b) {
    const TypePtr i64 = MakeInt(64);
    auto& maps = DefineFunction(b, "maps", {}, i64);
    Block& body = maps.body;

    Var(body, "hm", MakeHashmap(i64, i64, true), NewContainer(MakeHashmap(i64, i64, true), {IntConst(4)}));
    StoreIndex(body, Local(body, "hm"), {IntConst(1)}, IntConst(20));
    Return(body, Add(Index(Local(body, "hm"), {IntConst(1)}), BuiltinCall(Builtin::Capacity, Local(body, "hm"), {}, i64)));
  }

  void sumsFunction(PolyIRBuilder& b) {
    const TypePtr i64 = MakeInt(64);
    auto& sums = DefineFunction(b, "sums", {{"which", i64}}, i64);
    Block& body = sums.body;

    Var(body, "v", MakeVariant({i64, MakeFloat(64)}), VariantLit(MakeVariant({i64, MakeFloat(64)}), 0, IntConst(10)));
    Let(body, "r", Ok(MakeEither(i64, i64), IntConst(3)));
    IfElse(body, IsErr(Local(body, "r")), [&](Block& yes) { Return(yes, UnwrapErr(Local(yes, "r"))); }, [](Block&) {});
    Return(body, Add(VariantTag(Local(body, "v")), UnwrapOk(Local(body, "r"))));
  }

  void atomsFunction(PolyIRBuilder& b) {
    const TypePtr i64 = MakeInt(64);
    auto& atoms = DefineFunction(b, "atoms", {}, i64);
    Block& body = atoms.body;

    Let(body, "slot", Call(b, "malloc", {IntConst(8)}));
    Let(body, "old", BuiltinCall(Builtin::AtomicRmw, Local(body, "slot"), {StringConst("add"), IntConst(1), StringConst("seq_cst")}, i64));
    Eval(body, BuiltinCall(Builtin::AtomicFence, IntConst(0), {StringConst("acquire")}, MakeVoid()));
    Return(body, Add(Local(body, "old"), BuiltinCall(Builtin::AtomicLoad, Local(body, "slot"), {StringConst("seq_cst")}, i64)));
  }

  void hashingFunction(PolyIRBuilder& b) {
    auto& hashing = DefineFunction(b, "hashing", {}, MakeInt(64));
    Block& body = hashing.body;
    IfThen(body, Eq(StringConst("a"), StringConst("b")), [&](Block& yes) { Return(yes, IntConst(0)); });
    Return(body, BuiltinCall(Builtin::Hash, StringConst("a"), {}, MakeInt(64)));
  }

  // The fixed-size and growable arrays, the grid and the tensor.
  void arrayFunctions(PolyIRBuilder& b) {
    vecsFunction(b);
    dynFunction(b);
    fixedFunction(b);
    shapesFunction(b);
  }

  // The map, the variant and either values, the atomics and string hashing.
  void mapSumAndAtomFunctions(PolyIRBuilder& b) {
    mapsFunction(b);
    sumsFunction(b);
    atomsFunction(b);
    hashingFunction(b);
  }

  void containers() {
    PolyIRBuilder b;
    DeclareExtern(b, "malloc", {{"size", MakeInt(64)}}, MakePointer(MakeInt(64)));
    arrayFunctions(b);
    mapSumAndAtomFunctions(b);
    expectLines("containers and sum values", Build(b), kContainersExpected);
  }

  void scalarTypes() {
    expectText("type: optional", TypeText(*MakeOptional(MakeInt(32))), "optional<i32>");
    expectText("type: map", TypeText(*MakeHashmap(MakeString(), MakeFloat(64), true)), "mutmap<string,f64>");
    expectText("type: set", TypeText(*MakeSet(MakeUInt(8), false)), "set<u8>");
    expectText("type: sparse", TypeText(*MakeSparse(MakeInt(64))), "sparse<i64>");
  }

  void compoundTypes() {
    expectText("type: closure", TypeText(*MakeClosure({MakeInt(64), MakeBool()}, MakeInt(64))), "closure(i64,i1)->i64");
    expectText("type: func", TypeText(*MakeFunction({}, MakeVoid())), "func()->void");
    expectText("type: coro", TypeText(*MakeCoroutine(MakeInt(64))), "coro<i64>");
    expectText("type: either", TypeText(*MakeEither(MakeInt(64), MakeString())), "either<i64,string>");
  }

  void typesPrint() {
    scalarTypes();
    compoundTypes();
  }

  const Vector<String> kOnlyHereExpected = {
    "struct Dog{age:i64}",
    "struct Animal{data:ptr<i8>,vtable:ptr<i8>}",
    "interface Animal{age}:Dog",
    "func count(n:i64)->coro<i64> {yield n}",
    "func use()->i64 {var total:i64 = 0; let add:closure(i64)->i64 = <node>; return (*add):i64(1)}",
  };

  void animalTypes(PolyIRBuilder& b) {
    DefineStruct(b, "Dog", {{"age", MakeInt(64), false}});
    DefineInterface(b, "Animal", {{"age", {}, MakeInt(64)}});
    Implement(b, "Animal", "Dog");
  }

  void countCoroutine(PolyIRBuilder& b) {
    auto& gen = DefineCoroutine(b, "count", {{"n", MakeInt(64)}}, MakeInt(64));
    YieldValue(gen.body, Local(gen.body, "n"));
  }

  void addBy(Block& inner) {
    SetLocal(inner, "total", Add(Local(inner, "total"), Local(inner, "by")));
    Return(inner, Local(inner, "total"));
  }

  void useFunction(PolyIRBuilder& b) {
    auto& use = DefineFunction(b, "use", {}, MakeInt(64));
    Block& body = use.body;
    Var(body, "total", MakeInt(64), IntConst(0));
    Let(body, "add", ClosureLiteral(body, {{"by", MakeInt(64)}}, MakeInt(64), [](Block& inner) { addBy(inner); }));
    Return(body, CallIndirect(Local(body, "add"), {IntConst(1)}));
  }

  void onlyHere() {
    PolyIRBuilder b;
    animalTypes(b);
    countCoroutine(b);
    useFunction(b);
    expectLines("closures, interfaces and coroutines", Build(b), kOnlyHereExpected);
  }

  void closureWrongArgument(PolyIRBuilder& b) {
    auto& f = DefineFunction(b, "f", {}, MakeInt(64));
    const ExprPtr c = ClosureLiteral(f.body, {{"x", MakeInt(64)}}, MakeInt(64), [&](Block& in) { Return(in, Local(in, "x")); });
    CallIndirect(c, {StringConst("no")});
  }

  void yieldOutsideCoroutine(PolyIRBuilder& b) {
    auto& f = DefineFunction(b, "plain", {}, MakeInt(64));
    YieldValue(f.body, IntConst(1));
  }

  void boxWithoutImplementing(PolyIRBuilder& b) {
    DefineStruct(b, "Cat", {{"x", MakeInt(64), false}});
    DefineInterface(b, "Pet", {});
    InterfaceBox(b, "Pet", StructLit(b, "Cat", {{"x", IntConst(1)}}));
  }

  void containerMistakes() {
    expectRefused("pushing a string onto a list of numbers", [&] { Push(NewContainer(MakeDynamicArray(MakeInt(64), true)), StringConst("x")); });
    expectRefused("pushing onto an array that cannot change", [&] { Push(NewContainer(MakeDynamicArray(MakeInt(64), false)), IntConst(1)); });
    expectRefused("indexing a grid with one index", [&] { Index(NewContainer(MakeGrid(MakeInt(64), 2, 2)), {IntConst(0)}); });
    expectRefused("a variant case that does not exist", [&] { VariantLit(MakeVariant({MakeInt(64)}), 3, IntConst(1)); });
  }

  void functionMistakes(PolyIRBuilder& b) {
    expectRefused("unwrapping something that is not an optional", [&] { Unwrap(IntConst(1)); });
    expectRefused("calling something that is not a function", [&] { CallIndirect(IntConst(1), {}); });
    expectRefused("a closure called with the wrong argument", [&] { closureWrongArgument(b); });
    expectRefused("a yield outside a coroutine", [&] { yieldOutsideCoroutine(b); });
    expectRefused("boxing a struct that does not implement the interface", [&] { boxWithoutImplementing(b); });
  }

  void mistakes() {
    PolyIRBuilder b;
    containerMistakes();
    functionMistakes(b);
  }

  int finish() {
    cout << (failures == 0 ? "all checks passed" : "some checks failed") << "\n";

    return failures == 0 ? 0 : 1;
  }

}  // namespace

int main() {
  scalarsAndStructs();
  flow();
  containers();

  typesPrint();
  onlyHere();
  mistakes();

  return finish();
}
