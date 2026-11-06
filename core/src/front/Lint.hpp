// The lint pass over a parsed program (UnusedVarLint.cpp, ported from UnusedVarLint.nina). It is syntactic: it reads the syntax tree only,
// before and whether or not the program checks.
#pragma once

#include "front/Ast.hpp"

namespace polyir::front {

  // A warning for each `var` that is never assigned in its own function, in the order the Nina lint reports them. Each is
  // `file:line:col: 'x' is declared `var` but never reassigned in 'f' — use `let` instead` (a method is `Struct.method`;
  // no `file:` when `file` is empty).
  POLYIR_API Vector<String> LintWarnings(const Program& program, const String& file);

}  // namespace polyir::front
