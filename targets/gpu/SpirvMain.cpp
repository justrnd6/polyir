// `main`, the entry point every GPU thread starts (SpirvMain.nina). It finds which element the thread owns, hands the kernel's function the element
// values, the `uniform` values (from the push constants) and the thread's place, and stores the answer in the result buffer. A thread past the end of
// the data does nothing, except in a kernel with workgroup memory: there every thread runs the whole kernel (to reach its barriers) and only the
// final store is skipped.
#include "gpu/SpirvKernel.hpp"

namespace polyir {

  namespace {

    constexpr int kBuiltIn = 11;
    constexpr int kGlobalInvocationId = 28;
    constexpr int kExecutionModelGlCompute = 5;
    constexpr int kExecutionModeLocalSize = 17;

    // The push-constant block: its variable, and how many members come before the `uniform` values (the count, or a grid's width and height).
    struct SpvPush {
      Id variable = 0;
      int leading = 0;
    };

    // The thread's number: the built-in `GlobalInvocationId`, a 3-vector of unsigned numbers, as an Input variable.
    Id threadVariable(SpvModule& m) {
      const Id vector = TypeVector(m, TypeUint(m), 3);
      const Id variable = DeclareValue(m, "gid", OpVariable, TypePointer(m, kStorageInput, vector), {kStorageInput});
      Decorate(m, variable, kBuiltIn, {kGlobalInvocationId});
      Name(m, variable, "gl_GlobalInvocationID");

      return variable;
    }

    // A push-constant member must be a 32-bit number. A bool cannot be shared with the host, and wider values and vectors are not laid out here.
    Id pushMember(SpvModule& m, const Parameter& p) {
      if (!SpvIsNumber(p.type) || SpvIsVector(p.type) || SpvBits(p.type) == 64) {
        ReportProblem(m, "the uniform value " + p.name + " must be a 32-bit number to travel in the push constants");
      }

      return SpvType(m, p.type);
    }

    // The element count (or width and height), then every `uniform` value, each a 32-bit number at 4-byte steps.
    SpvPush pushConstants(SpvModule& m, const FuncDef& def) {
      const Id uint = TypeUint(m);
      Words members = IndexCount(def.roles) == 2 ? Words{uint, uint} : Words{uint};
      const int leading = static_cast<int>(members.size());

      for (size_t i = 0; i < def.parameters.size(); i++) {
        if (def.roles[i] == ParamRole::Uniform) {
          members.push_back(pushMember(m, def.parameters[i]));
        }
      }

      const Id variable = DeclareValue(m, "push", OpVariable, TypePointer(m, kStoragePushConstant, TypePushBlock(m, members)), {kStoragePushConstant});
      Name(m, variable, "params");

      return SpvPush{variable, leading};
    }

    Words localSize(int dims, bool grouped) {
      if (dims == 2) {
        return {8, 8, 1};
      }

      return grouped ? Words{kGroupSize, 1, 1} : Words{64, 1, 1};
    }

    // The `main` being written (passed around as `entry`): the kernel it runs, the item function it calls for this thread's
    // element, the push block holding the uniforms, and, once `position` has worked them out, the thread's column, row,
    // element and whether that element exists.
    struct SpvMain {
      SpvWriter& writer;
      SpvModule& module;
      const FuncDef& kernel;
      const SpvKnown& item;
      SpvPush push;
      SpvFunction& function;
      SpvBody& body;
      Id x = 0, y = 0, linear = 0, inRange = 0;
    };

    Words write(SpvMain& entry, Id thread);

    void position(SpvMain& entry, Id thread);

    void gridPosition(SpvMain& entry, Id vector, Id width);

    Id loadMember(SpvMain& entry, Id type, int index);

    void inRangeOnly(SpvMain& entry, const Function<void()>& work);

    void storeResult(SpvMain& entry, Id result);

    Id asInt(SpvMain& entry, Id unsignedId);

    Id callItem(SpvMain& entry);

    bool isPassedToItem(ParamRole role);

    Id argument(SpvMain& entry, size_t i);

    Id elementArgument(SpvMain& entry, const String& name);

    Words finish(SpvMain& entry, Id thread);

    bool grouped(const SpvMain& entry);

    Id asInt(SpvMain& entry, Id unsignedId) {
      return ValueOf(entry.function, OpBitcast, TypeInt(entry.module, 32), {unsignedId});
    }

    bool grouped(const SpvMain& entry) {
      return find(entry.kernel.roles.begin(), entry.kernel.roles.end(), ParamRole::Workgroup) != entry.kernel.roles.end();
    }

    Words write(SpvMain& entry, Id thread) {
      position(entry, thread);
      const Id result = grouped(entry) ? callItem(entry) : 0;
      inRangeOnly(entry, [&] { storeResult(entry, grouped(entry) ? result : callItem(entry)); });
      ReturnNothing(entry.function);

      return finish(entry, thread);
    }

    // Where this thread is: its column (and row), the element it owns, and whether that element exists (a launch rounds up to whole groups).
    void position(SpvMain& entry, Id thread) {
      const Id uint = TypeUint(entry.module), vectorType = TypeVector(entry.module, uint, 3), vector = Load(entry.function, vectorType, thread);
      entry.x = entry.linear = ValueOf(entry.function, OpCompositeExtract, uint, {vector, 0});
      const Id count = loadMember(entry, uint, 0);

      if (IndexCount(entry.kernel.roles) == 2) {
        return gridPosition(entry, vector, count);
      }

      const Id flag = TypeBool(entry.module);
      entry.inRange = ValueOf(entry.function, OpULessThan, flag, {entry.x, count});
    }

    // A grid: column x, row y, the element `y * width + x`, in range when both are inside the picture.
    void gridPosition(SpvMain& entry, Id vector, Id width) {
      const Id uint = TypeUint(entry.module), flag = TypeBool(entry.module);
      entry.y = ValueOf(entry.function, OpCompositeExtract, uint, {vector, 1});
      const Id height = loadMember(entry, uint, 1), product = ValueOf(entry.function, OpIMul, uint, {entry.y, width});
      entry.linear = ValueOf(entry.function, OpIAdd, uint, {product, entry.x});

      const Id columnInside = ValueOf(entry.function, OpULessThan, flag, {entry.x, width}), rowInside = ValueOf(entry.function, OpULessThan, flag, {entry.y, height});
      entry.inRange = ValueOf(entry.function, OpLogicalAnd, flag, {columnInside, rowInside});
    }

    // Member `index` of the push block, loaded as a value of type `type`.
    Id loadMember(SpvMain& entry, Id type, int index) {
      const Id pointer = TypePointer(entry.module, kStoragePushConstant, type), at = ConstInt(entry.module, 32, index);
      const Id member = ValueOf(entry.function, OpAccessChain, pointer, {entry.push.variable, at});

      return Load(entry.function, type, member);
    }

    // `work` runs only for threads inside the data.
    void inRangeOnly(SpvMain& entry, const Function<void()>& work) {
      const Id inside = Fresh(entry.module), done = Fresh(entry.module);
      SelectionMerge(entry.function, done);
      BranchIf(entry.function, entry.inRange, inside, done);

      Begin(entry.function, inside);
      work();
      BranchAndBegin(entry.function, done);
    }

    // Slot `linear` of the result buffer.
    void storeResult(SpvMain& entry, Id result) {
      const SpvArray* out = SpvArrayNamed(entry.writer.arrays, "gpu_out");

      if (out == nullptr) {
        return ReportProblem(entry.module, "the result buffer is missing");
      }

      const Id slot = asInt(entry, entry.linear);
      StoreSlot(entry.body, *out, slot, result);
    }

    // Element values read at this thread's element, `uniform` values from the push block, the thread's place for each `index` (the column, then the row).
    Id callItem(SpvMain& entry) {
      Words args{entry.item.id};

      for (size_t i = 0; i < entry.kernel.parameters.size(); i++) {
        if (isPassedToItem(entry.kernel.roles[i])) {
          args.push_back(argument(entry, i));
        }
      }

      return ValueOf(entry.function, OpFunctionCall, SpvType(entry.module, entry.item.returns), args);
    }

    // The arrays are not passed: the item function reaches them by name, as globals.
    bool isPassedToItem(ParamRole role) {
      return role == ParamRole::Element || role == ParamRole::Uniform || role == ParamRole::Index;
    }

    // How many of the first `count` parameters have the role. For a `uniform` parameter, that is its place in the push block. For an
    // `index` parameter, it is its axis (the first is the column, the second the row).
    int rolesBefore(const FuncDef& kernel, size_t count, ParamRole role) {
      int seen = 0;

      for (size_t k = 0; k < count; k++) {
        if (kernel.roles[k] == role) {
          seen++;
        }
      }

      return seen;
    }

    // Parameter `i`'s value: an element read from its buffer, a uniform loaded from the push block, or the thread's column or row.
    Id argument(SpvMain& entry, size_t i) {
      const Parameter& parameter = entry.kernel.parameters[i];

      if (entry.kernel.roles[i] == ParamRole::Element) {
        return elementArgument(entry, parameter.name);
      }

      const int earlier = rolesBefore(entry.kernel, i, entry.kernel.roles[i]);

      if (entry.kernel.roles[i] == ParamRole::Uniform) {
        return loadMember(entry, SpvType(entry.module, parameter.type), entry.push.leading + earlier);
      }

      return asInt(entry, earlier == 0 ? entry.x : entry.y);
    }

    // This thread's own slot of an element input's buffer.
    Id elementArgument(SpvMain& entry, const String& name) {
      const SpvArray* array = SpvArrayNamed(entry.writer.arrays, name + "_in");

      if (array == nullptr) {
        return ConstInt(entry.module, 32, 0);
      }

      const Id slot = asInt(entry, entry.linear);

      return SlotValue(entry.body, *array, slot);
    }

    // The function itself, `OpEntryPoint` (a compute shader named "main" that reads the thread-number variable) and the workgroup size.
    Words finish(SpvMain& entry, Id thread) {
      const Id voidType = TypeVoid(entry.module), mainId = Fresh(entry.module);
      Finish(entry.function, mainId, voidType, TypeFunction(entry.module, voidType, {}), {}, true);
      SpvAppend(entry.module.entry, SpvInstruction(OpEntryPoint, Concat(Concat({kExecutionModelGlCompute, mainId}, SpvString("main")), {thread})));
      Name(entry.module, mainId, "main");

      return SpvInstruction(OpExecutionMode, Concat({mainId, kExecutionModeLocalSize}, localSize(IndexCount(entry.kernel.roles), grouped(entry))));
    }

  }  // namespace

  Words SpvWriteMain(SpvWriter& writer, const FuncDef& def, const SpvKnown& item) {
    const Id thread = threadVariable(writer.module);
    const SpvPush push = pushConstants(writer.module, def);
    SpvFunction function = MakeSpvFunction(writer.module);
    SpvBody body{writer, function, writer.module};
    SpvMain entry{writer, writer.module, def, item, push, function, body};

    return write(entry, thread);
  }

}  // namespace polyir
