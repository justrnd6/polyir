// The C ABI's source entry points: polyir_check_source / polyir_check_file, then the same hir_text / compile / build calls as a built program.
#include "polyir/polyir.h"
#include "polyir/Std.hpp"

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

  void checksSource() {
    const int64_t context = polyir_context_new();
    expect(polyir_check_source(context, "func main() -> i64 { return 6 * 7 }") == 1, "a good program checks");
    expect(String(polyir_hir_text(context)).find("main") != String::npos, "hir text of the checked program");

    expect(polyir_compile(context) == 1 && String(polyir_output(context)).find("define") != String::npos, "it compiles");
    expect(polyir_build_executable(context, "./capi_source_test.bin", 0) == 1, String("it builds: ") + polyir_error(context));
    expect(polyir_run_executable("./capi_source_test.bin") == 42, "and runs, exit 42");

    polyir_context_free(context);
  }

  // The string built-ins (str, s[i], slice, find), escapes, `${…}`, a text block, a list literal with `for (x in …)` and the built-in StringBuilder: the
  // program gives 42 plus the number of results that were wrong.
  const char* const kStringProgram = R"poly(func off(got: string, want: string) -> i64 {
  if (got == want) {
    return 0
  }
  return 1
}

func offInt(got: i64, want: i64) -> i64 {
  if (got == want) {
    return 0
  }
  return 1
}

func strings() -> i64 {
  let s: string = "hello, world"
  let lowest: i64 = 0 - 9223372036854775807 - 1
  var bad: i64 = off(str(0 - 7) + str(0) + str(12.toI32()) + str(lowest), "-7012-9223372036854775808")
  bad = bad + off(str(true) + str(false) + str('x') + str(65.toU8()) + str((0 - 1).toU64()) + str(s), "truefalsexA18446744073709551615hello, world")
  bad = bad + offInt(s[0].toI64(), 104) + offInt(s[100].toI64(), 0) + offInt(s[0 - 1].toI64(), 0)
  bad = bad + off(s.slice(0, 5) + "|" + s.slice(7, 100) + "|" + s.slice(5, 2) + "|" + s.slice(0 - 3, 2), "hello|world||he")
  bad = bad + offInt(s.find("o"), 4) + offInt(s.find("o", 5), 8) + offInt(s.find("zz"), 0 - 1) + offInt(s.find("", 3), 3)
  return bad + offInt(s.find("", 100), 12) + offInt(s.find("d", 0 - 5), 11) + offInt(s.find("hello, world!"), 0 - 1)
}

func sugar() -> i64 {
  var bad: i64 = offInt(len("a\tb\n\"\\\$"), 7) + off("n=${1 + 2}!", "n=3!")
  let t: string = """
    a
      b
    """
  bad = bad + off(t, "a\n  b\n")
  var sum: i64 = 0
  for (x in [1, 2, 3]) {
    sum = sum + x
  }
  let out = StringBuilder()
  out.line("x").indent(2).line("y")
  return bad + offInt(sum, 6) + off(out.text(), "x\n  y\n")
}

func main() -> i64 {
  return 42 + strings() + sugar()
}
)poly";

  void runsStrings() {
    const int64_t context = polyir_context_new();
    expect(polyir_check_source(context, kStringProgram) == 1, String("the string program checks: ") + polyir_error(context));
    expect(polyir_build_executable(context, "./capi_strings_test.bin", 0) == 1, String("it builds: ") + polyir_error(context));
    expect(polyir_run_executable("./capi_strings_test.bin") == 42, "and every string result is right (exit 42)");
    polyir_context_free(context);
  }

  void reportsMistakes() {
    const int64_t context = polyir_context_new();
    expect(polyir_check_source(context, "func main() -> i64 { return nope }") == 0, "a bad program fails");
    expect(String(polyir_error(context)).rfind("check failed:\n  ", 0) == 0, "with the command line's headline: " + String(polyir_error(context)));
    expect(polyir_check_file(context, "/no/such.poly") == 0 && String(polyir_error(context)).rfind("no such file", 0) == 0, "a missing file");
    polyir_context_free(context);
  }

}  // namespace

int main() {
  checksSource();
  runsStrings();
  reportsMistakes();

  if (failures == 0) {
    cout << "CApiSourceTest ok\n";
  }

  return failures == 0 ? 0 : 1;
}
