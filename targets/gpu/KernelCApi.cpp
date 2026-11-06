// The GPU part of the C ABI (polyir/polyir_kernels.h): the program a polyir.h context holds, written as Metal or GLSL.
#include "polyir/polyir_kernels.h"

#include "polyir/Builder.hpp"
#include "polyir/CApiExtension.hpp"
#include "polyir/Kernels.hpp"

using namespace polyir;

namespace {

  String metalText(const Module& program) {
    String text = MetalSource(program);

    if (text.empty()) {
      throw BuildError("the program has no compute or reduce kernel");
    }

    return text;
  }

  String glslText(const Module& program, const String& stage) {
    const ShaderText shader = GlslStageSource(program, StageNamed(stage));

    if (!shader.errors.empty()) {
      throw BuildError(shader.errors[0]);
    }

    if (shader.text.empty()) {
      throw BuildError("the program has no " + stage + " stage");
    }

    return shader.text;
  }

}  // namespace

extern "C" {

  int32_t polyir_emit_metal(int64_t ctx) {
    return WriteContextOutput(ctx, metalText);
  }

  int32_t polyir_emit_glsl(int64_t ctx, const char* stage) {
    const String name = stage;

    return WriteContextOutput(ctx, [&](const Module& program) { return glslText(program, name); });
  }

}
