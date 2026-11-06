// Which comparisons are allowed (CheckEquality.nina). `==` and `!=` compare what two values hold (a struct field by field, a string by its characters,
// a list element by element); `===` and `!==` compare identity (the same object), so they only make sense for objects. A number has no identity, and a
// closure, a coroutine or a pointer has nothing to compare but identity.
#include "front/Checker.hpp"

namespace polyir::front {

  namespace {

    String typeName(const TypePtr& type) {
      return TypeText(*type);
    }

    Optional<String> useIdentity(const TypePtr& type) {
      return "a " + typeName(type) + " cannot be compared with ==: use === to ask whether it is the same one";
    }

    Optional<String> notYet(const TypePtr& type) {
      return "== is not supported yet on " + typeName(type);
    }

    // A struct compares field by field, but an interface value (a struct of two pointers) cannot yet.
    Optional<String> structProblem(Checker& check, const TypePtr& type) {
      if (!IsInterface(DeclsOf(check), SymbolOf(check, type->name))) {
        return nullopt;
      }

      return String("== on an interface value is not supported yet: compare the structs it holds");
    }

    // Why `==` cannot compare two values of this type, or nothing.
    Optional<String> valueProblem(Checker& check, const TypePtr& type) {
      switch (type->kind) {
        case TypeKind::Closure: case TypeKind::Coroutine: case TypeKind::Pointer: case TypeKind::Function:
          return useIdentity(type);
        case TypeKind::Struct:
          return structProblem(check, type);
        case TypeKind::Sparse: case TypeKind::Grid: case TypeKind::Tensor:
          return notYet(type);
        default:
          return nullopt;
      }
    }

    // Why `===` cannot compare two values of this type (they have no identity), or nothing.
    Optional<String> identityProblem(const TypePtr& type) {
      switch (type->kind) {
        case TypeKind::Int: case TypeKind::UInt: case TypeKind::Float: case TypeKind::Vec: case TypeKind::Variant: case TypeKind::Either:
        case TypeKind::Optional: case TypeKind::Void: case TypeKind::FixedArray:
          return "=== asks whether two objects are the same one, and a " + typeName(type) + " is a plain value: use ==";
        default:
          return nullopt;
      }
    }

  }  // namespace

  // Why `left op right` is not allowed (only the left operand's type matters), or nothing.
  Optional<String> EqualityProblem(Checker& check, BinOp op, const Expr& left) {
    if (op == BinOp::Eq || op == BinOp::Ne) {
      return valueProblem(check, left.type);
    }

    if (op == BinOp::Identical || op == BinOp::NotIdentical) {
      return identityProblem(left.type);
    }

    return nullopt;
  }

}  // namespace polyir::front
