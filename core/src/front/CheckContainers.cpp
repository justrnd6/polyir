// Reading from containers and making their storage (the parts of CheckContainers.nina the core needs): `x[i]` on anything indexable, and the fresh
// storage of `let x: T` with no value. The constructors are registered with `AddExpr(registry, kind, handler)` by CheckVec.cpp (`vec[...]`,
// `mutarray<T>()`, maps and sets), CheckSums.cpp (`variant<...>(...)`, `eitherOk`, `Some`, ...), CheckLists.cpp (`[...]`) and CheckClosure.cpp (closures).
#include "front/Checker.hpp"

namespace polyir::front {

  // What indexing a value of this type gives.
  Optional<TypePtr> ElementOf(const TypePtr& type) {
    switch (type->kind) {
      case TypeKind::Pointer: case TypeKind::Vec: case TypeKind::DynamicArray: case TypeKind::FixedArray: case TypeKind::Buffer: case TypeKind::Grid:
      case TypeKind::Tensor: case TypeKind::Sparse:
        return type->element;
      case TypeKind::Hashmap:
        return type->second;
      default:
        return nullopt;
    }
  }

  // `base[i, j]`: the element type depends on the container.
  ExprPtr IndexInto(ExprPtr receiver, Vector<ExprPtr> indices) {
    if (receiver->type->kind == TypeKind::String) {
      return CharAt(move(receiver), move(indices));
    }

    const Optional<TypePtr> element = ElementOf(receiver->type);
    const Span at = receiver->at;

    if (!element) {
      return Unsupported("index on a non-indexable value", at);
    }

    return IndexGetNode(move(receiver), move(indices), *element, at);
  }

  namespace {

    bool isTable(TypeKind kind) {
      return kind == TypeKind::Hashmap || kind == TypeKind::Set;
    }

    // The containers other than maps and sets that a constructor with no arguments makes empty.
    bool startsEmpty(TypeKind kind) {
      return kind == TypeKind::FixedArray || kind == TypeKind::Grid || kind == TypeKind::Tensor || kind == TypeKind::DynamicArray ||
        kind == TypeKind::Sparse;
    }

    // A map or set as its constructor makes it: room for 4 entries (immutable) or 16 (changeable); the key is checked.
    ExprPtr emptyTable(Checker& check, const TypePtr& type, Span at) {
      if (const Optional<String> why = KeyProblem(check, type->element)) {
        return Unsupported(*why, at);
      }

      Vector<ExprPtr> room{ConstInt(type->isMutable ? 16 : 4, IntType(64), NoSpan())};

      return ContainerNode(ExprKind::NewContainer, move(room), type, NoSpan());
    }

  }  // namespace

  // What a container declared with no value starts as: the same as its constructor with no arguments (`mutarray<T>()`, `sparse<T>()`,
  // `mutmap<K,V>()`, `map<K,V>()`, ...). It is null for any other type.
  ExprPtr FreshStorage(Checker& check, const TypePtr& type, Span at) {
    if (isTable(type->kind)) {
      return emptyTable(check, type, at);
    }

    if (!startsEmpty(type->kind)) {
      return nullptr;
    }

    return ContainerNode(ExprKind::NewContainer, {}, type, NoSpan());
  }

}  // namespace polyir::front
