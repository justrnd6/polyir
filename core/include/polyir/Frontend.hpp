// The C++ front end of polyir: source text in, checked HIR Module out. A port of the Nina front end in lang; see docs/frontend.md. The syntax tree
// is documented in src/front/Ast.hpp, the architecture of the checker in src/front/Checker.hpp, Registry.hpp and Pipeline.hpp.
#pragma once

#include "polyir/Std.hpp"
#include "polyir/Export.hpp"
#include "polyir/Hir.hpp"

namespace polyir::front {

  // The parser's own API (ParseSource, the syntax tree, the lint) is internal: include "front/Ast.hpp" and "front/Lint.hpp" from src/ to use it.

  // The result of checking a program. `module` is the checked HIR (`order` filled in, so `ModuleText(module)` prints exactly what the Nina
  // `polyir --emit-hir` prints, and `CompileToLlvm(module)` compiles it). When the program is wrong `errors` has the messages the Nina CLI prints, one
  // per entry (`line:col: message`, with `file:` in front when checked from a file), and `headline` the line the CLI prints above them:
  // "compile failed" for a syntax error, "check failed" for a mistake in the program, "no such file".
  struct FrontResult {
    Module module;
    Vector<String> errors;
    String headline;
    Vector<String> warnings;  // the lint warnings of CheckFile (the unused-`var` lint), `file:line:col: message`; they do not make the result fail
    int kernelCount = 0;  // how many `kernel func`s the checked program has
    Vector<String> kernelProblems;  // what the GPU rules say about them (the `kernel check failed:` messages of --emit-msl|glsl|spirv); empty when fine
    Vector<String> nativeProblems;  // what a native build (--check, --llvm) cannot take: host-only declarations the program uses; the CLI prints them as `compile failed:`
  };

  // Parses and checks one source held in memory (errors carry no file name).
  POLYIR_API FrontResult CheckSource(const String& text);

  // Reads, parses and checks a file (errors carry its normalized path). Imports are not resolved yet.
  POLYIR_API FrontResult CheckFile(const String& path);

  // The program checked with no errors.
  inline bool Ok(const FrontResult& result) {
    return result.errors.empty();
  }

}  // namespace polyir::front
