// Reads the checked HIR the Nina front end writes (`polyir program.poly --emit-hir-json`) into a Module, so a program checked by the Nina
// implementation can be compiled, printed or inspected by this library.
#pragma once

#include "polyir/Std.hpp"
#include "polyir/Export.hpp"
#include "polyir/Hir.hpp"

#include <stdexcept>
#include <string>

namespace polyir {

  class POLYIR_API HirReadError : public RuntimeError {
    public:
      using RuntimeError::RuntimeError;
  };

  POLYIR_API Module ReadHirJson(const String& json);  // throws HirReadError

  // The module as the JSON the Nina front end writes (`polyir program.poly --emit-hir-json`, without the final newline). The
  // declarations are in the order they were written, and every node is an object whose `k` names its case. A position is
  // `[line, column]`, a float an exact C99 hexadecimal (`0x1.8p3`).
  // `ReadHirJson(WriteHirJson(m))` is `m`, and for a module the Nina checker made the text is the Nina text, byte for byte.
  POLYIR_API String WriteHirJson(const Module& module);

}  // namespace polyir
