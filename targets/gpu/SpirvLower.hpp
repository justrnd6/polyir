// What the SPIR-V writer of a kernel's functions shares (SpirvTypes.nina, SpirvExpr.nina, SpirvStmt.nina, SpirvAtomic.nina, SpirvRemainder.nina):
// the module, the arrays every function reaches by name (a GPU kernel has no pointers: an array parameter is a global buffer), the kernels written so
// far, and the lowering of statements and expressions. The order in which IDs are handed out follows the Nina writer exactly, so both write the same
// bytes: every value an instruction needs is made before the instruction itself.
#pragma once

#include "gpu/SpirvModule.hpp"

namespace polyir {

  // An array reached by name: a storage buffer (Uniform storage class) or workgroup memory (Workgroup). `holdsFloatBits` marks a float
  // array that `atomicAddFloat` updates: its slots are stored as raw 32-bit words and read back as floats.
  struct SpvArray {
    String name;
    Id variable = 0;
    TypePtr element;
    int storage = kStorageUniform;
    bool holdsFloatBits = false;
  };

  // A kernel already written, so another can call it: its function, what it returns, and the types of the parameters passed (not the arrays).
  struct SpvKnown {
    String name;
    Id id = 0;
    TypePtr returns;
    Vector<TypePtr> params;
  };

  // A name in scope: a `let` or a parameter is a value, a `var` or a loop variable is a variable (a pointer to load from and store to).
  struct SpvBinding {
    String name;
    Id id = 0;
    TypePtr type;
    bool isVar = false;
  };

  using SpvScope = Vector<SpvBinding>;  // searched from the back: the newest name first

  // The writer of one kernel's SPIR-V module: the module, the arrays every function reaches by name, and the functions written so far.
  struct SpvWriter {
    SpvModule& module;
    Vector<SpvArray> arrays;
    Vector<SpvKnown> functions;
    Id glsl = 0;  // the "GLSL.std.450" instruction set
  };

  // What every lowering function takes as `body`: the kernel being written, the one function whose blocks the statements and
  // expressions are lowered into, and the module that hands out IDs, types and constants.
  struct SpvBody {
    SpvWriter& writer;
    SpvFunction& function;
    SpvModule& module;
  };

  // ── the SPIR-V side of HIR types ──
  // The SPIR-V type of a HIR type: integers are 32-bit unless 64-bit; anything that is no number, bool or vector is `void`.
  Id SpvType(SpvModule& m, const TypePtr& t);

  bool SpvIsBool(const TypePtr& t);

  bool SpvIsFloat(const TypePtr& t);  // a float or a vector of floats

  bool SpvIsVector(const TypePtr& t);

  bool SpvIsNumber(const TypePtr& t);

  TypePtr SpvScalar(const TypePtr& t);

  int SpvLanes(const TypePtr& t);

  int SpvBits(const TypePtr& t);  // 64 for a 64-bit integer, 32 for anything else

  TypePtr SpvPointee(const TypePtr& t);

  Id SpvConstant(SpvModule& m, const TypePtr& t, double value);  // a scalar, or the same number in every lane of a vector

  Words SpvRepeat(Id id, int count);

  const TypePtr& SpvInt32();

  const TypePtr& SpvFlag();

  const TypePtr& SpvFloat32();

  const SpvArray* SpvArrayNamed(const Vector<SpvArray>& arrays, const String& name);

  // ── the lowering of statements and expressions ──
  SpvScope LowerBlock(SpvBody& body, SpvScope scope, const StmtList& stmts, const TypePtr& returns);

  Id LowerExpr(SpvBody& body, const SpvScope& scope, const Expr& e);

  Id ExprTo(SpvBody& body, const SpvScope& scope, const Expr& e, const TypePtr& target);

  Id SlotValue(SpvBody& body, const SpvArray& array, Id slot);

  void StoreSlot(SpvBody& body, const SpvArray& array, Id slot, Id value);

  SpvScope LowerStmt(SpvBody& body, SpvScope scope, const Stmt& s, const TypePtr& returns);

  void LowerFlow(SpvBody& body, const SpvScope& scope, const Stmt& s, const TypePtr& returns);

  SpvScope Declaration(SpvBody& body, SpvScope scope, const Stmt& s);

  Id StartValue(SpvBody& body, const SpvScope& scope, const Stmt& s);

  void Assign(SpvBody& body, const SpvScope& scope, const Stmt& s);

  void AssignVariable(SpvBody& body, const SpvScope& scope, const String& name, const Expr& value);

  void AssignSlot(SpvBody& body, const SpvScope& scope, const Expr& base, const Expr& position, const Expr& value);

  void IfStmt(SpvBody& body, const SpvScope& scope, const Stmt& s, const TypePtr& returns);

  void Arm(SpvBody& body, const SpvScope& scope, Id label, const StmtList& statements, Id merge, const TypePtr& returns);

  void ForStmt(SpvBody& body, const SpvScope& scope, const Stmt& s, const TypePtr& returns);

  SpvScope CarriedVariables(SpvBody& body, const SpvScope& scope, const Stmt& s);

  SpvBinding CarriedVariable(SpvBody& body, const SpvScope& scope, const String& name, const Expr& init);

  void Advance(SpvBody& body, const SpvScope& scope, const SpvScope& carried, const Stmt& s);

  Id Unsupported(SpvBody& body, const String& what);

  Id Read(SpvBody& body, const SpvScope& scope, const String& name);

  Id LowerLiteral(SpvBody& body, const Expr& e);

  Id Compound(SpvBody& body, const SpvScope& scope, const Expr& e);

  Id LiteralTo(SpvBody& body, const SpvScope& scope, const Expr& e, const TypePtr& target, double value);

  Id Convert(SpvBody& body, Id id, const TypePtr& from, const TypePtr& to);

  Id ConvertNumber(SpvBody& body, Id id, const TypePtr& from, const TypePtr& to);

  Id BoolToNumber(SpvBody& body, Id flag, const TypePtr& to);

  Id NumberToBool(SpvBody& body, Id id, const TypePtr& from);

  Id Binary(SpvBody& body, const SpvScope& scope, const Expr& e);

  Id ShortCircuit(SpvBody& body, const SpvScope& scope, const Expr& e);

  void RightSide(SpvBody& body, const SpvScope& scope, const Expr& right, Id answer, Id block, Id merge);

  Id Arithmetic(SpvBody& body, BinOp op, const TypePtr& t, Id a, Id b);

  Id Unary(SpvBody& body, const SpvScope& scope, const Expr& e);

  Id VectorLiteral(SpvBody& body, const SpvScope& scope, const Expr& e);

  Id Index(SpvBody& body, const SpvScope& scope, const Expr& e);

  Id Lane(SpvBody& body, const SpvScope& scope, const Expr& e);

  const SpvArray* ArrayOf(const SpvBody& body, const Expr& e);

  Id SlotPointer(SpvBody& body, const SpvArray& array, Id slot);

  Id StoredType(SpvBody& body, const SpvArray& array);

  Id Call(SpvBody& body, const SpvScope& scope, const Expr& e);

  Id Math(SpvBody& body, const SpvScope& scope, const Expr& e);

  Id KernelCall(SpvBody& body, const SpvScope& scope, const Expr& e);

  Id Atomic(SpvBody& body, const SpvScope& scope, const Expr& e);

  Id IntegerUpdate(SpvBody& body, const String& name, Id pointer, Id amount);

  Id AddFloat(SpvBody& body, Id pointer, Id amount);

  void SwapLoop(SpvBody& body, Id pointer, Id amount, Id seen);

  void TrySwap(SpvBody& body, Id pointer, Id amount, Id seen, Id merge, Id step);

  Id Group(SpvBody& body, const SpvScope& scope, const Expr& e);

  Id Barrier(SpvBody& body);

  Id Device(SpvBody& body);

  Id Relaxed(SpvBody& body);

  Id Remainder(SpvBody& body, const TypePtr& t, Id a, Id b);

  Id RoughRemainder(SpvBody& body, const TypePtr& t, Id ax, Id ay);

  Id MoveIntoRange(SpvBody& body, const TypePtr& t, Id r, Id ay);

  Id WithSignOf(SpvBody& body, const TypePtr& t, Id a, Id r);

  Id Absolute(SpvBody& body, const TypePtr& t, Id x);

  Id FlagType(SpvBody& body, const TypePtr& t);

  inline Id Device(SpvBody& body) {
    return ConstUint(body.module, 1);
  }

  inline Id Relaxed(SpvBody& body) {
    return ConstUint(body.module, 0);
  }

}  // namespace polyir
