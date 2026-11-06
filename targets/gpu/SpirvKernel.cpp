// One compute kernel as a SPIR-V module, written directly from the HIR without GLSL and glslang (SpirvKernel.nina). The module has the shape of the
// GLSL shader: every array parameter is a storage buffer at a binding (parameter order, the result last), the `uniform` values travel in one
// push-constant block after the element count, the kernels it calls are ordinary functions before it, and `main` is what each thread starts.
#include "polyir/Kernels.hpp"
#include "gpu/SpirvKernel.hpp"

namespace polyir {

  namespace {

    constexpr int kDecorationBinding = 33;
    constexpr int kDecorationDescriptorSet = 34;

    // The distance in bytes between two slots: 4 for a number, 8 for a 64-bit one or a 2-vector, 16 for a 3- or 4-vector.
    int strideOf(const TypePtr& t) {
      if (SpvIsVector(t)) {
        return SpvLanes(t) == 2 ? 8 : 16;
      }

      return SpvBits(t) == 64 ? 8 : 4;
    }

    // The variable of a storage buffer: a pointer, in the Uniform storage class, to the struct that holds the array.
    Id bufferVariable(SpvModule& m, const String& name, Id array, bool readOnly) {
      const Id pointer = TypePointer(m, kStorageUniform, TypeBuffer(m, array, readOnly));

      return DeclareValue(m, "buffer:" + name, OpVariable, pointer, {kStorageUniform});
    }

    // Descriptor set 0 at `binding`, and the name `spirv-dis` shows.
    void placeBuffer(SpvModule& m, Id variable, int binding, const String& name) {
      Decorate(m, variable, kDecorationDescriptorSet, {0});
      Decorate(m, variable, kDecorationBinding, {binding});
      Name(m, variable, name);
    }

    // A storage buffer: a struct holding a runtime-sized array, in descriptor set 0 at `binding`.
    SpvArray storage(SpvModule& m, const String& name, const TypePtr& element, bool readOnly, bool holdsFloatBits, int binding) {
      const Id elementType = SpvType(m, element);  // declared even for a float-bits array: it keeps the Nina writer's ID order
      const Id storedType = holdsFloatBits ? TypeUint(m) : elementType;
      const Id array = TypeRuntimeArray(m, storedType, strideOf(element));
      const Id variable = bufferVariable(m, name, array, readOnly);
      placeBuffer(m, variable, binding, name);

      return SpvArray{name, variable, element, kStorageUniform, holdsFloatBits};
    }

    // An element input is read through `<name>_in`; a `uniform` array is read-only and a `writable` one is not.
    SpvArray bufferFor(SpvModule& m, const Parameter& p, ParamRole role, bool holdsFloatBits, int binding) {
      if (role == ParamRole::Element) {
        return storage(m, p.name + "_in", p.type, true, false, binding);
      }

      return storage(m, p.name, SpvPointee(p.type), role != ParamRole::Writable, holdsFloatBits, binding);
    }

    // Workgroup memory: an array of `kGroupSize` slots in the Workgroup storage class.
    SpvArray tile(SpvModule& m, const Parameter& p) {
      const TypePtr element = SpvPointee(p.type);
      const Id length = ConstUint(m, kGroupSize);
      const Id array = TypeArray(m, SpvType(m, element), length);
      const Id variable = DeclareValue(m, "tile:" + p.name, OpVariable, TypePointer(m, kStorageWorkgroup, array), {kStorageWorkgroup});
      Name(m, variable, p.name);

      return SpvArray{p.name, variable, element, kStorageWorkgroup, false};
    }

    // An element input (read in `main` from `<name>_in`), a `uniform` array and a `writable` one each get a storage buffer.
    bool getsStorageBuffer(ParamRole role) {
      return role == ParamRole::Element || role == ParamRole::Buffer || role == ParamRole::Writable;
    }

    // An element input is read in `main` and passed to the kernel function as a value, as a uniform value or an index is. Buffers
    // and workgroup memory are globals the function reaches by name.
    bool isPassedToKernelFunction(ParamRole role) {
      return role != ParamRole::Buffer && role != ParamRole::Writable && role != ParamRole::Workgroup;
    }

    void addArray(SpvModule& m, const FuncDef& def, size_t i, const Vector<String>& wordArrays, int& binding, Vector<SpvArray>& out) {
      const Parameter& p = def.parameters[i];

      if (getsStorageBuffer(def.roles[i])) {
        out.push_back(bufferFor(m, p, def.roles[i], Listed(wordArrays, p.name), binding++));
      }

      if (def.roles[i] == ParamRole::Workgroup) {
        out.push_back(tile(m, p));
      }
    }

    // A `SpvArray` for every array parameter, then the result buffer `gpu_out`; `wordArrays` are the ones `atomicAddFloat` updates.
    Vector<SpvArray> arraysOf(SpvModule& m, const FuncDef& def, const Vector<String>& wordArrays) {
      Vector<SpvArray> out;
      int binding = 0;

      for (size_t i = 0; i < def.parameters.size(); i++) {
        addArray(m, def, i, wordArrays, binding, out);
      }

      out.push_back(storage(m, "gpu_out", def.result, false, false, binding));

      return out;
    }

    // The arrays `atomicAddFloat` updates, in the target and in the kernels it calls.
    Vector<String> floatAtomicArrays(const FuncDef& target, const Vector<const FuncDef*>& earlier, const Vector<String>& called) {
      Vector<String> names = FirstArguments(target.body, "atomicAddFloat");

      for (const FuncDef* def : earlier) {
        const Vector<String> more = Listed(called, def->name) ? FirstArguments(def->body, "atomicAddFloat") : Vector<String>{};
        names.insert(names.end(), more.begin(), more.end());
      }

      return names;
    }

    // The positions of the parameters a kernel function receives.
    Vector<size_t> passedParams(const FuncDef& def) {
      Vector<size_t> out;

      for (size_t i = 0; i < def.roles.size(); i++) {
        if (isPassedToKernelFunction(def.roles[i])) {
          out.push_back(i);
        }
      }

      return out;
    }

    // A function parameter for each passed parameter: its ID, added to `pairs` after its type, and bound to its name.
    SpvScope parameterScope(SpvModule& m, const FuncDef& def, const Words& typeIds, Words& pairs) {
      SpvScope scope;
      const Vector<size_t> passed = passedParams(def);

      for (size_t k = 0; k < passed.size(); k++) {
        pairs.insert(pairs.end(), {typeIds[k], Fresh(m)});
        scope.push_back(SpvBinding{def.parameters[passed[k]].name, pairs.back(), def.parameters[passed[k]].type, false});
      }

      return scope;
    }

    SpvKnown finishItem(SpvModule& m, SpvFunction& function, const SpvKnown& known, const Words& typeIds, const Words& pairs) {
      const Id returns = SpvType(m, known.returns);
      const Id functionType = TypeFunction(m, returns, typeIds);
      Finish(function, known.id, returns, functionType, pairs, false);
      Name(m, known.id, known.name + "_item");

      return known;
    }

    // The body of the item function, once its parameter types and its id are known.
    SpvKnown writeItemFunction(SpvWriter& writer, const FuncDef& def, const SpvKnown& known, const Words& typeIds) {
      Words pairs;
      SpvFunction function = MakeSpvFunction(writer.module);
      SpvBody body{writer, function, writer.module};
      LowerBlock(body, parameterScope(writer.module, def, typeIds, pairs), def.body, def.result);

      return finishItem(writer.module, function, known, typeIds, pairs);
    }

    // The types of the parameters the kernel function receives, in order: as HIR types into `known.params`, and as SPIR-V type IDs.
    Words passedTypeIds(SpvWriter& writer, const FuncDef& def, SpvKnown& known) {
      Words typeIds;

      for (size_t i : passedParams(def)) {
        const TypePtr& type = def.parameters[i].type;
        known.params.push_back(type);
        typeIds.push_back(SpvType(writer.module, type));
      }

      return typeIds;
    }

    // The kernel as a function of the parameters that are not arrays (element values, uniform values, indexes).
    SpvKnown writeItem(SpvWriter& writer, const FuncDef& def) {
      SpvKnown known{def.name, 0, def.result, {}};
      const Words typeIds = passedTypeIds(writer, def, known);
      known.id = Fresh(writer.module);

      return writeItemFunction(writer, def, known, typeIds);
    }

    SpirvBinary finishModule(SpvWriter& writer, const FuncDef& target) {
      const SpvKnown item = writeItem(writer, target);
      const Words execution = SpvWriteMain(writer, target, item);

      if (!writer.module.problems.empty()) {
        return SpirvBinary{{}, writer.module.problems};
      }

      SpirvBinary binary;

      for (int64_t word : ModuleWords(writer.module, execution)) {
        binary.words.push_back(static_cast<uint32_t>(word));
      }

      return binary;
    }

    // The kernels the target calls, written first as ordinary functions it can call.
    void writeCalledKernels(SpvWriter& writer, const Vector<const FuncDef*>& earlier, const Vector<String>& called) {
      for (const FuncDef* def : earlier) {
        if (Listed(called, def->name)) {
          writer.functions.push_back(writeItem(writer, *def));
        }
      }
    }

    // The words of the module for `target` (`earlier` are the kernels before it), or the reasons it cannot be written.
    SpirvBinary build(const FuncDef& target, const Vector<const FuncDef*>& earlier) {
      SpvModule m;
      const Id glsl = Fresh(m);
      SpvAppend(m.imports, SpvInstruction(OpExtInstImport, Concat({glsl}, SpvString("GLSL.std.450"))));

      const Vector<String> called = CalledKernels(target, earlier);
      SpvWriter writer{m, arraysOf(m, target, floatAtomicArrays(target, earlier, called)), {}, glsl};
      writeCalledKernels(writer, earlier, called);

      return finishModule(writer, target);
    }

    Vector<String> labelled(const String& name, const Vector<String>& errors) {
      Vector<String> out;

      for (const String& error : errors) {
        out.push_back(name + ": " + error);
      }

      return out;
    }

    // `// ==== name.spv ====` and the module's words, after the sections before it; or the reasons it cannot be written.
    void addModule(const FuncDef& kernel, const Vector<const FuncDef*>& earlier, ShaderText& result) {
      const SpirvBinary binary = build(kernel, earlier);

      if (!binary.errors.empty()) {
        result.errors = labelled(kernel.name, binary.errors);
      } else {
        result.text += (result.text.empty() ? "" : "\n") + ("// ==== " + kernel.name + ".spv ====\n") + SpirvHex(binary.words);
      }
    }

    String hexByte(uint32_t byte) {
      static const char* const kDigits = "0123456789abcdef";

      return String{kDigits[byte / 16], kDigits[byte % 16]};
    }

  }  // namespace

  String SpirvHex(const Vector<uint32_t>& words) {
    String out;

    for (size_t i = 0; i < words.size(); i++) {
      for (int b = 0; b < 4; b++) {
        out += hexByte((words[i] >> (8 * b)) & 0xffU);
      }

      if (i % 8 == 7) {
        out += "\n";
      }
    }

    return out + "\n";
  }

  SpirvBinary SpirvModule(const Module& module, const String& kernel) {
    const Vector<const FuncDef*> kernels = ComputeKernels(module);

    for (size_t i = 0; i < kernels.size(); i++) {
      if (kernels[i]->name == kernel) {
        return build(*kernels[i], KernelsBefore(kernels, i));
      }
    }

    return SpirvBinary{{}, {"no compute kernel named " + kernel}};
  }

  // Kernel after kernel; the first the writer cannot handle ends it with its reasons.
  ShaderText SpirvSource(const Module& module) {
    ShaderText result;
    const Vector<const FuncDef*> kernels = ComputeKernels(module);

    for (size_t i = 0; i < kernels.size() && result.errors.empty(); i++) {
      addModule(*kernels[i], KernelsBefore(kernels, i), result);
    }

    if (!result.errors.empty()) {
      result.text.clear();
    }

    return result;
  }

}  // namespace polyir
