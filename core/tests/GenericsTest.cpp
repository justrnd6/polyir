// Generics carried in the HIR: each generic as declared (`struct Box<T>`, `func pick<T>`) beside the copies made for real types. Every
// test writes a small program to a file, runs the command line on it (RunPolyirCli, as `polyir-check` runs it), and looks at what it printed.
#include "polyir/Cli.hpp"
#include "polyir/Frontend.hpp"
#include "polyir/HirJson.hpp"
#include "polyir/Pbc.hpp"

using namespace polyir;
using namespace polyir::front;

namespace {

  int failures = 0;

  void expect(bool ok, const String& what) {
    if (ok) {
      return;
    }

    cerr << "FAIL " << what << "\n";
    failures++;
  }

  struct Outcome {
    int status;
    String out;
    String err;
  };

  // The command line run on `program`, written to a file named `name` in the build folder.
  Outcome run(const String& name, const String& program, const String& flag) {
    OFStream(name) << program;
    OStringStream out;
    OStringStream err;
    const int status = RunPolyirCli({name, flag}, out, err);

    return {status, out.str(), err.str()};
  }

  bool has(const String& text, const String& part) {
    return text.find(part) != String::npos;
  }

  // How many times `part` is in `text`.
  int countOf(const String& text, const String& part) {
    int count = 0;

    for (size_t at = text.find(part); at != String::npos; at = text.find(part, at + 1)) {
      count++;
    }

    return count;
  }

  const String kBox = "struct Box<T>(value: T) {\n"
                      "  func get() -> T {\n"
                      "    return this.value\n"
                      "  }\n"
                      "}\n"
                      "func pick<T>(a: T, b: T, first: bool) -> T {\n"
                      "  if (first) {\n"
                      "    return a\n"
                      "  }\n"
                      "  return b\n"
                      "}\n"
                      "func main() -> void {\n"
                      "  let a: Box<i64> = Box(value: 1)\n"
                      "  let b: Box<string> = Box(value: \"two\")\n"
                      "  print(\"${a.get()} ${b.get()} ${pick(3, 4, false)}\")\n"
                      "}\n";

  // ── written once ──

  // The .poly writes a generic struct once, with its type parameter, and its uses as `Box<i64>`, not as the copies.
  void structWrittenOnce() {
    const String poly = run("generics_test_box.poly", kBox, "--emit-poly").out;

    expect(countOf(poly, "struct Box<T>(value: T) {") == 1, "struct Box<T> is written once, got " + poly);
    expect(has(poly, "let a: Box<i64> = Box(value: 1)") && has(poly, "let b: Box<string> = Box(value: \"two\")"), "a use is Box<i64>");
    expect(!has(poly, "Box_i64") && !has(poly, "Box_string"), "no copy is written");
  }

  // A generic function is written once too, and a call of it by the generic's name.
  void functionWrittenOnce() {
    const String poly = run("generics_test_pick.poly", kBox, "--emit-poly").out;

    expect(has(poly, "func pick<T>(a: T, b: T, first: bool) -> T {"), "func pick<T> is written once, got " + poly);
    expect(has(poly, "pick(3, 4, false)") && !has(poly, "pick_i64"), "a call names pick, not its copy");
  }

  // The .poly of a program with generics reads back as the same program, which prints the same.
  void genericsPrintBack() {
    const String once = run("generics_test_back.poly", kBox, "--emit-poly").out;
    const Outcome twice = run("generics_test_back_again.poly", once, "--emit-poly");
    const Outcome ran = run("generics_test_back_run.poly", once, "--interp");

    expect(twice.status == 0 && twice.out == once, "the .poly prints back the same, got " + twice.out + twice.err);
    expect(ran.status == 0 && has(ran.out, "1 two 4\n"), "the .poly runs, got " + ran.out + ran.err);
  }

  // A generic in a module is written in its module's block, and named through the module outside it.
  void genericInModule() {
    const String program = "module Shapes {\n"
                           "  struct Box<T>(value: T)\n"
                           "}\n"
                           "func main() -> void {\n"
                           "  let b: Shapes.Box<i64> = Shapes.Box(value: 5)\n"
                           "  print(str(b.value))\n"
                           "}\n";
    const String poly = run("generics_test_module.poly", program, "--emit-poly").out;

    expect(has(poly, "module Shapes {\n  struct Box<T>(value: T)\n}"), "the generic is in its module's block, got " + poly);
    expect(has(poly, "let b: Shapes.Box<i64> = Shapes.Box(value: 5)"), "outside the module it is Shapes.Box<i64>");
  }

  // ── what does not work for every type ──

  const String kDoubler = "struct Doubler<T>(value: T) {\n"
                          "  func doubled() -> T {\n"
                          "    return this.value + this.value\n"
                          "  }\n"
                          "}\n"
                          "func main() -> void {\n"
                          "  let d: Doubler<i64> = Doubler(value: 21)\n"
                          "  print(str(d.doubled()))\n"
                          "}\n";

  // `+` on a T needs T to be a number. The program still runs, but the generic is written as its copies.
  void notForEveryType() {
    const Outcome ran = run("generics_test_doubler.poly", kDoubler, "--interp");
    const String poly = run("generics_test_doubler.poly", kDoubler, "--emit-poly").out;

    expect(ran.status == 0 && has(ran.out, "42\n"), "Doubler<i64> runs, got " + ran.out + ran.err);
    expect(has(poly, "struct Doubler_i64(value: i64) {") && !has(poly, "Doubler<T>"), "Doubler is written as its copies, got " + poly);
  }

  // The HIR says why: the generic as declared keeps the first problem its body had with T standing for any type.
  void reasonIsKept() {
    const Module module = CheckSource(kDoubler).module;
    const bool one = module.genericStructs.size() == 1;
    const String reason = one ? module.genericStructs[0].generic.notParametric : "";

    expect(reason == "3:23: T can be any type, and `+` does not work on every type", "the reason is kept, got " + reason);
  }

  // ── carried by the formats ──

  // The HIR text, which LLVM and the VM follow, is the copies, as it always was.
  void hirKeepsCopies() {
    const String hir = run("generics_test_hir.poly", kBox, "--hir").out;

    expect(has(hir, "struct Box_i64{value:i64}") && has(hir, "struct Box_string{value:string}"), "the HIR lists the copies, got " + hir);
    expect(!has(hir, "Box_T"), "the HIR text has no generic as declared");
  }

  // The generic as declared, with its parameter.
  bool hasBoxOfT(const Module& module) {
    const bool one = module.genericStructs.size() == 1;

    return one && module.genericStructs[0].generic.params == Vector<String>{"T"} && module.genericStructs[0].fields.size() == 1;
  }

  // HIR JSON and .pbc keep the generics as declared and what each copy is to its generic.
  void formatsKeepGenerics() {
    const Module module = CheckSource(kBox).module;
    const Module json = ReadHirJson(WriteHirJson(module));
    const Module pbc = ReadPbc(WritePbc(module));

    expect(hasBoxOfT(json) && hasBoxOfT(pbc), "JSON and .pbc keep struct Box<T>");
    expect(json.genericFunctions.size() == module.genericFunctions.size(), "JSON keeps the generic functions");
    expect(FindStruct(pbc, "Box_i64") != nullptr && FindStruct(pbc, "Box_i64")->generic.of == "Box", ".pbc says Box_i64 is from Box");
  }

}  // namespace

int main() {
  structWrittenOnce();
  functionWrittenOnce();
  genericsPrintBack();
  genericInModule();

  notForEveryType();
  reasonIsKept();

  hirKeepsCopies();
  formatsKeepGenerics();

  if (failures == 0) {
    cout << "GenericsTest ok\n";
  }

  return failures == 0 ? 0 : 1;
}
