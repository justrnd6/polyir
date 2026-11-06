// The programs in docs/PolyIRBuilder.md, built exactly as the guide shows and run, so the guide stays true.
#include "RunSupport.hpp"

using namespace polyir;
using namespace polyir::testing;

namespace {

  // 1. A loop: 5! with a variable and a while.
  void factorialBody(PolyIRBuilder&, Block& body) {
    Var(body, "result", MakeInt(64), IntConst(1));
    Var(body, "n", MakeInt(64), IntConst(5));
    WhileLoop(body, Gt(Local(body, "n"), IntConst(1)), [&](Block& loop) {
      SetLocal(loop, "result", Mul(Local(loop, "result"), Local(loop, "n")));
      SetLocal(loop, "n", Sub(Local(loop, "n"), IntConst(1)));
    });
    Return(body, Local(body, "result"));
  }

  // 2. Functions: declare them, call them.
  void gcdDefinition(PolyIRBuilder& b) {
    auto& gcd = DefineFunction(b, "gcd", {{"a", MakeInt(64)}, {"b", MakeInt(64)}}, MakeInt(64));
    IfThen(gcd.body, Eq(Local(gcd.body, "b"), IntConst(0)), [&](Block& then) { Return(then, Local(then, "a")); });
    Return(gcd.body, Call(b, "gcd", {Local(gcd.body, "b"), Rem(Local(gcd.body, "a"), Local(gcd.body, "b"))}));
  }

  void gcdBody(PolyIRBuilder& b, Block& body) {
    Return(body, Call(b, "gcd", {IntConst(48), IntConst(18)}));
  }

  // 3. Structs.
  void pointDefinition(PolyIRBuilder& b) {
    DefineStruct(b, "Point", {{"x", MakeInt(64), false}, {"y", MakeInt(64), false}});
  }

  void pointBody(PolyIRBuilder& b, Block& body) {
    Let(body, "p", StructLit(b, "Point", {{"x", IntConst(3)}, {"y", IntConst(4)}}));
    Return(body, Add(FieldAccess(b, Local(body, "p"), "x"), FieldAccess(b, Local(body, "p"), "y")));
  }

  // 4. Lists.
  void listBody(PolyIRBuilder&, Block& body) {
    Let(body, "xs", NewContainer(MakeDynamicArray(MakeInt(64), true)));
    ForRange(body, "i", IntConst(0), IntConst(5), [&](Block& loop) { Eval(loop, Push(Local(loop, "xs"), Mul(Local(loop, "i"), Local(loop, "i")))); });
    Return(body, Add(Len(Local(body, "xs")), Index(Local(body, "xs"), {IntConst(4)})));
  }

  // 5. Maps.
  void mapBody(PolyIRBuilder&, Block& body) {
    Let(body, "ages", NewContainer(MakeHashmap(MakeString(), MakeInt(64), true), {IntConst(8)}));
    StoreIndex(body, Local(body, "ages"), {StringConst("ann")}, IntConst(31));
    StoreIndex(body, Local(body, "ages"), {StringConst("bo")}, IntConst(27));
    Return(body, Add(Index(Local(body, "ages"), {StringConst("ann")}), Index(Local(body, "ages"), {StringConst("bo")})));
  }

  // 6. Optionals.
  void optionalBody(PolyIRBuilder&, Block& body) {
    Let(body, "found", Some(IntConst(40)));
    Let(body, "missing", None(MakeInt(64)));
    IfThen(body, IsNone(Local(body, "missing")), [&](Block& then) { Return(then, Add(Unwrap(Local(body, "found")), IntConst(2))); });
    Return(body, IntConst(0));
  }

  // 7. Closures: a counter that shares the variable it captures.
  void counterBody(PolyIRBuilder&, Block& body) {
    Var(body, "count", MakeInt(64), IntConst(0));
    Let(body, "tick", ClosureLiteral(body, {}, MakeInt(64), [&](Block& inner) {
      SetLocal(inner, "count", Add(Local(inner, "count"), IntConst(1)));
      Return(inner, Local(inner, "count"));
    }));
    Eval(body, CallIndirect(Local(body, "tick"), {}));
    Return(body, Add(CallIndirect(Local(body, "tick"), {}), Local(body, "count")));
  }

  // 8. Exceptions.
  void tryBody(PolyIRBuilder&, Block& body) {
    Var(body, "result", MakeInt(64), IntConst(0));
    TryCatch(body, [&](Block& attempt) { ThrowValue(attempt, IntConst(7)); },
                  "code", [&](Block& handler) { SetLocal(handler, "result", Mul(Local(handler, "code"), IntConst(6))); });
    Return(body, Local(body, "result"));
  }

  // 9. Interfaces.
  void shapeDefinition(PolyIRBuilder& b) {
    DefineStruct(b, "Square", {{"side", MakeInt(64), false}});
    DefineInterface(b, "Shape", {{"area", {}, MakeInt(64)}});
    Implement(b, "Shape", "Square");
    auto& area = DefineFunction(b, "Square_area", {{"self", MakeStruct("Square")}}, MakeInt(64));
    Return(area.body, Mul(FieldAccess(b, Local(area.body, "self"), "side"), FieldAccess(b, Local(area.body, "self"), "side")));
  }

  void shapeBody(PolyIRBuilder& b, Block& body) {
    Let(body, "shape", InterfaceBox(b, "Shape", StructLit(b, "Square", {{"side", IntConst(7)}})));
    Return(body, InterfaceCall(b, Local(body, "shape"), "area", {}));
  }

  // 10. Mistakes are reported where they are made.
  bool refused(const Function<void()>& mistake, const String& wanted) {
    try {
      mistake();
    } catch (const BuildError& problem) {
      return String(problem.what()).find(wanted) != String::npos;
    }

    return false;
  }

  void mistakes() {
    PolyIRBuilder b;
    const bool types = refused([&] { Add(IntConst(1), IntConst(1, MakeInt(32))); }, "different types");
    const bool names = refused([&] { Call(b, "nothing", {}); }, "no function called 'nothing'");
    cout << (types && names ? "ok   mistakes are reported where they are made\n" : "FAIL mistakes\n");
    failures += types && names ? 0 : 1;
  }

  void basics() {
    Check("tour_factorial", 120, MakeProgram(factorialBody));
    Check("tour_gcd", 6, MakeProgram(gcdBody, gcdDefinition));
    Check("tour_struct", 7, MakeProgram(pointBody, pointDefinition));
    Check("tour_list", 21, MakeProgram(listBody));
    Check("tour_map", 58, MakeProgram(mapBody));
  }

  void programs() {
    basics();
    Check("tour_optional", 42, MakeProgram(optionalBody));
    Check("tour_closure", 4, MakeProgram(counterBody));
    Check("tour_try", 42, MakeProgram(tryBody));
    Check("tour_interface", 49, MakeProgram(shapeBody, shapeDefinition));
  }

  // 11. A sub-builder: part of a program built on its own, then added to the module.
  void addNormHelper(PolyIRBuilder& b) {
    PolyIRBuilder helpers = SubBuilder(b);
    auto& norm = DefineFunction(helpers, "norm1", {{"p", MakeStruct("Point")}}, MakeInt(64));
    Return(norm.body, Add(FieldAccess(helpers, Local(norm.body, "p"), "x"), FieldAccess(helpers, Local(norm.body, "p"), "y")));
    Adopt(b, helpers);
  }

  Module withSubBuilder() {
    PolyIRBuilder b;
    DefineStruct(b, "Point", {{"x", MakeInt(64), false}, {"y", MakeInt(64), false}});
    addNormHelper(b);
    auto& entry = DefineFunction(b, "main", {}, MakeInt(64));
    Return(entry.body, Call(b, "norm1", {StructLit(b, "Point", {{"x", IntConst(30)}, {"y", IntConst(12)}})}));

    return Build(b);
  }

  bool sameNameRefused() {
    PolyIRBuilder b;
    PolyIRBuilder one = SubBuilder(b);
    PolyIRBuilder two = SubBuilder(b);

    Return(DefineFunction(one, "same", {}, MakeInt(64)).body, IntConst(1));
    Return(DefineFunction(two, "same", {}, MakeInt(64)).body, IntConst(2));
    Adopt(b, one);

    return refused([&] { Adopt(b, two); }, "function same is already defined");
  }

  void subBuilders() {
    Check("tour_sub_builder", 42, withSubBuilder());
    const bool refusedTwice = sameNameRefused();
    cout << (refusedTwice ? "ok   the same name in two sub-builders is reported\n" : "FAIL sub-builder clash\n");
    failures += refusedTwice ? 0 : 1;
  }

  // 12. A function compiled on its own, as LLVM text, added to another module.
  void addSafeDiv(PolyIRBuilder& h) {
    auto& f = DefineFunction(h, "safe_div", {{"a", MakeInt(64)}, {"b", MakeInt(64)}}, MakeInt(64));
    Var(f.body, "r", MakeInt(64), IntConst(-1));
    TryCatch(f.body, [&](Block& attempt) {
      IfThen(attempt, Eq(Local(f.body, "b"), IntConst(0)), [&](Block& then) { ThrowValue(then, IntConst(1)); });
      SetLocal(attempt, "r", Div(Local(f.body, "a"), Local(f.body, "b")));
    }, "code", [&](Block&) {});
    Return(f.body, Local(f.body, "r"));
  }

  FunctionIr compiledAlone() {
    PolyIRBuilder h;
    addSafeDiv(h);
    Return(DefineFunction(h, "label_length", {}, MakeInt(64)).body, Len(StringConst("fragment")));

    return CompileFunctionToLlvm(Build(h), "safe_div");
  }

  Module withFragment() {
    const FunctionIr alone = compiledAlone();
    PolyIRBuilder b;
    DeclareFunction(b, "safe_div", {{"a", MakeInt(64)}, {"b", MakeInt(64)}}, MakeInt(64));
    AddLlvm(b, alone.text, alone.declarations);

    auto& entry = DefineFunction(b, "main", {}, MakeInt(64));
    Return(entry.body, Add(Call(b, "safe_div", {IntConst(84), IntConst(2)}), Call(b, "safe_div", {IntConst(1), IntConst(0)})));

    return Build(b);
  }

  void fragments() {
    Check("tour_fragment", 41, withFragment());
    const FunctionIr missing = CompileFunctionToLlvm(Build(PolyIRBuilder()), "nothing");
    cout << (!Ok(missing) ? "ok   a missing function is reported\n" : "FAIL missing function\n");
    failures += Ok(missing) ? 1 : 0;
  }

}  // namespace

int main() {
  programs();
  subBuilders();
  fragments();
  mistakes();

  return Finish();
}
