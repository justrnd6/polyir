// Operator rules (CheckOps.nina): which source operator is which HIR operator, and what type the result has.
#include "front/Checker.hpp"

namespace polyir::front {

  // The HIR operator of a source operator (the table is in the order of the Operator enum).
  BinOp BinOpOf(Operator op) {
    static const BinOp table[] = {BinOp::Mul, BinOp::Div, BinOp::Rem, BinOp::Add, BinOp::Sub, BinOp::Shl, BinOp::Shr, BinOp::And, BinOp::Xor, BinOp::Or,
      BinOp::Eq, BinOp::Ne, BinOp::Lt, BinOp::Le, BinOp::Gt, BinOp::Ge, BinOp::Identical, BinOp::NotIdentical, BinOp::LogAnd, BinOp::LogOr};

    return table[static_cast<size_t>(op)];
  }

  namespace {

    bool isComparison(BinOp op) {
      return (op >= BinOp::Eq && op <= BinOp::Ge) || op == BinOp::Identical || op == BinOp::NotIdentical;
    }

  }  // namespace

  // Comparisons give i1 (a vector of i1 for vector operands); every other operator keeps the left operand's type.
  TypePtr ResultType(BinOp op, const TypePtr& left) {
    if (!isComparison(op)) {
      return left;
    }

    if (left->kind == TypeKind::Vec) {
      return MakeVec(IntType(1), left->size);
    }

    return IntType(1);
  }

  // `+` on these types joins them instead of adding.
  bool IsConcatType(const TypePtr& type) {
    return type->kind == TypeKind::String || type->kind == TypeKind::DynamicArray;
  }

}  // namespace polyir::front
