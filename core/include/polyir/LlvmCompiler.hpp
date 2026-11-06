// Compiles a polyir Module to LLVM IR text (opaque-pointer syntax, LLVM 15 and later). The result can be assembled by `clang -x ir`, `llc`, `lli`, or
// read by anything that takes `.ll`.
//
// How polyir maps: `i1`..`i64` and `u8`..`u64` are `i1`..`i64`; `f32`/`f64` are `float`/`double`; a string, a struct and a pointer are `ptr` (a
// struct is a heap object shared by reference, made by the garbage collector); `+ - *` wrap, a shift takes its count modulo the width, and the smallest signed
// number divided by -1 wraps. Every variable is a stack slot (run `opt -O1` or compile with `-O1` to turn them into registers). A function
// `main` with no parameters becomes `polyir_main`, and an `i32 main()` calls it and returns its result, so the program's result is its exit status.
#pragma once

#include "polyir/Std.hpp"
#include "polyir/Export.hpp"
#include "polyir/Hir.hpp"

#include <string>
#include <vector>

namespace polyir {

  struct POLYIR_API CompileOptions {
    String targetTriple;  // written as the module's `target triple` when not empty
    bool debugInfo = false;  // describe the functions, variables and the line and column of each statement and expression as DWARF metadata
    String sourceFile;  // the file the program was written in (named in the debug info and the module's header)
    String sourceDirectory;  // ... and the directory it is in
    // Objects come from a garbage collector (docs/memory.md, Part 2) and are reclaimed once unreachable; off, they come from malloc and stay until
    // the program's `Free`. A wasm target triple never collects.
    bool collectGarbage = true;
  };

  struct POLYIR_API CompileResult {
    String ir;
    Vector<String> errors;
  };

  // One function compiled on its own: its definition (and any helper functions it needed, such as closure bodies) as `text`, and the module-level lines
  // it relies on as `declarations`. Add it to a module with `AddLlvm(builder, text, declarations)`; functions it calls must be defined or declared
  // there, and structs it uses must be defined there too (or come in as declarations). With `options.debugInfo` the fragment carries its debug metadata:
  // `metadata` are the node bodies (the `!0`.. of the fragment's text) and `namedMetadata` the named lists ("llvm.dbg.cu=1"); `AddLlvm` takes all four, and
  // the module renumbers the `!N` references.
  struct POLYIR_API FunctionIr {
    String text;
    Vector<String> declarations;
    Vector<String> errors;
    Vector<String> metadata;
    Vector<String> namedMetadata;
  };

  POLYIR_API CompileResult CompileToLlvm(const Module& module, const CompileOptions& options = {});

  POLYIR_API FunctionIr CompileFunctionToLlvm(const Module& module, const String& function, const CompileOptions& options = {});

  // The module compiled with no errors.
  inline bool Ok(const CompileResult& result) {
    return result.errors.empty();
  }

  // The function compiled with no errors.
  inline bool Ok(const FunctionIr& result) {
    return result.errors.empty();
  }

}  // namespace polyir
