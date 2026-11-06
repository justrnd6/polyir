// The checked HIR written back as .poly source: polyir's own language, so a program a front end built (or polyir read) can be
// read, edited and compiled again by polyir's front end.
//
//   const PolySource poly = WritePoly(module);
//   if (poly.problems.empty()) { cout << poly.text; }
#pragma once

#include "polyir/Std.hpp"
#include "polyir/Export.hpp"
#include "polyir/Hir.hpp"

namespace polyir {

  // The program as .poly text, and what in it has no .poly spelling (written `?what?` in the text). An empty list means the text
  // compiles back into the same program.
  struct PolySource {
    String text;
    Vector<String> problems;
  };

  POLYIR_API PolySource WritePoly(const Module& module);

}  // namespace polyir
