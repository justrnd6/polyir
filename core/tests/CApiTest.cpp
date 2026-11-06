// Builds programs through the C ABI only (include/polyir/polyir.h), compiles them, and runs them.
#include "polyir/Std.hpp"
#include "polyir/polyir.h"

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <sys/wait.h>

using namespace polyir;

namespace {

  int failures = 0;

  bool assemble(const String& name, const char* ir) {
    const String ll = "capi_" + name + ".ll";
    OFStream(ll) << ir;
    const String build = "clang -x ir -O1 -Wno-override-module " + ll + " -o ./capi_" + name + ".bin 2> capi_" + name + ".log";

    return system(build.c_str()) == 0;
  }

  int execute(const String& name, const char* ir) {
    if (!assemble(name, ir)) {
      return -1;
    }

    const int status = system(("./capi_" + name + ".bin").c_str());

    return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
  }

  void expect(const String& name, int wanted, int got) {
    cout << (wanted == got ? "ok   " : "FAIL ") << name << " = " << got << (wanted == got ? "" : " (wanted " + to_string(wanted) + ")") << "\n";
    failures += wanted == got ? 0 : 1;
  }

  // True when the context compiled; otherwise reports why and counts the failure.
  bool compiled(int64_t context, const String& label) {
    if (polyir_compile(context)) {
      return true;
    }

    cout << "FAIL " << label << ": " << polyir_error(context) << "\n";
    failures++;

    return false;
  }

  void conclude(int64_t context, const String& label, const String& name, int wanted) {
    if (compiled(context, label)) {
      expect(name, wanted, execute(label, polyir_output(context)));
    }

    polyir_context_free(context);
  }

  int64_t constant(int64_t context, int bits, int64_t n) {
    return polyir_const_int(context, polyir_type_int(context, bits), n);
  }

  void factLoop(int64_t context, int64_t i64, int64_t body) {
    const int64_t loop = polyir_while(context, body, polyir_binary(context, POLYIR_LE, polyir_local(context, body, "i"), polyir_local(context, body, "n")));
    polyir_set(context, loop, "r", polyir_binary(context, POLYIR_MUL, polyir_local(context, loop, "r"), polyir_local(context, loop, "i")));
    polyir_set(context, loop, "i", polyir_binary(context, POLYIR_ADD, polyir_local(context, loop, "i"), polyir_const_int(context, i64, 1)));
  }

  void factDefinition(int64_t context, int64_t i64) {
    const int64_t fact = polyir_function_begin(context, "fact", i64);
    polyir_function_param(context, fact, "n", i64);
    const int64_t body = polyir_function_body(context, fact);

    polyir_var(context, body, "r", i64, polyir_const_int(context, i64, 1));
    polyir_var(context, body, "i", i64, polyir_const_int(context, i64, 2));
    factLoop(context, i64, body);
    polyir_ret(context, body, polyir_local(context, body, "r"));
  }

  void factMain(int64_t context, int64_t i64) {
    const int64_t main = polyir_function_begin(context, "main", i64);
    const int64_t mainBody = polyir_function_body(context, main);
    const int64_t args = polyir_args_new(context);
    polyir_args_push(context, args, polyir_const_int(context, i64, 5));
    polyir_ret(context, mainBody, polyir_binary(context, POLYIR_REM, polyir_call(context, "fact", args), polyir_const_int(context, i64, 256)));
  }

  // func fact(n: i64) -> i64 { var r = 1; var i = 2; while (i <= n) { r = r * i; i = i + 1 }; return r }  and main() = fact(5) % 256
  void factorial() {
    const int64_t context = polyir_context_new();
    const int64_t i64 = polyir_type_int(context, 64);
    factDefinition(context, i64);
    factMain(context, i64);
    conclude(context, "factorial", "factorial", 120);
  }

  void pointStruct(int64_t context, int64_t i64) {
    const int64_t pt = polyir_struct_begin(context, "Pt");
    polyir_struct_field(context, pt, "x", i64, 1);
    polyir_struct_field(context, pt, "y", i64, 0);
  }

  void putsAndTotal(int64_t context, int64_t i64) {
    const int64_t puts = polyir_extern_begin(context, "puts", polyir_type_int(context, 32));
    polyir_extern_param(context, puts, "line", polyir_type_string(context));
    polyir_global(context, "total", i64, polyir_const_int(context, i64, 10));
  }

  void pointLet(int64_t context, int64_t i64, int64_t body) {
    const int64_t fields = polyir_args_new(context);
    polyir_args_field(context, fields, "x", polyir_const_int(context, i64, 3));
    polyir_args_field(context, fields, "y", polyir_const_int(context, i64, 4));
    polyir_let(context, body, "p", polyir_struct_lit(context, "Pt", fields));
  }

  void sayHello(int64_t context, int64_t body) {
    const int64_t say = polyir_args_new(context);
    polyir_args_push(context, say, polyir_const_string(context, "from the C ABI"));
    polyir_eval(context, body, polyir_call(context, "puts", say));
  }

  int64_t pointSum(int64_t context, int64_t body) {
    return polyir_binary(context, POLYIR_ADD, polyir_field(context, polyir_local(context, body, "p"), "x"), polyir_field(context, polyir_local(context, body, "p"), "y"));
  }

  void branchOnSum(int64_t context, int64_t i64, int64_t body, int64_t sum) {
    const int64_t branch = polyir_if(context, body, polyir_binary(context, POLYIR_GT, sum, polyir_const_int(context, i64, 5)));
    polyir_assign(context, polyir_if_then(context, branch), polyir_global_ref(context, "total"), polyir_const_int(context, i64, 20));
    polyir_assign(context, polyir_if_else(context, branch), polyir_global_ref(context, "total"), polyir_const_int(context, i64, 30));
  }

  void structsMain(int64_t context, int64_t i64) {
    const int64_t body = polyir_function_body(context, polyir_function_begin(context, "main", i64));
    pointLet(context, i64, body);
    sayHello(context, body);

    const int64_t sum = pointSum(context, body);
    branchOnSum(context, i64, body, sum);
    polyir_ret(context, body, polyir_binary(context, POLYIR_ADD, polyir_global_ref(context, "total"), sum));
  }

  // A struct, a global and a call to puts, with an if/else.
  void structsAndBranches() {
    const int64_t context = polyir_context_new();
    const int64_t i64 = polyir_type_int(context, 64);

    pointStruct(context, i64);
    putsAndTotal(context, i64);
    structsMain(context, i64);
    conclude(context, "structs", "structs_and_branches", 27);
  }

  // A mistake gives 0 and a reason, and the context keeps working.
  void mistakes() {
    const int64_t context = polyir_context_new();
    const int64_t bad = polyir_binary(context, POLYIR_ADD, constant(context, 64, 1), constant(context, 32, 1));
    const String reason = polyir_error(context);
    const bool good = bad == 0 && reason.find("different types") != String::npos && constant(context, 64, 2) != 0;

    cout << (good ? "ok   " : "FAIL ") << "mistakes: " << reason << "\n";
    failures += good ? 0 : 1;
    polyir_context_free(context);
  }

}  // namespace

int main() {
  factorial();
  structsAndBranches();
  mistakes();
  cout << (failures == 0 ? "all checks passed" : "some checks failed") << "\n";

  return failures == 0 ? 0 : 1;
}
