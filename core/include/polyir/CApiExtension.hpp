// Lets another library add C functions of its own that work on a polyir.h context (libpolyir-lang-targets does this for
// polyir_kernels.h). WriteContextOutput hands the context's program to a writer and keeps the text it returns as
// polyir_output; when the writer throws, the message becomes polyir_error and the result is 0.
#pragma once

#include "polyir/Std.hpp"
#include "polyir/Export.hpp"
#include "polyir/Hir.hpp"

namespace polyir {

  using ProgramWriter = Function<String(const Module&)>;

  // 1 when `write` gave the text, 0 (and the reason in polyir_error) when it threw or `contextId` is not a context.
  POLYIR_API int32_t WriteContextOutput(int64_t contextId, const ProgramWriter& write);

}  // namespace polyir
