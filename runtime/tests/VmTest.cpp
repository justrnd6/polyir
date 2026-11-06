// The bytecode VM (polyir/Vm.hpp) on programs built with PolyIRBuilder: each is run with RunModule and its result, its output or its trap is
// checked to the letter; a program the VM cannot run is declined before anything runs. tests/vm_vs_native.sh runs the real example
// programs on the VM and as LLVM builds.
#include "polyir/Std.hpp"
#include "polyir/Builder.hpp"
#include "polyir/Pbc.hpp"
#include "polyir/Vm.hpp"

using namespace polyir;

namespace {

  int failures = 0;

  void pass(const String& name, const String& got) {
    cout << "ok   " << name << " = " << got << "\n";
  }

  void fail(const String& name, const String& want, const String& got) {
    cout << "FAIL " << name << ": expected " << want << ", got " << got << "\n";
    failures++;
  }

  String shown(const VmOutcome& ran) {
    return ran.ok ? ran.result + " (exit " + to_string(ran.exitStatus) + ")" : "trap: " + ran.trap;
  }

  VmOutcome runEntry(const Module& program, const String& entry, const Vector<String>& args, OStream& out) {
    VmOptions options;
    options.entry = entry;
    options.args = args;
    options.out = &out;

    return RunModule(program, options);
  }

  // The entry's value must read as `want` and be the exit status when it is a whole number.
  void expectValue(const String& name, const Module& program, const String& want, int64_t status, const String& entry = "main", const Vector<String>& args = {}) {
    OStringStream out;
    const VmOutcome ran = runEntry(program, entry, args, out);

    if (ran.ok && ran.result == want && ran.exitStatus == status) {
      pass(name, shown(ran));
    } else {
      fail(name, want + " (exit " + to_string(status) + ")", shown(ran));
    }
  }

  void expectTrap(const String& name, const Module& program, const String& want) {
    OStringStream out;
    const VmOutcome ran = runEntry(program, "main", {}, out);

    if (!ran.ok && ran.trap == want && ran.exitStatus == 1) {
      pass(name, shown(ran));
    } else {
      fail(name, "trap: " + want, shown(ran));
    }
  }

  // func main() -> i64 { <body> }, after `before` has declared what it needs.
  Module program(const Function<void(PolyIRBuilder&, Block&)>& body, const Function<void(PolyIRBuilder&)>& before = [](PolyIRBuilder&) {}) {
    PolyIRBuilder b;
    before(b);
    auto& main = DefineFunction(b, "main", {}, MakeInt(64));
    body(b, main.body);

    return Build(b);
  }

  // 1. gcd with labels, goto and branch (examples/labels.poly), called as the entry with its arguments parsed from words.
  void gcdStep(Block& body) {
    const TypePtr i64 = MakeInt(64);
    Label(body, "step", {{"x", i64}, {"y", i64}});
    Jump(body, "loop", {Local(body, "y"), Rem(Local(body, "x"), Local(body, "y"))});
    Label(body, "done", {{"result", i64}});
    Return(body, Local(body, "result"));
  }

  void gcdLabels(PolyIRBuilder&, Block& body) {
    const TypePtr i64 = MakeInt(64);
    Jump(body, "loop", {Local(body, "a0"), Local(body, "b0")});
    Label(body, "loop", {{"a", i64}, {"b", i64}});
    Branch(body, Eq(Local(body, "b"), IntConst(0)), "done", {Local(body, "a")}, "step", {Local(body, "a"), Local(body, "b")});
    gcdStep(body);
  }

  Module gcdProgram() {
    PolyIRBuilder b;
    auto& gcd = DefineFunction(b, "gcd", {{"a0", MakeInt(64)}, {"b0", MakeInt(64)}}, MakeInt(64));
    gcdLabels(b, gcd.body);

    return Build(b);
  }

  void gcd() {
    expectValue("gcd", gcdProgram(), "6", 6, "gcd", {"48", "18"});
    OStringStream out;
    const VmOutcome wrong = runEntry(gcdProgram(), "gcd", {"48"}, out);

    if (!wrong.ok && wrong.trap == "the entry takes 2 argument(s), got 1") {
      pass("gcd argument count", wrong.trap);
    } else {
      fail("gcd argument count", "the entry takes 2 argument(s), got 1", shown(wrong));
    }
  }

  // 2. A closure capturing a var: the counter shares `count` with the code around it (docs/PolyIRBuilder.md).
  void counterBody(PolyIRBuilder&, Block& body) {
    Var(body, "count", MakeInt(64), IntConst(0));
    Let(body, "tick", ClosureLiteral(body, {}, MakeInt(64), [&](Block& inner) {
      SetLocal(inner, "count", Add(Local(inner, "count"), IntConst(1)));
      Return(inner, Local(inner, "count"));
    }));
    Eval(body, CallIndirect(Local(body, "tick"), {}));
    Return(body, Add(CallIndirect(Local(body, "tick"), {}), Local(body, "count")));
  }

  // 3. Exceptions: a catch, a cleanup that lets the throw go on, and a throw nobody catches.
  void caughtBody(PolyIRBuilder&, Block& body) {
    Var(body, "r", MakeInt(64), IntConst(0));
    TryCatch(body, [&](Block& attempt) {
      ThrowValue(attempt, IntConst(7));
      SetLocal(attempt, "r", IntConst(99));
    }, "code", [&](Block& handler) { SetLocal(handler, "r", Add(Local(handler, "code"), IntConst(1))); });
    Return(body, Local(body, "r"));
  }

  // 7 thrown, the cleanup sets 100, the catch adds the code: 107.
  void cleanupBody(PolyIRBuilder&, Block& body) {
    Var(body, "r", MakeInt(64), IntConst(0));
    TryCatch(body, [&](Block& outerTry) {
      TryCleanup(outerTry, [&](Block& inside) { ThrowValue(inside, IntConst(7)); }, [&](Block& cleanup) { SetLocal(cleanup, "r", IntConst(100)); });
    }, "code", [&](Block& handler) { SetLocal(handler, "r", Add(Local(handler, "r"), Local(handler, "code"))); });
    Return(body, Local(body, "r"));
  }

  void uncaughtBody(PolyIRBuilder&, Block& body) {
    ThrowValue(body, IntConst(3));
    Return(body, IntConst(0));
  }

  // 4. A coroutine resumed three times: 0 and 10 come out as case 0, the third resume is case 1 (done) and `done` says so.
  void countCoroutine(PolyIRBuilder& b) {
    auto& gen = DefineCoroutine(b, "count", {{"n", MakeInt(64)}}, MakeInt(64));
    ForRange(gen.body, "i", IntConst(0), Local(gen.body, "n"), [&](Block& loop) { YieldValue(loop, Mul(Local(loop, "i"), IntConst(10))); });
  }

  ExprPtr resumedValues(Block& body) {
    return Add(VariantUnwrap(Local(body, "first"), 0), VariantUnwrap(Local(body, "second"), 0));
  }

  ExprPtr finishedFlags(Block& body) {
    const ExprPtr finished = Mul(Cast(VariantIsCase(Local(body, "last"), 1), MakeInt(64)), IntConst(100));

    return Add(finished, Mul(Cast(Done(Local(body, "c")), MakeInt(64)), IntConst(5)));
  }

  void coroutineBody(PolyIRBuilder& b, Block& body) {
    Let(body, "c", Call(b, "count", {IntConst(2)}));

    for (const char* step : {"first", "second", "last"}) {
      Let(body, step, Resume(Local(body, "c")));
    }

    Return(body, Add(resumedValues(body), finishedFlags(body)));
  }

  // 5. A mutmap: entries set, one removed, membership asked; the frozen map's `with` is a bigger copy that leaves it alone.
  void mapEntries(Block& body) {
    Let(body, "m", NewContainer(MakeHashmap(MakeInt(64), MakeInt(64), true), {IntConst(8)}));

    for (int64_t k : {1, 2, 3}) {
      StoreIndex(body, Local(body, "m"), {IntConst(k)}, IntConst(k * 10));
    }

    Let(body, "removed", Remove(Local(body, "m"), IntConst(2)));
    Let(body, "bigger", With(Freeze(Local(body, "m")), {IntConst(4), IntConst(40)}));
  }

  ExprPtr membership(Block& body) {
    const ExprPtr first = Cast(Has(Local(body, "m"), IntConst(1)), MakeInt(64));
    const ExprPtr second = Mul(Cast(Has(Local(body, "m"), IntConst(2)), MakeInt(64)), IntConst(2));

    return Add(Add(first, second), Mul(Cast(Local(body, "removed"), MakeInt(64)), IntConst(4)));
  }

  // 2 entries left (200), 3 in the copy (30), key 1 present (1), key 2 gone (0), removed (4): 235.
  void mapBody(PolyIRBuilder&, Block& body) {
    mapEntries(body);
    const ExprPtr sizes = Add(Mul(Len(Local(body, "m")), IntConst(100)), Mul(Len(Local(body, "bigger")), IntConst(10)));
    Return(body, Add(sizes, membership(body)));
  }

  // 6. Strings: concat, slice, find, str, and print to the given stream. The entry returns a string, so the exit status is 0.
  ExprPtr sliceOf(Block& body, int64_t from, int64_t to) {
    return BuiltinCall(Builtin::Slice, Local(body, "s"), {IntConst(from), IntConst(to)}, MakeString());
  }

  void stringBody(PolyIRBuilder&, Block& body) {
    Let(body, "s", Concat(StringConst("hello"), StringConst(", world")));
    Eval(body, BuiltinCall(Builtin::Print, Local(body, "s"), {}, MakeVoid()));
    Let(body, "at", BuiltinCall(Builtin::Find, Local(body, "s"), {StringConst("world"), IntConst(0)}, MakeInt(64)));
    Let(body, "shown", BuiltinCall(Builtin::StrOf, Local(body, "at"), {}, MakeString()));
    Return(body, Concat(sliceOf(body, 7, 12), Concat(StringConst("@"), Local(body, "shown"))));
  }

  Module stringProgram() {
    PolyIRBuilder b;
    auto& main = DefineFunction(b, "main", {}, MakeString());
    stringBody(b, main.body);

    return Build(b);
  }

  void strings() {
    OStringStream out;
    const VmOutcome ran = runEntry(stringProgram(), "main", {}, out);

    if (ran.ok && ran.result == "world@7" && ran.exitStatus == 0 && out.str() == "hello, world\n") {
      pass("strings", ran.result + " printed " + out.str());
    } else {
      fail("strings", "world@7 with 'hello, world' printed", shown(ran) + " printed '" + out.str() + "'");
    }
  }

  // 7. A struct literal, a field assigned, and == comparing the fields: equal after the assignment (100) plus the new x (33).
  void pointDefinition(PolyIRBuilder& b) {
    DefineStruct(b, "Pt", {{"x", MakeInt(64), true}, {"y", MakeInt(64), false}});
  }

  void structBody(PolyIRBuilder& b, Block& body) {
    Let(body, "p", StructLit(b, "Pt", {{"x", IntConst(3)}, {"y", IntConst(4)}}));
    Let(body, "q", StructLit(b, "Pt", {{"x", IntConst(33)}, {"y", IntConst(4)}}));
    Let(body, "before", Cast(Eq(Local(body, "p"), Local(body, "q")), MakeInt(64)));

    SetField(body, Local(body, "p"), "x", IntConst(33));
    const ExprPtr after = Mul(Cast(Eq(Local(body, "p"), Local(body, "q")), MakeInt(64)), IntConst(100));
    Return(body, Add(Add(after, Mul(Local(body, "before"), IntConst(1000))), FieldAccess(b, Local(body, "p"), "x")));
  }

  // 8. A for loop adding its counter to a global: 5 + 0 + 1 + ... + 9 = 50.
  void totalGlobal(PolyIRBuilder& b) {
    DefineGlobal(b, "total", MakeInt(64), IntConst(5));
  }

  void forBody(PolyIRBuilder& b, Block& body) {
    ForRange(body, "i", IntConst(0), IntConst(10), [&](Block& loop) { Assign(loop, Global(b, "total"), Add(Global(b, "total"), Local(loop, "i"))); });
    Return(body, Global(b, "total"));
  }

  // 9. Traps: an index outside its list, and an entry that does not exist.
  void outOfBoundsBody(PolyIRBuilder&, Block& body) {
    Let(body, "xs", VecLit(MakeDynamicArray(MakeInt(64), false), {IntConst(1), IntConst(2)}));
    Return(body, Index(Local(body, "xs"), {IntConst(5)}));
  }

  void missingEntry() {
    OStringStream out;
    const VmOutcome ran = runEntry(program(forBody, totalGlobal), "nothing", {}, out);

    if (!ran.ok && ran.trap == "entry point 'nothing' not found") {
      pass("missing entry", ran.trap);
    } else {
      fail("missing entry", "entry point 'nothing' not found", shown(ran));
    }
  }

  // 10. Preflight: an extern nothing binds declines the run before anything runs (main's `puts` never prints), with status 125.
  Module unboundExternProgram() {
    PolyIRBuilder b;
    DeclareExtern(b, "puts", {{"text", MakeString()}}, MakeInt(32));
    DeclareExtern(b, "no_such_function_anywhere", {}, MakeInt(64));

    auto& main = DefineFunction(b, "main", {}, MakeInt(64));
    Eval(main.body, Call(b, "puts", {StringConst("ran")}));
    Return(main.body, IntConst(0));

    return Build(b);
  }

  void declined() {
    OStringStream out;
    const VmOutcome ran = runEntry(unboundExternProgram(), "main", {}, out);
    const String want = "'no_such_function_anywhere' has no binding on the VM";

    if (ran.declined && !ran.ok && ran.declineReason == want && ran.exitStatus == 125 && out.str().empty()) {
      pass("declined", ran.declineReason);
    } else {
      fail("declined", want + " (exit 125, nothing printed)", shown(ran) + " printed '" + out.str() + "'");
    }
  }

  // The options that run `gcd 48 18`, printing to `out`.
  VmOptions gcdOptions(OStream& out) {
    VmOptions options;
    options.entry = "gcd";
    options.args = {"48", "18"};
    options.out = &out;

    return options;
  }

  // 11. .pbc bytes in memory: RunBytecode runs them; bytes that are not a .pbc are a trap.
  void bytecode() {
    OStringStream out;
    const VmOutcome ran = RunBytecode(WritePbc(gcdProgram()), gcdOptions(out));

    if (ran.ok && ran.result == "6") {
      pass("bytecode", shown(ran));
    } else {
      fail("bytecode", "6", shown(ran));
    }
  }

  void badBytecode() {
    const VmOutcome ran = RunBytecode({1, 2, 3}, VmOptions{});
    const bool trapped = !ran.ok && !ran.declined && ran.trap.rfind("the bytecode is not a .pbc file", 0) == 0;

    if (trapped) {
      pass("bad bytecode", ran.trap);
    } else {
      fail("bad bytecode", "trap: the bytecode is not a .pbc file: ...", shown(ran));
    }
  }

  // What a C++17 host sees through RunPbcBytesEntry: nothing on success, one line on the error stream for a decline or a trap.
  void expectHostRun(const String& name, const Module& program, int wantStatus, const String& wantError) {
    const Vector<uint8_t> bytes = WritePbc(program);
    OStringStream out, err;
    const int status = RunPbcBytesEntry(bytes.data(), static_cast<long>(bytes.size()), "main", {}, out, err);
    const String got = to_string(status) + " " + err.str();

    if (status == wantStatus && err.str() == wantError) {
      pass(name, got);
    } else {
      fail(name, to_string(wantStatus) + " " + wantError, got);
    }
  }

  // What a C++17 host that shows the entry's value sees through RunPbcBytesOutcome: one line saying how the run went.
  void expectOutcome(const String& name, const Module& program, const String& want) {
    const Vector<uint8_t> bytes = WritePbc(program);
    OStringStream out;
    const String got = RunPbcBytesOutcome(bytes.data(), static_cast<long>(bytes.size()), "main", {}, out);

    if (got == want) {
      pass(name, got);
    } else {
      fail(name, want, got);
    }
  }

  void hostOutcomes() {
    expectOutcome("outcome value", program(forBody, totalGlobal), "value: 50");
    expectOutcome("outcome declined", unboundExternProgram(), "declined: 'no_such_function_anywhere' has no binding on the VM");
    expectOutcome("outcome trap", program(outOfBoundsBody), "trap: index 5 is out of bounds (length 2)");
  }

  void hostEntries() {
    expectHostRun("host run", program(forBody, totalGlobal), 50, "");
    expectHostRun("host run declined", unboundExternProgram(), 125, "declined: 'no_such_function_anywhere' has no binding on the VM\n");
    expectHostRun("host run trap", program(outOfBoundsBody), 1, "trap: index 5 is out of bounds (length 2)\n");
  }

  void controlPrograms() {
    gcd();
    expectValue("closure", program(counterBody), "4", 4);

    expectValue("catch", program(caughtBody), "8", 8);
    expectValue("cleanup", program(cleanupBody), "107", 107);
    expectTrap("uncaught throw", program(uncaughtBody), "uncaught throw 3");

    expectValue("coroutine", program(coroutineBody, countCoroutine), "115", 115);
  }

  void valuePrograms() {
    expectValue("mutmap", program(mapBody), "235", 235);
    strings();
    expectValue("struct", program(structBody, pointDefinition), "133", 133);

    expectValue("for with global", program(forBody, totalGlobal), "50", 50);
    expectTrap("index out of bounds", program(outOfBoundsBody), "index 5 is out of bounds (length 2)");
    missingEntry();
  }

  // The listing names what each immediate stands for: the callee and its argument count, the operator and its types.
  void expectListed(const String& name, const String& listing, const String& line) {
    if (listing.find(line) != String::npos) {
      pass(name, line);
    } else {
      fail(name, line, listing);
    }
  }

  void listing() {
    const String text = BytecodeListing(gcdProgram());
    expectListed("listing header", text, "function gcd  (params 2, slots ");
    expectListed("listing operator", text, "binary        Rem i64 -> i64");
    expectListed("listing bound slot", text, "loadBound     s");
    expectListed("listing unbound marker", text, "pushConst     unbound");
  }

  // 12. Memory (src/vm/Collector.cpp, docs/memory.md): garbage is reclaimed, what is still used is kept, and a heap that really keeps
  // everything stops at the ceiling. `peakObjects` is the most objects the heap held at once.
  VmOutcome runWith(const Module& program, VmOptions options) {
    OStringStream out;
    options.out = &out;

    return RunModule(program, options);
  }

  String memoryShown(const VmOutcome& ran) {
    return shown(ran) + ", peak " + to_string(ran.peakObjects) + " objects, " + to_string(ran.collections) + " collections";
  }

  // The value must be `want` and the heap must never have held more than `peakBound` objects.
  void expectBounded(const String& name, const VmOutcome& ran, const String& want, int64_t peakBound) {
    if (ran.ok && ran.result == want && ran.peakObjects <= peakBound && ran.collections > 0) {
      pass(name, memoryShown(ran));
    } else {
      fail(name, want + " with a peak of at most " + to_string(peakBound) + " objects", memoryShown(ran));
    }
  }

  constexpr int64_t kChurnIterations = 1000000;

  // Every iteration makes a struct, a list, a map and a closure (plus the boxes they need) and drops them all. Reading them back
  // gives 1 + 2 + 3 + 2 a round.
  void churnRound(PolyIRBuilder& b, Block& loop) {
    Let(loop, "p", StructLit(b, "Pt", {{"x", Local(loop, "i")}, {"y", IntConst(1)}}));
    Let(loop, "xs", VecLit(MakeDynamicArray(MakeInt(64), true), {Local(loop, "i"), IntConst(2)}));
    Let(loop, "m", NewContainer(MakeHashmap(MakeInt(64), MakeInt(64), true), {IntConst(4)}));
    StoreIndex(loop, Local(loop, "m"), {Local(loop, "i")}, IntConst(3));
    Let(loop, "f", ClosureLiteral(loop, {}, MakeInt(64), [&](Block& inner) { Return(inner, Add(FieldAccess(b, Local(inner, "p"), "y"), IntConst(1))); }));

    const ExprPtr round = Add(Add(FieldAccess(b, Local(loop, "p"), "y"), Index(Local(loop, "xs"), {IntConst(1)})), Add(Index(Local(loop, "m"), {Local(loop, "i")}), CallIndirect(Local(loop, "f"), {})));
    SetLocal(loop, "total", Rem(Add(Local(loop, "total"), Add(round, FieldAccess(b, Local(loop, "p"), "x"))), IntConst(1000003)));
  }

  void churnBody(PolyIRBuilder& b, Block& body) {
    Var(body, "total", MakeInt(64), IntConst(0));
    ForRange(body, "i", IntConst(0), IntConst(kChurnIterations), [&](Block& loop) { churnRound(b, loop); });
    Return(body, Local(body, "total"));
  }

  // What churnBody computes, worked out here.
  String churnTotal() {
    int64_t total = 0;

    for (int64_t i = 0; i < kChurnIterations; i++) {
      total = (total + 8 + i) % 1000003;
    }

    return to_string(total);
  }

  // Millions of objects made, a few live at a time: the heap stays near the collector's minimum threshold (65,536) instead of growing.
  void churn() {
    expectBounded("garbage is reclaimed", runWith(program(churnBody, pointDefinition), VmOptions{}), churnTotal(), 70000);
  }

  // Two nodes that refer to each other and that nothing else reaches, made 200,000 times: mark and sweep reclaims cycles.
  void nodeDefinition(PolyIRBuilder& b) {
    DefineStruct(b, "Node", {{"id", MakeInt(64), false}, {"peer", MakeOptional(MakeStruct("Node")), true}});
    auto& pair = DefineFunction(b, "pair", {{"i", MakeInt(64)}}, MakeInt(64));
    Block& body = pair.body;

    Let(body, "a", StructLit(b, "Node", {{"id", Local(body, "i")}, {"peer", None(MakeStruct("Node"))}}));
    Let(body, "z", StructLit(b, "Node", {{"id", IntConst(-1)}, {"peer", Some(Local(body, "a"))}}));
    SetField(body, Local(body, "a"), "peer", Some(Local(body, "z")));
    Return(body, FieldAccess(b, Unwrap(FieldAccess(b, Unwrap(FieldAccess(b, Local(body, "a"), "peer")), "peer")), "id"));  // a.peer.peer is a
  }

  void cycleBody(PolyIRBuilder& b, Block& body) {
    Var(body, "count", MakeInt(64), IntConst(0));
    ForRange(body, "i", IntConst(0), IntConst(200000), [&](Block& loop) {
      IfThen(loop, Eq(Call(b, "pair", {Local(loop, "i")}), Local(loop, "i")), [&](Block& then) { SetLocal(then, "count", Add(Local(then, "count"), IntConst(1))); });
    });
    Return(body, Local(body, "count"));
  }

  // The same cycles under a ceiling of 1,000 objects: garbage never counts against the ceiling, only what is still reachable does. The
  // heap passes the ceiling only by the one object whose allocation sets the collection off.
  void cycles() {
    expectBounded("cycles are reclaimed", runWith(program(cycleBody, nodeDefinition), VmOptions{}), "200000", 70000);
    VmOptions tight;
    tight.heapCeiling = 1000;
    expectBounded("garbage under a small ceiling", runWith(program(cycleBody, nodeDefinition), tight), "200000", 1001);
  }

  // A coroutine that keeps a list in its own frame across its yields and makes garbage in between.
  void garbage(Block& body) {
    ForRange(body, "k", IntConst(0), IntConst(10), [&](Block& loop) { Eval(loop, VecLit(MakeDynamicArray(MakeInt(64), true), {Local(loop, "k")})); });
  }

  void keeperCoroutine(PolyIRBuilder& b) {
    auto& keeper = DefineCoroutine(b, "keeper", {}, MakeInt(64));
    Let(keeper.body, "kept", VecLit(MakeDynamicArray(MakeInt(64), true), {IntConst(40)}));

    garbage(keeper.body);
    YieldValue(keeper.body, Index(Local(keeper.body, "kept"), {IntConst(0)}));

    garbage(keeper.body);
    YieldValue(keeper.body, Add(Index(Local(keeper.body, "kept"), {IntConst(0)}), IntConst(2)));
  }

  // `once` resumes a coroutine nothing else refers to (only the resume's pin keeps it); `held` lives in main across every resume.
  void keeperBody(PolyIRBuilder& b, Block& body) {
    Let(body, "held", VecLit(MakeDynamicArray(MakeInt(64), true), {IntConst(7)}));
    Let(body, "once", VariantUnwrap(Resume(Call(b, "keeper", {})), 0));

    Let(body, "c", Call(b, "keeper", {}));
    Let(body, "first", VariantUnwrap(Resume(Local(body, "c")), 0));
    Let(body, "second", VariantUnwrap(Resume(Local(body, "c")), 0));
    Return(body, Add(Add(Local(body, "once"), Local(body, "first")), Add(Local(body, "second"), Index(Local(body, "held"), {IntConst(0)}))));
  }

  // 40 + 40 + 42 + 7, with a collection between every two instructions (inside the coroutines too).
  void coroutineRoots() {
    VmOptions stress;
    stress.collectEverySafePoint = true;
    expectBounded("coroutine roots under stress", runWith(program(keeperBody, keeperCoroutine), stress), "129", 100);
  }

  // A program that keeps every struct it makes in a list stops at the ceiling with the trap.
  void keepAllBody(PolyIRBuilder& b, Block& body) {
    Let(body, "all", VecLit(MakeDynamicArray(MakeStruct("Pt"), true), {}));
    ForRange(body, "i", IntConst(0), IntConst(5000), [&](Block& loop) {
      Eval(loop, Push(Local(loop, "all"), StructLit(b, "Pt", {{"x", Local(loop, "i")}, {"y", IntConst(0)}})));
    });
    Return(body, Len(Local(body, "all")));
  }

  bool isOutOfMemory(const VmOutcome& ran) {
    const String tail = " objects are still reachable (limit 1000)";
    const bool ends = ran.trap.size() > tail.size() && ran.trap.compare(ran.trap.size() - tail.size(), tail.size(), tail) == 0;

    return !ran.ok && ran.exitStatus == 1 && ran.trap.rfind("out of memory: ", 0) == 0 && ends;
  }

  void ceiling() {
    VmOptions tight;
    tight.heapCeiling = 1000;
    const VmOutcome ran = runWith(program(keepAllBody, pointDefinition), tight);

    if (isOutOfMemory(ran)) {
      pass("ceiling", ran.trap);
    } else {
      fail("ceiling", "trap: out of memory: N objects are still reachable (limit 1000)", shown(ran));
    }
  }

  // String identity is a counter, not a heap cell: a string is identical to itself, not to an equal string made apart from it.
  void identityBody(PolyIRBuilder&, Block& body) {
    Let(body, "a", Concat(StringConst("ab"), StringConst("c")));
    Let(body, "same", Local(body, "a"));
    Let(body, "other", Concat(StringConst("ab"), StringConst("c")));

    const ExprPtr itself = Cast(Identical(Local(body, "a"), Local(body, "same")), MakeInt(64));
    const ExprPtr apart = Mul(Cast(Identical(Local(body, "a"), Local(body, "other")), MakeInt(64)), IntConst(2));
    Return(body, Add(Add(itself, apart), Mul(Cast(StrEq(Local(body, "a"), Local(body, "other")), MakeInt(64)), IntConst(4))));
  }

  // The incremental collector (docs/memory.md, Part 3). Stopping the world, every collection is one pause; incremental, a cycle is many
  // slices (and the same garbage goes, in about the same heap).
  String pausesShown(const VmOutcome& ran) {
    return memoryShown(ran) + ", " + to_string(ran.pauses) + " pauses, longest " + to_string(ran.longestPauseNs / 1000) + " us, p99 " +
           to_string(ran.p99PauseNs / 1000) + " us";
  }

  // POLYIR_GC_MODE or POLYIR_VM_STRESS_GC in the environment (a whole suite run in one mode) decide the mode over VmOptions. The checks of
  // one mode then only check the answer.
  bool modeForced() {
    return getenv("POLYIR_GC_MODE") != nullptr || getenv("POLYIR_VM_STRESS_GC") != nullptr;
  }

  void stopTheWorld() {
    VmOptions stop;
    stop.incrementalCollector = false;
    const VmOutcome ran = runWith(program(churnBody, pointDefinition), stop);
    const bool good = ran.ok && ran.result == churnTotal() && (modeForced() || (ran.pauses == ran.collections && ran.peakObjects <= 70000));

    if (good) {
      pass("stop-the-world collections", pausesShown(ran));
    } else {
      fail("stop-the-world collections", "one pause per collection", pausesShown(ran));
    }
  }

  void slicedCycles() {
    const VmOutcome ran = runWith(program(churnBody, pointDefinition), VmOptions{});
    const bool good = ran.ok && ran.result == churnTotal() && (modeForced() || ran.pauses > 2 * ran.collections) && ran.p99PauseNs <= ran.longestPauseNs;

    if (good) {
      pass("incremental cycles are sliced", pausesShown(ran));
    } else {
      fail("incremental cycles are sliced", "more pauses than collections", pausesShown(ran));
    }
  }

  // The incremental stress mode on the programs above: garbage, cycles, and coroutines whose frames change between slices. The mode
  // runs a slice of one unit at every safe point, and checks the marks at the end of every marking.
  void incrementalStress() {
    VmOptions verify;
    verify.verifyIncremental = true;
    expectBounded("garbage under incremental stress", runWith(program(churnBody, pointDefinition), verify), churnTotal(), 100);
    expectBounded("cycles under incremental stress", runWith(program(cycleBody, nodeDefinition), verify), "200000", 100);
    expectBounded("coroutines under incremental stress", runWith(program(keeperBody, keeperCoroutine), verify), "129", 100);
  }

  void incrementalPrograms() {
    stopTheWorld();
    slicedCycles();
    incrementalStress();
  }

  void memoryPrograms() {
    churn();
    cycles();
    coroutineRoots();
    ceiling();

    expectValue("string identity", program(identityBody), "5", 5);
    incrementalPrograms();
  }

  // 13. Native memory (src/vm/NativeMemory.cpp): a `ptr<T>` holding an address from C's calloc is indexed with T's width, as the LLVM build
  // does. The atomics work on the same bytes. A C function of the libraries takes and returns f32 as single precision.
  void callocDeclaration(PolyIRBuilder& b) {
    DeclareExtern(b, "calloc", {{"count", MakeInt(64)}, {"size", MakeInt(64)}}, MakeInt(64));
  }

  // 64 zeroed bytes from C, seen as a `ptr<element>` (the address is the same whole number, retyped).
  ExprPtr nativeBlock(const PolyIRBuilder& b, const TypePtr& element) {
    return BuiltinCall(Builtin::Retype, Call(b, "calloc", {IntConst(8), IntConst(8)}), {}, MakePointer(element));
  }

  // p[1] = value through a ptr<element>, then the 64-bit word `word` of the block: the bytes the store wrote, where it wrote them.
  Module storedBitsProgram(const TypePtr& element, const ExprPtr& value, int64_t word) {
    return program([=](PolyIRBuilder& b, Block& body) {
      Let(body, "p", nativeBlock(b, element));
      StoreIndex(body, Local(body, "p"), {IntConst(1)}, value);
      Return(body, Index(BuiltinCall(Builtin::Retype, Local(body, "p"), {}, MakePointer(MakeInt(64))), {IntConst(word)}));
    }, callocDeclaration);
  }

  // p[1] = value, read back through the same pointer as a whole number (a float scaled by 10^9 first, so f32 rounding shows).
  ExprPtr asWhole(const ExprPtr& read) {
    if (read->type->kind != TypeKind::Float) {
      return Cast(read, MakeInt(64));
    }

    return Cast(Mul(Cast(read, MakeFloat(64)), FloatConst(1e9)), MakeInt(64));
  }

  Module readBackProgram(const TypePtr& element, const ExprPtr& value) {
    return program([=](PolyIRBuilder& b, Block& body) {
      Let(body, "p", nativeBlock(b, element));
      StoreIndex(body, Local(body, "p"), {IntConst(1)}, value);
      Return(body, asWhole(Index(Local(body, "p"), {IntConst(1)})));
    }, callocDeclaration);
  }

  struct NativeCase {
    String name;
    TypePtr element;
    ExprPtr value;
    int64_t word;  // the word of the block element 1 lands in: 0 for elements of up to 4 bytes, 1 for 8-byte ones
    String bits;  // that word after the store
    String readBack;  // the element read back, as asWhole shows it
  };

  Vector<NativeCase> nativeCases() {
    const ExprPtr address = BuiltinCall(Builtin::Retype, IntConst(0x1234), {}, MakePointer(MakeInt(8)));

    return {{"i8", MakeInt(8), IntConst(-2, MakeInt(8)), 0, "65024", "-2"}, {"u8", MakeUInt(8), IntConst(200, MakeUInt(8)), 0, "51200", "200"},
      {"i16", MakeInt(16), IntConst(-2, MakeInt(16)), 0, "4294836224", "-2"}, {"u16", MakeUInt(16), IntConst(65535, MakeUInt(16)), 0, "4294901760", "65535"},
      {"i32", MakeInt(32), IntConst(-2, MakeInt(32)), 0, "-8589934592", "-2"}, {"u32", MakeUInt(32), IntConst(4000000000, MakeUInt(32)), 0, "-1266874889709551616", "4000000000"},
      {"i64", MakeInt(64), IntConst(-2), 1, "-2", "-2"}, {"u64", MakeUInt(64), IntConst(-1, MakeUInt(64)), 1, "-1", "-1"},
      {"f32", MakeFloat(32), FloatConst(1.1, MakeFloat(32)), 0, "4579260101969313792", "1100000023"}, {"f64", MakeFloat(64), FloatConst(1.1), 1, "4607632778762754458", "1100000000"},
      {"i1", MakeBool(), BoolConst(true), 0, "256", "1"}, {"ptr", MakePointer(MakeInt(8)), address, 1, "4660", ""}};
  }

  // Only the value is compared: most of these are far too large to be an exit status.
  void expectNative(const String& name, const Module& program, const String& want) {
    OStringStream out;
    const VmOutcome ran = runEntry(program, "main", {}, out);

    if (ran.ok && ran.result == want) {
      pass(name, shown(ran));
    } else {
      fail(name, want, shown(ran));
    }
  }

  void pointerWidths() {
    for (const NativeCase& c : nativeCases()) {
      expectNative("ptr<" + c.name + "> store", storedBitsProgram(c.element, c.value, c.word), c.bits);

      if (!c.readBack.empty()) {
        expectNative("ptr<" + c.name + "> read back", readBackProgram(c.element, c.value), c.readBack);
      }
    }
  }

  ExprPtr single(double v) {
    return FloatConst(v, MakeFloat(32));
  }

  ExprPtr atomic(Builtin op, const ExprPtr& pointer, Vector<ExprPtr> args, const TypePtr& result) {
    return BuiltinCall(op, pointer, move(args), result);
  }

  ExprPtr seqCst() {
    return StringConst("seq_cst");
  }

  // 41 -> add 1 (old 41) -> load 42 -> cmpxchg 42 => 50 (old 42) -> cmpxchg 999 => 7 fails (old 50) -> load 50.
  void atomicSteps(Block& body, const TypePtr& element) {
    Let(body, "old", atomic(Builtin::AtomicRmw, Local(body, "p"), {StringConst("add"), IntConst(1), seqCst()}, element));
    Let(body, "after", atomic(Builtin::AtomicLoad, Local(body, "p"), {seqCst()}, element));
    Let(body, "swapped", atomic(Builtin::AtomicCmpxchg, Local(body, "p"), {IntConst(42), IntConst(50), seqCst(), seqCst()}, element));
    Let(body, "kept", atomic(Builtin::AtomicCmpxchg, Local(body, "p"), {IntConst(999), IntConst(7), seqCst(), seqCst()}, element));
    Eval(body, atomic(Builtin::AtomicFence, IntConst(0), {StringConst("acquire")}, element));
  }

  ExprPtr digits(Block& body, const Vector<String>& names) {
    ExprPtr out = IntConst(0);

    for (const String& n : names) {
      out = Add(Mul(out, IntConst(100)), Local(body, n));
    }

    return out;
  }

  void atomicBody(PolyIRBuilder& b, Block& body) {
    Let(body, "p", nativeBlock(b, MakeInt(64)));
    Eval(body, atomic(Builtin::AtomicStore, Local(body, "p"), {IntConst(41), seqCst()}, MakeVoid()));
    atomicSteps(body, MakeInt(64));
    Return(body, Add(Mul(digits(body, {"old", "after", "swapped", "kept"}), IntConst(100)), atomic(Builtin::AtomicLoad, Local(body, "p"), {seqCst()}, MakeInt(64))));
  }

  ExprPtr rmw(Block& body, const String& pointer, const String& op, const ExprPtr& value) {
    return atomic(Builtin::AtomicRmw, Local(body, pointer), {StringConst(op), value, seqCst()}, value->type);
  }

  // An i32 cell holding -5: umax keeps it (as unsigned it is the larger), max replaces it with 3 and gives the old -5.
  void signedness(PolyIRBuilder& b, Block& body) {
    Let(body, "n", nativeBlock(b, MakeInt(32)));
    StoreIndex(body, Local(body, "n"), {IntConst(0)}, IntConst(-5, MakeInt(32)));
    Eval(body, rmw(body, "n", "umax", IntConst(3, MakeInt(32))));
    Let(body, "was", Cast(rmw(body, "n", "max", IntConst(3, MakeInt(32))), MakeInt(64)));
  }

  // -5 * 1000 + 3 * 10 + the old f32 sum before the second fadd (0.5, as asWhole shows it).
  void narrowAtomicBody(PolyIRBuilder& b, Block& body) {
    signedness(b, body);

    Let(body, "f", nativeBlock(b, MakeFloat(32)));
    Eval(body, rmw(body, "f", "fadd", single(0.5)));
    Let(body, "sum", asWhole(rmw(body, "f", "fadd", single(0.25))));

    Let(body, "now", Cast(Index(Local(body, "n"), {IntConst(0)}), MakeInt(64)));
    Return(body, Add(Add(Mul(Local(body, "was"), IntConst(1000)), Mul(Local(body, "now"), IntConst(10))), Local(body, "sum")));
  }

  // A C function with f32 parameters and results: fabsf, and ldexpf (an f32 and an int), and fmaf (three f32s); each read back as asWhole.
  void floatExterns(PolyIRBuilder& b) {
    const TypePtr f = MakeFloat(32);
    DeclareExtern(b, "fabsf", {{"x", f}}, f);
    DeclareExtern(b, "ldexpf", {{"x", f}, {"exponent", MakeInt(32)}}, f);
    DeclareExtern(b, "fmaf", {{"x", f}, {"y", f}, {"z", f}}, f);
  }

  void floatCallBody(PolyIRBuilder& b, Block& body) {
    Let(body, "a", asWhole(Call(b, "fabsf", {single(-1.1)})));
    Let(body, "e", asWhole(Call(b, "ldexpf", {single(1.5), IntConst(3, MakeInt(32))})));
    Let(body, "m", asWhole(Call(b, "fmaf", {single(2.0), single(3.0), single(0.5)})));
    Return(body, Add(Add(Local(body, "a"), Local(body, "e")), Local(body, "m")));
  }

  void nativeMemoryPrograms() {
    pointerWidths();
    expectNative("atomics", program(atomicBody, callocDeclaration), "4142425050");
    expectNative("narrow atomics", program(narrowAtomicBody, callocDeclaration), "499995030");
    expectNative("f32 foreign calls", program(floatCallBody, floatExterns), "19600000023");
  }

  void runPrograms() {
    listing();
    declined();

    bytecode();
    badBytecode();

    hostEntries();
    hostOutcomes();
  }

}  // namespace

int main() {
  controlPrograms();
  valuePrograms();
  runPrograms();
  memoryPrograms();
  nativeMemoryPrograms();

  cout << (failures == 0 ? "all checks passed" : "some checks failed") << "\n";

  return failures == 0 ? 0 : 1;
}
