// The kernel rules (KernelVerify.nina, KernelRoles.nina, Kernels.nina): which `kernel func`s a GPU can run. Only the checking half exists here, the half
// that reports mistakes as `line:col: message` (what `polyir --emit-msl | --emit-glsl | --emit-spirv` prints under "kernel check failed:"); the shader
// text itself is not written by this library.
#pragma once

#include "front/Session.hpp"

namespace polyir::front {

  // What the kernel rules say about the checked program.
  struct KernelReport {
    int count = 0;            // how many `kernel func`s the program has
    Vector<String> problems;  // one message per violation, kernel by kernel in source order, each with `file:` in front when the program came from a file
  };

  // How a `kernel func` parameter is used by the GPU: the number the Nina KernelRoles uses. The numbers must stay the same as ParamRole's in
  // polyir/Hir.hpp (`Value` is ParamRole::Uniform there), because MarkKernel turns one into the other with a cast.
  enum class KernelRole : int { Element = 0, Value = 1, Buffer = 2, Index = 3, Writable = 4, Workgroup = 5 };

  // The roles of the parameters of a kernel's declaration, read from its marks and types.
  Vector<KernelRole> KernelRoles(const Program& program, const AstFuncDecl& decl);

  // Records on a function's HIR that it is a compute kernel, with the role of each parameter (`FuncDef::stage`, `FuncDef::roles`).
  void MarkKernel(FuncDef& function, const Program& program, const AstFuncDecl& decl);

  // The problems of one kernel. `before` are the kernels it may call ("name"; "grouped name" for one with workgroup memory, which it may not).
  Vector<String> KernelProblems(const FuncDef& function, const Vector<KernelRole>& roles, const Vector<String>& before);

  // Finds the kernels of a checked session and verifies each one.
  KernelReport CheckKernels(const Session& session);

}  // namespace polyir::front
