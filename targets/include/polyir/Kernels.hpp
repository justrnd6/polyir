// The GPU back ends: the shader text of a Module's kernels. A kernel is a function with a `stage` and one `ParamRole` per parameter (see Hir.hpp); it
// is written for ONE element and the GPU runs it on every element at once. These read the HIR only, so the same call works on a program checked from
// `.poly` source, built with PolyIRBuilder, read from HIR JSON, or lowered by ninac.
//
// How a compute entry point takes its data. A Metal compute kernel's entry `name` takes its element inputs and `uniform` values as buffers 0,
// 1, ... in parameter order, then the result buffer (`index` and workgroup parameters take no buffer). A Metal reduce entry takes the input
// array as buffer 0, the count as buffer 1, the output as buffer 2 and the workgroup memory as threadgroup 0. In a Vulkan GLSL shader, every
// array parameter is a storage buffer at `binding = n` in parameter order (the result `gpu_out` the next one). The element count (or a grid's
// width and height) and the `uniform` values travel in one push-constant block.
#pragma once

#include "polyir/Hir.hpp"

namespace polyir {

  // The GLSL of a program's graphics stages, one shader per stage, for desktop OpenGL 3.3 (`#version 330 core`). A `Vertex` function's `Attribute`
  // parameters are `in`, its `Output` parameters are `out` varyings (the fragment stage reads each as a `Varying` parameter of the same name) and its
  // `Position` parameter is `gl_Position`; a `Fragment` function's `Varying` parameters are `in` and its `Output` parameters are `out`. `Uniform` is
  // `uniform` in either. The stage's body assigns its outputs and returns nothing. `errors` lists what a shader cannot do (a float remainder, a call to
  // something the GPU does not have); the text is empty then.
  struct POLYIR_API ShaderText {
    String text;
    Vector<String> errors;
  };

  // One compute kernel written directly as a SPIR-V module (no GLSL, no glslang): the binary Vulkan loads, with the same buffers, push constants and
  // entry point as the GLSL shader. `errors` lists what the SPIR-V writer cannot do (the words are empty then).
  struct POLYIR_API SpirvBinary {
    Vector<uint32_t> words;
    Vector<String> errors;
  };

  // The Metal Shading Language for every compute and reduce kernel of `module`, in the order they were written; empty when it has none. A
  // compute kernel is written as the per-element function `name_item` and the entry point `name` (its buffers: see the top of the file). A
  // `Reduce` function is written as `name_combine` and an entry point `name` that folds `__in[0..__count)` in one workgroup. Each thread folds
  // a slice of the input in a register. The partial results are merged pairwise in workgroup memory, and thread 0 writes the answer to `__out[0]`.
  POLYIR_API String MetalSource(const Module& module);

  POLYIR_API ShaderText GlslStageSource(const Module& module, KernelStage stage);  // `stage` is Vertex or Fragment

  // Vulkan compute shaders in GLSL (`#version 450`), one per compute kernel in the order they were written, joined by blank lines. Each comes
  // after a `// ==== name.comp ====` line, so the text can be split into files. Each shader holds `name_item`, the `_item` functions of the
  // earlier kernels it calls, and `main`. The top of the file describes their buffers and push constants. Empty when the module has no compute
  // kernel.
  POLYIR_API String GlslComputeSource(const Module& module);

  // What `polyir --emit-glsl` writes: when the module has graphics stages, one `#version 330 core` shader per `Vertex` / `Fragment` function, each after
  // a `// ==== name.vert ====` (or `.frag`) line; otherwise `GlslComputeSource`. `errors` names what a stage cannot do (the text is empty then).
  POLYIR_API ShaderText GlslSource(const Module& module);

  POLYIR_API SpirvBinary SpirvModule(const Module& module, const String& kernel);

  // What `polyir --emit-spirv` writes: every compute kernel's module after a `// ==== name.spv ====` line, as `SpirvHex` text, joined by blank lines.
  // The first kernel the writer cannot handle ends it with its reasons (`name: reason`).
  POLYIR_API ShaderText SpirvSource(const Module& module);

  // SPIR-V words as hexadecimal text, eight words to a line, each word's bytes in the order a binary file holds them (lowest first). `xxd -r -p`
  // turns it into the `.spv` file.
  POLYIR_API String SpirvHex(const Vector<uint32_t>& words);

  // The names of the module's kernels of one stage, in order.
  POLYIR_API Vector<String> KernelNames(const Module& module, KernelStage stage);

}  // namespace polyir
