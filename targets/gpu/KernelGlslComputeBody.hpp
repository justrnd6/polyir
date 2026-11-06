// Shared by the Vulkan GLSL writer: the text of a compute kernel's types, statements and expressions (KernelGlsl.nina `typeText`, `stmt`, `expr`).
#pragma once

#include "polyir/Hir.hpp"

namespace polyir {

  String GlslComputeType(const TypePtr& t);  // `int`, `int64_t`, `float`, `vec3`, `ivec2`, `bvec4`; `void` for anything else (an unsigned number included)

  bool IsGlslNumber(const TypePtr& t);       // a signed integer wider than a bool, or a float

  // The statements of one compute kernel body as GLSL text, each line indented `depth` levels.
  String GlslComputeBlockCode(const StmtList& stmts, int depth);

}  // namespace polyir
