// The containers, sum values, closures and interfaces through the C ABI only.
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
    const String ll = "capi2_" + name + ".ll";
    OFStream(ll) << ir;
    const String build = "clang -x ir -O1 -Wno-override-module " + ll + " -o ./capi2_" + name + ".bin 2> capi2_" + name + ".log";

    return system(build.c_str()) == 0;
  }

  int execute(const String& name, const char* ir) {
    if (!assemble(name, ir)) {
      return -1;
    }

    const int status = system(("./capi2_" + name + ".bin").c_str());

    return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
  }

  void expect(const String& name, int wanted, int got) {
    cout << (wanted == got ? "ok   " : "FAIL ") << name << " = " << got << (wanted == got ? "" : " (wanted " + to_string(wanted) + ")") << "\n";
    failures += wanted == got ? 0 : 1;
  }

  // A context with a main() to write into.
  struct Program {
    int64_t context = polyir_context_new();
    int64_t i64 = polyir_type_int(context, 64);
    int64_t body = 0;

    Program() {
      const int64_t main = polyir_function_begin(context, "main", i64);
      body = polyir_function_body(context, main);
    }

    ~Program() {
      polyir_context_free(context);
    }
  };

  int64_t number(const Program& p, int64_t n) {
    return polyir_const_int(p.context, p.i64, n);
  }

  int64_t local(const Program& p, const char* name) {
    return polyir_local(p.context, p.body, name);
  }

  int64_t list(const Program& p, InitializerList<int64_t> values) {
    const int64_t args = polyir_args_new(p.context);

    for (const int64_t v : values) {
      polyir_args_push(p.context, args, v);
    }

    return args;
  }

  void run(const Program& p, const String& name, int wanted) {
    if (!polyir_compile(p.context)) {
      cout << "FAIL " << name << ": " << polyir_error(p.context) << "\n";
      failures++;
      return;
    }

    expect(name, wanted, execute(name, polyir_output(p.context)));
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

  void fillList(Program& p) {
    const int64_t array = polyir_type_array(p.context, p.i64, 1);
    polyir_let(p.context, p.body, "xs", polyir_new_container(p.context, array, 0));
    polyir_eval(p.context, p.body, polyir_push(p.context, local(p, "xs"), number(p, 40)));
    polyir_eval(p.context, p.body, polyir_push(p.context, local(p, "xs"), number(p, 2)));
  }

  void makeOptionals(Program& p) {
    polyir_let(p.context, p.body, "maybe", polyir_some(p.context, polyir_index(p.context, local(p, "xs"), list(p, {number(p, 1)}))));
    polyir_let(p.context, p.body, "nothing", polyir_none(p.context, p.i64));
  }

  void addIfSome(Program& p) {
    polyir_var(p.context, p.body, "r", p.i64, number(p, 0));
    const int64_t some = polyir_if(p.context, p.body, polyir_is_some(p.context, local(p, "maybe")));
    polyir_set(p.context, polyir_if_then(p.context, some), "r", polyir_binary(p.context, POLYIR_ADD, polyir_unwrap(p.context, local(p, "maybe")), polyir_index(p.context, local(p, "xs"), list(p, {number(p, 0)}))));
  }

  void addIfNone(Program& p) {
    const int64_t none = polyir_if(p.context, p.body, polyir_is_none(p.context, local(p, "nothing")));
    polyir_set(p.context, polyir_if_then(p.context, none), "r", polyir_binary(p.context, POLYIR_ADD, local(p, "r"), polyir_len(p.context, local(p, "xs"))));
  }

  void listsAndOptionals() {
    Program p;
    fillList(p);
    makeOptionals(p);

    addIfSome(p);
    addIfNone(p);

    polyir_ret(p.context, p.body, local(p, "r"));
    run(p, "lists_and_optionals", 44);
  }

  void makeBump(Program& p) {
    const int64_t closure = polyir_closure_begin(p.context, p.body, p.i64);
    polyir_closure_param(p.context, closure, "by", p.i64);
    const int64_t inner = polyir_closure_body(p.context, closure);

    polyir_set(p.context, inner, "count", polyir_binary(p.context, POLYIR_ADD, polyir_local(p.context, inner, "count"), polyir_local(p.context, inner, "by")));
    polyir_ret(p.context, inner, polyir_local(p.context, inner, "count"));
    polyir_let(p.context, p.body, "bump", polyir_closure_end(p.context, closure));
  }

  void closureSharesTheVariable() {
    Program p;
    polyir_var(p.context, p.body, "count", p.i64, number(p, 10));
    makeBump(p);

    polyir_eval(p.context, p.body, polyir_call_indirect(p.context, local(p, "bump"), list(p, {number(p, 5)})));
    polyir_eval(p.context, p.body, polyir_call_indirect(p.context, local(p, "bump"), list(p, {number(p, 7)})));

    polyir_ret(p.context, p.body, local(p, "count"));
    run(p, "closure_shares_the_variable", 22);
  }

  void squareAndShape(int64_t context, int64_t i64) {
    const int64_t square = polyir_struct_begin(context, "Square");
    polyir_struct_field(context, square, "side", i64, 0);
    const int64_t shape = polyir_interface_begin(context, "Shape");
    polyir_interface_method(context, shape, "area", 0, i64);
    polyir_implement(context, "Shape", "Square");
  }

  void areaMethod(int64_t context, int64_t i64) {
    const int64_t area = polyir_function_begin(context, "Square_area", i64);
    polyir_function_param(context, area, "self", polyir_type_struct(context, "Square"));
    const int64_t areaBody = polyir_function_body(context, area);
    const int64_t side = polyir_field(context, polyir_local(context, areaBody, "self"), "side");
    polyir_ret(context, areaBody, polyir_binary(context, POLYIR_MUL, side, side));
  }

  void interfaceMain(int64_t context, int64_t i64) {
    const int64_t body = polyir_function_body(context, polyir_function_begin(context, "main", i64));
    const int64_t fields = polyir_args_new(context);
    polyir_args_field(context, fields, "side", polyir_const_int(context, i64, 9));
    polyir_let(context, body, "s", polyir_iface_box(context, "Shape", polyir_struct_lit(context, "Square", fields)));
    polyir_ret(context, body, polyir_iface_call(context, polyir_local(context, body, "s"), "area", 0));
  }

  void buildInterface(int64_t context) {
    const int64_t i64 = polyir_type_int(context, 64);
    squareAndShape(context, i64);
    areaMethod(context, i64);
    interfaceMain(context, i64);
  }

  // `s.area()` must stay an interface call: an indirect call would print as "(*callee):" (callText in HirPrint.cpp).
  void expectHirText(int64_t context) {
    const String text = polyir_hir_text(context);
    const bool listsInterface = text.find("interface Shape{area}:Square") != String::npos;
    const bool hasNoIndirectCall = text.find("(*") == String::npos;
    const bool printed = listsInterface && hasNoIndirectCall;

    cout << (printed ? "ok   " : "FAIL ") << "hir text lists the interface\n";
    failures += printed ? 0 : 1;
  }

  void interfaceCall() {
    const int64_t context = polyir_context_new();
    buildInterface(context);

    if (compiled(context, "interface_call")) {
      expect("interface_call", 81, execute("interface_call", polyir_output(context)));
    }

    expectHirText(context);
    polyir_context_free(context);
  }

  void declareHost(int64_t context) {
    const int64_t list = polyir_host_type(context, "ArrayList", "ArrayList", 0, 0);
    polyir_host_type_binding(context, list, "java=java.util.ArrayList");
    polyir_host_type_binding(context, list, "python=list");

    polyir_bridge(context, polyir_type_string(context), "java", "java.lang.String", "$", "$");

    const int64_t add = polyir_host_member(context, "ArrayList_add", "ArrayList", "add", 0, 0);
    polyir_host_member_binding(context, add, "java=$0.add($1)");
  }

  bool shows(const String& text, const String& piece) {
    const bool found = text.find(piece) != String::npos;
    cout << (found ? "ok   " : "FAIL ") << "hir text shows " << piece << "\n";
    failures += found ? 0 : 1;

    return found;
  }

  // Host declarations are for the back ends: the text lists them, the LLVM compiler ignores them.
  void showHostText(const String& text) {
    shows(text, "host ArrayList = java=java.util.ArrayList | python=list");
    shows(text, "bridge string <-> java java.lang.String");
    shows(text, "host member ArrayList_add = java=$0.add($1)");
  }

  void hostDeclarations() {
    Program p;
    declareHost(p.context);
    polyir_ret(p.context, p.body, number(p, 7));
    showHostText(polyir_hir_text(p.context));
    run(p, "host_declarations_ignored_by_llvm", 7);
  }

  void lateBinding() {
    Program p;
    const int64_t list = polyir_host_type(p.context, "Box", "Box", 1, 0);

    // Printing the program hands the declaration to the builder, so a binding added after it must be refused.
    polyir_hir_text(p.context);
    const int64_t late = polyir_host_type_binding(p.context, list, "java=Box");
    const bool refused = late == 0 && String(polyir_error(p.context)).find("come before") != String::npos;

    cout << (refused ? "ok   " : "FAIL ") << "a binding after the declaration was used is refused\n";
    failures += refused ? 0 : 1;
  }

  void mistakes() {
    Program p;
    const int64_t made = polyir_new_container(p.context, polyir_type_array(p.context, p.i64, 0), 0);
    const int64_t pushed = polyir_push(p.context, made, number(p, 1));

    const String reason = polyir_error(p.context);
    const bool good = pushed == 0 && reason.find("not mutable") != String::npos;

    cout << (good ? "ok   " : "FAIL ") << "pushing onto an array that cannot change: " << reason << "\n";
    failures += good ? 0 : 1;
  }

}  // namespace

void HostTests() {
  hostDeclarations();
  lateBinding();
}

int main() {
  listsAndOptionals();
  closureSharesTheVariable();
  interfaceCall();
  HostTests();
  mistakes();

  cout << (failures == 0 ? "all checks passed" : "some checks failed") << "\n";

  return failures == 0 ? 0 : 1;
}
