// The container constructors (CheckContainers.nina `constructor`): `vec[a, b]`, `mutarray<T>()`, `array<T>()`, `sparse<T>()`, `buffer<T, N>()`, and the
// keyed ones `map<K,V>()`, `mutmap<K,V>(cap)`, `set<T>()`, `mutset<T>(cap)`, whose key type must be one that keeps its hash.
#include "front/Checker.hpp"

namespace polyir::front {

  namespace {

    // A new container with nothing in it.
    ExprPtr emptyOf(const TypePtr& type) {
      return ContainerNode(ExprKind::NewContainer, {}, type, NoSpan());
    }

    // An empty immutable map or set starts with room for a few entries; `with` makes a bigger one when it needs to.
    ExprPtr sizedOf(const TypePtr& type) {
      Vector<ExprPtr> room{ConstInt(4, IntType(64), NoSpan())};

      return ContainerNode(ExprKind::NewContainer, move(room), type, NoSpan());
    }

    // The size the table starts at: the one written, else 16 (it grows as it fills).
    ExprPtr capacityOf(Checker& check, const AstExpr& node) {
      return Valid(node.first) ? CheckExpr(check, node.first) : ConstInt(16, IntType(64), NoSpan());
    }

    ExprPtr withCapacity(Checker& check, const AstExpr& node, const TypePtr& type) {
      Vector<ExprPtr> capacity{capacityOf(check, node)};

      return ContainerNode(ExprKind::NewContainer, move(capacity), type, NoSpan());
    }

    // The constructor `made`, or a problem node when the key type cannot be a map or set key.
    ExprPtr keyed(Checker& check, const AstExpr& node, const TypePtr& key, ExprPtr made) {
      if (const Optional<String> why = KeyProblem(check, key)) {
        return Unsupported(*why, SpanAt(node.pos));
      }

      return made;
    }

    TypePtr typeArg(Checker& check, TypeId id) {
      return ResolveType(check, id);
    }

    ExprPtr newMutarray(Checker& check, const AstExpr& node) {
      return emptyOf(MakeDynamicArray(typeArg(check, node.type), true));
    }

    ExprPtr newArray(Checker& check, const AstExpr& node) {
      return emptyOf(MakeDynamicArray(typeArg(check, node.type), false));
    }

    ExprPtr newSparse(Checker& check, const AstExpr& node) {
      return emptyOf(MakeSparse(typeArg(check, node.type)));
    }

    ExprPtr newBuffer(Checker& check, const AstExpr& node) {
      return emptyOf(MakeBuffer(typeArg(check, node.type), static_cast<int>(ParseInt(node.literal.text))));
    }

    ExprPtr newMap(Checker& check, const AstExpr& node) {
      const TypePtr key = typeArg(check, node.type);

      return keyed(check, node, key, sizedOf(MakeHashmap(key, typeArg(check, node.type2), false)));
    }

    ExprPtr newSet(Checker& check, const AstExpr& node) {
      const TypePtr key = typeArg(check, node.type);

      return keyed(check, node, key, sizedOf(MakeSet(key, false)));
    }

    ExprPtr newMutmap(Checker& check, const AstExpr& node) {
      const TypePtr key = typeArg(check, node.type);

      return keyed(check, node, key, withCapacity(check, node, MakeHashmap(key, typeArg(check, node.type2), true)));
    }

    ExprPtr newMutset(Checker& check, const AstExpr& node) {
      const TypePtr key = typeArg(check, node.type);

      return keyed(check, node, key, withCapacity(check, node, MakeSet(key, true)));
    }

    // Element and count are inferred from what is listed.
    ExprPtr vecLiteral(Checker& check, const AstExpr& node) {
      Vector<ExprPtr> values = CheckAll(check, node.args);
      const TypePtr element = values.empty() ? IntType(64) : values[0]->type;
      const TypePtr type = MakeVec(element, static_cast<int>(values.size()));

      return ContainerNode(ExprKind::VecLit, move(values), type, NoSpan());
    }

    // The handler of one constructor's expression kind.
    struct ConstructorRow {
      AstExprKind kind;
      ExprHandler handler;
    };

    const ConstructorRow kConstructors[] = {{AstExprKind::VecLit, vecLiteral}, {AstExprKind::NewMutarray, newMutarray}, {AstExprKind::NewArray, newArray},
      {AstExprKind::NewBuffer, newBuffer}, {AstExprKind::NewSparse, newSparse}, {AstExprKind::NewMap, newMap}, {AstExprKind::NewMutmap, newMutmap},
      {AstExprKind::NewSet, newSet}, {AstExprKind::NewMutset, newMutset}};

  }  // namespace

  void RegisterConstructors(Registry& registry) {
    for (const ConstructorRow& row : kConstructors) {
      AddExpr(registry, row.kind, row.handler);
    }
  }

}  // namespace polyir::front
