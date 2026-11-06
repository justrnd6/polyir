// Metal Shading Language for a Module's compute kernels (KernelMsl.nina). For each kernel the text has two parts:
//   1. `name_item`: the kernel's body as an ordinary function that works on ONE element, translated line by line;
//   2. `name`: the entry point the GPU starts. Each thread gets its own number (`__gid`), reads element `__gid` of every input buffer, calls
//      `name_item`, and writes the answer to element `__gid` of the output buffer.
// A `uniform` value is passed whole, a `uniform` array is a buffer every thread may read anywhere, and an `index` parameter receives `__gid` itself.
#include "polyir/Kernels.hpp"
#include "gpu/KernelCommon.hpp"
#include "gpu/KernelMslBody.hpp"

namespace polyir {

  namespace {

    const char* const kHeader = "#include <metal_stdlib>\nusing namespace metal;\n";

    // Metal has no atomic add for floats everywhere, so a float is added with a compare-and-swap loop: read the slot as raw bits, work out the new bits,
    // and swap them in only if no other thread changed the slot in between. It is written out only when a kernel calls `atomicAddFloat`.
    const char* const kFloatAtomicHelper =
      "inline float __atomic_add_float(device float* array, int slot, float amount) {\n"
      "    device atomic_uint* word = reinterpret_cast<device atomic_uint*>(&array[slot]);\n"
      "    uint seen = atomic_load_explicit(word, memory_order_relaxed);\n"
      "    while (true) {\n"
      "        uint wanted = as_type<uint>(as_type<float>(seen) + amount);\n"
      "        if (atomic_compare_exchange_weak_explicit(word, &seen, wanted, memory_order_relaxed, memory_order_relaxed)) {\n"
      "            return as_type<float>(seen);\n"
      "        }\n"
      "    }\n"
      "}\n";

    // A reduction runs in ONE workgroup, as wide as the host chooses but never wider than the input, so every thread's first read is in bounds and no
    // "this thread got nothing" case needs an identity element. Each thread folds a slice of the input into a register, the partial results are merged
    // pairwise in workgroup memory (`__half` shrinking each round), and thread 0 writes the answer. `@` stands for the name of the combine function.
    const char* const kReductionEntry =
      "(device const float* __in [[buffer(0)]],\n"
      "    constant uint& __count [[buffer(1)]],\n"
      "    device float* __out [[buffer(2)]],\n"
      "    threadgroup float* __shared [[threadgroup(0)]],\n"
      "    uint __tid [[thread_position_in_grid]],\n"
      "    uint __groupWidth [[threads_per_threadgroup]]) {\n"
      "    float __acc = __in[__tid];\n"
      "    for (uint __i = __tid + __groupWidth; __i < __count; __i += __groupWidth) {\n"
      "        __acc = @(__acc, __in[__i]);\n"
      "    }\n"
      "    __shared[__tid] = __acc;\n"
      "    threadgroup_barrier(mem_flags::mem_threadgroup);\n"
      "    uint __n = __groupWidth;\n"
      "    while (__n > 1) {\n"
      "        uint __half = (__n + 1) / 2;\n"
      "        if (__tid < __half && __tid + __half < __n) {\n"
      "            __shared[__tid] = @(__shared[__tid], __shared[__tid + __half]);\n"
      "        }\n"
      "        threadgroup_barrier(mem_flags::mem_threadgroup);\n"
      "        __n = __half;\n"
      "    }\n"
      "    if (__tid == 0) {\n"
      "        __out[0] = __shared[0];\n"
      "    }\n"
      "}\n";

    // The combine as an ordinary function of two numbers, then the entry point that folds with it.
    String reduction(const FuncDef& def) {
      const String combine = def.name + "_combine";
      const String signature = "inline float " + combine + "(" + MslParamType(def.parameters[0].type, def.roles[0]) + " " + MslIdentifier(def.parameters[0].name) + ", " +
        MslParamType(def.parameters[1].type, def.roles[1]) + " " + MslIdentifier(def.parameters[1].name) + ") {\n";

      return signature + MslBlockCode(def.body, 1) + "}\n\nkernel void " + def.name + ReplacedAll(kReductionEntry, "@", combine);
    }

    // ── one compute kernel: `kernel` is its function ──

    String parameterName(const FuncDef& kernel, size_t i) {
      return MslIdentifier(kernel.parameters[i].name);
    }

    String paramList(const FuncDef& kernel) {
      Vector<String> parts;

      for (size_t i = 0; i < kernel.parameters.size(); i++) {
        parts.push_back(MslParamType(kernel.parameters[i].type, kernel.roles[i]) + " " + parameterName(kernel, i));
      }

      return Joined(parts, ", ");
    }

    String item(const FuncDef& kernel) {
      return "inline " + MslType(kernel.result) + " " + kernel.name + "_item(" + paramList(kernel) + ") {\n" + MslBlockCode(kernel.body, 1) + "}\n";
    }

    String bufferSlot(const char* space, const String& type, const String& label, int slot) {
      return String(space) + " " + type + "* " + label + " [[buffer(" + to_string(slot) + ")]]";
    }

    // An element input is a read-only `device const T* x_in`, and a uniform value is `constant T& x`. A uniform array is `device const T* x` (its
    // type already says pointer), and a writable array is `device T* x`. Buffer slots count up over the parameters that need one; the result buffer
    // takes the next.
    String bufferParam(const FuncDef& kernel, size_t i, int slot) {
      const TypePtr& t = kernel.parameters[i].type;
      const String where = " [[buffer(" + to_string(slot) + ")]]";

      if (kernel.roles[i] == ParamRole::Uniform) {
        return "constant " + MslType(t) + "& " + parameterName(kernel, i) + where;
      }

      if (kernel.roles[i] == ParamRole::Buffer) {
        return MslType(t) + " " + parameterName(kernel, i) + where;
      }

      if (kernel.roles[i] == ParamRole::Writable) {
        return MslPointerType("device", t) + " " + parameterName(kernel, i) + where;
      }

      return bufferSlot("device const", MslType(t), parameterName(kernel, i) + "_in", slot);
    }

    Vector<String> positionParams(const FuncDef& kernel) {
      if (IndexCount(kernel.roles) == 2) {
        return {"uint2 __gid [[thread_position_in_grid]]", "uint2 __grid [[threads_per_grid]]"};
      }

      return {"uint __gid [[thread_position_in_grid]]"};
    }

    String bufferDeclarations(const FuncDef& kernel) {
      Vector<String> decls;

      for (size_t i = 0; i < kernel.parameters.size(); i++) {
        if (HasBuffer(kernel.roles[i])) {
          decls.push_back(bufferParam(kernel, i, static_cast<int>(decls.size())));
        }
      }

      decls.push_back(bufferSlot("device", MslType(kernel.result), "__out", static_cast<int>(decls.size())));

      for (const String& position : positionParams(kernel)) {
        decls.push_back(position);
      }

      return Joined(decls, ",\n    ");
    }

    // One thread per element of a line (`__gid`), or per cell of a grid (`__gid.x` is the column and `__gid.y` the row).
    String elementNumber(const FuncDef& kernel) {
      return IndexCount(kernel.roles) == 2 ? "(__gid.y * __grid.x + __gid.x)" : "__gid";
    }

    // `threadgroup float name[256];` for each workgroup array: one slot per thread of the group.
    String workgroupDecls(const FuncDef& kernel) {
      String out;

      for (size_t i = 0; i < kernel.parameters.size(); i++) {
        if (kernel.roles[i] == ParamRole::Workgroup) {
          out += "    threadgroup " + MslType(kernel.parameters[i].type->element) + " " + parameterName(kernel, i) + "[" + to_string(kGroupSize) + "];\n";
        }
      }

      return out;
    }

    // An element input is read at this thread's element. An `index` parameter is the thread's position (in a grid the first is the column, the
    // second the row). Anything else is passed by name. `indexes` counts the `index` parameters before this one.
    String argument(const FuncDef& kernel, size_t i, int indexes) {
      if (kernel.roles[i] == ParamRole::Element) {
        return parameterName(kernel, i) + "_in[" + elementNumber(kernel) + "]";
      }

      if (kernel.roles[i] != ParamRole::Index) {
        return parameterName(kernel, i);
      }

      if (IndexCount(kernel.roles) < 2) {
        return "(int)__gid";
      }

      return indexes == 0 ? "(int)__gid.x" : "(int)__gid.y";
    }

    String arguments(const FuncDef& kernel) {
      Vector<String> args;
      int indexes = 0;

      for (size_t i = 0; i < kernel.parameters.size(); i++) {
        args.push_back(argument(kernel, i, indexes));

        if (kernel.roles[i] == ParamRole::Index) {
          indexes++;
        }
      }

      return Joined(args, ", ");
    }

    String entryBody(const FuncDef& kernel) {
      const String element = elementNumber(kernel);

      return workgroupDecls(kernel) + "    __out[" + element + "] = " + kernel.name + "_item(" + arguments(kernel) + ");\n";
    }

    String entry(const FuncDef& kernel) {
      return "kernel void " + kernel.name + "(" + bufferDeclarations(kernel) + ") {\n" + entryBody(kernel) + "}\n";
    }

    String computeKernelText(const FuncDef& kernel) {
      return item(kernel) + "\n" + entry(kernel);
    }

    bool anyUses(const Vector<String>& kernels, const char* word) {
      for (const String& kernel : kernels) {
        if (kernel.find(word) != String::npos) {
          return true;
        }
      }

      return false;
    }

    String kernelText(const FuncDef& def) {
      if (def.stage == KernelStage::Compute) {
        return computeKernelText(def);
      }

      return def.stage == KernelStage::Reduce ? reduction(def) : String();
    }

  }  // namespace

  Vector<String> KernelNames(const Module& module, KernelStage stage) {
    Vector<String> names;

    for (const FuncDef& def : module.functions) {
      if (def.stage == stage) {
        names.push_back(def.name);
      }
    }

    return names;
  }

  String MetalSource(const Module& module) {
    Vector<String> kernels;

    for (const FuncDef& def : module.functions) {
      if (def.stage == KernelStage::Compute || def.stage == KernelStage::Reduce) {
        kernels.push_back(kernelText(def));
      }
    }

    if (kernels.empty()) {
      return "";
    }

    const String helper = anyUses(kernels, "__atomic_add_float(") ? String(kFloatAtomicHelper) + "\n" : "";

    return String(kHeader) + "\n" + helper + Joined(kernels, "\n");
  }

}  // namespace polyir
