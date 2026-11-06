// Native debug info that names a program's first source (core/docs/translate.md, "Spans and source maps"). A `.poly` text another
// front end wrote says where its lines came from (`//# from Hops.nina:3:5`). With debug info asked for, every function, statement
// and variable is placed there, so a debugger stops at Hops.nina's lines. A program that says nothing is placed in its own lines.
#include "polyir/Frontend.hpp"
#include "polyir/LlvmCompiler.hpp"

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

  bool has(const String& text, const String& part) {
    return text.find(part) != String::npos;
  }

  // The program's LLVM IR with debug info, for a program in /work/Hops.poly.
  String debugIr(const String& program) {
    const FrontResult checked = CheckSource(program);
    CompileOptions options;
    options.debugInfo = true;
    options.sourceFile = "Hops.poly";
    options.sourceDirectory = "/work";

    if (!Ok(checked)) {
      cerr << "the test's program does not check: " << checked.errors[0] << "\n";
    }

    return CompileToLlvm(checked.module, options).ir;
  }

  const String kFromNina = "//# from Hops.nina:2:8 action f(a: Int) -> Int {\nfunc f(a: i64) -> i64 {\n"
                           "  //# from Hops.nina:3:5 let b = a + 1\n  let b: i64 = a + 1\n  return b\n}\n";

  // The function and its statements are placed in Hops.nina, found from the checked file's directory.
  void placedInTheFirstSource() {
    const String ir = debugIr(kFromNina);

    expect(has(ir, "!DIFile(filename: \"/work/Hops.nina\", directory: \"/work\")"), "a file for Hops.nina, got " + ir);
    expect(has(ir, "!DISubprogram(name: \"f\"") && has(ir, ", line: 2, type: "), "f at Hops.nina's line 2");
    expect(has(ir, "!DILocation(line: 3, column: 5"), "the let at line 3, column 5");
    expect(has(ir, "!DILocalVariable(name: \"b\"") && has(ir, "line: 3, type: "), "b declared at line 3");
  }

  // A line in another file than its function's is scoped to a block of the function in that file.
  void anotherFileInOneFunction() {
    const String program = "//# from Hops.nina:2:8\nfunc f(a: i64) -> i64 {\n  //# from Other.nina:9:3\n  let b: i64 = a + 1\n  return b\n}\n";
    const String ir = debugIr(program);

    expect(has(ir, "!DIFile(filename: \"/work/Other.nina\"") && has(ir, "!DILexicalBlockFile("), "a block of f in Other.nina, got " + ir);
    expect(has(ir, "!DILocation(line: 9, column: 3"), "the let at Other.nina's line 9");
  }

  // A program that says nothing of where its lines came from is placed in its own lines, in its own file.
  void ownLinesWithoutFromLines() {
    const String ir = debugIr("func f(a: i64) -> i64 {\n  let b: i64 = a + 1\n  return b\n}\n");

    expect(!has(ir, "Hops.nina") && !has(ir, "DILexicalBlockFile"), "no other file, got " + ir);
    expect(has(ir, "!DIFile(filename: \"Hops.poly\", directory: \"/work\")") && has(ir, ", line: 1, type: "), "f at its own line 1");
  }

}  // namespace

int main() {
  placedInTheFirstSource();
  anotherFileInOneFunction();
  ownLinesWithoutFromLines();

  if (failures == 0) {
    cout << "DebugSourceTest ok\n";
  }

  return failures == 0 ? 0 : 1;
}
