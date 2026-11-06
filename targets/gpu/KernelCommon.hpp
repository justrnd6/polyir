// What every kernel writer (Metal, GLSL and SPIR-V) shares: the math functions a kernel may call and how C, Metal and GLSL spell them, the
// names a shading language reserves, float literal text, the text helpers of the Metal and GLSL writers, and small questions about
// parameter roles, group functions and the kernels a kernel calls.
#pragma once

#include "polyir/Hir.hpp"

namespace polyir {

  constexpr int kGroupSize = 256;  // threads in one workgroup; the host launches whole groups of this many

  bool IsKernelMath(const String& name);           // a C library `float` function: `sqrtf`, `fminf`, ...

  String MslMathName(const String& name);          // Metal's name for it (`sqrt`, `metal::precise::fmod`)

  bool IsGroupFunction(const String& name);        // `groupSize`, `localIndex`, `groupIndex`, `barrier`

  String MslIdentifier(const String& name);        // a kernel's own name, with `_` added when Metal reserves it

  bool HasBuffer(ParamRole role);                  // false for `index` and workgroup parameters: the GPU supplies those

  int IndexCount(const Vector<ParamRole>& roles);  // 2 means a two-dimensional launch

  String GlslMathName(const String& name);        // GLSL's name for it (`sqrt`, `abs`, `min`); "" when GLSL has none (`fmodf`)

  String FloatLiteral(double v);                   // `2.5`, `1.0`, `0.05`, `6.2831853`: rounded to nine decimals, trailing zeros dropped; `1e-07` outside a ten-thousandth to a billion

  String ScientificLiteral(double v);              // all the digits a number needs (`1e-09`), for one outside that range

  String GlslIdentifier(const String& name);       // a kernel's own name, with `_` added when GLSL reserves it (`input_`, `gl_x_`, `vec3_`)

  bool IsKernelAtomic(const String& name);         // `atomicAdd`, `atomicAddFloat`, `atomicMin`, `atomicMax`

  bool Listed(const Vector<String>& names, const String& name);

  // ── text helpers of the Metal and GLSL writers ──
  String ShaderIndent(int depth);  // four spaces per level

  String Joined(const Vector<String>& parts, const String& separator);

  String ReplacedAll(String text, const String& from, const String& to);  // every `from` in `text` replaced by `to`

  const char* OperatorSymbol(BinOp op);  // how Metal and GLSL both spell a binary operator: `+`, `==`, `&&`, ...

  const char* UnaryPrefix(UnOp op);  // the open bracket and the operator of a unary expression: `(-`, `(!` or `(~`

  // The callee of every call in a kernel's body, answered from the HIR (HirWalk.nina). Nested blocks and arguments are included, in
  // the order they are met, repeats included.
  Vector<String> CalleeNames(const StmtList& body);

  // The local passed first to each call of `callee` in a kernel's body (`atomicAddFloat(sums, ...)` gives `sums`).
  Vector<String> FirstArguments(const StmtList& body, const String& callee);

  // Every kernel `target` calls among `earlier`, directly or through each other (four levels, as in Nina's KernelGlsl.nina `calledKernels`).
  Vector<String> CalledKernels(const FuncDef& target, const Vector<const FuncDef*>& earlier);

  Vector<const FuncDef*> ComputeKernels(const Module& module);  // the module's compute kernels in the order they were written

  Vector<const FuncDef*> KernelsBefore(const Vector<const FuncDef*>& kernels, size_t i);  // the first `i`: the ones kernel `i` may call

}  // namespace polyir
