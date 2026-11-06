// Atomic memory operations, vector shuffles and string hashing: the builtins that map to one LLVM instruction or a small helper.
#include "polyir/Std.hpp"
#include "emit/llvm/LlvmEmitter.hpp"

namespace polyir::llvm {

  namespace {

    // An ordering or operation name is written in the program as a string constant.
    String literal(const Expr& expr) {
      if (expr.kind != ExprKind::ConstString) {
        Fail("an atomic operation's ordering and kind must be string constants");
      }

      return expr.text;
    }

    int byteSize(const TypePtr& type) {
      if (type->kind == TypeKind::Float) {
        return type->bits / 8;
      }

      return IsInteger(*type) ? (type->bits + 7) / 8 : 8;
    }

    const char* const kHashFunction = R"(define i64 @string_hash(ptr %s) {
entry:
  br label %loop
loop:
  %h = phi i64 [ 5381, %entry ], [ %next, %body ]
  %i = phi i64 [ 0, %entry ], [ %i2, %body ]
  %p = getelementptr i8, ptr %s, i64 %i
  %c = load i8, ptr %p
  %end = icmp eq i8 %c, 0
  br i1 %end, label %done, label %body
body:
  %cz = zext i8 %c to i64
  %m = mul i64 %h, 33
  %next = add i64 %m, %cz
  %i2 = add i64 %i, 1
  br label %loop
done:
  ret i64 %h
})";

    bool isAtomic(Builtin op) {
      return op == Builtin::AtomicLoad || op == Builtin::AtomicStore || op == Builtin::AtomicRmw || op == Builtin::AtomicCmpxchg || op == Builtin::AtomicFence;
    }

    TypePtr atomicType(const Expr& expr) {
      return expr.operands[0]->type->element;
    }

    String alignText(const Expr& expr) {
      return ", align " + to_string(byteSize(atomicType(expr)));
    }

    String shufflePicks(const Expr& expr) {
      String picks;

      for (size_t i = 1; i < expr.operands.size(); i++) {
        picks += (i == 1 ? "" : ", ") + String("i32 ") + to_string(expr.operands[i]->intValue);
      }

      return picks;
    }

  }  // namespace

  const char* HashFunctionText() {
    return kHashFunction;
  }

  bool AtomicBuiltin(FunctionEmitter& emitter, const Expr& expr, Value& out) {
    if (!isAtomic(expr.builtin)) {
      return false;
    }

    out = expr.builtin == Builtin::AtomicFence ? AtomicFence(emitter, expr) : AtomicAccess(emitter, expr, EmitExpr(emitter, *expr.operands[0]));

    return true;
  }

  // `fence(ordering)`: it has no receiver, so operand 0 is a placeholder zero and operand 1 the ordering.
  Value AtomicFence(FunctionEmitter& emitter, const Expr& expr) {
    Line(emitter, "fence " + literal(*expr.operands[1]));

    return Value{"0", MakeInt(64)};
  }

  Value AtomicAccess(FunctionEmitter& emitter, const Expr& expr, const Value& pointer) {
    switch (expr.builtin) {
      case Builtin::AtomicLoad:
        return AtomicLoad(emitter, expr, pointer);
      case Builtin::AtomicStore:
        return AtomicStore(emitter, expr, pointer);
      case Builtin::AtomicRmw:
        return AtomicRmw(emitter, expr, pointer);
      default:
        return AtomicCmpxchg(emitter, expr, pointer);
    }
  }

  // `p.atomicLoad(ordering)`: the operands are p and the ordering.
  Value AtomicLoad(FunctionEmitter& emitter, const Expr& expr, const Value& pointer) {
    const TypePtr type = atomicType(expr);

    return Value{Instruction(emitter, "load atomic " + LlvmTypeText(type) + ", ptr " + pointer.text + " " + literal(*expr.operands[1]) + alignText(expr)), type};
  }

  // `p.atomicStore(value, ordering)`: the operands are p, the value and the ordering.
  Value AtomicStore(FunctionEmitter& emitter, const Expr& expr, const Value& pointer) {
    const Value value = EmitExpr(emitter, *expr.operands[1]);
    Line(emitter, "store atomic " + Typed(value) + ", ptr " + pointer.text + " " + literal(*expr.operands[2]) + alignText(expr));

    return value;
  }

  // `p.atomicRmw(op, value, ordering)`, which gives the old value: the operands are p, the operation, the value and the ordering.
  Value AtomicRmw(FunctionEmitter& emitter, const Expr& expr, const Value& pointer) {
    const Value value = EmitExpr(emitter, *expr.operands[2]);

    return Value{Instruction(emitter, "atomicrmw " + literal(*expr.operands[1]) + " ptr " + pointer.text + ", " + Typed(value) + " " + literal(*expr.operands[3]) + alignText(expr)), atomicType(expr)};
  }

  // `p.cmpxchg(expected, replacement, success, failure)`, which gives the old value: the last two operands are the orderings when the
  // exchange happens and when it does not.
  Value AtomicCmpxchg(FunctionEmitter& emitter, const Expr& expr, const Value& pointer) {
    const Value expected = EmitExpr(emitter, *expr.operands[1]);
    const Value replacement = EmitExpr(emitter, *expr.operands[2]);
    const String orders = literal(*expr.operands[3]) + " " + literal(*expr.operands[4]) + alignText(expr);
    const String pair = Instruction(emitter, "cmpxchg ptr " + pointer.text + ", " + Typed(expected) + ", " + Typed(replacement) + " " + orders);

    return Value{Instruction(emitter, "extractvalue { " + LlvmTypeText(atomicType(expr)) + ", i1 } " + pair + ", 0"), atomicType(expr)};
  }

  // `v.shuffle(3, 2, 1, 0)`: the elements of the vector picked by the (constant) numbers.
  bool ShuffleBuiltin(FunctionEmitter& emitter, const Expr& expr, Value& out) {
    if (expr.builtin != Builtin::Shuffle) {
      return false;
    }

    const Value value = EmitExpr(emitter, *expr.operands[0]);
    const String count = to_string(expr.operands.size() - 1);
    out = Value{Instruction(emitter, "shufflevector " + Typed(value) + ", " + Typed(value) + ", <" + count + " x i32> <" + shufflePicks(expr) + ">"), expr.type};

    return true;
  }

  Value StringHash(FunctionEmitter& emitter, const Value& text) {
    Need(emitter.context_, kHashFunction);

    return Value{Instruction(emitter, "call i64 @string_hash(ptr " + text.text + ")"), MakeInt(64)};
  }

}  // namespace polyir::llvm
