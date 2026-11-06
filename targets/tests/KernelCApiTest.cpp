// A GPU kernel built through the C ABI only (polyir/polyir.h) and written as Metal by libpolyir-lang-targets (polyir/polyir_kernels.h).
#include "polyir/Std.hpp"
#include "polyir/polyir_kernels.h"

#include <iostream>

using namespace polyir;

namespace {

  int failures = 0;

  // `kernel func scale(x: float, k: float) -> float`: the signature of a compute kernel.
  int64_t scaleKernel(int64_t ctx) {
    const int64_t f32 = polyir_type_float(ctx, 32);
    const int64_t scale = polyir_function_begin(ctx, "scale", f32);
    polyir_function_param(ctx, scale, "x", f32);
    polyir_function_param(ctx, scale, "k", f32);
    polyir_function_kernel(ctx, scale, "compute");

    return scale;
  }

  // `x` is an element, `k` a uniform; the body is `return x * k`.
  void scaleRolesAndBody(int64_t ctx, int64_t scale) {
    polyir_function_param_role(ctx, scale, 0);
    polyir_function_param_role(ctx, scale, 1);
    const int64_t body = polyir_function_body(ctx, scale);
    polyir_ret(ctx, body, polyir_binary(ctx, POLYIR_MUL, polyir_local(ctx, body, "x"), polyir_local(ctx, body, "k")));
  }

  void checkMetal(int64_t ctx) {
    const bool written = polyir_emit_metal(ctx) == 1;
    const String metal = polyir_output(ctx);
    const bool good = written && metal.find("constant float& k [[buffer(1)]]") != String::npos && metal.find("return (x * k);") != String::npos;
    cout << (good ? "ok   " : "FAIL ") << "kernel_as_metal: " << (written ? "" : polyir_error(ctx)) << "\n";
    failures += good ? 0 : 1;
  }

  // A kernel built through the C ABI: `kernel func scale(x: float, uniform k: float) -> float { return x * k }`, written as Metal.
  void kernelAsMetal() {
    const int64_t ctx = polyir_context_new();
    scaleRolesAndBody(ctx, scaleKernel(ctx));
    checkMetal(ctx);
    polyir_context_free(ctx);
  }

  // A program with no kernel has nothing to write: 0, and the reason.
  void noKernel() {
    const int64_t ctx = polyir_context_new();
    const bool refused = polyir_emit_metal(ctx) == 0 && String(polyir_error(ctx)).find("no compute or reduce kernel") != String::npos;
    cout << (refused ? "ok   " : "FAIL ") << "no_kernel: " << polyir_error(ctx) << "\n";
    failures += refused ? 0 : 1;
    polyir_context_free(ctx);
  }

}  // namespace

int main() {
  kernelAsMetal();
  noKernel();
  cout << (failures == 0 ? "all checks passed" : "some checks failed") << "\n";

  return failures == 0 ? 0 : 1;
}
