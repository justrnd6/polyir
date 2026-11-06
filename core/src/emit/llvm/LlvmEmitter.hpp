// The LLVM compiler's inside: what a module shares while its functions are written (ModuleContext) and the writer of one function
// (FunctionEmitter). Both are records; the functions over them are spread over several files by what they compile, and the declarations
// below are grouped the same way, each group headed by what it compiles and its file: LlvmCompiler.cpp (the scalar core, statements and the
// module), LlvmSums.cpp (optionals, eithers, variants), LlvmLists.cpp (lists, buffers, grids, vectors), LlvmMaps.cpp, LlvmClosures.cpp,
// LlvmInterface.cpp, LlvmCoroutine.cpp, LlvmRoots.cpp (the frame records and safe points the garbage collector needs), LlvmCollector.cpp (the
// collector itself) and the smaller ones.
#pragma once

#include "polyir/Std.hpp"
#include "emit/llvm/LlvmDebug.hpp"
#include "emit/llvm/LlvmNames.hpp"
#include "polyir/Hir.hpp"

#include <map>
#include <set>
#include <string>
#include <vector>

namespace polyir::llvm {

  // A value as the IR refers to it: `text` is a register (`%sum3`) or a constant (`42`, `null`), `type` its polyir type.
  struct Value {
    String text;
    TypePtr type;
  };

  // A variable's stack slot. A name a closure captures is `boxed`: the slot holds a pointer to a heap cell the closure shares.
  struct Slot {
    String pointer;
    TypePtr type;
    bool boxed = false;
    String value;  // a label parameter that is a phi: the phi's name (there is no stack slot)
  };

  // One jump of a `branch` or `switch`: the label it goes to and the arguments it hands over.
  struct JumpEdge {
    String label;
    Vector<ExprPtr> args;
  };

  // A name a closure body reads from the code around it.
  struct Capture {
    String name;
    TypePtr type;
  };

  // What the whole module shares while its functions are written: the string constants, the library functions it needs and the functions
  // generated on the way (the bodies of closures).
  struct ModuleContext {
    const Module& module_;
    DebugInfo debug_;
    Map<String, String> strings_{};
    Vector<String> stringLines_{};
    Vector<String> generated_{};
    Set<String> needed_{};
    Set<String> used_{};
    int ids_ = 0;
    bool throws_ = false;
    bool collects_ = true;  // CompileOptions::collectGarbage: objects come from the collector, and functions keep frame records and safe points
  };

  // A value as a 64-bit word (a map key) and back: the IR lines that do it and the register that holds the result.
  struct WordCast {
    Vector<String> lines;
    String result;
  };

  // How the collector looks inside an object (runtime/Collector.c): it holds no references, or any of its 8-byte words may be one.
  enum class Layout { Data, References };

  // Writes one function (a closure's body is one too: its first parameter is the environment and `captures` are read from it).
  struct FunctionEmitter {
    using ExprHandler = Value (*)(FunctionEmitter&, const Expr&);
    using StmtHandler = void (*)(FunctionEmitter&, const Stmt&);
    static const Map<ExprKind, ExprHandler> kExprHandlers;  // what writes each kind of expression and statement
    static const Map<StmtKind, StmtHandler> kStmtHandlers;

    struct LabelInfo {
      String block;
      Vector<Slot> slots;  // a slot with no pointer is a phi parameter
      Vector<Vector<String>> incoming;  // per phi parameter: the `[ value, %block ]` entries, one per jump
      Vector<size_t> phiLines;  // per phi parameter: the line of lines_ holding its placeholder
      bool allPhis = true;  // is every parameter a phi?
    };

    ModuleContext& context_;
    const FuncDef& function_;
    Vector<Capture> captures_;
    Set<String> boxed_{};
    Set<String> assigned_{};  // the names some statement assigns to (a label parameter among them keeps a stack slot)
    Vector<String> allocas_{};
    Vector<String> lines_{};  // the IR text written so far, one line each
    Vector<Map<String, Slot>> scopes_{};  // the variables in sight, innermost scope last
    Map<String, LabelInfo> labels_{};
    Vector<String> handlers_{};  // the catch blocks of the `try`s around the statement being written, innermost last
    String currentBlock_ = "entry";
    Set<String> names_{};  // every name the function uses so far
    Map<String, int> suffixes_{};  // the last number given to each base
    int debugScope_ = -1;  // the debug-info subprogram of this function, -1 without debug info
    int location_ = -1;  // the debug-info location the next instruction gets
    int sourceLine_ = 0;  // the source line being written, for debug info
    bool terminated_ = false;

    struct Coroutine {  // what the function being written as a coroutine keeps: its id, handle, result block, yield slot and two shared blocks
      String id, handle, block, slot, cleanup, suspend, finish;  // finish: the one block a `return` jumps to, when there is one
    } coroutine_{};

    struct Roots {  // what the garbage collector needs of the function (LlvmRoots.cpp)
      Vector<String> slots{};  // the slots of its frame record: one per local and per temporary that holds a reference
      Vector<size_t> polls{};  // the lines that are safe points, kept only when the function allocates
      Vector<size_t> frameLines{};  // the lines that link and unlink the record, kept only when the function has one
      bool allocates = false;
      bool calls = false;  // it calls code that may reach a safe point
      bool linked = false;  // (a coroutine) the record is linked where the code being written runs
    } roots_{};
  };

  [[noreturn]] void Fail(const String& message);

  // Structural `==` (LlvmEquality.cpp): the name of the function that compares two values of a type.
  String EqualityName(const TypePtr& type);

  // Whether the type is compared by such a function.
  bool NeedsEquality(const Module& module, const TypePtr& type);

  // The module with the comparing functions it needs added.
  Module WithEqualityHelpers(const Module& module);

  // Whether a global's starting value `init` is a constant (LlvmGlobals.cpp). A global that starts with more than a constant gets
  // its value from a function the compiler writes.
  bool IsConstantInit(const ExprPtr& init);

  // Whether the module has that function.
  bool HasGlobalInitializer(const Module& module);

  // The module with that function added (the module as it is when every global starts with a constant).
  Module WithGlobalInitializers(const Module& module);

  const Module& ModuleOf(const ModuleContext& context);

  DebugInfo& Debug(ModuleContext& context);

  const Vector<String>& StringLines(const ModuleContext& context);

  const Set<String>& Needed(const ModuleContext& context);

  const Vector<String>& Generated(const ModuleContext& context);

  void Need(ModuleContext& context, const String& declaration);

  void AddFunction(ModuleContext& context, String text);

  int NextId(ModuleContext& context);

  bool FirstUse(ModuleContext& context, const String& what);

  bool Throws(const ModuleContext& context);

  bool Collects(const ModuleContext& context);

  void SetThrows(ModuleContext& context, bool value);

  String StringGlobal(ModuleContext& context, const String& text);

  String ConstantInit(ModuleContext& context, const GlobalDef& global);  // the text of a global's constant starting value

  void AddDeclaration(ModuleContext& context, const String& line);  // a module-level line from a fragment: a string constant goes with the strings, anything else is a need

  void DeclareThrowFlags(ModuleContext& context);  // the two globals a throw uses, when the module throws

  String SymbolName(const String& function);  // a user function called `main` makes room for the real one

  String AddString(ModuleContext& context, const String& text);

  String SafeName(const String& name);

  String StructName(const String& name);

  String StructDefinition(const StructDef& structure);

  const char* HashFunctionText();  // the string hash function, as IR

  Layout LayoutOf(const TypePtr& type);  // a block of values of the type

  String LayoutOperand(Layout layout);  // `i64 0` or `i64 1`, the last argument of the collector's allocation functions

  void NeedCollector(ModuleContext& context);  // the collector's runtime (or the plain allocator, without collection) in the module

  WordCast ToWord(const TypePtr& type, const String& value, const String& name);

  WordCast FromWord(const TypePtr& type, const String& word, const String& name);

  String Typed(const Value& value);

  String VtableName(const String& structure, const String& interfaceName);

  Vector<String> Vtables(const Module& module);  // one constant table of method pointers per (struct, interface) pair

  // The type arithmetic works in: a vector's elements, otherwise the type itself.
  inline TypePtr ScalarOf(const TypePtr& type) {
    return type->kind == TypeKind::Vec ? type->element : type;
  }

  inline const Module& ModuleOf(const ModuleContext& context) {
    return context.module_;
  }

  inline DebugInfo& Debug(ModuleContext& context) {
    return context.debug_;
  }

  inline const Vector<String>& StringLines(const ModuleContext& context) {
    return context.stringLines_;
  }

  inline const Set<String>& Needed(const ModuleContext& context) {
    return context.needed_;
  }

  inline const Vector<String>& Generated(const ModuleContext& context) {
    return context.generated_;
  }

  inline void Need(ModuleContext& context, const String& declaration) {
    context.needed_.insert(declaration);
  }

  inline void AddFunction(ModuleContext& context, String text) {
    context.generated_.push_back(move(text));
  }

  inline int NextId(ModuleContext& context) {
    return ++context.ids_;
  }

  inline bool FirstUse(ModuleContext& context, const String& what) {
    return context.used_.insert(what).second;
  }

  inline bool Throws(const ModuleContext& context) {
    return context.throws_;
  }

  inline bool Collects(const ModuleContext& context) {
    return context.collects_;
  }

  inline void SetThrows(ModuleContext& context, bool value) {
    context.throws_ = value;
  }

  inline String SymbolName(const String& function) {
    return function == "main" ? "program_main" : function;
  }

  // ── a function: its blocks, its instructions and its variables (LlvmCompiler.cpp) ──
  String Emit(FunctionEmitter& emitter);

  // The function written as IR text (a closure's body is one too: its first parameter is the environment and `captures` are read from it).
  String EmitFunction(ModuleContext& context, const FuncDef& function, Vector<Capture> captures = {});

  String NewTemporary(FunctionEmitter& emitter);

  String UniqueName(FunctionEmitter& emitter, const String& base);  // `add`, then `add1`, `add2`: a name no other value or block of the function has

  String ValueName(const String& instruction);  // what to call the result of an instruction: `add`, `cmp`, `call`, `x.val`

  void EnsureOpen(FunctionEmitter& emitter);

  void Line(FunctionEmitter& emitter, const String& text);

  void Terminate(FunctionEmitter& emitter, const String& text);

  void StartBlock(FunctionEmitter& emitter, const String& name);

  void BranchTo(FunctionEmitter& emitter, const String& block);

  String Instruction(FunctionEmitter& emitter, const String& text);

  Slot MakeSlot(FunctionEmitter& emitter, const String& name, const TypePtr& type, const String& slotName = "");  // slotName: what to call it, when not `name`

  // `argumentNumber`, here and in DescribeVariable, is a parameter's position counting from 1, and 0 for a local variable (for debug info).
  Slot BindVariable(FunctionEmitter& emitter, const String& name, const Value& start, const String& slotName = "", int argumentNumber = 0);

  void DescribeVariable(FunctionEmitter& emitter, const String& name, const Slot& slot, int argumentNumber);

  void MarkLocation(FunctionEmitter& emitter, const Span& at);

  void OpenDebugScope(FunctionEmitter& emitter);

  FunctionEmitter::ExprHandler ExprHandlerFor(ExprKind kind);

  String LocationSuffix(const FunctionEmitter& emitter);

  void AllocateCell(FunctionEmitter& emitter, const Slot& slot);

  String AddressOfSlot(FunctionEmitter& emitter, const Slot& slot);

  void SlotStore(FunctionEmitter& emitter, const Value& value, const Slot& slot);  // a store into a variable: its stack slot, or (captured) its heap cell

  Slot Lookup(const FunctionEmitter& emitter, const String& name);

  void Declare(FunctionEmitter& emitter, const String& name, const Slot& slot);

  void Store(FunctionEmitter& emitter, const Value& value, const String& pointer);

  Value LoadFrom(FunctionEmitter& emitter, const String& pointer, const TypePtr& type);

  // The five one-line ones are written out here rather than in LlvmCompiler.cpp, so every file can inline them.
  inline String NewTemporary(FunctionEmitter& emitter) {
    return "%" + UniqueName(emitter, "tmp");
  }

  inline void BranchTo(FunctionEmitter& emitter, const String& block) {
    Terminate(emitter, "br label %" + block);
  }

  inline void Declare(FunctionEmitter& emitter, const String& name, const Slot& slot) {
    emitter.scopes_.back()[name] = slot;
  }

  inline void Store(FunctionEmitter& emitter, const Value& value, const String& pointer) {
    Line(emitter, "store " + Typed(value) + ", ptr " + pointer);
  }

  inline Value LoadFrom(FunctionEmitter& emitter, const String& pointer, const TypePtr& type) {
    return Value{Instruction(emitter, "load " + LlvmTypeText(type) + ", ptr " + pointer), type};
  }

  // ── memory: objects from the collector, and the frame records and safe points it needs (LlvmMemory.cpp, LlvmRoots.cpp, LlvmCollector.cpp) ──
  String WordAddress(FunctionEmitter& emitter, const String& base, int64_t word);  // the 8-byte cell `word` of a block

  String WordAddress(FunctionEmitter& emitter, const String& base, const String& wordIndex);

  Value Allocate(FunctionEmitter& emitter, const String& bytes, Layout layout);  // a new object from the collector (malloc without it)

  Value AllocateZeroed(FunctionEmitter& emitter, const String& count, const String& elementBytes, Layout layout);  // ... zeroed (calloc)

  void FreeHint(FunctionEmitter& emitter, const String& block);  // the program's `Free`: a hint to the collector (a free() without it)

  void NoteAllocation(FunctionEmitter& emitter);  // the function allocates (so it gets safe points)

  void NoteCall(FunctionEmitter& emitter);  // the function calls code that may reach a safe point

  Layout StructLayout(const FunctionEmitter& emitter, const String& name);

  bool HoldsRoot(const FunctionEmitter& emitter, const TypePtr& type, bool boxed);  // a slot of the type belongs in the frame record

  String RootSlot(FunctionEmitter& emitter, const String& base);  // a new slot of the frame record, named after `base`

  void KeepRoot(FunctionEmitter& emitter, const Expr& expr, const Value& value);  // a reference an expression made goes into a slot of its own

  void HeapStore(FunctionEmitter& emitter, const Value& value, const String& pointer);  // a store into an object: the write barrier first, for a reference

  void SafePoint(FunctionEmitter& emitter);  // a place the collector may run (a loop head, the start of the body)

  void LeaveFrame(FunctionEmitter& emitter);  // before a `ret`: the record is unlinked

  void FrameLine(FunctionEmitter& emitter, const String& call);  // a call of the collector about the record, kept only when there is one

  void LinkCoroutineFrame(FunctionEmitter& emitter);  // after a coroutine resumes

  void UnlinkCoroutineFrame(FunctionEmitter& emitter);  // before it suspends

  bool HasFrame(const FunctionEmitter& emitter);

  Vector<String> FrameAllocas(const FunctionEmitter& emitter);  // the record and its slots (plain stack slots without one)

  Vector<String> FrameEntry(const FunctionEmitter& emitter);  // the record linked, before the function's first instruction

  Vector<String> EntryLines(const FunctionEmitter& emitter);  // the entry block before the body: the record, the stack slots, the record linked

  Vector<String> FinishedLines(const FunctionEmitter& emitter);  // the body's lines, with the collector's lines kept or dropped

  String CollectorStart(const Module& module, ModuleContext& context);  // the entry point's call of gc_start (empty when nothing allocates)

  void NeedCollectorFor(const RawIr& raw, ModuleContext& context);  // added IR that allocates through gc_allocate gets it, `main` or not

  String GlobalRoots(const Module& module);  // the table of the globals that hold references

  int64_t GlobalRootCount(const Module& module);

  String SizeOf(FunctionEmitter& emitter, const TypePtr& type);  // the byte size of a value of the type, as an i64 operand

  void TrapUnless(FunctionEmitter& emitter, const String& condition, const String& reason);

  void EmitTrap(FunctionEmitter& emitter, const String& reason);

  void DeclareTrapFunctions(ModuleContext& context);

  // ── exceptions (LlvmTry.cpp) ──
  void TryStatement(FunctionEmitter& emitter, const Stmt& statement);

  void ThrowStatement(FunctionEmitter& emitter, const Stmt& statement);

  void PassOnThrown(FunctionEmitter& emitter);  // after a call: when the callee threw, go to the nearest handler or return from this function too

  void GoToHandler(FunctionEmitter& emitter);

  void TryBody(FunctionEmitter& emitter, const Stmt& statement, const String& handler);

  void CatchBody(FunctionEmitter& emitter, const Stmt& statement, const String& handler);

  void CatchStatements(FunctionEmitter& emitter, const Stmt& statement, const Value& thrown);

  void ThrowAgain(FunctionEmitter& emitter, const Value& thrown);

  void PassOnThrownIfAny(FunctionEmitter& emitter);

  void JumpUnlessTerminated(FunctionEmitter& emitter, const String& block);

  // ── expressions and calls (LlvmCompiler.cpp) ──
  Value EmitExpr(FunctionEmitter& emitter, const Expr& expr);

  Value CastExpr(FunctionEmitter& emitter, const Expr& expr);

  Value Unsupported(FunctionEmitter& emitter, const Expr& expr);

  Value BinaryOperation(FunctionEmitter& emitter, BinOp op, const Value& left, const Value& right);

  Value WithOpcode(FunctionEmitter& emitter, const String& opcode, const Value& left, const Value& right);

  Value SignedDivision(FunctionEmitter& emitter, bool remainder, const Value& left, const Value& right);

  Value LogicEnd(FunctionEmitter& emitter, const Expr& operand, const String& decided, const String& leftBlock, const String& rhs, const String& end);

  Value CastTo(FunctionEmitter& emitter, const String& opcode, const Value& from, const TypePtr& to);

  Value ConvertAddress(FunctionEmitter& emitter, const Value& from, const TypePtr& to);

  Value ConvertFromFloat(FunctionEmitter& emitter, const Value& from, const TypePtr& to);

  Value ConvertFromInteger(FunctionEmitter& emitter, const Value& from, const TypePtr& to);

  String StructSize(FunctionEmitter& emitter, const String& structType);

  void StoreFields(FunctionEmitter& emitter, const Expr& expr, const Value& object, const Vector<Value>& fields);

  String ElementTarget(FunctionEmitter& emitter, const Expr& target);

  Value Constant(FunctionEmitter& emitter, const Expr& expr);

  Value Load(FunctionEmitter& emitter, const Expr& expr);

  Value Binary(FunctionEmitter& emitter, const Expr& expr);

  TypePtr OperandType(const Expr& expr, const Value& left, const Value& right);

  Value Fit(FunctionEmitter& emitter, const Value& value, const TypePtr& to);

  Value FitVector(FunctionEmitter& emitter, const Value& value, const TypePtr& to);

  Value Arithmetic(FunctionEmitter& emitter, BinOp op, const Value& left, const Value& right);

  Value Division(FunctionEmitter& emitter, BinOp op, const Value& left, const Value& right);

  Value Shift(FunctionEmitter& emitter, BinOp op, const Value& left, const Value& right);

  Value Comparison(FunctionEmitter& emitter, BinOp op, const Value& left, const Value& right);

  Value StringComparison(FunctionEmitter& emitter, BinOp op, const Value& left, const Value& right);

  Value Identity(FunctionEmitter& emitter, BinOp op, const Value& left, const Value& right);

  Value ShortCircuit(FunctionEmitter& emitter, const Expr& expr);

  Value Unary(FunctionEmitter& emitter, const Expr& expr);

  Value Convert(FunctionEmitter& emitter, const Value& from, const TypePtr& to);

  Value Call(FunctionEmitter& emitter, const Expr& expr);

  bool CallsOwn(FunctionEmitter& emitter, const String& name);

  String ArgumentList(FunctionEmitter& emitter, const Expr& expr, size_t from);

  Value StructLit(FunctionEmitter& emitter, const Expr& expr);

  Value FieldGet(FunctionEmitter& emitter, const Expr& expr);

  String FieldAddress(FunctionEmitter& emitter, const Value& object, int index);

  String AddressOf(FunctionEmitter& emitter, const Expr& target);

  Value Resized(FunctionEmitter& emitter, const Value& value, int bits, bool isSigned);

  Vector<Value> ValuesOf(FunctionEmitter& emitter, const Vector<ExprPtr>& exprs);

  Value BuiltinCall(FunctionEmitter& emitter, const Expr& expr);

  // ── optionals, eithers and variants (LlvmSums.cpp) ──
  Value SumValue(FunctionEmitter& emitter, const TypePtr& type, int tag, const Value* payload);

  Value SumTag(FunctionEmitter& emitter, const Value& sum);

  Value SumPayload(FunctionEmitter& emitter, const Value& sum, const TypePtr& caseType, int tag);

  Value EitherLit(FunctionEmitter& emitter, const Expr& expr);

  Value VariantLit(FunctionEmitter& emitter, const Expr& expr);

  Value CasePayload(FunctionEmitter& emitter, const Expr& expr, const TypePtr& caseType);

  bool SumBuiltin(FunctionEmitter& emitter, const Expr& expr, Value& out);

  Value SumOperation(FunctionEmitter& emitter, const Expr& expr, const Value& sum);

  Value FreedSum(FunctionEmitter& emitter, const Expr& expr);

  Value EitherOrOption(FunctionEmitter& emitter, Builtin op, const Value& sum);

  Value TagIs(FunctionEmitter& emitter, const Value& sum, int tag);

  // ── lists, buffers, grids, tensors and vectors, and the string helpers `==` and `+` use (LlvmLists.cpp) ──
  Value VecLit(FunctionEmitter& emitter, const Expr& expr);

  Value NewContainer(FunctionEmitter& emitter, const Expr& expr);

  Value EmptyContainer(FunctionEmitter& emitter, const TypePtr& type);

  Value IndexGet(FunctionEmitter& emitter, const Expr& expr);

  String ElementAddress(FunctionEmitter& emitter, const Expr& expr, const Value& base, const Vector<Value>& indices);

  String BlockElement(FunctionEmitter& emitter, const Expr& expr, const Value& base, const Vector<Value>& indices);

  String PointerAddress(FunctionEmitter& emitter, const Value& pointer, const Value& index);

  String ListAddress(FunctionEmitter& emitter, const Value& list, const Value& index);

  Value ListHeader(FunctionEmitter& emitter, const TypePtr& elementType, const String& capacity);

  Value ListPush(FunctionEmitter& emitter, const Value& list, const Value& element);

  Value ListCopy(FunctionEmitter& emitter, const Value& list);

  Value ListTruncate(FunctionEmitter& emitter, const Value& list, const Value& length);

  Value ListLength(FunctionEmitter& emitter, const Value& list);

  Value CopyBlock(FunctionEmitter& emitter, const Value& block);

  void Memcopy(FunctionEmitter& emitter, const String& to, const String& from, const String& bytes);

  Value LengthOf(FunctionEmitter& emitter, const Value& container);

  Value DataBlock(FunctionEmitter& emitter, const TypePtr& elementType, int64_t count);

  int64_t BlockCount(const TypePtr& container);

  bool ContainerBuiltin(FunctionEmitter& emitter, const Expr& expr, Value& out);

  bool StringBuiltin(FunctionEmitter& emitter, const Expr& expr, Value& out);

  Vector<Value> OperandValues(FunctionEmitter& emitter, const Expr& expr, size_t from);

  String BlockAddress(FunctionEmitter& emitter, const Expr& expr, const Value& base, const Vector<Value>& indices);

  String NextOffset(FunctionEmitter& emitter, const String& offset, int64_t extent, const Value& index);

  Value VectorElement(FunctionEmitter& emitter, const Expr& expr, const Value& vector, const Value& index);

  Value VectorOf(FunctionEmitter& emitter, const TypePtr& type, const Vector<Value>& items);

  Value ListOf(FunctionEmitter& emitter, const TypePtr& type, const Vector<Value>& items);

  Value BlockOf(FunctionEmitter& emitter, const TypePtr& type, const Vector<Value>& items);

  void StoreItems(FunctionEmitter& emitter, const Value& block, const TypePtr& elementType, const Vector<Value>& items);

  String GrowIfFull(FunctionEmitter& emitter, const Value& list, const Value& length);

  void GrowBlock(FunctionEmitter& emitter, const String& name, const String& next, const Value& list, const Value& capacity);

  void CopyElements(FunctionEmitter& emitter, const Value& list, const Value& copy, const Value& length);

  String BiggerCapacity(FunctionEmitter& emitter, const Value& capacity);

  void Reallocate(FunctionEmitter& emitter, const Value& list, const String& capacity);

  void AppendElement(FunctionEmitter& emitter, const Value& list, const Value& element, const Value& length);

  Value StringLength(FunctionEmitter& emitter, const Value& string);

  Value FixedLength(const TypePtr& type);

  String ConcatSize(FunctionEmitter& emitter, const String& leftLength, const String& rightLength);

  void CopyTail(FunctionEmitter& emitter, const Value& joined, const Value& right, const String& leftLength, const String& rightLength);

  bool EqualsOrConcatBuiltin(FunctionEmitter& emitter, const Expr& expr, Value& out);

  Value ContainerOperation(FunctionEmitter& emitter, const Expr& expr, const Value& value);

  Value ContainerEdit(FunctionEmitter& emitter, const Expr& expr, const Value& value);

  Value RetypeFreeOrCopy(FunctionEmitter& emitter, const Expr& expr, const Value& value);

  Value ListWith(FunctionEmitter& emitter, const Expr& expr, const Value& list);

  Value FreeContainer(FunctionEmitter& emitter, const Value& value);

  Value CopyContainer(FunctionEmitter& emitter, const Value& value);

  Value CopyStruct(FunctionEmitter& emitter, const Value& object);

  Value Concatenated(FunctionEmitter& emitter, const Expr& expr, Value list, const Value& total);

  Value ListConcat(FunctionEmitter& emitter, const Expr& expr, const Value& left, const Value& right);

  void CopyElementsAfter(FunctionEmitter& emitter, const Value& list, const Value& to, const Value& offset, const Value& count);

  Value IndexValue(FunctionEmitter& emitter, const Value& index);  // widened to i64, checked to be in range [0, limit) by the caller

  void BoundsCheck(FunctionEmitter& emitter, const Value& index, const String& limit);

  Value StringEquals(FunctionEmitter& emitter, const Value& left, const Value& right);

  Value StringConcat(FunctionEmitter& emitter, const Value& left, const Value& right);

  // ── the text built-ins: str, charAt, slice, find, ... (LlvmText.cpp) ──
  bool TextBuiltin(FunctionEmitter& emitter, const Expr& expr, Value& out);

  Value TextSlice(FunctionEmitter& emitter, const Vector<Value>& args);

  Value TextFind(FunctionEmitter& emitter, const Vector<Value>& args);

  Value TextCall(FunctionEmitter& emitter, const char* definition, const String& target, const Vector<Value>& args, const TypePtr& result);

  Value Printed(FunctionEmitter& emitter, const Value& text);

  void NeedTextHelpers(FunctionEmitter& emitter, InitializerList<const char*> lines);

  Value TextOf(FunctionEmitter& emitter, const Value& value);

  Value BoolText(FunctionEmitter& emitter, const Value& flag);

  Value CharText(FunctionEmitter& emitter, const Value& value);

  Value SignedText(FunctionEmitter& emitter, const Value& wide);

  Value DecimalText(FunctionEmitter& emitter, const Value& magnitude, const String& negative);

  // ── atomics, vector shuffles and the string hash (LlvmAtomics.cpp) ──
  bool AtomicBuiltin(FunctionEmitter& emitter, const Expr& expr, Value& out);

  bool ShuffleBuiltin(FunctionEmitter& emitter, const Expr& expr, Value& out);

  Value AtomicFence(FunctionEmitter& emitter, const Expr& expr);

  Value AtomicAccess(FunctionEmitter& emitter, const Expr& expr, const Value& pointer);

  Value AtomicLoad(FunctionEmitter& emitter, const Expr& expr, const Value& pointer);

  Value AtomicStore(FunctionEmitter& emitter, const Expr& expr, const Value& pointer);

  Value AtomicRmw(FunctionEmitter& emitter, const Expr& expr, const Value& pointer);

  Value AtomicCmpxchg(FunctionEmitter& emitter, const Expr& expr, const Value& pointer);

  Value StringHash(FunctionEmitter& emitter, const Value& text);

  // ── maps and sets (LlvmMaps.cpp) ──
  String EnsureKeyFunctions(FunctionEmitter& emitter, const TypePtr& key);  // the key's functions, once per module; the answer is its tag

  void DefineKeyFunctions(FunctionEmitter& emitter, const TypePtr& key, const String& tag);

  void DefineKeyKind(FunctionEmitter& emitter, const TypePtr& key, const String& tag);

  void DefineStructKey(FunctionEmitter& emitter, const StructDef& structure);

  void DefineListKey(FunctionEmitter& emitter, const TypePtr& list);

  String CastLines(FunctionEmitter& emitter, const WordCast& cast);

  String KeyWord(FunctionEmitter& emitter, const Value& key);

  Value WordValue(FunctionEmitter& emitter, const String& word, const TypePtr& type);

  String MapReadCell(FunctionEmitter& emitter, const Value& map, const Value& key);

  String MapWriteCell(FunctionEmitter& emitter, const Value& map, const Value& key);

  Value NewMap(FunctionEmitter& emitter, const TypePtr& type, const String& requested);

  Value NewMapFrom(FunctionEmitter& emitter, const Expr& expr);

  Value SetLiteral(FunctionEmitter& emitter, const Expr& expr);

  Value MapAdd(FunctionEmitter& emitter, const Value& set, const Value& member);

  Value MapHas(FunctionEmitter& emitter, const Value& map, const Value& key);

  Value MapRemove(FunctionEmitter& emitter, const Value& map, const Value& key);

  Value MapCopy(FunctionEmitter& emitter, const Value& map);

  Value MapWith(FunctionEmitter& emitter, const Expr& expr);

  Value MapWord(FunctionEmitter& emitter, const Value& map, int64_t cell);

  void MapFree(FunctionEmitter& emitter, const Value& map);

  bool MapBuiltin(FunctionEmitter& emitter, const Expr& expr, Value& out);

  bool MapOperation(FunctionEmitter& emitter, const Expr& expr, Value& out);

  bool MapUpdate(FunctionEmitter& emitter, const Expr& expr, Value& out);

  bool MapOther(FunctionEmitter& emitter, const Expr& expr, Value& out);

  bool MapSlot(FunctionEmitter& emitter, const Expr& expr, Value& out);  // AssocLive, AssocKey, AssocValue: walking the slots of a map or set

  String SlotAddress(FunctionEmitter& emitter, const Value& map, int64_t table, const Value& slot);

  Value FreedMap(FunctionEmitter& emitter, const Expr& expr);

  void NeedKeyRuntime(FunctionEmitter& emitter);

  void NeedMapNew(FunctionEmitter& emitter);  // @map_new, which makes a map, and the collector it allocates from

  void DefineStringKey(FunctionEmitter& emitter);

  // `==` on maps and sets (through the key functions).
  Value MapComparison(FunctionEmitter& emitter, BinOp op, const Value& left, const Value& right);

  // Whether the type is a value struct (no `var` field): two are equal when their fields are.
  bool IsValueStruct(const FunctionEmitter& emitter, const TypePtr& type);

  // Whether the module has the LlvmEquality.cpp helper that compares two values of the type.
  bool HasEqualityHelper(const FunctionEmitter& emitter, const TypePtr& type);

  // `==` on a value that has an LlvmEquality.cpp helper (a call of it).
  Value HelperComparison(FunctionEmitter& emitter, BinOp op, const Value& left, const Value& right);

  // `==` on any other value struct (through the key functions).
  Value StructComparison(FunctionEmitter& emitter, BinOp op, const Value& left, const Value& right);

  // ── closures and function values (LlvmClosures.cpp) ──
  Value ClosureLit(FunctionEmitter& emitter, const Expr& expr);

  bool ClosureBuiltin(FunctionEmitter& emitter, const Expr& expr, Value& out);

  Value FreedClosure(FunctionEmitter& emitter, const Expr& expr);

  void FreeEnvironment(FunctionEmitter& emitter, const Value& environment);

  bool IsCaptured(const FunctionEmitter& emitter, const Expr& closure, const String& name);

  Value FuncRef(FunctionEmitter& emitter, const Expr& expr);

  Value CallIndirect(FunctionEmitter& emitter, const Expr& expr);

  Vector<String> CaptureCells(FunctionEmitter& emitter, const Expr& expr, Vector<Capture>& captures);

  String CaptureCell(FunctionEmitter& emitter, const String& name, Vector<Capture>& captures);

  Value EnvironmentOf(FunctionEmitter& emitter, const Vector<String>& cells);

  Value ClosureBlock(FunctionEmitter& emitter, const String& body, const Value& environment, const TypePtr& type);

  String IndirectTarget(FunctionEmitter& emitter, const Value& callee, String& leading);

  String CallInstruction(FunctionEmitter& emitter, const TypePtr& result, const String& target, const String& args);  // writes the call; the register of its result (empty for a void call)

  // ── interfaces (LlvmInterface.cpp) ──
  bool InterfaceBuiltin(FunctionEmitter& emitter, const Expr& expr, Value& out);

  Value InterfaceBox(FunctionEmitter& emitter, const Expr& expr, const Value& object);

  Value InterfaceCall(FunctionEmitter& emitter, const Expr& expr, const Value& interfaceValue);

  Value MethodOf(FunctionEmitter& emitter, const Value& interfaceValue, int64_t slot);

  // ── coroutines (LlvmCoroutine.cpp) ──
  void OpenCoroutine(FunctionEmitter& emitter);

  void FinishBody(FunctionEmitter& emitter);

  void YieldStatement(FunctionEmitter& emitter, const Stmt& statement);

  void SuspendAt(FunctionEmitter& emitter, bool last, const String& next);

  void CoroutineFrame(FunctionEmitter& emitter);

  void CoroutineRamp(FunctionEmitter& emitter);

  String YieldSlot(FunctionEmitter& emitter);

  void EndCoroutine(FunctionEmitter& emitter);

  void JoinFinalSuspend(FunctionEmitter& emitter);

  void LeaveCoroutine(FunctionEmitter& emitter, const Stmt& statement);

  void ReturnCoroutine(FunctionEmitter& emitter);

  Value CoroutineVariant(FunctionEmitter& emitter, const Value& coroutine, const String& finished);

  void CoroutineExit(FunctionEmitter& emitter);

  String Named(FunctionEmitter& emitter, const String& base, const String& text);

  String FunctionAttributes(const FunctionEmitter& emitter);

  bool CoroutineBuiltin(FunctionEmitter& emitter, const Expr& expr, Value& out);

  Value CoroutineResume(FunctionEmitter& emitter, const Value& coroutine);

  Value CoroutineDone(FunctionEmitter& emitter, const Value& coroutine);

  Value CoroutineYielded(FunctionEmitter& emitter, const Expr& expr);

  Value CoroutineClose(FunctionEmitter& emitter, const Value& coroutine);

  void SetCoroutineCell(FunctionEmitter& emitter, const String& block, int64_t word, int64_t value);

  String LoadCoroutinePointer(FunctionEmitter& emitter, const Value& coroutine, int64_t word);

  // ── sparse grids (LlvmSparse.cpp) ──
  Value CopySparse(FunctionEmitter& emitter, const Value& sparse);

  Value EmptySparse(FunctionEmitter& emitter, const TypePtr& type);

  Value SparseList(FunctionEmitter& emitter, const Value& sparse, int64_t which);

  Value SparseRead(FunctionEmitter& emitter, const Expr& expr, const Value& sparse, const Vector<Value>& keys);

  String SparseCell(FunctionEmitter& emitter, const Value& sparse, const Vector<Value>& keys);

  Value SparseEntry(FunctionEmitter& emitter, const TypePtr& type, const Value& sparse, const String& found);

  Vector<Value> SparseKeys(FunctionEmitter& emitter, const Vector<Value>& indices);

  // ── statements (LlvmCompiler.cpp; labels, branches and switches in LlvmLabels.cpp) ──
  void EmitBlock(FunctionEmitter& emitter, const StmtList& statements);

  void Statement(FunctionEmitter& emitter, const Stmt& statement);

  void DeclareVariable(FunctionEmitter& emitter, const Stmt& statement);

  void Assign(FunctionEmitter& emitter, const Stmt& statement);

  void IfStatement(FunctionEmitter& emitter, const Stmt& statement);

  void ForStatement(FunctionEmitter& emitter, const Stmt& statement);

  void ReturnStatement(FunctionEmitter& emitter, const Stmt& statement);

  void LabelStatement(FunctionEmitter& emitter, const Stmt& statement);

  void GotoStatement(FunctionEmitter& emitter, const Stmt& statement);

  void BranchStatement(FunctionEmitter& emitter, const Stmt& statement);

  void SwitchStatement(FunctionEmitter& emitter, const Stmt& statement);

  void CollectLabels(FunctionEmitter& emitter);

  FunctionEmitter::LabelInfo LabelInfoOf(FunctionEmitter& emitter, const Stmt& statement);

  FunctionEmitter::LabelInfo& LabelNamed(FunctionEmitter& emitter, const String& label);

  Slot LabelSlot(FunctionEmitter& emitter, const Stmt& statement, size_t i);

  Slot OpenParameter(FunctionEmitter& emitter, FunctionEmitter::LabelInfo& info, const Stmt& statement, size_t i);

  void FallInto(FunctionEmitter& emitter, FunctionEmitter::LabelInfo& info);

  void FeedPhi(FunctionEmitter& emitter, FunctionEmitter::LabelInfo& info, size_t i, const Value& value);

  void HandOne(FunctionEmitter& emitter, FunctionEmitter::LabelInfo& info, size_t i, const Value& value);

  void HandOverAll(FunctionEmitter& emitter, const Vector<JumpEdge>& edges);

  bool GoesDirect(FunctionEmitter& emitter, const Vector<JumpEdge>& edges);

  bool EdgeIsDirect(FunctionEmitter& emitter, const JumpEdge& edge, Set<String>& seen);

  void DirectBranch(FunctionEmitter& emitter, const Stmt& statement, const Value& condition);

  void TrampolineBranch(FunctionEmitter& emitter, const Stmt& statement, const Value& condition);

  void DirectSwitch(FunctionEmitter& emitter, const Stmt& statement, const Value& subject);

  void TrampolineSwitch(FunctionEmitter& emitter, const Stmt& statement, const Value& subject);

  void PatchPhis(FunctionEmitter& emitter);

  void PatchLabel(FunctionEmitter& emitter, const FunctionEmitter::LabelInfo& info);

  Vector<Vector<Value>> ValuesOfEdges(FunctionEmitter& emitter, const Vector<JumpEdge>& edges);

  void FeedAll(FunctionEmitter& emitter, FunctionEmitter::LabelInfo& info, const Vector<Value>& values);

  void ExpressionStatement(FunctionEmitter& emitter, const Stmt& statement);

  void ScopedBlock(FunctionEmitter& emitter, const StmtList& statements);

  void BranchIf(FunctionEmitter& emitter, const Expr& condition, const String& yes, const String& no);

  void ArmBlock(FunctionEmitter& emitter, const String& name, const StmtList& body, const String& next);

  void IfArms(FunctionEmitter& emitter, const Stmt& statement, const String& then, const String& otherwise, const String& end);

  void ForLoop(FunctionEmitter& emitter, const Stmt& statement);

  void LoopTest(FunctionEmitter& emitter, const String& test, const Expr& condition, const String& body, const String& end);

  void LoopStep(FunctionEmitter& emitter, const Stmt& statement, const String& step, const String& test, const String& end);

  void ReturnValue(FunctionEmitter& emitter, const Stmt& statement);

  void JumpToLabel(FunctionEmitter& emitter, const String& label, const Vector<ExprPtr>& args);

  void EdgeTo(FunctionEmitter& emitter, const String& block, const String& label, const Vector<ExprPtr>& args);

  String CaseTable(const Stmt& statement, const Value& subject, const Vector<String>& targets);

  void CaseBlocks(FunctionEmitter& emitter, const Stmt& statement, const Vector<String>& targets);

  void AllocateLabelCells(FunctionEmitter& emitter);

  String Prepare(FunctionEmitter& emitter);

  String DefinitionScope(const FunctionEmitter& emitter);

  String Definition(FunctionEmitter& emitter, const String& parameters);

  void StoreArguments(FunctionEmitter& emitter, const String& label, const Vector<ExprPtr>& args);

  void BindParameters(FunctionEmitter& emitter, String& declaration);

  void ReadCaptures(FunctionEmitter& emitter);

  // ── walks over a function's statements (LlvmMemory.cpp) and the tidying of its blocks (LlvmBlocks.cpp) ──
  // The names the closures in these statements capture (so a function can keep them in heap cells).
  Set<String> CapturedNames(const StmtList& body);

  Set<String> AssignedNames(const StmtList& body);

  bool ThrowsAnywhere(const Module& module);

  Vector<String> TidyBlocks(const Vector<String>& lines);  // the function's lines without the blocks nothing needs

}  // namespace polyir::llvm
