// The PBC writer and reader (WritePbc, ReadPbc). Every example program (front_examples.txt, listed by the build) that the C++
// front end accepts is written to bytes, read back and compared as HIR JSON and as HIR text. A few small modules check the floats,
// the strings beyond ASCII, a kernel's stage and roles, an origin, and the errors (bad magic, a version from the future, truncation).
#include "polyir/Std.hpp"
#include "polyir/Frontend.hpp"
#include "polyir/Builder.hpp"
#include "polyir/HirJson.hpp"
#include "polyir/HirPrint.hpp"
#include "polyir/Pbc.hpp"
#include "ExampleList.hpp"

using namespace polyir;
using namespace polyir::testing;
using namespace polyir::front;

namespace {

  int failures = 0;

  void fail(const String& what, const String& want, const String& got) {
    failures++;
    cerr << "FAIL " << what << "\n  want: " << want.substr(0, 200) << "\n  got:  " << got.substr(0, 200) << "\n";
  }

  void expectEqual(const String& what, const String& want, const String& got) {
    if (want != got) {
      fail(what, want, got);
    }
  }

  void roundTrip(const String& path, const Module& module) {
    const Module again = ReadPbc(WritePbc(module));
    expectEqual(path + " (pbc round trip, JSON)", WriteHirJson(module), WriteHirJson(again));
    expectEqual(path + " (pbc round trip, HIR text)", ModuleText(module), ModuleText(again));
  }

  bool oneExample(const String& path) {
    const FrontResult result = CheckFile(path);

    if (!Ok(result)) {
      return false;
    }

    roundTrip(path, result.module);

    return true;
  }

  int examples() {
    int checked = 0;

    for (const String& path : LinesOf("front_examples.txt")) {
      checked += oneExample(path) ? 1 : 0;
    }

    return checked;
  }

  // ── small modules ──

  Module edgesModule() {
    PolyIRBuilder b;
    auto& f = DefineFunction(b, "edges", {{"x", MakeFloat(64)}}, MakeFloat(64));
    Return(f.body, FloatConst(0.1));

    return Build(b);
  }

  StmtPtr statementOf(const ExprPtr& value) {
    auto statement = make_shared<Stmt>();
    statement->kind = StmtKind::ExprStmt;
    statement->value = value;

    return statement;
  }

  ExprPtr stringConstant(const String& text) {
    auto constant = make_shared<Expr>();
    constant->kind = ExprKind::ConstString;
    constant->type = MakeString();
    constant->text = text;

    return constant;
  }

  ExprPtr floatConstant(double value) {
    auto constant = make_shared<Expr>();
    constant->kind = ExprKind::ConstFloat;
    constant->type = MakeFloat(64);
    constant->floatValue = value;

    return constant;
  }

  // Each statement goes in front of the ones before it, so the `return` the builder made stays last. The order is the floats
  // (last first), then the string, then the return.
  void addEdgeStatements(FuncDef& function) {
    function.origin = "Maths";
    function.body.insert(function.body.begin(), statementOf(stringConstant("héllo → 🌍")));

    for (const double v : {0.0, -2.25, 1e300, 5e-324, 1.0 / 0.0, -1.0 / 0.0}) {
      function.body.insert(function.body.begin(), statementOf(floatConstant(v)));
    }
  }

  // A module with floats of every shape, a string beyond ASCII and an origin: everything the examples may not cover.
  Module handMade() {
    Module module = edgesModule();
    addEdgeStatements(module.functions[0]);
    module.order.clear();
    NoteAdded(module, Vector<size_t>(Counts(module).size(), 0));

    return module;
  }

  void checkHandMade() {
    roundTrip("hand-made module", handMade());
  }

  // A kernel's stage and roles come back through the HKernel record (tag 9).
  void checkKernel() {
    Module module = handMade();
    module.functions[0].stage = StageNamed("compute");
    module.functions[0].roles = {ParamRole::Buffer, ParamRole::Writable};
    roundTrip("kernel", module);
    const Module again = ReadPbc(WritePbc(module));

    if (again.functions[0].stage != KernelStage::Compute) {
      fail("kernel stage", "compute", StageName(again.functions[0].stage));
    }

    if (again.functions[0].roles.size() != 2) {
      fail("kernel roles", "2", to_string(again.functions[0].roles.size()));
    }
  }

  void expectRefused(const String& what, const Vector<uint8_t>& bytes) {
    try {
      ReadPbc(bytes);
      fail(what, "a refusal", "a module");
    } catch (const HirReadError&) {
    }
  }

  void checkErrors() {
    expectRefused("empty file", {});
    expectRefused("bad magic", {1, 2, 3, 4, 1, 0, 0, 0});
    expectRefused("future version", {80, 66, 67, 1, 2, 0, 0, 0});

    Vector<uint8_t> good = WritePbc(handMade());
    good.resize(good.size() / 2);
    expectRefused("truncated file", good);
  }

  int run() {
    const int checked = examples();
    cout << "wrote and read back " << checked << " example programs as .pbc\n";

    if (checked == 0) {
      fail("examples", "some programs", "none");
    }

    return failures == 0 ? 0 : 1;
  }

}  // namespace

int main() {
  checkHandMade();
  checkKernel();
  checkErrors();

  return run();
}
