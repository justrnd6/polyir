// RunPolyirCli in-process: the same function `polyir-check` and `nina polyir` call.
#include "polyir/Cli.hpp"

using namespace polyir;

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

  Outcome cli(const Vector<String>& args) {
    OStringStream out;
    OStringStream err;
    const int status = RunPolyirCli(args, out, err);

    return {status, out.str(), err.str()};
  }

  String flow() {
    return String(POLYIR_SOURCE_DIR) + "/tests/programs/mapping/Flow.poly";
  }

  void emitsLlvm() {
    const Outcome o = cli({flow(), "--emit-llvm"});
    expect(o.status == 0 && o.out.find("define") != String::npos, "--emit-llvm prints IR");
    const Outcome debug = cli({flow(), "--emit-llvm", "--debug"});
    expect(debug.out.find("DICompileUnit") != String::npos && debug.out.find("Flow.poly") != String::npos, "--debug writes DWARF naming the file");
  }

  void reportsErrors() {
    expect(cli({"/no/such/file.poly"}).out.rfind("no such file:", 0) == 0, "no such file headline");
    expect(cli({}).status == 2, "no file is a usage error");
    expect(cli({flow(), "--bogus"}).status == 2, "unknown flag is a usage error");
  }

  void runsPrograms() {
    const Outcome o = cli({flow(), "--run"});
    expect(o.status == 3, "--run returns the program's exit status (3), got " + to_string(o.status));

    const String exe = "./cli_test_flow.bin";
    const Outcome built = cli({flow(), "--out", exe, "--release"});
    expect(built.status == 0 && built.out == "compiled to " + exe + "\n", "--out builds");
    expect(RunExecutable(exe, {}) == 3, "the built executable exits 3");
  }

  // ── modules, `internal`, `if` values and generic functions ──

  String example(const String& name) {
    return String(POLYIR_SOURCE_DIR) + "/examples/" + name;
  }

  String misuse(const String& name) {
    return String(POLYIR_SOURCE_DIR) + "/tests/programs/fixtures/modules/" + name;
  }

  bool has(const String& text, const String& part) {
    return text.find(part) != String::npos;
  }

  // examples/modules.poly runs, and its .poly keeps the modules, the `internal` marks and the `if` values.
  void runsModules() {
    const Outcome run = cli({example("modules.poly"), "--interp"});
    expect(run.status == 0 && run.out.rfind("7 big 7 x first\n", 0) == 0, "modules.poly prints `7 big 7 x first`, got " + run.out);

    const String poly = cli({example("modules.poly"), "--emit-poly"}).out;
    expect(has(poly, "module Geometry {\n  struct Point(x: i64, y: i64) {") && has(poly, "\n  module Shapes {\n"), "--emit-poly writes the modules");
    expect(has(poly, "  internal func clamp(n: i64, low: i64, high: i64) -> i64 {") && has(poly, "    internal func absolute("), "--emit-poly marks internal");
    expect(has(poly, "return if (n < low) low else if (n > high) high else n"), "--emit-poly writes the `if` value chain");
    expect(has(poly, "let size: string = if (Geometry.Shapes.total(shapes) > 10) \"big\" else \"small\""), "--emit-poly names through modules");
  }

  // The .poly written for examples/modules.poly is written again the same: it says the whole program.
  void modulesPrintBack() {
    const String once = cli({example("modules.poly"), "--emit-poly"}).out;
    OFStream("./cli_test_modules.poly") << once;
    const Outcome twice = cli({"./cli_test_modules.poly", "--emit-poly"});
    expect(twice.status == 0 && twice.out == once, "the .poly of modules.poly prints back the same");
  }

  void refuses(const String& file, const Vector<String>& messages) {
    const Outcome o = cli({misuse(file), "--check"});
    expect(o.status == 1, file + " is refused");

    for (const String& message : messages) {
      expect(has(o.out, file + ":" + message + "\n"), file + " says " + message + ", got " + o.out);
    }
  }

  // Each misuse is refused at its place, with what to do instead.
  void refusesMisuse() {
    refuses("internal-use.poly", {"17:25: Geometry.clamp is internal: only code inside module Geometry can use it",
                                  "18:18: Geometry.Point.bonus is internal: only Geometry.Point's own methods can use it",
                                  "19:27: Geometry.Cache is internal: only code inside module Geometry can use it",
                                  "20:16: a module is not a value: name one of its declarations (Geometry.name)"});
    refuses("if-values.poly", {"6:33: expected an integer but found a string", "7:11: both branches of an `if` value have one type: i64 and string",
                               "8:11: an `if` value needs a value in each branch",
                               "10:12: an `if` value cannot sit in a loop's test or step: work it out in the loop's body"});
    refuses("generic-unbound.poly", {"6:27: make's type T cannot be worked out from its arguments"});
  }

}  // namespace

int main() {
  emitsLlvm();
  reportsErrors();
  runsPrograms();

  runsModules();
  modulesPrintBack();
  refusesMisuse();

  if (failures == 0) {
    cout << "CliTest ok\n";
  }

  return failures == 0 ? 0 : 1;
}
