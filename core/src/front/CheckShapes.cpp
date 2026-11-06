// The builtins that belong to one kind of receiver (CheckBuiltins.nina `shapeResult`): `rows` / `cols` of a grid, `dim` of a tensor, `capacity` of a
// buffer, fixed array, map or set, `shuffle` of a vec, the atomics of a pointer (`atomicLoad`, `atomicStore`, `atomicRmw`, `cmpxchg`) and the free `fence`.
#include "front/Checker.hpp"

namespace polyir::front {

  namespace {

    ExprPtr rowsOf(MethodCall& call) {
      return BuiltinOf(call, Builtin::Rows, IntType(64));
    }

    ExprPtr colsOf(MethodCall& call) {
      return BuiltinOf(call, Builtin::Cols, IntType(64));
    }

    ExprPtr dimOf(MethodCall& call) {
      return BuiltinOf(call, Builtin::Dim, IntType(64));
    }

    // A map or set has no capacity every target agrees on (CheckCollections.cpp says why); a buffer or fixed array has its declared size.
    ExprPtr capacityOf(MethodCall& call) {
      if (const Optional<String> why = CollectionProblem("capacity", call.receiver->type)) {
        return Unsupported(*why, call.at);
      }

      return BuiltinOf(call, Builtin::Capacity, IntType(64));
    }

    ExprPtr shuffleOf(MethodCall& call) {
      return BuiltinOf(call, Builtin::Shuffle, call.receiver->type);
    }

    ExprPtr atomicLoadOf(MethodCall& call) {
      return BuiltinOf(call, Builtin::AtomicLoad, call.receiver->type->element);
    }

    ExprPtr atomicStoreOf(MethodCall& call) {
      return BuiltinOf(call, Builtin::AtomicStore, VoidType());
    }

    ExprPtr atomicRmwOf(MethodCall& call) {
      return BuiltinOf(call, Builtin::AtomicRmw, call.receiver->type->element);
    }

    ExprPtr cmpxchgOf(MethodCall& call) {
      return BuiltinOf(call, Builtin::AtomicCmpxchg, call.receiver->type->element);
    }

    // `fence(order)`: a builtin with no receiver; the node's receiver is a placeholder zero.
    ExprPtr fenceOf(Checker&, Vector<ExprPtr>& args, Span at) {
      return BuiltinNode(Builtin::AtomicFence, ConstInt(0, IntType(64), at), move(args), VoidType(), at);
    }

    // One builtin: its name, the receiver kinds it takes, and its handler.
    struct ShapeRow {
      const char* name;
      KindMask receivers;
      BuiltinHandler handler;
    };

    // The receivers `capacity` is registered for: a buffer or fixed array answers its size; a map or set is listed only so capacityOf can
    // explain why it has none.
    constexpr KindMask kCapacityReceivers = KindBit(TypeKind::Buffer) | KindBit(TypeKind::FixedArray) | KindBit(TypeKind::Hashmap) | KindBit(TypeKind::Set);

    const ShapeRow kShapes[] = {{"rows", KindBit(TypeKind::Grid), rowsOf}, {"cols", KindBit(TypeKind::Grid), colsOf}, {"dim", KindBit(TypeKind::Tensor), dimOf},
      {"capacity", kCapacityReceivers, capacityOf}, {"shuffle", KindBit(TypeKind::Vec), shuffleOf}, {"atomicLoad", KindBit(TypeKind::Pointer), atomicLoadOf},
      {"atomicStore", KindBit(TypeKind::Pointer), atomicStoreOf}, {"atomicRmw", KindBit(TypeKind::Pointer), atomicRmwOf},
      {"cmpxchg", KindBit(TypeKind::Pointer), cmpxchgOf}};

  }  // namespace

  void RegisterShapes(Registry& registry) {
    for (const ShapeRow& row : kShapes) {
      AddMethod(registry, row.name, row.receivers, row.handler);
    }

    AddFree(registry, "fence", fenceOf);
  }

}  // namespace polyir::front
