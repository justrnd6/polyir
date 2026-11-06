// The garbage collector of native builds (docs/memory.md, Parts 2 and 3; src/emit/llvm/runtime/Collector.c). Each program is compiled to LLVM IR,
// built with clang and run, with the collector's environment switches: POLYIR_GC_REPORT=1 (what it did, on standard error), POLYIR_GC_STRESS=1 (a
// whole collection at every safe point, so a reference the collector missed is freed while in use and the answer goes wrong),
// POLYIR_GC_STRESS=incremental (a slice of one word at every safe point, and the marks checked at the end of every marking, so a store without
// its write barrier stops the program), POLYIR_GC_MODE=stop and POLYIR_GC_CEILING. The programs are in tests/data/gc; the VM's answer is the
// reference where there is one.
#include "polyir/Std.hpp"
#include "polyir/Builder.hpp"
#include "polyir/Frontend.hpp"
#include "polyir/LlvmCompiler.hpp"
#include "polyir/Vm.hpp"

#include <sys/resource.h>
#include <sys/wait.h>
#include <unistd.h>

using namespace polyir;
using polyir::front::CheckSource;
using polyir::front::FrontResult;

namespace {

  int failures = 0;

  void pass(const String& name, const String& got) {
    cout << "ok   " << name << ": " << got << "\n";
  }

  void fail(const String& name, const String& want, const String& got) {
    cout << "FAIL " << name << ": expected " << want << ", got " << got << "\n";
    failures++;
  }

  void expect(const String& name, bool good, const String& want, const String& got) {
    if (good) {
      pass(name, got);
    } else {
      fail(name, want, got);
    }
  }

  // ── building and running ──

  struct Ran {
    int status = -1;  // the exit status, or 128 + the signal
    int64_t maxResident = 0;  // the most memory the process held, in bytes
    String out;
    String err;
  };

  String readAll(const String& path) {
    IFStream in(path);
    StringStream text;
    text << in.rdbuf();

    return text.str();
  }

  String programText(const String& file) {
    return readAll(String(POLYIR_SOURCE_DIR) + "/tests/data/gc/" + file);
  }

  Module checked(const String& text) {
    const FrontResult result = CheckSource(text);

    if (!result.errors.empty()) {
      cout << "FAIL the program does not check: " << result.errors[0] << "\n";
    }

    return result.module;
  }

  CompileOptions withCollector(bool collect) {
    CompileOptions options;
    options.collectGarbage = collect;

    return options;
  }

  bool assemble(const String& name, const String& ir, const String& optimization) {
    OFStream("gc_" + name + ".ll") << ir;
    const String command = "clang -x ir " + optimization + " -Wno-override-module gc_" + name + ".ll -o gc_" + name + ".bin 2> gc_" + name + ".log";

    return system(command.c_str()) == 0;
  }

  // The IR of `program` (with the collector unless `collect` is off), built with clang at `optimization`: whether it built.
  bool build(const String& name, const Module& program, const String& optimization, bool collect = true) {
    const CompileResult compiled = CompileToLlvm(program, withCollector(collect));

    if (Ok(compiled)) {
      return assemble(name, compiled.ir, optimization);
    }

    cout << "FAIL " << name << ": " << compiled.errors[0] << "\n";

    return false;
  }

  int64_t residentBytes(const struct rusage& usage) {
#ifdef __APPLE__
    return usage.ru_maxrss;  // bytes on macOS
#else
    return usage.ru_maxrss * 1024;  // kilobytes on Linux
#endif
  }

  int statusOf(int status) {
    return WIFEXITED(status) ? WEXITSTATUS(status) : 128 + WTERMSIG(status);
  }

  // The shell runs the program in its own place (`exec`), so the child's rusage is the program's.
  pid_t start(const String& command) {
    const pid_t child = fork();

    if (child != 0) {
      return child;
    }

    execl("/bin/sh", "sh", "-c", command.c_str(), static_cast<char*>(nullptr));
    _exit(127);
  }

  // Runs the built program with the environment `settings` (`NAME=value ...`).
  Ran run(const String& name, const String& settings) {
    const pid_t child = start("exec env " + settings + " ./gc_" + name + ".bin > gc_" + name + ".out 2> gc_" + name + ".err");
    int status = 0;
    struct rusage usage {};
    wait4(child, &status, 0, &usage);

    return Ran{statusOf(status), residentBytes(usage), readAll("gc_" + name + ".out"), readAll("gc_" + name + ".err")};
  }

  // The VM's printed lines (without the entry's value), for comparing. `verify` runs it in its incremental stress mode, where a missing
  // barrier ends the run with a trap (whose text is then the answer).
  String onTheVm(const Module& program, bool verify = false) {
    OStringStream out;
    VmOptions options;
    options.out = &out;
    options.verifyIncremental = verify;
    const VmOutcome ran = RunModule(program, options);

    return ran.ok ? out.str() : out.str() + "trap: " + ran.trap;
  }

  // ── what POLYIR_GC_REPORT says ──

  // "gc: C collections, R objects (B bytes) reclaimed, peak P bytes, ...": the number just before `word` in the report
  // (C for "collections", R for "objects").
  int64_t numberBefore(const Ran& ran, const String& word) {
    const size_t at = ran.err.find(" " + word);

    if (at == String::npos) {
      return -1;
    }

    const size_t start = ran.err.rfind(' ', at - 1);

    return atoll(ran.err.substr(start + 1, at - start - 1).c_str());
  }

  // The number just after `word` in the report ("longest 41.0 us": the longest pause, in microseconds).
  double numberAfter(const Ran& ran, const String& word) {
    const size_t at = ran.err.find(word + " ");

    return at == String::npos ? -1 : atof(ran.err.c_str() + at + word.size() + 1);
  }

  String summary(const Ran& ran) {
    const String report = ran.err.substr(0, ran.err.find('\n'));

    return "exit " + to_string(ran.status) + ", " + to_string(ran.maxResident >> 20) + " MB resident; " + report;
  }

  // ── 1. garbage is reclaimed and memory stays flat ──

  String churnProgram(int64_t rounds) {
    String text = programText("Churn.poly");
    const String marker = "churn(1000000)";

    return text.replace(text.find(marker), marker.size(), "churn(" + to_string(rounds) + ")");
  }

  // What Churn.poly prints, worked out here: each round adds 8 + i.
  String churnTotal(int64_t rounds) {
    int64_t total = 0;

    for (int64_t i = 0; i < rounds; i++) {
      total = (total + 8 + i) % 1000003;
    }

    return to_string(total) + "\n";
  }

  Ran churn(int64_t rounds) {
    const String name = "churn" + to_string(rounds);

    if (!build(name, checked(churnProgram(rounds)), "-O1")) {
      return Ran{};
    }

    return run(name, "POLYIR_GC_REPORT=1");
  }

  // Ten times the rounds (some 17 million objects made in all) and the resident memory does not grow. It stays within 8 MB of the
  // shorter run and under 64 MB. Without the collector the long run holds over 700 MB.
  void flatMemory() {
    const Ran shorter = churn(100000);
    const Ran longer = churn(1000000);

    const bool right = shorter.out == churnTotal(100000) && longer.out == churnTotal(1000000) && numberBefore(longer, "collections") > 10;
    const bool flat = longer.maxResident <= shorter.maxResident + (8 << 20) && longer.maxResident < (64 << 20);

    expect("garbage is reclaimed", right, churnTotal(1000000) + " after some collections", summary(longer));
    expect("memory stays flat", right && flat, "at most 8 MB more than " + to_string(shorter.maxResident >> 20) + " MB", summary(longer));
  }

  // Two nodes that refer to each other and that nothing else reaches, 200,000 times: every pair goes at a collection like any garbage.
  void cycles() {
    const bool built = build("cycles", checked(programText("Cycles.poly")), "-O1");
    const Ran ran = built ? run("cycles", "POLYIR_GC_REPORT=1") : Ran{};
    expect("cycles are reclaimed", ran.out == "200000\n" && numberBefore(ran, "objects") > 400000, "200000 and the pairs reclaimed", summary(ran));
  }

  // A generator made and dropped half way every round: its frame, yield slot and result block are garbage like the rest.
  void generators() {
    const bool built = build("generators", checked(programText("Generators.poly")), "-O1");
    const Ran ran = built ? run("generators", "POLYIR_GC_REPORT=1") : Ran{};
    expect("coroutines are reclaimed", ran.out == "730003\n" && numberBefore(ran, "objects") > 1000000, "730003 and the frames reclaimed", summary(ran));
  }

  // ── 2. every root, under stress ──

  // Roots.poly checks temporaries across calls, a struct whose fields are calls, closures sharing a cell and an escaping closure. It
  // also checks interface values, coroutines (one only the resume keeps), and a throw out of functions holding references and
  // strings. It runs at -O0 and -O2.
  void rootsUnderStress(const String& optimization) {
    const Module program = checked(programText("Roots.poly"));
    const String name = "roots" + optimization;
    const Ran ran = build(name, program, optimization) ? run(name, "POLYIR_GC_STRESS=1 POLYIR_GC_REPORT=1") : Ran{};
    const String want = onTheVm(program);
    expect("roots under stress " + optimization, ran.out == want && numberBefore(ran, "collections") > 100, want, ran.out + summary(ran));
  }

  // A global that holds a list is a root: main pushes into it while the helper makes garbage (the source language's globals hold numbers
  // only, so this program is built with PolyIRBuilder; the global starts with a list, given by the module's initializer function).

  // A mutable list of 64-bit numbers.
  TypePtr numberListType() {
    return MakeDynamicArray(MakeInt(64), true);
  }

  void churnFunction(PolyIRBuilder& b) {
    auto& churn = DefineFunction(b, "churn", {{"n", MakeInt(64)}}, MakeInt(64));
    ForRange(churn.body, "k", IntConst(0), Local(churn.body, "n"), [&](Block& loop) { Eval(loop, VecLit(numberListType(), {Local(loop, "k")})); });
    Return(churn.body, IntConst(0));
  }

  void globalMain(PolyIRBuilder& b, Block& body) {
    ForRange(body, "i", IntConst(0), IntConst(50), [&](Block& loop) {
      Eval(loop, Push(Global(b, "kept"), Local(loop, "i")));
      Eval(loop, Call(b, "churn", {IntConst(5)}));
    });
    Return(body, Add(Index(Global(b, "kept"), {IntConst(49)}), Len(Global(b, "kept"))));  // 49 + 50
  }

  Module globalProgram() {
    PolyIRBuilder b;
    DefineGlobal(b, "kept", numberListType(), VecLit(numberListType(), {}));
    churnFunction(b);
    globalMain(b, DefineFunction(b, "main", {}, MakeInt(64)).body);

    return Build(b);
  }

  void globalRoots() {
    const Ran ran = build("global", globalProgram(), "-O0") ? run("global", "POLYIR_GC_STRESS=1 POLYIR_GC_REPORT=1") : Ran{};
    expect("a global is a root", ran.status == 99 && numberBefore(ran, "collections") > 100, "exit 99", summary(ran));
  }

  // `Free` is a hint: a list the program frees and still reads is not reclaimed while it can be reached.
  void freedMain(PolyIRBuilder& b, Block& body) {
    Let(body, "xs", VecLit(numberListType(), {IntConst(41), IntConst(42)}));
    Eval(body, BuiltinCall(Builtin::Free, Local(body, "xs"), {}, MakeVoid()));
    Eval(body, Call(b, "churn", {IntConst(20)}));
    Return(body, Index(Local(body, "xs"), {IntConst(1)}));
  }

  Module freedProgram() {
    PolyIRBuilder b;
    churnFunction(b);
    freedMain(b, DefineFunction(b, "main", {}, MakeInt(64)).body);

    return Build(b);
  }

  void freeIsAHint() {
    const Ran ran = build("freed", freedProgram(), "-O0") ? run("freed", "POLYIR_GC_STRESS=1") : Ran{};
    expect("free is a hint", ran.status == 42, "exit 42", summary(ran));
  }

  // ── 3. the policy: the ceiling, and builds without the collector ──

  // A program that keeps 5,000 structs in a list stops at a 4,096-byte ceiling with the message (and runs to the end without one).
  void ceiling() {
    const bool built = build("keepall", checked(programText("KeepAll.poly")), "-O1");
    const Ran tight = built ? run("keepall", "POLYIR_GC_CEILING=4096") : Ran{};
    const Ran roomy = built ? run("keepall", "POLYIR_GC_REPORT=1") : Ran{};
    const bool stopped = tight.status != 0 && tight.err.rfind("out of memory: ", 0) == 0 && tight.err.find("bytes are still reachable (limit 4096)") != String::npos;
    expect("ceiling", stopped && roomy.out == "5000\n", "out of memory: N bytes are still reachable (limit 4096)", tight.err + summary(roomy));
  }

  // CompileOptions::collectGarbage off: plain malloc, no frame records and no safe points, and the same answer.
  void withoutTheCollector() {
    const Module program = checked(churnProgram(10000));
    const String ir = CompileToLlvm(program, withCollector(false)).ir;
    const bool plain = ir.find("@gc_poll") == String::npos && ir.find("gc_push_frame") == String::npos && ir.find("@malloc(") != String::npos;
    const Ran ran = build("plain", program, "-O1", false) ? run("plain", "POLYIR_GC_REPORT=1") : Ran{};
    expect("without the collector", plain && ran.out == churnTotal(10000) && ran.err.empty(), churnTotal(10000) + " and no report", summary(ran));
  }

  // ── 4. the incremental collector: its write barriers and its pauses ──

  // Barriers.poly takes the only reference to a list out of an object the collector may not have looked at yet. It keeps that
  // reference only where the collector has looked already. The places tried: an element, a field, a captured variable's cell, a
  // truncated list's element, a coroutine's yield slot and its own variables. Under the incremental stress mode a missing barrier
  // stops the program. The VM, in its own incremental stress mode, must print the same lines.
  void barriersUnderStress(const String& optimization) {
    const Module program = checked(programText("Barriers.poly"));
    const String name = "barriers" + optimization;
    const Ran ran = build(name, program, optimization) ? run(name, "POLYIR_GC_STRESS=incremental POLYIR_GC_REPORT=1") : Ran{};

    const String want = onTheVm(program);
    const bool vm = onTheVm(program, true) == want;
    expect("barriers under incremental stress " + optimization, vm && ran.out == want && numberBefore(ran, "collections") > 2, want, ran.out + summary(ran));
  }

  // Roots.poly (every kind of root) in the incremental stress mode too.
  void rootsUnderIncrementalStress(const String& optimization) {
    const Module program = checked(programText("Roots.poly"));
    const String name = "iroots" + optimization;
    const Ran ran = build(name, program, optimization) ? run(name, "POLYIR_GC_STRESS=incremental POLYIR_GC_REPORT=1") : Ran{};
    const String want = onTheVm(program);
    expect("roots under incremental stress " + optimization, ran.out == want && onTheVm(program, true) == want, want, ran.out + summary(ran));
  }

  // A map whose values are lists, each taken out by `remove` (which shifts the entries after it in its table) and kept only in a list made
  // afterwards: map_remove's stores go through the barrier. 0 + 1 + ... + 63 (built with PolyIRBuilder: the source language has no remove).

  // A mutable list whose elements are lists of numbers.
  TypePtr listOfNumberListsType() {
    return MakeDynamicArray(numberListType(), true);
  }

  void removeRounds(PolyIRBuilder& b, Block& body) {
    ForRange(body, "r", IntConst(0), IntConst(64), [&](Block& loop) {
      Let(loop, "held", Index(Local(loop, "m"), {Local(loop, "r")}));
      Eval(loop, Remove(Local(loop, "m"), Local(loop, "r")));
      Eval(loop, Call(b, "churn", {IntConst(3)}));
      Eval(loop, Push(Local(loop, "kept"), Local(loop, "held")));
    });
  }

  void sumOfKept(Block& body) {
    Var(body, "total", MakeInt(64), IntConst(0));
    ForRange(body, "i", IntConst(0), Len(Local(body, "kept")), [&](Block& loop) { SetLocal(loop, "total", Add(Local(loop, "total"), Index(Index(Local(loop, "kept"), {Local(loop, "i")}), {IntConst(0)}))); });
    Return(body, Local(body, "total"));
  }

  void removeMain(PolyIRBuilder& b, Block& body) {
    Let(body, "m", NewContainer(MakeHashmap(MakeInt(64), numberListType(), true)));
    ForRange(body, "i", IntConst(0), IntConst(64), [&](Block& loop) { StoreIndex(loop, Local(loop, "m"), {Local(loop, "i")}, VecLit(numberListType(), {Local(loop, "i")})); });
    Eval(body, Call(b, "churn", {IntConst(30)}));

    Let(body, "kept", VecLit(listOfNumberListsType(), {}));
    removeRounds(b, body);
    sumOfKept(body);
  }

  Module removeProgram() {
    PolyIRBuilder b;
    churnFunction(b);
    removeMain(b, DefineFunction(b, "main", {}, MakeInt(64)).body);

    return Build(b);
  }

  VmOutcome verifiedOnTheVm(const Module& program) {
    OStringStream out;
    VmOptions options;
    options.out = &out;
    options.verifyIncremental = true;

    return RunModule(program, options);
  }

  // 2016 is more than an exit status holds: the status is its low 8 bits, 224.
  void mapRemovals() {
    const Module program = removeProgram();
    const Ran ran = build("remove", program, "-O1") ? run("remove", "POLYIR_GC_STRESS=incremental") : Ran{};
    const VmOutcome vm = verifiedOnTheVm(program);
    expect("map removals under incremental stress", ran.status == 224 && vm.ok && vm.result == "2016", "exit 224 and the VM's 2016", summary(ran) + " " + vm.result + vm.trap);
  }

  // Game.poly keeps a world of 200,000 entities and makes garbage every frame. Stopping the world, every collection is one pause as long as
  // marking the whole world. Incremental, the same collections are hundreds of short slices, each far shorter than that.
  void slicedPauses() {
    const bool built = build("game", checked(programText("Game.poly")), "-O1");
    const Ran stop = built ? run("game", "POLYIR_GC_STRESS=0 POLYIR_GC_MODE=stop POLYIR_GC_REPORT=1") : Ran{};
    const Ran sliced = built ? run("game", "POLYIR_GC_STRESS=0 POLYIR_GC_MODE=incremental POLYIR_GC_REPORT=1") : Ran{};

    const bool same = stop.out == "347063\n" && sliced.out == stop.out && numberBefore(stop, "pauses") == numberBefore(stop, "collections");
    const bool shorter = numberBefore(sliced, "pauses") > 10 * numberBefore(sliced, "collections") && numberAfter(sliced, "longest") * 4 < numberAfter(stop, "longest");
    expect("pauses are sliced", same && shorter, "347063, many pauses, each far shorter than a whole collection", summary(stop) + " | " + summary(sliced));
  }

  void garbagePrograms() {
    flatMemory();
    cycles();
    generators();
  }

  void rootPrograms() {
    rootsUnderStress("-O0");
    rootsUnderStress("-O2");
    globalRoots();
    freeIsAHint();
  }

  void incrementalPrograms() {
    barriersUnderStress("-O0");
    barriersUnderStress("-O2");

    rootsUnderIncrementalStress("-O0");
    rootsUnderIncrementalStress("-O2");

    mapRemovals();
    slicedPauses();
  }

}  // namespace

int main() {
  garbagePrograms();
  rootPrograms();
  incrementalPrograms();
  ceiling();
  withoutTheCollector();

  cout << (failures == 0 ? "all checks passed" : "some checks failed") << "\n";

  return failures == 0 ? 0 : 1;
}
