// A WebAssembly binary written straight from the HIR, with no clang or wasm-ld: for a module of numbers and booleans (i64, f64, i1 values and locals, `if`, loops, calls
// between its functions). A module that allocates (records, lists, text), prints or uses a function the module does not define needs the LLVM route (`polyir --emit-wasm`,
// which links a heap), and `WasmModule` says so in `errors`.
#pragma once

#include "polyir/Hir.hpp"

namespace polyir {

  struct POLYIR_API WasmExport {
    String function;    // the function of the module
    String exportName;  // what the host calls it
  };

  struct POLYIR_API WasmResult {
    Vector<uint8_t> bytes;
    Vector<String> errors;
  };

  // Every function of `module` is in the binary (so they can call each other); the ones in `exports` are exported under their export name.
  POLYIR_API WasmResult WasmModule(const Module& module, const Vector<WasmExport>& exports);

  // True when the module was written: no errors.
  inline bool Ok(const WasmResult& result) {
    return result.errors.empty();
  }

}  // namespace polyir
