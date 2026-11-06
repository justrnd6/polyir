// A Module's compute kernels as Vulkan compute shaders in GLSL (KernelGlsl.nina), the Vulkan twin of the Metal writer: `name_item` is the kernel for
// ONE element and `main` is the entry point each GPU thread starts. What differs from Metal:
//   * a GLSL file holds ONE entry point, so every kernel gets its own shader, which also carries the `_item` functions of the earlier kernels it calls;
//   * each array (an element input, a `uniform` or `writable` array) is a storage buffer at `binding = n` in parameter order, the result the next one;
//     GLSL has no pointers, so a whole array is read by its name, not passed. The `uniform` values travel in one push-constant block after the count;
//   * Vulkan starts whole groups of threads, so `main` returns early past the end of the data (a kernel with workgroup memory only skips the store);
//   * `%` on floats (and `fmodf`) is the helper `gpu_fmod`, which corrects an inexact GPU quotient.
#include "polyir/Kernels.hpp"
#include "gpu/KernelCommon.hpp"
#include "gpu/KernelGlslComputeBody.hpp"

namespace polyir {

  namespace {

    const char* const kVersion = "#version 450\n#extension GL_EXT_shader_explicit_arithmetic_types : require\n";

    // C's `fmod` (the sign of `x`, a size below |y|) without trusting the GPU's division: the quotient is rounded down and a remainder that lands outside
    // [0, |y|) is moved back in. A vector is done one lane at a time.
    const char* const kRemainderHelper =
      "float gpu_fmod(float x, float y) {\n"
      "    float ax = abs(x);\n    float ay = abs(y);\n"
      "    float r = ax - floor(ax / ay) * ay;\n"
      "    r = (r < 0.0) ? r + ay : r;\n    r = (r >= ay) ? r - ay : r;\n"
      "    return (x < 0.0) ? -r : r;\n}\n"
      "vec2 gpu_fmod(vec2 x, vec2 y) { return vec2(gpu_fmod(x.x, y.x), gpu_fmod(x.y, y.y)); }\n"
      "vec3 gpu_fmod(vec3 x, vec3 y) { return vec3(gpu_fmod(x.x, y.x), gpu_fmod(x.y, y.y), gpu_fmod(x.z, y.z)); }\n"
      "vec4 gpu_fmod(vec4 x, vec4 y) { return vec4(gpu_fmod(x.x, y.x), gpu_fmod(x.y, y.y), gpu_fmod(x.z, y.z), gpu_fmod(x.w, y.w)); }\n\n";

    // GLSL can only swap integers atomically, so a float array that `atomicAddFloat` updates is declared a second time at the same binding as `uint`,
    // with a compare-and-swap helper. `@` stands for the array's name, `#` for its binding.
    const char* const kFloatAtomicView =
      "layout(std430, set = 0, binding = #) buffer @_bits_block { uint @_bits[]; };\n"
      "float gpu_add_float_@(int slot, float amount) {\n"
      "    uint seen = @_bits[slot];\n"
      "    for (int tries = 0; tries < 1000000; tries = tries + 1) {\n"
      "        uint wanted = floatBitsToUint(uintBitsToFloat(seen) + amount);\n"
      "        uint previous = atomicCompSwap(@_bits[slot], seen, wanted);\n"
      "        if (previous == seen) { return uintBitsToFloat(previous); }\n"
      "        seen = previous;\n"
      "    }\n"
      "    return 0.0;\n"
      "}\n";

    const char* const kBarrierHelper = "void gpu_barrier() {\n    memoryBarrierShared();\n    barrier();\n}\n";

    bool usesWorkgroup(const Vector<ParamRole>& roles) {
      return find(roles.begin(), roles.end(), ParamRole::Workgroup) != roles.end();
    }

    // An element input (read from `<name>_in`), a `uniform` array and a `writable` one each get a storage buffer.
    bool getsStorageBuffer(ParamRole role) {
      return role == ParamRole::Element || role == ParamRole::Buffer || role == ParamRole::Writable;
    }

    // Arrays and workgroup memory are global names in GLSL, so they are not passed to an `_item` function.
    bool isPassedToKernelFunction(ParamRole role) {
      return role != ParamRole::Buffer && role != ParamRole::Writable && role != ParamRole::Workgroup;
    }

    TypePtr pointee(const TypePtr& t) {
      return t->kind == TypeKind::Pointer ? t->element : t;
    }

    String storage(const String& access, const String& name, const TypePtr& t, int binding) {
      return "layout(std430, set = 0, binding = " + to_string(binding) + ") " + access + " buffer " + name + "_block { " + GlslComputeType(t) + " " + name +
        "[]; };\n";
    }

    // What a kernel needs its enclosing shader to declare: "read name" for an array it reads, "write name" (and "read name") for one it writes.
    Vector<String> bufferNames(const FuncDef& def) {
      Vector<String> out;

      for (size_t i = 0; i < def.parameters.size(); i++) {
        if (def.roles[i] == ParamRole::Buffer) {
          out.push_back("read " + def.parameters[i].name);
        }

        if (def.roles[i] == ParamRole::Writable) {
          out.insert(out.end(), {"write " + def.parameters[i].name, "read " + def.parameters[i].name});
        }
      }

      return out;
    }

    bool allDeclared(const Vector<String>& needed, const Vector<String>& have) {
      for (const String& name : needed) {
        if (!Listed(have, name)) {
          return false;
        }
      }

      return true;
    }

    String item(const FuncDef& def) {
      Vector<String> params;

      for (size_t i = 0; i < def.parameters.size(); i++) {
        if (isPassedToKernelFunction(def.roles[i])) {
          params.push_back(GlslComputeType(def.parameters[i].type) + " " + GlslIdentifier(def.parameters[i].name));
        }
      }

      return GlslComputeType(def.result) + " " + def.name + "_item(" + Joined(params, ", ") + ") {\n" + GlslComputeBlockCode(def.body, 1) + "}\n";
    }

    // ── one shader ──

    // The kernel the shader is for, and the kernels written before it (the ones it may call).
    struct GlslComputeKernel {
      const FuncDef& function;
      const Vector<const FuncDef*> earlierKernels;
    };

    String parameterName(const GlslComputeKernel& kernel, size_t i) {
      return GlslIdentifier(kernel.function.parameters[i].name);
    }

    ParamRole roleOf(const GlslComputeKernel& kernel, size_t i) {
      return kernel.function.roles[i];
    }

    // Threads per workgroup: an 8 by 8 tile for a grid, a whole `kGroupSize` for a kernel with workgroup memory, otherwise a line of 64.
    String groupLayout(const GlslComputeKernel& kernel) {
      if (IndexCount(kernel.function.roles) == 2) {
        return "local_size_x = 8, local_size_y = 8";
      }

      return usesWorkgroup(kernel.function.roles) ? "local_size_x = " + to_string(kGroupSize) : "local_size_x = 64";
    }

    String header(const GlslComputeKernel& kernel) {
      return String(kVersion) + "layout(" + groupLayout(kernel) + ") in;\n\n";
    }

    // The element count (or the width and the height of a grid), then the `uniform` values.
    String pushBlock(const GlslComputeKernel& kernel) {
      String members = IndexCount(kernel.function.roles) == 2 ? "    uint gpu_width;\n    uint gpu_height;\n" : "    uint gpu_count;\n";

      for (size_t i = 0; i < kernel.function.parameters.size(); i++) {
        if (roleOf(kernel, i) == ParamRole::Uniform) {
          members += "    " + GlslComputeType(kernel.function.parameters[i].type) + " " + parameterName(kernel, i) + ";\n";
        }
      }

      return "layout(push_constant) uniform Params {\n" + members + "};\n";
    }

    // The binding of parameter `i`: the number of arrays before it.
    int bindingOf(const GlslComputeKernel& kernel, size_t i) {
      int binding = 0;

      for (size_t j = 0; j < i; j++) {
        if (getsStorageBuffer(roleOf(kernel, j))) {
          binding++;
        }
      }

      return binding;
    }

    // An element input is read as `x_in[gpu_gid]`; a uniform array is used by its own name.
    String inputBuffer(const GlslComputeKernel& kernel, size_t i, int binding) {
      const TypePtr& t = kernel.function.parameters[i].type;

      if (roleOf(kernel, i) == ParamRole::Buffer) {
        return storage("readonly", parameterName(kernel, i), pointee(t), binding);
      }

      if (roleOf(kernel, i) == ParamRole::Writable) {
        return storage("", parameterName(kernel, i), pointee(t), binding);
      }

      return storage("readonly", parameterName(kernel, i) + "_in", t, binding);
    }

    String floatAtomicView(const GlslComputeKernel& kernel, size_t i, const Vector<String>& floatArrays, int binding) {
      if (!Listed(floatArrays, kernel.function.parameters[i].name)) {
        return "";
      }

      return ReplacedAll(ReplacedAll(kFloatAtomicView, "@", parameterName(kernel, i)), "#", to_string(binding));
    }

    // Each array gets a storage buffer in parameter order; the result is the next binding.
    String buffers(const GlslComputeKernel& kernel) {
      String out;
      const Vector<String> floatArrays = FirstArguments(kernel.function.body, "atomicAddFloat");

      for (size_t i = 0; i < kernel.function.parameters.size(); i++) {
        if (getsStorageBuffer(roleOf(kernel, i))) {
          out += inputBuffer(kernel, i, bindingOf(kernel, i)) + floatAtomicView(kernel, i, floatArrays, bindingOf(kernel, i));
        }
      }

      return out + storage("writeonly", "gpu_out", kernel.function.result, bindingOf(kernel, kernel.function.parameters.size()));
    }

    // `shared float name[256];`: memory the threads of one group share, one slot per thread.
    String workgroupArrays(const GlslComputeKernel& kernel) {
      String out;

      for (size_t i = 0; i < kernel.function.parameters.size(); i++) {
        if (roleOf(kernel, i) == ParamRole::Workgroup) {
          out += "shared " + GlslComputeType(pointee(kernel.function.parameters[i].type)) + " " + parameterName(kernel, i) + "[" + to_string(kGroupSize) + "];\n";
        }
      }

      return out;
    }

    String bindings(const GlslComputeKernel& kernel) {
      const bool usesBarrier = Listed(CalleeNames(kernel.function.body), "barrier");

      return pushBlock(kernel) + buffers(kernel) + workgroupArrays(kernel) + (usesBarrier ? kBarrierHelper : "");
    }

    // An earlier kernel that reads (or writes) an array by name is included only when this shader declares that array (writable, if it is written).
    String earlierItems(const GlslComputeKernel& kernel) {
      const Vector<String> have = bufferNames(kernel.function);
      const Vector<String> called = CalledKernels(kernel.function, kernel.earlierKernels);
      String out;

      for (const FuncDef* def : kernel.earlierKernels) {
        if (Listed(called, def->name) && allDeclared(bufferNames(*def), have)) {
          out += item(*def);
        }
      }

      return out;
    }

    // The `_item` functions (the earlier kernels this one calls, then this one), after the remainder helper when they use it.
    String functions(const GlslComputeKernel& kernel) {
      const String items = earlierItems(kernel) + item(kernel.function) + "\n";

      return (items.find("gpu_fmod(") == String::npos ? "" : kRemainderHelper) + items;
    }

    // An element input is read at this thread's element, and a uniform value is passed whole. The index is the thread number (a grid has two:
    // the column, then the row). `indexes` counts the `index` parameters before this one.
    String argument(const GlslComputeKernel& kernel, size_t i, int indexes) {
      if (roleOf(kernel, i) == ParamRole::Element) {
        return parameterName(kernel, i) + "_in[gpu_gid]";
      }

      if (roleOf(kernel, i) == ParamRole::Uniform) {
        return parameterName(kernel, i);
      }

      if (IndexCount(kernel.function.roles) < 2) {
        return "int(gpu_gid)";
      }

      return indexes == 0 ? "int(gpu_x)" : "int(gpu_y)";
    }

    // The values `main` passes to the `_item` function: element inputs, uniform values and indexes, in parameter order.
    bool isArgument(ParamRole role) {
      return role == ParamRole::Element || role == ParamRole::Uniform || role == ParamRole::Index;
    }

    String arguments(const GlslComputeKernel& kernel) {
      Vector<String> args;
      int indexes = 0;

      for (size_t i = 0; i < kernel.function.parameters.size(); i++) {
        if (isArgument(roleOf(kernel, i))) {
          args.push_back(argument(kernel, i, indexes));
        }

        if (roleOf(kernel, i) == ParamRole::Index) {
          indexes++;
        }
      }

      return Joined(args, ", ");
    }

    String gridEntry(const String& call) {
      return "void main() {\n    uint gpu_x = gl_GlobalInvocationID.x;\n    uint gpu_y = gl_GlobalInvocationID.y;\n    if (gpu_x >= gpu_width || gpu_y >= gpu_height) "
        "{ return; }\n    uint gpu_gid = gpu_y * gpu_width + gpu_x;\n    gpu_out[gpu_gid] = " + call + ";\n}\n";
    }

    String groupedEntry(const GlslComputeKernel& kernel, const String& call) {
      return "void main() {\n    uint gpu_gid = gl_GlobalInvocationID.x;\n    " + GlslComputeType(kernel.function.result) + " gpu_result = " + call +
        ";\n    if (gpu_gid < gpu_count) { gpu_out[gpu_gid] = gpu_result; }\n}\n";
    }

    // A line of threads returns at once past the end; a grid does the same for either coordinate. A kernel with workgroup memory must NOT return
    // early, because every thread must reach its barriers. So it runs the whole body, and only the final write is guarded.
    String entry(const GlslComputeKernel& kernel) {
      const String call = kernel.function.name + "_item(" + arguments(kernel) + ")";

      if (IndexCount(kernel.function.roles) == 2) {
        return gridEntry(call);
      }

      if (usesWorkgroup(kernel.function.roles)) {
        return groupedEntry(kernel, call);
      }

      return "void main() {\n    uint gpu_gid = gl_GlobalInvocationID.x;\n    if (gpu_gid >= gpu_count) { return; }\n    gpu_out[gpu_gid] = " + call + ";\n}\n";
    }

    String text(const GlslComputeKernel& kernel) {
      return header(kernel) + bindings(kernel) + "\n" + functions(kernel) + entry(kernel);
    }

  }  // namespace

  String GlslComputeSource(const Module& module) {
    const Vector<const FuncDef*> kernels = ComputeKernels(module);
    Vector<String> shaders;

    for (size_t i = 0; i < kernels.size(); i++) {
      shaders.push_back("// ==== " + kernels[i]->name + ".comp ====\n" + text(GlslComputeKernel{*kernels[i], KernelsBefore(kernels, i)}));
    }

    return Joined(shaders, "\n");
  }

}  // namespace polyir
