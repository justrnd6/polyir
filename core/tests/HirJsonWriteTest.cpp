// The HIR JSON writer (WriteHirJson). Every example program (front_examples.txt, listed by the build) that the C++ front end accepts is
// written, read back and written again: the two texts are equal and the module prints the same HIR text.
// A few small modules check the floats (C99 hex), the escapes and the order of the declarations.
#include "polyir/Std.hpp"
#include "polyir/Frontend.hpp"
#include "front/Ast.hpp"
#include "front/Lint.hpp"
#include "polyir/Builder.hpp"
#include "polyir/HirJson.hpp"
#include "polyir/HirPrint.hpp"
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
    const String json = WriteHirJson(module);
    const Module again = ReadHirJson(json);
    expectEqual(path + " (written twice)", json, WriteHirJson(again));
    expectEqual(path + " (HIR text)", ModuleText(module), ModuleText(again));
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

  StmtPtr statement(const ExprPtr& value) {
    auto s = make_shared<Stmt>();
    s->kind = StmtKind::ExprStmt;
    s->value = value;

    return s;
  }

  SharedPtr<Expr> constant(ExprKind kind, const TypePtr& type) {
    SharedPtr<Expr> e = make_shared<Expr>();
    e->kind = kind;
    e->type = type;

    return e;
  }

  Module moduleWith(const Vector<ExprPtr>& values) {
    Module module;
    FuncDef main;
    main.name = "main";
    main.result = MakeFloat(64);

    for (const ExprPtr& v : values) {
      main.body.push_back(statement(v));
    }

    module.functions.push_back(main);

    return module;
  }

  ExprPtr floatConstant(double v) {
    SharedPtr<Expr> e = constant(ExprKind::ConstFloat, MakeFloat(64));
    e->floatValue = v;

    return e;
  }

  bool contains(const String& text, const String& part) {
    return text.find(part) != String::npos;
  }

  void checkFloats() {
    Vector<ExprPtr> values;

    for (const double v : {0.0, 1.0, 1.5, -2.25, 0.1, 1e300, 5e-324, 1.0 / 0.0}) {
      values.push_back(floatConstant(v));
    }

    const String json = WriteHirJson(moduleWith(values));

    for (const char* want : {"\"0x0p0\"", "\"0x1p0\"", "\"0x1.8p0\"", "\"-0x1.2p1\"", "\"0x1.999999999999ap-4\"", "\"0x1p-1074\"", "\"inf\""}) {
      if (!contains(json, want)) {
        fail(String("float ") + want, want, json);
      }
    }
  }

  void checkEscapes() {
    SharedPtr<Expr> e = constant(ExprKind::ConstString, MakeString());
    e->text = "a\"b\\c\nd\te\r";
    const String json = WriteHirJson(moduleWith({e}));

    if (!contains(json, "\"text\":\"a\\\"b\\\\c\\nd\\te\r\"")) {
      fail("string escapes", "a\\\"b\\\\c\\nd\\te<CR>", json);
    }

    expectEqual("escapes round trip", json, WriteHirJson(ReadHirJson(json)));
  }

  // `truncate` and `remove` have names in the JSON (a reader finds them by name): the module comes back as it went.
  Module shrinkModule() {
    PolyIRBuilder b;
    auto& f = DefineFunction(b, "shrink", {{"xs", MakeDynamicArray(MakeInt(64), true)}, {"m", MakeHashmap(MakeInt(64), MakeInt(64), true)}}, MakeVoid());

    Eval(f.body, Truncate(Local(f.body, "xs"), IntConst(1)));
    Eval(f.body, Remove(Local(f.body, "m"), IntConst(2)));

    Module module = Build(b);
    NoteAdded(module, Vector<size_t>(Counts(module).size(), 0));

    return module;
  }

  void checkContainerBuiltins() {
    const Module module = shrinkModule();
    const String json = WriteHirJson(module);

    if (!contains(json, "\"op\":\"truncate\"") || !contains(json, "\"op\":\"remove\"")) {
      fail("builtin names", "truncate and remove", json);
    }

    roundTrip("truncate and remove", module);
  }

  void checkEmpty() {
    expectEqual("empty module", "[]", WriteHirJson(Module()));
  }

  // A function's source file is written as `origin` and read back; a function without one has no such key.
  void checkOrigin() {
    Module module = moduleWith({floatConstant(1.0)});
    module.functions[0].origin = "Maths";
    const String json = WriteHirJson(module);

    if (!contains(json, "\"isCoroutine\":false,\"origin\":\"Maths\",\"at\"")) {
      fail("origin field", "\"origin\":\"Maths\" after isCoroutine", json);
    }

    expectEqual("origin read back", "Maths", ReadHirJson(json).functions[0].origin);

    if (contains(WriteHirJson(moduleWith({floatConstant(1.0)})), "origin")) {
      fail("no origin", "no origin key", json);
    }
  }

  int run() {
    const int checked = examples();
    cout << "wrote and read back " << checked << " example programs\n";

    if (checked == 0) {
      fail("examples", "some programs", "none");
    }

    return failures == 0 ? 0 : 1;
  }

}  // namespace

int main() {
  checkFloats();
  checkEscapes();
  checkEmpty();
  checkOrigin();
  checkContainerBuiltins();

  return run();
}
