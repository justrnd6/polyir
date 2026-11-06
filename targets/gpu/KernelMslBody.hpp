// Shared by the Metal writers: the text of a kernel's types, statements and expressions (KernelMsl.nina `type`, `stmt`, `expr`).
#pragma once

#include "polyir/Hir.hpp"

namespace polyir {

  String MslType(const TypePtr& t);

  String MslParamType(const TypePtr& t, ParamRole role);  // a `writable` array is `device T*`, workgroup memory is `threadgroup T*`

  String MslPointerType(const char* space, const TypePtr& t);

  // The statements of one function body as Metal text, each line indented `depth` levels.
  String MslBlockCode(const StmtList& stmts, int depth);

}  // namespace polyir
