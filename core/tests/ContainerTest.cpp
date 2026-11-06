// Programs that use the HIR beyond the scalar core, compiled and run: lists, buffers, grids, vectors, optionals, eithers, variants, closures, function
// values and interfaces. Each returns a number the test knows.
#include "polyir/Std.hpp"
#include "RunSupport.hpp"

using namespace polyir;
using namespace polyir::testing;

namespace {

  void listPushAndIndex() {
    Check("list_push", 30, MakeProgram([](PolyIRBuilder&, Block& body) {
      Let(body, "xs", NewContainer(MakeDynamicArray(MakeInt(64), true)));
      Eval(body, Push(Local(body, "xs"), IntConst(10)));
      Eval(body, Push(Local(body, "xs"), IntConst(20)));
      Eval(body, Push(Local(body, "xs"), IntConst(30)));
      Return(body, Index(Local(body, "xs"), {IntConst(2)}));
    }));
  }

  void sumBody(PolyIRBuilder&, Block& body) {
    Let(body, "xs", NewContainer(MakeDynamicArray(MakeInt(64), true)));
    ForRange(body, "i", IntConst(0), IntConst(100), [&](Block& loop) { Eval(loop, Push(Local(loop, "xs"), Local(loop, "i"))); });
    Var(body, "total", MakeInt(64), IntConst(0));
    ForRange(body, "j", IntConst(0), Len(Local(body, "xs")), [&](Block& loop) {
      SetLocal(loop, "total", Add(Local(loop, "total"), Index(Local(loop, "xs"), {Local(loop, "j")})));
    });
    Return(body, Local(body, "total"));
  }

  void listGrowsAndSums() {
    Check("list_sum", 4950 % 256, MakeProgram(sumBody));
  }

  void listLiteralAndSetIndex() {
    Check("list_set", 47, MakeProgram([](PolyIRBuilder&, Block& body) {
      Let(body, "xs", VecLit(MakeDynamicArray(MakeInt(64), true), {IntConst(1), IntConst(2), IntConst(3)}));
      StoreIndex(body, Local(body, "xs"), {IntConst(1)}, IntConst(40));
      Return(body, Add(Index(Local(body, "xs"), {IntConst(1)}), Add(Index(Local(body, "xs"), {IntConst(0)}), IntConst(6))));
    }));
  }

  void copyIsApart() {
    Check("list_copy", 91, MakeProgram([](PolyIRBuilder&, Block& body) {
      Let(body, "a", VecLit(MakeDynamicArray(MakeInt(64), true), {IntConst(1), IntConst(2)}));
      Let(body, "c", Copy(Local(body, "a")));
      StoreIndex(body, Local(body, "c"), {IntConst(0)}, IntConst(9));
      Return(body, Add(Mul(Index(Local(body, "c"), {IntConst(0)}), IntConst(10)), Index(Local(body, "a"), {IntConst(0)})));  // 9*10 + 1
    }));
  }

  void gridBody(PolyIRBuilder&, Block& body) {
    Let(body, "g", NewContainer(MakeGrid(MakeInt(64), 3, 4)));
    StoreIndex(body, Local(body, "g"), {IntConst(2), IntConst(3)}, IntConst(12));
    Return(body, Index(Local(body, "g"), {IntConst(2), IntConst(3)}));
  }

  void bufferBody(PolyIRBuilder&, Block& body) {
    Let(body, "buf", NewContainer(MakeBuffer(MakeInt(32), 8)));
    StoreIndex(body, Local(body, "buf"), {IntConst(5)}, IntConst(7, MakeInt(32)));
    Return(body, Cast(Index(Local(body, "buf"), {IntConst(5)}), MakeInt(64)));
  }

  void bufferAndGrid() {
    Check("grid", 12, MakeProgram(gridBody));
    Check("buffer", 7, MakeProgram(bufferBody));
  }

  void pastTheEnd(PolyIRBuilder&, Block& body) {
    Let(body, "xs", VecLit(MakeDynamicArray(MakeInt(64), true), {IntConst(1)}));
    Return(body, Index(Local(body, "xs"), {IntConst(5)}));
  }

  void outOfRangeStops() {
    const int got = Run("out_of_range", MakeProgram(pastTheEnd));
    const bool stopped = got != 0 && got != 1;
    cout << (stopped ? "ok   out_of_range stops\n" : "FAIL out_of_range: reading past the end went on (" + to_string(got) + ")\n");
    failures += stopped ? 0 : 1;
  }

  void vectors() {
    Check("vec", 10, MakeProgram([](PolyIRBuilder&, Block& body) {
      const TypePtr v = MakeVec(MakeFloat(32), 4);
      Let(body, "a", VecLit(v, {FloatConst(1, MakeFloat(32)), FloatConst(2, MakeFloat(32)), FloatConst(3, MakeFloat(32)), FloatConst(4, MakeFloat(32))}));
      Let(body, "sum", Add(Local(body, "a"), Local(body, "a")));
      Return(body, Cast(Add(Index(Local(body, "sum"), {IntConst(3)}), Index(Local(body, "sum"), {IntConst(0)})), MakeInt(64)));
    }));
  }

  void optionalsBody(PolyIRBuilder&, Block& body) {
    Let(body, "a", Some(IntConst(40)));
    Let(body, "n", None(MakeInt(64)));
    Var(body, "r", MakeInt(64), IntConst(0));

    IfThen(body, IsSome(Local(body, "a")), [&](Block& then) { SetLocal(then, "r", Add(Local(then, "r"), Unwrap(Local(body, "a")))); });
    IfThen(body, IsNone(Local(body, "n")), [&](Block& then) { SetLocal(then, "r", Add(Local(then, "r"), IntConst(2))); });
    IfThen(body, IsSome(Local(body, "n")), [&](Block& then) { SetLocal(then, "r", IntConst(-1)); });
    Return(body, Local(body, "r"));
  }

  void optionals() {
    Check("optional", 42, MakeProgram(optionalsBody));
  }

  void unwrapOfNoneStops() {
    const int got = Run("unwrap_none", MakeProgram([](PolyIRBuilder&, Block& body) { Return(body, Unwrap(None(MakeInt(64)))); }));
    cout << (got != 0 && got != 1 ? "ok   unwrap_none stops\n" : "FAIL unwrap_none: it went on\n");
    failures += (got != 0 && got != 1) ? 0 : 1;
  }

  void eitherValues(PolyIRBuilder&, Block& body) {
    const TypePtr t = MakeEither(MakeInt(64), MakeInt(32));
    Let(body, "good", Ok(t, IntConst(10)));
    Let(body, "bad", Err(t, IntConst(5, MakeInt(32))));
    Return(body, Add(UnwrapOk(Local(body, "good")), Cast(UnwrapErr(Local(body, "bad")), MakeInt(64))));
  }

  void eitherTests(PolyIRBuilder&, Block& body) {
    const TypePtr t = MakeEither(MakeInt(64), MakeInt(64));
    Let(body, "e", Err(t, IntConst(1)));
    Var(body, "r", MakeInt(64), IntConst(0));

    IfThen(body, IsErr(Local(body, "e")), [&](Block& then) { SetLocal(then, "r", IntConst(3)); });
    IfThen(body, IsOk(Local(body, "e")), [&](Block& then) { SetLocal(then, "r", IntConst(99)); });
    Return(body, Local(body, "r"));
  }

  void eithers() {
    Check("either", 15, MakeProgram(eitherValues));
    Check("either_tests", 3, MakeProgram(eitherTests));
  }

  void variantTagged(Block& then, Block& body) {
    SetLocal(then, "r", Add(Mul(Cast(VariantUnwrap(Local(body, "v"), 2), MakeInt(64)), IntConst(10)), IntConst(7)));
  }

  void variantsBody(PolyIRBuilder&, Block& body) {
    const TypePtr t = MakeVariant({MakeInt(64), MakeFloat(64), MakeInt(32)});
    Let(body, "v", VariantLit(t, 2, IntConst(7, MakeInt(32))));
    Var(body, "r", MakeInt(64), IntConst(0));
    IfThen(body, Eq(VariantTag(Local(body, "v")), IntConst(2)), [&](Block& then) { variantTagged(then, body); });
    Return(body, Local(body, "r"));
  }

  void variants() {
    Check("variant", 77, MakeProgram(variantsBody));
  }

  void twiceMain(PolyIRBuilder& b, Block& body) {
    Let(body, "f", FuncRef(b, "twice"));
    Return(body, CallIndirect(Local(body, "f"), {IntConst(21)}));
  }

  void twiceDefinition(PolyIRBuilder& b) {
    auto& twice = DefineFunction(b, "twice", {{"x", MakeInt(64)}}, MakeInt(64));
    Return(twice.body, Mul(Local(twice.body, "x"), IntConst(2)));
  }

  void functionValues() {
    Check("func_value", 42, MakeProgram(twiceMain, twiceDefinition));
  }

  void bumpClosure(Block& inner) {
    SetLocal(inner, "count", Add(Local(inner, "count"), Local(inner, "by")));
    Return(inner, Local(inner, "count"));
  }

  // A builtin `Free` on `value`, as a statement.
  void release(Block& body, ExprPtr value) {
    Eval(body, BuiltinCall(Builtin::Free, move(value), {}, MakeVoid()));
  }

  void freeSumsBody(PolyIRBuilder&, Block& body) {
    Let(body, "a", Some(IntConst(40)));
    Let(body, "e", Ok(MakeEither(MakeInt(64), MakeInt(64)), IntConst(2)));
    Var(body, "r", MakeInt(64), Add(Unwrap(Local(body, "a")), UnwrapOk(Local(body, "e"))));

    release(body, Local(body, "a"));
    release(body, Local(body, "e"));
    Return(body, Local(body, "r"));
  }

  void freePlainClosure(PolyIRBuilder&, Block& body) {
    Let(body, "plain", ClosureLiteral(body, {}, MakeInt(64), [&](Block& inner) { Return(inner, IntConst(12)); }));
    SetLocal(body, "count", Add(Local(body, "count"), CallIndirect(Local(body, "plain"), {})));
    release(body, Local(body, "plain"));
  }

  // One closure captures `count` (the cell outlives the free), one captures nothing (a null environment).
  void freeClosuresBody(PolyIRBuilder& b, Block& body) {
    Var(body, "count", MakeInt(64), IntConst(30));
    Let(body, "bump", ClosureLiteral(body, {{"by", MakeInt(64)}}, MakeInt(64), [](Block& inner) { bumpClosure(inner); }));
    Eval(body, CallIndirect(Local(body, "bump"), {IntConst(0)}));
    release(body, Local(body, "bump"));

    freePlainClosure(b, body);
    Return(body, Local(body, "count"));
  }

  void freeing() {
    Check("free_sums", 42, MakeProgram(freeSumsBody));
    Check("free_closures", 42, MakeProgram(freeClosuresBody));
  }

  void closureBody(PolyIRBuilder&, Block& body) {
    Var(body, "count", MakeInt(64), IntConst(100));
    Let(body, "bump", ClosureLiteral(body, {{"by", MakeInt(64)}}, MakeInt(64), [](Block& inner) { bumpClosure(inner); }));

    Eval(body, CallIndirect(Local(body, "bump"), {IntConst(5)}));
    Eval(body, CallIndirect(Local(body, "bump"), {IntConst(5)}));
    SetLocal(body, "count", Add(Local(body, "count"), IntConst(2)));
    Return(body, Local(body, "count"));
  }

  void adderMain(PolyIRBuilder& b, Block& body) {
    Let(body, "add5", Call(b, "adder", {IntConst(5)}));
    Return(body, Add(CallIndirect(Local(body, "add5"), {IntConst(7)}), CallIndirect(Local(body, "add5"), {IntConst(0)})));
  }

  void adderDefinition(PolyIRBuilder& b) {
    const TypePtr closureType = MakeClosure({MakeInt(64)}, MakeInt(64));
    auto& adder = DefineFunction(b, "adder", {{"n", MakeInt(64)}}, closureType);
    Return(adder.body, ClosureLiteral(adder.body, {{"x", MakeInt(64)}}, MakeInt(64), [&](Block& inner) { Return(inner, Add(Local(inner, "x"), Local(inner, "n"))); }));
  }

  void closuresCaptureByReference() {
    Check("closure", 112, MakeProgram(closureBody));
    Check("closure_made_in_function", 17, MakeProgram(adderMain, adderDefinition));
  }

  void shapesMain(PolyIRBuilder& b, Block& body) {
    Let(body, "shapes", VecLit(MakeDynamicArray(MakeStruct("Shape"), true), {
      InterfaceBox(b, "Shape", StructLit(b, "Square", {{"side", IntConst(5)}})),
      InterfaceBox(b, "Shape", StructLit(b, "Rect", {{"w", IntConst(4)}, {"h", IntConst(7)}}))}));
    Return(body, Add(InterfaceCall(b, Index(Local(body, "shapes"), {IntConst(0)}), "area", {}),
                   InterfaceCall(b, Index(Local(body, "shapes"), {IntConst(1)}), "scaled", {IntConst(1)})));
  }

  void shapeTypes(PolyIRBuilder& b) {
    DefineStruct(b, "Square", {{"side", MakeInt(64), false}});
    DefineStruct(b, "Rect", {{"w", MakeInt(64), false}, {"h", MakeInt(64), false}});
    DefineInterface(b, "Shape", {{"area", {}, MakeInt(64)}, {"scaled", {MakeInt(64)}, MakeInt(64)}});
    Implement(b, "Shape", "Square");
    Implement(b, "Shape", "Rect");
  }

  void squareMethods(PolyIRBuilder& b) {
    FunctionBuilder& squareArea = DefineFunction(b, "Square_area", {{"self", MakeStruct("Square")}}, MakeInt(64));
    Return(squareArea.body, Mul(FieldAccess(b, Local(squareArea.body, "self"), "side"), FieldAccess(b, Local(squareArea.body, "self"), "side")));
    FunctionBuilder& squareScaled = DefineFunction(b, "Square_scaled", {{"self", MakeStruct("Square")}, {"k", MakeInt(64)}}, MakeInt(64));
    Return(squareScaled.body, Mul(FieldAccess(b, Local(squareScaled.body, "self"), "side"), Local(squareScaled.body, "k")));
  }

  void rectMethods(PolyIRBuilder& b) {
    FunctionBuilder& rectArea = DefineFunction(b, "Rect_area", {{"self", MakeStruct("Rect")}}, MakeInt(64));
    Return(rectArea.body, Mul(FieldAccess(b, Local(rectArea.body, "self"), "w"), FieldAccess(b, Local(rectArea.body, "self"), "h")));
    FunctionBuilder& rectScaled = DefineFunction(b, "Rect_scaled", {{"self", MakeStruct("Rect")}, {"k", MakeInt(64)}}, MakeInt(64));
    Return(rectScaled.body, Mul(FieldAccess(b, Local(rectScaled.body, "self"), "w"), Local(rectScaled.body, "k")));
  }

  void shapeDefinitions(PolyIRBuilder& b) {
    shapeTypes(b);
    squareMethods(b);
    rectMethods(b);
  }

  void interfaces() {
    Check("interface", 29, MakeProgram(shapesMain, shapeDefinitions));
  }

  void strings() {
    Check("strings", 5, MakeProgram([](PolyIRBuilder&, Block& body) {
      Let(body, "s", Concat(StringConst("ab"), StringConst("cde")));
      Var(body, "r", MakeInt(64), Len(Local(body, "s")));
      IfThen(body, LogNot(StrEq(Local(body, "s"), StringConst("abcde"))), [&](Block& then) { SetLocal(then, "r", IntConst(0)); });
      Return(body, Local(body, "r"));
    }));
  }

  void identityBody(PolyIRBuilder&, Block& body) {
    Let(body, "a", VecLit(MakeDynamicArray(MakeInt(64), true), {IntConst(1)}));
    Let(body, "same", Local(body, "a"));
    Let(body, "other", Copy(Local(body, "a")));

    Var(body, "r", MakeInt(64), IntConst(0));
    IfThen(body, LogAnd(Identical(Local(body, "a"), Local(body, "same")), NotIdentical(Local(body, "a"), Local(body, "other"))), [&](Block& then) { SetLocal(then, "r", IntConst(1)); });
    Return(body, Local(body, "r"));
  }

  void identity() {
    Check("identity", 1, MakeProgram(identityBody));
  }

  void caughtHere(PolyIRBuilder&, Block& body) {
    Var(body, "r", MakeInt(64), IntConst(0));
    TryCatch(body, [&](Block& attempt) {
      ThrowValue(attempt, IntConst(7));
      SetLocal(attempt, "r", IntConst(99));
    }, "code", [&](Block& handler) { SetLocal(handler, "r", Add(Local(handler, "code"), IntConst(1))); });
    Return(body, Local(body, "r"));
  }

  void throughCallsMain(PolyIRBuilder& b, Block& body) {
    Var(body, "r", MakeInt(64), IntConst(0));
    TryCatch(body, [&](Block& attempt) { SetLocal(attempt, "r", Call(b, "outer", {IntConst(5)})); },
                  "code", [&](Block& handler) { SetLocal(handler, "r", Mul(Local(handler, "code"), IntConst(9))); });
    Return(body, Local(body, "r"));
  }

  void throwingCalls(PolyIRBuilder& b) {
    auto& inner = DefineFunction(b, "inner", {{"n", MakeInt(64)}}, MakeInt(64));
    IfThen(inner.body, Gt(Local(inner.body, "n"), IntConst(3)), [&](Block& then) { ThrowValue(then, Local(then, "n")); });
    Return(inner.body, Local(inner.body, "n"));
    auto& outer = DefineFunction(b, "outer", {{"n", MakeInt(64)}}, MakeInt(64));
    Return(outer.body, Add(Call(b, "inner", {Local(outer.body, "n")}), IntConst(1000)));
  }

  void rethrowing(Block& handler) {
    SetLocal(handler, "r", Add(Local(handler, "r"), Local(handler, "a")));
    ThrowValue(handler, IntConst(10));
  }

  void nestedTryBody(PolyIRBuilder&, Block& body) {
    Var(body, "r", MakeInt(64), IntConst(0));
    TryCatch(body, [&](Block& outerTry) {
      TryCatch(outerTry, [&](Block& innerTry) { ThrowValue(innerTry, IntConst(2)); }, "a", [](Block& handler) { rethrowing(handler); });
    }, "b", [&](Block& handler) { SetLocal(handler, "r", Add(Local(handler, "r"), Local(handler, "b"))); });
    Return(body, Local(body, "r"));
  }

  // A cleanup runs when the body throws, then the throw goes on to the enclosing catch. The sums: 7 thrown, +100 by the cleanup,
  // caught as 7 -> 107.
  void cleanupBody(PolyIRBuilder&, Block& body) {
    Var(body, "r", MakeInt(64), IntConst(0));
    TryCatch(body, [&](Block& outerTry) {
      TryCleanup(outerTry, [&](Block& inside) { ThrowValue(inside, IntConst(7)); },
                          [&](Block& cleanup) { SetLocal(cleanup, "r", IntConst(100)); });
    }, "code", [&](Block& handler) { SetLocal(handler, "r", Add(Local(handler, "r"), Local(handler, "code"))); });
    Return(body, Local(body, "r"));
  }

  void noThrowBody(PolyIRBuilder&, Block& body) {
    Var(body, "r", MakeInt(64), IntConst(1));
    TryCatch(body, [&](Block& attempt) { SetLocal(attempt, "r", Add(Local(attempt, "r"), IntConst(5))); }, "code", [&](Block& handler) { SetLocal(handler, "r", IntConst(0)); });
    Return(body, Local(body, "r"));
  }

  void closureThrowsBody(PolyIRBuilder&, Block& body) {
    Let(body, "boom", ClosureLiteral(body, {}, MakeInt(64), [&](Block& inner) { ThrowValue(inner, IntConst(30)); Return(inner, IntConst(0)); }));
    Var(body, "r", MakeInt(64), IntConst(0));
    TryCatch(body, [&](Block& attempt) { SetLocal(attempt, "r", CallIndirect(Local(attempt, "boom"), {})); },
                  "code", [&](Block& handler) { SetLocal(handler, "r", Add(Local(handler, "code"), IntConst(1))); });
    Return(body, Local(body, "r"));
  }

  void uncaughtBody(PolyIRBuilder&, Block& body) {
    ThrowValue(body, IntConst(3));
    Return(body, IntConst(0));
  }

  void uncaughtStops() {
    const int got = Run("uncaught", MakeProgram(uncaughtBody));
    cout << (got != 0 && got != 3 ? "ok   uncaught throw stops\n" : "FAIL uncaught throw went on\n");
    failures += (got != 0 && got != 3) ? 0 : 1;
  }

  void exceptions() {
    Check("throw_caught_here", 8, MakeProgram(caughtHere));
    Check("throw_through_calls", 45, MakeProgram(throughCallsMain, throwingCalls));
    Check("nested_try", 12, MakeProgram(nestedTryBody));
    Check("no_throw_runs_on", 6, MakeProgram(noThrowBody));

    Check("cleanup_runs_then_rethrows", 107, MakeProgram(cleanupBody));
    Check("thrown_from_a_closure", 31, MakeProgram(closureThrowsBody));
    uncaughtStops();
  }

  void atomicsBody(PolyIRBuilder& b, Block& body) {
    Let(body, "cell", Call(b, "malloc", {IntConst(8)}));
    Eval(body, BuiltinCall(Builtin::AtomicStore, Local(body, "cell"), {IntConst(3), StringConst("seq_cst")}, MakeInt(64)));

    Let(body, "old", BuiltinCall(Builtin::AtomicRmw, Local(body, "cell"), {StringConst("add"), IntConst(2), StringConst("seq_cst")}, MakeInt(64)));
    Eval(body, BuiltinCall(Builtin::AtomicFence, IntConst(0), {StringConst("acquire")}, MakeVoid()));
    Let(body, "swapped", BuiltinCall(Builtin::AtomicCmpxchg, Local(body, "cell"), {IntConst(5), IntConst(4), StringConst("seq_cst"), StringConst("seq_cst")}, MakeInt(64)));
    Return(body, Add(Add(Local(body, "old"), Local(body, "swapped")), BuiltinCall(Builtin::AtomicLoad, Local(body, "cell"), {StringConst("seq_cst")}, MakeInt(64))));
  }

  void mallocDeclaration(PolyIRBuilder& b) {
    DeclareExtern(b, "malloc", {{"size", MakeInt(64)}}, MakePointer(MakeInt(64)));
  }

  void shuffleBody(PolyIRBuilder&, Block& body) {
    const TypePtr v = MakeVec(MakeInt(64), 4);
    Let(body, "v", VecLit(v, {IntConst(10), IntConst(20), IntConst(30), IntConst(40)}));
    Let(body, "r", BuiltinCall(Builtin::Shuffle, Local(body, "v"), {IntConst(3), IntConst(2), IntConst(1), IntConst(0)}, v));
    Return(body, Add(Index(Local(body, "r"), {IntConst(0)}), Sub(Index(Local(body, "v"), {IntConst(0)}), IntConst(9))));
  }

  ExprPtr hashOf(ExprPtr text) {
    return BuiltinCall(Builtin::Hash, text, {}, MakeInt(64));
  }

  void hashBody(PolyIRBuilder&, Block& body) {
    const ExprPtr joined = Concat(StringConst("a"), StringConst("bc"));
    const ExprPtr same = Eq(hashOf(StringConst("abc")), hashOf(joined));
    const ExprPtr different = Ne(hashOf(StringConst("abc")), hashOf(StringConst("abd")));
    Return(body, Cast(LogAnd(same, different), MakeInt(64)));
  }

  void atomicsShuffleHash() {
    Check("atomics", 12, MakeProgram(atomicsBody, mallocDeclaration));
    Check("shuffle", 41, MakeProgram(shuffleBody));
    Check("hash", 1, MakeProgram(hashBody));
  }

  void countCoroutine(PolyIRBuilder& b) {
    auto& gen = DefineCoroutine(b, "count", {{"n", MakeInt(64)}}, MakeInt(64));
    ForRange(gen.body, "i", IntConst(0), Local(gen.body, "n"), [&](Block& loop) { YieldValue(loop, Mul(Local(loop, "i"), IntConst(10))); });
  }

  ExprPtr yielded(Block& body, const String& step) {
    return VariantUnwrap(Local(body, step), 0);
  }

  ExprPtr finishedFlag(Block& body, const String& step) {
    return Cast(VariantIsCase(Local(body, step), 1), MakeInt(64));
  }

  void coroutineBody(PolyIRBuilder& b, Block& body) {
    Let(body, "c", Call(b, "count", {IntConst(2)}));

    for (const char* step : {"first", "second", "last"}) {
      Let(body, step, Resume(Local(body, "c")));
    }

    const ExprPtr values = Add(yielded(body, "first"), yielded(body, "second"));  // 0 + 10
    const ExprPtr flags = Add(Mul(finishedFlag(body, "last"), IntConst(100)), Mul(Cast(Done(Local(body, "c")), MakeInt(64)), IntConst(5)));
    Return(body, Add(values, flags));
  }

  // The same generator, but its loop would run far past `n`: a `return` at the third turn ends it instead.
  void countThenReturn(PolyIRBuilder& b) {
    auto& gen = DefineCoroutine(b, "count", {{"n", MakeInt(64)}}, MakeInt(64));
    ForRange(gen.body, "i", IntConst(0), Add(Local(gen.body, "n"), IntConst(100)), [&](Block& loop) {
      IfThen(loop, Eq(Local(loop, "i"), Local(loop, "n")), [&](Block& stop) { ReturnVoid(stop); });
      YieldValue(loop, Mul(Local(loop, "i"), IntConst(10)));
    });
  }

  // `watched` notes 99 in the log when it is abandoned at a checked yield. `plain` yields without looking and would log 7 if
  // anything of it ran again.
  void watchedCoroutine(PolyIRBuilder& b) {
    auto& gen = DefineCoroutine(b, "watched", {{"log", MakeDynamicArray(MakeInt(64), true)}}, MakeInt(64));
    ForRange(gen.body, "i", IntConst(0), IntConst(5), [&](Block& loop) {
      IfThen(loop, Yielded(Local(loop, "i")), [&](Block& stop) {
        Eval(stop, Push(Local(stop, "log"), IntConst(99)));
        ReturnVoid(stop);
      });
    });
  }

  void plainCoroutine(PolyIRBuilder& b) {
    auto& gen = DefineCoroutine(b, "plain", {{"log", MakeDynamicArray(MakeInt(64), true)}}, MakeInt(64));
    YieldValue(gen.body, IntConst(1));
    Eval(gen.body, Push(Local(gen.body, "log"), IntConst(7)));
    YieldValue(gen.body, IntConst(2));
  }

  void closingCoroutines(PolyIRBuilder& b) {
    watchedCoroutine(b);
    plainCoroutine(b);
  }

  void closeWatchedAfterTwoResumes(PolyIRBuilder& b, Block& body) {
    Let(body, "w", Call(b, "watched", {Local(body, "log")}));
    Eval(body, Resume(Local(body, "w")));
    Eval(body, Resume(Local(body, "w")));
    Eval(body, CloseCoroutine(Local(body, "w")));
  }

  void closePlainAfterOneResume(PolyIRBuilder& b, Block& body) {
    Let(body, "p", Call(b, "plain", {Local(body, "log")}));
    Eval(body, Resume(Local(body, "p")));
    Eval(body, CloseCoroutine(Local(body, "p")));
  }

  void closeWatchedBeforeStarting(PolyIRBuilder& b, Block& body) {
    Let(body, "u", Call(b, "watched", {Local(body, "log")}));
    Eval(body, CloseCoroutine(Local(body, "u")));
  }

  void closeBody(PolyIRBuilder& b, Block& body) {
    Let(body, "log", NewContainer(MakeDynamicArray(MakeInt(64), true)));

    closeWatchedAfterTwoResumes(b, body);
    closePlainAfterOneResume(b, body);
    closeWatchedBeforeStarting(b, body);

    const ExprPtr first = Sub(Index(Local(body, "log"), {IntConst(0)}), IntConst(90));
    Return(body, Add(Mul(Len(Local(body, "log")), IntConst(10)), first));
  }

  void coroutines() {
    Check("coroutine", 115, MakeProgram(coroutineBody, countCoroutine));
    Check("coroutine_return", 115, MakeProgram(coroutineBody, countThenReturn));
    Check("coroutine_close", 19, MakeProgram(closeBody, closingCoroutines));
  }

  void sparseBody(PolyIRBuilder&, Block& body) {
    Let(body, "sp", NewContainer(MakeSparse(MakeInt(64))));
    StoreIndex(body, Local(body, "sp"), {IntConst(1), IntConst(2)}, IntConst(10));
    StoreIndex(body, Local(body, "sp"), {IntConst(1), IntConst(2)}, IntConst(20));
    StoreIndex(body, Local(body, "sp"), {IntConst(5)}, IntConst(7));

    const ExprPtr stored = Add(Index(Local(body, "sp"), {IntConst(1), IntConst(2)}), Index(Local(body, "sp"), {IntConst(5)}));
    const ExprPtr unset = Mul(Index(Local(body, "sp"), {IntConst(9), IntConst(9)}), IntConst(100));
    Return(body, Add(Add(stored, unset), Mul(Len(Local(body, "sp")), IntConst(10))));  // 20 + 7 + 0 + 3 * 10
  }

  void sparseCells() {
    Check("sparse", 57, MakeProgram(sparseBody));
  }

  // ── structural values: ==, copies and concatenation ──

  ExprPtr weigh(const ExprPtr& condition, int64_t weight) {
    return Mul(Cast(condition, MakeInt(64)), IntConst(weight));
  }

  ExprPtr counter(PolyIRBuilder& b, int64_t n) {
    return StructLit(b, "Counter", {{"n", IntConst(n)}, {"id", IntConst(2)}});
  }

  void counterType(PolyIRBuilder& b) {
    DefineStruct(b, "Counter", {{"n", MakeInt(64), true}, {"id", MakeInt(64), false}});
  }

  void structEqualityBody(PolyIRBuilder& b, Block& body) {
    Let(body, "a", counter(b, 1));
    Let(body, "c", counter(b, 1));
    Let(body, "first", weigh(Eq(Local(body, "a"), Local(body, "c")), 1));
    SetField(body, Local(body, "c"), "n", IntConst(5));
    Return(body, Add(Local(body, "first"), Add(weigh(Ne(Local(body, "a"), Local(body, "c")), 2), weigh(Identical(Local(body, "a"), Local(body, "a")), 4))));
  }

  void structEquality() {
    Check("struct_equality", 7, MakeProgram(structEqualityBody, counterType));
  }

  void structCopyBody(PolyIRBuilder& b, Block& body) {
    Let(body, "a", counter(b, 1));
    Let(body, "c", Copy(Local(body, "a")));
    SetField(body, Local(body, "c"), "n", IntConst(9));
    Return(body, Add(Mul(FieldAccess(b, Local(body, "a"), "n"), IntConst(10)), FieldAccess(b, Local(body, "c"), "n")));  // 1 * 10 + 9
  }

  void structCopy() {
    Check("struct_copy", 19, MakeProgram(structCopyBody, counterType));
  }

  ExprPtr numberList(Vector<int64_t> values) {
    Vector<ExprPtr> items;

    for (const int64_t v : values) {
      items.push_back(IntConst(v));
    }

    return VecLit(MakeDynamicArray(MakeInt(64), true), items);
  }

  void listEqualityBody(PolyIRBuilder&, Block& body) {
    Let(body, "a", numberList({1, 2, 3}));
    Let(body, "c", numberList({1, 2, 3}));
    Let(body, "d", numberList({1, 2}));
    Let(body, "e", numberList({1, 2, 4}));

    const ExprPtr same = weigh(Eq(Local(body, "a"), Local(body, "c")), 1);
    const ExprPtr shorter = weigh(Ne(Local(body, "a"), Local(body, "d")), 2);
    Return(body, Add(same, Add(shorter, weigh(Ne(Local(body, "a"), Local(body, "e")), 4))));
  }

  void listEquality() {
    Check("list_equality", 7, MakeProgram(listEqualityBody));
  }

  void listConcatBody(PolyIRBuilder&, Block& body) {
    Let(body, "a", numberList({1, 2}));
    Let(body, "c", BuiltinCall(Builtin::Concat, Local(body, "a"), {numberList({3, 4})}, MakeDynamicArray(MakeInt(64), true)));
    Eval(body, Push(Local(body, "a"), IntConst(99)));
    Return(body, Add(Mul(Len(Local(body, "c")), IntConst(10)), Index(Local(body, "c"), {IntConst(3)})));  // 4 * 10 + 4
  }

  void listConcat() {
    Check("list_concat", 44, MakeProgram(listConcatBody));
  }

  void sumEqualityBody(PolyIRBuilder&, Block& body) {
    const TypePtr t = MakeEither(MakeInt(64), MakeInt(64));
    const ExprPtr eithers = Eq(Ok(t, IntConst(3)), Ok(t, IntConst(3)));
    const ExprPtr differ = Ne(Ok(t, IntConst(3)), Err(t, IntConst(3)));
    const ExprPtr optionals = Eq(Some(IntConst(8)), Some(IntConst(8)));
    Return(body, Add(weigh(eithers, 1), Add(weigh(differ, 2), weigh(optionals, 4))));
  }

  void sumEquality() {
    Check("sum_equality", 7, MakeProgram(sumEqualityBody));
  }

  void sparseCopyBody(PolyIRBuilder&, Block& body) {
    Let(body, "sp", NewContainer(MakeSparse(MakeInt(64))));
    StoreIndex(body, Local(body, "sp"), {IntConst(1), IntConst(2)}, IntConst(7));
    Let(body, "c", Copy(Local(body, "sp")));
    StoreIndex(body, Local(body, "sp"), {IntConst(1), IntConst(2)}, IntConst(9));
    Return(body, Index(Local(body, "c"), {IntConst(1), IntConst(2)}));
  }

  void structuralValues() {
    structEquality();
    structCopy();

    listEquality();
    listConcat();

    sumEquality();
    Check("sparse_copy", 7, MakeProgram(sparseCopyBody));
  }

  void truncateBody(PolyIRBuilder&, Block& body) {
    Let(body, "xs", VecLit(MakeDynamicArray(MakeInt(64), true), {IntConst(1), IntConst(2), IntConst(3), IntConst(4)}));
    Eval(body, Truncate(Local(body, "xs"), IntConst(2)));
    Eval(body, Truncate(Local(body, "xs"), IntConst(9)));
    Eval(body, Push(Local(body, "xs"), IntConst(7)));
    Return(body, Add(Mul(Len(Local(body, "xs")), IntConst(10)), Index(Local(body, "xs"), {IntConst(2)})));  // length 3, xs[2] = 7
  }

  void listTruncate() {
    Check("list_truncate", 37, MakeProgram(truncateBody));
  }

  // Fills a map past its first growth, removes every other key and reads back what is left (the cluster shifts must keep every survivor findable).
  // m[i] = 2 * i for i in 0..<200.
  void fillMap(Block& body) {
    ForRange(body, "i", IntConst(0), IntConst(200), [&](Block& loop) { StoreIndex(loop, Local(loop, "m"), {Local(loop, "i")}, Mul(Local(loop, "i"), IntConst(2))); });
  }

  void removeEvenKeys(Block& body) {
    ForRange(body, "j", IntConst(0), IntConst(200), [&](Block& loop) {
      IfThen(loop, Eq(Rem(Local(loop, "j"), IntConst(2)), IntConst(0)), [&](Block& then) { Eval(then, Remove(Local(then, "m"), Local(then, "j"))); });
    });
  }

  // An odd key `k` whose value is not 2 * k, or an even key still present.
  ExprPtr wrongEntry(Block& loop) {
    const ExprPtr odd = Eq(Rem(Local(loop, "k"), IntConst(2)), IntConst(1));
    const ExprPtr right = Eq(Index(Local(loop, "m"), {Local(loop, "k")}), Mul(Local(loop, "k"), IntConst(2)));

    return LogOr(LogAnd(odd, LogNot(right)), LogAnd(LogNot(odd), Has(Local(loop, "m"), Local(loop, "k"))));
  }

  void countWrongEntries(Block& body) {
    Var(body, "bad", MakeInt(64), IntConst(0));
    ForRange(body, "k", IntConst(0), IntConst(200), [&](Block& loop) {
      IfThen(loop, wrongEntry(loop), [&](Block& then) { SetLocal(then, "bad", Add(Local(then, "bad"), IntConst(1))); });
    });
  }

  void removeBody(PolyIRBuilder&, Block& body) {
    Let(body, "m", NewContainer(MakeHashmap(MakeInt(64), MakeInt(64), true), {IntConst(2)}));
    fillMap(body);
    removeEvenKeys(body);
    countWrongEntries(body);
    Return(body, Add(Local(body, "bad"), Len(Local(body, "m"))));
  }

  void removeFromMap() {
    Check("map_remove", 100, MakeProgram(removeBody));
  }

  void listBasics() {
    listPushAndIndex();
    listGrowsAndSums();
    listLiteralAndSetIndex();
    copyIsApart();
  }

  // Lists, buffers, grids, removing from a map and vecs.
  void containerTests() {
    listBasics();
    bufferAndGrid();
    outOfRangeStops();
    listTruncate();

    removeFromMap();
    vectors();
  }

  // Optionals, eithers and variants.
  void sumValueTests() {
    optionals();
    unwrapOfNoneStops();
    eithers();
    variants();
  }

  // Function values, closures and interface calls, then freeing sum values and closures.
  void functionValueTests() {
    functionValues();
    closuresCaptureByReference();
    interfaces();
    freeing();
  }

  // The builtins that are not about containers (strings, identity, atomics, shuffles and hashing) and the control flow
  // (exceptions, coroutines).
  void builtinAndControlFlowTests() {
    strings();
    identity();
    exceptions();
    atomicsShuffleHash();
    coroutines();
  }

}  // namespace

// The tests run in this order so their ok/FAIL lines print in the order they always have.
int main() {
  containerTests();
  sumValueTests();
  functionValueTests();

  builtinAndControlFlowTests();
  sparseCells();
  structuralValues();

  return Finish();
}
