/* The GPU part of polyir's C ABI, in libpolyir-lang-targets: a program made through polyir.h, with its kernels marked there
 * (polyir_function_kernel, polyir_function_param_role), written as a shader.
 *
 * polyir_emit_metal writes the compute and reduce kernels as Metal Shading Language and polyir_emit_glsl the "vertex" or
 * "fragment" stage as GLSL; the text is polyir_output, valid until the next compile or emit. Both return 0 and set
 * polyir_error when the program is wrong or the GPU cannot run it. Link libpolyir-lang-targets (and libpolyir) to use them.
 */
#ifndef POLYIR_KERNELS_H
#define POLYIR_KERNELS_H

#include "polyir/polyir.h"

#ifdef __cplusplus
extern "C" {
#endif

  POLYIR_C_API int32_t polyir_emit_metal(int64_t ctx);

  POLYIR_C_API int32_t polyir_emit_glsl(int64_t ctx, const char* stage);

#ifdef __cplusplus
}
#endif

#endif
