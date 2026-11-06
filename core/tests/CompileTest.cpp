// Builds programs with PolyIRBuilder, compiles them to LLVM IR, has clang assemble and run them, and checks the exit status.
#include "polyir/Std.hpp"
#include "RunSupport.hpp"

using namespace polyir;
using namespace polyir::testing;

namespace {

  void gcdSteps(Block& loop) {
    Let(loop, "rest", Rem(Local(loop, "a"), Local(loop, "b")));
    SetLocal(loop, "a", Local(loop, "b"));
    SetLocal(loop, "b", Local(loop, "rest"));
  }

  void gcdBody(PolyIRBuilder&, Block& body) {
    Var(body, "a", MakeInt(64), IntConst(48));
    Var(body, "b", MakeInt(64), IntConst(18));
    WhileLoop(body, Ne(Local(body, "b"), IntConst(0)), [](Block& loop) { gcdSteps(loop); });
    Return(body, Local(body, "a"));
  }

  void gcdLoop() {
    Check("gcd", 6, MakeProgram(gcdBody));
  }

  void sumBody(PolyIRBuilder&, Block& body) {
    Var(body, "total", MakeInt(64), IntConst(0));
    ForRange(body, "i", IntConst(0), IntConst(10), [&](Block& loop) { SetLocal(loop, "total", Add(Local(loop, "total"), Local(loop, "i"))); });
    Return(body, Local(body, "total"));
  }

  void sumWithFor() {
    Check("sum_for", 45, MakeProgram(sumBody));
  }

  void fibMain(PolyIRBuilder& b, Block& body) {
    Return(body, Call(b, "fib", {IntConst(10)}));
  }

  void fibDefinition(PolyIRBuilder& b) {
    auto& fib = DefineFunction(b, "fib", {{"n", MakeInt(64)}}, MakeInt(64));
    Block& body = fib.body;
    IfThen(body, Lt(Local(body, "n"), IntConst(2)), [&](Block& small) { Return(small, Local(small, "n")); });
    Return(body, Add(Call(b, "fib", {Sub(Local(body, "n"), IntConst(1))}), Call(b, "fib", {Sub(Local(body, "n"), IntConst(2))})));
  }

  void recursion() {
    Check("fib", 55, MakeProgram(fibMain, fibDefinition));
  }

  void labelsStep(Block& body) {
    const TypePtr i64 = MakeInt(64);
    Label(body, "step", {{"t", i64}, {"k", i64}});
    Jump(body, "loop", {Add(Local(body, "t"), Local(body, "k")), Add(Local(body, "k"), IntConst(1))});
    Label(body, "done", {{"result", i64}});
    Return(body, Local(body, "result"));
  }

  void labelsBody(PolyIRBuilder&, Block& body) {
    const TypePtr i64 = MakeInt(64);
    Jump(body, "loop", {IntConst(0), IntConst(1)});
    Label(body, "loop", {{"total", i64}, {"i", i64}});
    Branch(body, Gt(Local(body, "i"), IntConst(10)), "done", {Local(body, "total")}, "step", {Local(body, "total"), Local(body, "i")});
    labelsStep(body);
  }

  void labels() {
    Check("labels", 55, MakeProgram(labelsBody));
  }

  void switchBody(PolyIRBuilder&, Block& body) {
    SwitchOn(body, IntConst(2), {{1, "one"}, {2, "two"}}, "other");

    Label(body, "one");
    Return(body, IntConst(101));

    Label(body, "two");
    Return(body, IntConst(203));

    Label(body, "other");
    Return(body, IntConst(9));
  }

  void switching() {
    Check("switch", 203, MakeProgram(switchBody));
  }

  void structsBody(PolyIRBuilder& b, Block& body) {
    Let(body, "p", StructLit(b, "Pt", {{"x", IntConst(3)}, {"y", IntConst(4)}}));
    SetField(body, Local(body, "p"), "x", IntConst(33));
    Return(body, Add(FieldAccess(b, Local(body, "p"), "x"), FieldAccess(b, Local(body, "p"), "y")));
  }

  void pointStruct(PolyIRBuilder& b) {
    DefineStruct(b, "Pt", {{"x", MakeInt(64), true}, {"y", MakeInt(64), false}});
  }

  void structs() {
    Check("structs", 37, MakeProgram(structsBody, pointStruct));
  }

  void globalsBody(PolyIRBuilder& b, Block& body) {
    Eval(body, Call(b, "puts", {StringConst("hello from polyir")}));
    Assign(body, Global(b, "counter"), Add(Global(b, "counter"), IntConst(2)));
    Return(body, Global(b, "counter"));
  }

  void counterAndPuts(PolyIRBuilder& b) {
    DefineGlobal(b, "counter", MakeInt(64), IntConst(5));
    DeclareExtern(b, "puts", {{"text", MakeString()}}, MakeInt(32));
  }

  void globalsAndStrings() {
    Check("globals", 7, MakeProgram(globalsBody, counterAndPuts));
  }

  void addOne(Block& then) {
    SetLocal(then, "result", Add(Local(then, "result"), IntConst(1)));
  }

  void wrappingLets(Block& body) {
    Let(body, "small", Add(IntConst(200, MakeUInt(8)), IntConst(100, MakeUInt(8))));
    Let(body, "big", Add(IntConst(2147483647, MakeInt(32)), IntConst(1, MakeInt(32))));
    Let(body, "least", Div(IntConst(-128, MakeInt(8)), IntConst(-1, MakeInt(8))));
  }

  void wrappingBody(PolyIRBuilder&, Block& body) {
    wrappingLets(body);
    Var(body, "result", MakeInt(64), Cast(Local(body, "small"), MakeInt(64)));
    IfThen(body, Lt(Local(body, "big"), IntConst(0, MakeInt(32))), [](Block& then) { addOne(then); });
    IfThen(body, Eq(Local(body, "least"), IntConst(-128, MakeInt(8))), [](Block& then) { addOne(then); });
    Return(body, Local(body, "result"));
  }

  void wrapping() {
    // u8 200 + 100 wraps to 44; i32 max + 1 wraps negative; -128 / -1 stays -128 in i8.
    Check("wrapping", 44 + 1 + 1, MakeProgram(wrappingBody));
  }

  void floatsBody(PolyIRBuilder&, Block& body) {
    Let(body, "x", Mul(FloatConst(3.5), FloatConst(4.0)));
    Return(body, Cast(Local(body, "x"), MakeInt(64)));
  }

  void floats() {
    Check("floats", 14, MakeProgram(floatsBody));
  }

  void shiftsBody(PolyIRBuilder&, Block& body) {
    Let(body, "a", Shl(IntConst(5), IntConst(2)));
    Return(body, BitOr(Local(body, "a"), Shr(IntConst(1), IntConst(66))));
  }

  void shifts() {
    Check("shifts", 20, MakeProgram(shiftsBody));
  }

  void shortCircuitBody(PolyIRBuilder& b, Block& body) {
    Let(body, "flag", LogAnd(BoolConst(false), Eq(Call(b, "bump", {}), IntConst(1))));
    Let(body, "other", LogOr(BoolConst(true), Eq(Call(b, "bump", {}), IntConst(1))));
    Return(body, Add(Global(b, "counter"), Cast(LogAnd(LogNot(Local(body, "flag")), Local(body, "other")), MakeInt(64))));
  }

  void bumpDefinition(PolyIRBuilder& b) {
    DefineGlobal(b, "counter", MakeInt(64), IntConst(0));
    auto& bump = DefineFunction(b, "bump", {}, MakeInt(64));
    Assign(bump.body, Global(b, "counter"), Add(Global(b, "counter"), IntConst(100)));
    Return(bump.body, Global(b, "counter"));
  }

  void shortCircuit() {
    // The right side must not run when the left decides: `bump` would add 100 to the counter.
    Check("short_circuit", 1, MakeProgram(shortCircuitBody, bumpDefinition));
  }

  bool mixedTypesRefused() {
    try {
      Add(IntConst(1), IntConst(1, MakeInt(32)));
    } catch (const BuildError&) {
      return true;
    }

    return false;
  }

  void mistakes() {
    if (!mixedTypesRefused()) {
      cout << "FAIL mistakes: adding an i64 to an i32 was accepted\n";
      failures++;
      return;
    }

    cout << "ok   mistakes\n";
  }

  void controlTests() {
    gcdLoop();
    sumWithFor();
    recursion();
    labels();
    switching();
  }

  void valueTests() {
    structs();
    globalsAndStrings();

    wrapping();
    floats();
    shifts();
    shortCircuit();
  }

}  // namespace

int main() {
  controlTests();
  valueTests();
  mistakes();

  return Finish();
}
