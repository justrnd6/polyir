// The parts of the SPIR-V kernel writer (SpirvKernel.cpp, SpirvMain.cpp).
#pragma once

#include "gpu/KernelCommon.hpp"
#include "gpu/SpirvLower.hpp"

namespace polyir {

  // Writes `main` for the kernel `def` whose function is `item`, and returns its `OpExecutionMode` (the entry point and the names go to their piles).
  Words SpvWriteMain(SpvWriter& writer, const FuncDef& def, const SpvKnown& item);

}  // namespace polyir
