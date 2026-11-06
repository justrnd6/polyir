// A SPIR-V module under construction, and one function of it (SpirvModule.nina, SpirvFn.nina). SPIR-V is the binary Vulkan loads: a five-word header,
// then instructions, each one word packing its length (top 16 bits) and opcode (low 16 bits), then its operands. Almost everything is named by an ID
// the writer hands out. The module is kept in the piles the format orders (imports, entry points, names, decorations, then types, constants and
// globals, then functions) and joined at the end; a type or constant is remembered under a key so it is declared once.
#pragma once

#include "polyir/Hir.hpp"

namespace polyir {

  using Id = int64_t;
  using Words = Vector<int64_t>;

  // The opcodes the writer uses, by their names in the SPIR-V specification.
  enum SpvOp : int {
    OpName = 5, OpExtInstImport = 11, OpExtInst = 12, OpMemoryModel = 14, OpEntryPoint = 15, OpExecutionMode = 16, OpCapability = 17, OpTypeVoid = 19,
    OpTypeBool = 20, OpTypeInt = 21, OpTypeFloat = 22, OpTypeVector = 23, OpTypeArray = 28, OpTypeRuntimeArray = 29, OpTypeStruct = 30,
    OpTypePointer = 32, OpTypeFunction = 33, OpConstantTrue = 41, OpConstantFalse = 42, OpConstant = 43, OpConstantComposite = 44, OpFunction = 54,
    OpFunctionParameter = 55, OpFunctionEnd = 56, OpFunctionCall = 57, OpVariable = 59, OpLoad = 61, OpStore = 62, OpAccessChain = 65, OpDecorate = 71,
    OpMemberDecorate = 72, OpVectorExtractDynamic = 77, OpCompositeConstruct = 80, OpCompositeExtract = 81, OpConvertFToS = 110, OpConvertSToF = 111,
    OpSConvert = 114, OpBitcast = 124, OpSNegate = 126, OpFNegate = 127, OpIAdd = 128, OpFAdd = 129, OpISub = 130, OpFSub = 131, OpIMul = 132,
    OpFMul = 133, OpSDiv = 135, OpFDiv = 136, OpSRem = 138, OpLogicalEqual = 164, OpLogicalNotEqual = 165, OpLogicalOr = 166, OpLogicalAnd = 167,
    OpLogicalNot = 168, OpSelect = 169, OpIEqual = 170, OpINotEqual = 171, OpSGreaterThan = 173, OpSGreaterThanEqual = 175, OpULessThan = 176,
    OpSLessThan = 177, OpSLessThanEqual = 179, OpFOrdEqual = 180, OpFUnordNotEqual = 183, OpFOrdLessThan = 184, OpFOrdGreaterThan = 186,
    OpFOrdLessThanEqual = 188, OpFOrdGreaterThanEqual = 190, OpShiftRightArithmetic = 195, OpShiftLeftLogical = 196, OpBitwiseOr = 197,
    OpBitwiseXor = 198, OpBitwiseAnd = 199, OpNot = 200, OpControlBarrier = 224, OpAtomicLoad = 227, OpAtomicCompareExchange = 230,
    OpAtomicIAdd = 234, OpAtomicSMin = 236, OpAtomicSMax = 238, OpLoopMerge = 246, OpSelectionMerge = 247, OpLabel = 248, OpBranch = 249,
    OpBranchConditional = 250, OpReturn = 253, OpReturnValue = 254, OpUnreachable = 255
  };

  // Storage classes: Input (1), Uniform (2, a storage buffer in the pre-1.3 spelling), Workgroup (4), Function (7), PushConstant (9).
  constexpr int kStorageInput = 1;
  constexpr int kStorageUniform = 2;
  constexpr int kStorageWorkgroup = 4;
  constexpr int kStorageFunction = 7;
  constexpr int kStoragePushConstant = 9;

  struct SpvModule {
    Id nextId = 1;
    bool int64 = false;
    Words imports, entry, names, decorations, declarations, functions;
    Vector<String> problems;

    HashMap<String, Id> idsByKey;
  };

  // One function under construction: a list of blocks, each opened by `OpLabel` and closed by one terminator. A block is always open until a
  // terminator closes it (`openLabel` is then 0); the next instruction starts a fresh, unreachable block. Local variables are collected apart, since they
  // must all sit at the very start of the first block.
  struct SpvFunction {
    SpvModule& module;
    Id entryLabel;
    Id openLabel;
    Words variables;
    Words body;
  };

  Words SpvInstruction(int opcode, const Words& operands);

  Words SpvString(const String& text);  // four characters a word, lowest byte first, ended by a zero byte

  void SpvAppend(Words& pile, const Words& words);

  int64_t SpvFloatBits(double x);         // IEEE single precision, rounded half up, too small a number flushed to zero

  Id Fresh(SpvModule& m);

  void ReportProblem(SpvModule& m, const String& message);

  Id DeclareType(SpvModule& m, const String& key, int opcode, const Words& operands);

  Id DeclareValue(SpvModule& m, const String& key, int opcode, Id type, const Words& operands);

  Id TypeVoid(SpvModule& m);

  Id TypeBool(SpvModule& m);

  Id TypeInt(SpvModule& m, int bits);

  Id TypeUint(SpvModule& m);

  Id TypeFloat(SpvModule& m);

  Id TypeVector(SpvModule& m, Id element, int count);

  Id TypePointer(SpvModule& m, int storage, Id pointee);

  Id TypeRuntimeArray(SpvModule& m, Id element, int stride);

  Id TypeArray(SpvModule& m, Id element, Id length);

  Id TypeFunction(SpvModule& m, Id returns, const Words& params);

  Id TypeBuffer(SpvModule& m, Id array, bool readOnly);

  Id TypePushBlock(SpvModule& m, const Words& members);

  Id ConstBool(SpvModule& m, bool value);

  Id ConstInt(SpvModule& m, int bits, int64_t value);

  Id ConstUint(SpvModule& m, int64_t value);

  Id ConstFloat(SpvModule& m, double value);

  void Decorate(SpvModule& m, Id target, int decoration, const Words& operands);

  void DecorateMember(SpvModule& m, Id target, int member, int decoration, const Words& operands);

  void Name(SpvModule& m, Id target, const String& text);

  Words ModuleWords(const SpvModule& m, const Words& execution);

  Words Concat(Words head, const Words& tail);

  Id Known(const SpvModule& m, const String& key);

  SpvFunction MakeSpvFunction(SpvModule& m);  // a function with its entry block open

  bool IsOpen(const SpvFunction& function);

  void Begin(SpvFunction& function, Id label);

  void Effect(SpvFunction& function, int opcode, const Words& operands);

  Id ValueOf(SpvFunction& function, int opcode, Id type, const Words& operands);

  void Branch(SpvFunction& function, Id target);

  void BranchIf(SpvFunction& function, Id condition, Id whenTrue, Id whenFalse);

  void ReturnValue(SpvFunction& function, Id id);

  void ReturnNothing(SpvFunction& function);

  void SelectionMerge(SpvFunction& function, Id merge);

  void LoopMerge(SpvFunction& function, Id merge, Id continueLabel);

  Id Variable(SpvFunction& function, Id type);

  Id Load(SpvFunction& function, Id type, Id pointer);

  void Store(SpvFunction& function, Id pointer, Id id);

  void Finish(SpvFunction& function, Id functionId, Id returns, Id functionType, const Words& params, bool isVoid);

  void Close(SpvFunction& function, int opcode, const Words& operands);

  inline Id Fresh(SpvModule& m) {
    return m.nextId++;
  }

  inline void ReportProblem(SpvModule& m, const String& message) {
    m.problems.push_back(message);
  }

  inline Id TypeVoid(SpvModule& m) {
    return DeclareType(m, "void", OpTypeVoid, {});
  }

  inline Id TypeBool(SpvModule& m) {
    return DeclareType(m, "bool", OpTypeBool, {});
  }

  inline Id TypeUint(SpvModule& m) {
    return DeclareType(m, "uint", OpTypeInt, {32, 0});
  }

  inline Id TypeFloat(SpvModule& m) {
    return DeclareType(m, "float", OpTypeFloat, {32});
  }

  inline void Decorate(SpvModule& m, Id target, int decoration, const Words& operands) {
    SpvAppend(m.decorations, SpvInstruction(OpDecorate, Concat({target, decoration}, operands)));
  }

  inline bool IsOpen(const SpvFunction& function) {
    return function.openLabel != 0;
  }

  inline void Branch(SpvFunction& function, Id target) {
    Close(function, OpBranch, {target});
  }

  inline void BranchIf(SpvFunction& function, Id condition, Id whenTrue, Id whenFalse) {
    Close(function, OpBranchConditional, {condition, whenTrue, whenFalse});
  }

  inline void ReturnValue(SpvFunction& function, Id id) {
    Close(function, OpReturnValue, {id});
  }

  inline void ReturnNothing(SpvFunction& function) {
    Close(function, OpReturn, {});
  }

  inline void SelectionMerge(SpvFunction& function, Id merge) {
    Effect(function, OpSelectionMerge, {merge, 0});
  }

  inline void LoopMerge(SpvFunction& function, Id merge, Id continueLabel) {
    Effect(function, OpLoopMerge, {merge, continueLabel, 0});
  }

  // Ends the open block with a branch to `label` and opens the block `label` names: straight-line code moving into a new block.
  inline void BranchAndBegin(SpvFunction& function, Id label) {
    Branch(function, label);
    Begin(function, label);
  }

  // A loop's header block: it names where the loop ends (`merge`) and its continue block (`step`). Then it branches to the loop's
  // first block (`first`), which is left open.
  inline void LoopHeader(SpvFunction& function, Id header, Id merge, Id step, Id first) {
    BranchAndBegin(function, header);
    LoopMerge(function, merge, step);
    BranchAndBegin(function, first);
  }

  inline Id Load(SpvFunction& function, Id type, Id pointer) {
    return ValueOf(function, OpLoad, type, {pointer});
  }

  inline void Store(SpvFunction& function, Id pointer, Id id) {
    Effect(function, OpStore, {pointer, id});
  }

}  // namespace polyir
