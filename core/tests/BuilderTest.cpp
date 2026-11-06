// Structs inside structs, static functions, field defaults, annotations and `@Builder`, each checked by running a small program through
// the command line (RunPolyirCli, as `polyir-check` runs it). Every test writes its program to a file, runs it, and looks at what it printed.
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

  // ── structs inside structs ──

  // A struct declared in a struct is named through it (`Point.Step`), and its outer struct's code names it bare.
  void nestedStructs() {
    const String program = "struct Point(x: i64, y: i64) {\n"
                           "  func moved(by: Step) -> Point {\n"
                           "    return Point(x: this.x + by.dx, y: this.y + by.dy)\n"
                           "  }\n"
                           "  struct Step(dx: i64, dy: i64)\n"
                           "}\n"
                           "func main() -> void {\n"
                           "  let p: Point = Point(x: 1, y: 2).moved(Point.Step(dx: 3, dy: 4))\n"
                           "  print(\"${p.x} ${p.y}\")\n"
                           "}\n";
    const Outcome ran = run("builder_test_nested.poly", program, "--interp");

    expect(ran.status == 0 && has(ran.out, "4 6\n"), "a nested struct is used inside and outside its struct, got " + ran.out + ran.err);
    expect(has(run("builder_test_nested.poly", program, "--hir").out, "struct Point.Step{dx:i64,dy:i64}"), "the HIR keeps the path Point.Step");
  }

  // `static func` is called through its struct, with no `this`.
  void staticFunctions() {
    const String program = "struct Point(x: i64, y: i64) {\n"
                           "  static func origin() -> Point {\n"
                           "    return Point(x: 0, y: 0)\n"
                           "  }\n"
                           "}\n"
                           "func main() -> void {\n"
                           "  print(str(Point.origin().x))\n"
                           "}\n";
    const Outcome ran = run("builder_test_static.poly", program, "--interp");

    expect(ran.status == 0 && has(ran.out, "0\n"), "Point.origin() runs, got " + ran.out + ran.err);
  }

  // ── field defaults and annotations ──

  // A field left out of a struct literal takes its default; a default that is no literal of the field's type is refused.
  void fieldDefaults() {
    const String good = "struct Label(text: string, size: i64 = 12)\n"
                        "func main() -> void {\n"
                        "  print(str(Label(text: \"a\").size))\n"
                        "}\n";
    const String bad = "struct Label(size: i64 = \"big\")\n"
                       "func main() -> void {\n"
                       "}\n";

    expect(has(run("builder_test_default.poly", good, "--interp").out, "12\n"), "a left-out field takes its default");
    expect(has(run("builder_test_bad_default.poly", bad, "--check").out, "1:26: the default is no literal of the field's type, i64"),
           "a default of the wrong type is refused");
  }

  // An annotation is kept in the HIR and written back in the .poly.
  void annotationsKept() {
    const String program = "@Doc(text: \"a point\", since: 2)\n"
                           "struct Point(x: i64)\n"
                           "func main() -> void {\n"
                           "}\n";

    expect(has(run("builder_test_doc.poly", program, "--hir").out, "@Doc(text=\"a point\",since=2) struct Point"), "the HIR keeps the annotation");
    expect(has(run("builder_test_doc.poly", program, "--emit-poly").out, "@Doc(text: \"a point\", since: 2)\nstruct Point"), "the .poly keeps it");
  }

  // ── @Builder ──

  const String kPoint = "@Builder\n"
                        "struct Point(x: i64, y: i64, name: string = \"origin\", tags: mutarray<string>)\n";

  // `Point.builder().x(1).y(2).build()` makes a Point; the fields it leaves out take their defaults or start empty.
  void builderChain() {
    const String program = kPoint + "func main() -> void {\n"
                                    "  let p: Point = Point.builder().x(1).y(2).build()\n"
                                    "  print(\"${p.x} ${p.y} ${p.name} ${len(p.tags)}\")\n"
                                    "}\n";
    const Outcome ran = run("builder_test_chain.poly", program, "--interp");

    expect(ran.status == 0 && has(ran.out, "1 2 origin 0\n"), "the builder chain makes the point, got " + ran.out + ran.err);
  }

  // A builder handed to a function and filled there is checked when build() runs: a missing field stops the program.
  void builderFilledElsewhere() {
    const String program = kPoint + "func withX(b: Point.Builder) -> Point.Builder {\n"
                                    "  return b.x(7)\n"
                                    "}\n"
                                    "func main() -> void {\n"
                                    "  print(str(withX(Point.builder()).y(1).build().x))\n"
                                    "  let p: Point = withX(Point.builder()).build()\n"
                                    "}\n";
    const Outcome ran = run("builder_test_elsewhere.poly", program, "--interp");

    expect(has(ran.out, "7\n"), "a builder filled in a function builds");
    expect(ran.status == 1 && has(ran.err, "trap: Point.build: y is not set"), "a missing field stops the program, got " + ran.err);
  }

  // A chain that visibly leaves out a required field is refused when the program is checked.
  void missingFieldIsSeen() {
    const String program = kPoint + "func main() -> void {\n"
                                    "  let p: Point = Point.builder().x(1).build()\n"
                                    "}\n";
    const Outcome ran = run("builder_test_missing.poly", program, "--check");

    expect(ran.status == 1 && has(ran.out, "4:39: Point.build: y is not set"), "the missing field is reported at .build, got " + ran.out);
  }

  // The .poly of a @Builder struct says `@Builder`, not what it makes, and reads back as the same program.
  void builderPrintsBack() {
    const String program = kPoint + "func main() -> void {\n"
                                    "  print(str(Point.builder().x(1).y(2).build().y))\n"
                                    "}\n";
    const String once = run("builder_test_back.poly", program, "--emit-poly").out;
    const String twice = run("builder_test_back_again.poly", once, "--emit-poly").out;

    expect(has(once, "@Builder\nstruct Point(") && !has(once, "struct Builder"), "the .poly says @Builder, got " + once);
    expect(once == twice, "the .poly of a @Builder struct prints back the same");
  }

}  // namespace

int main() {
  nestedStructs();
  staticFunctions();
  fieldDefaults();
  annotationsKept();

  builderChain();
  builderFilledElsewhere();
  missingFieldIsSeen();
  builderPrintsBack();

  if (failures == 0) {
    cout << "BuilderTest ok\n";
  }

  return failures == 0 ? 0 : 1;
}
