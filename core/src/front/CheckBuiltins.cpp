// The built-in operations (CheckBuiltins.nina): `len`, `push`, `.tag()`, `.atomicLoad(...)` and so on. One name can mean different things for different
// receiver types, so the question asked is: for THIS receiver type, is `name` a built-in, and what does it give? The answers are the handlers
// registered with `AddMethod` (Registry.hpp); when none answers the call is an ordinary function call (see CheckSuffix.cpp).
#include "front/Checker.hpp"

namespace polyir::front {

  // `recv.name(args)` as a built-in, or null when it is not one for this receiver. A handler that answers null must leave `args` as it found them.
  ExprPtr BuiltinMethod(Checker& check, Symbol name, const ExprPtr& receiver, Vector<ExprPtr>& args, Span at) {
    MethodCall call{check, name, receiver, args, at};

    return Method(check.registry, call);
  }

  ExprPtr BuiltinOf(MethodCall& call, Builtin op, const TypePtr& type) {
    return BuiltinNode(op, call.receiver, call.args, type, call.at);
  }

  // Every container that is a reference can be copied; the copy has the same type.
  Optional<TypePtr> CopyResult(const TypePtr& receiver) {
    switch (receiver->kind) {
      case TypeKind::DynamicArray: case TypeKind::Sparse: case TypeKind::Hashmap: case TypeKind::Set: case TypeKind::Grid: case TypeKind::Tensor:
      case TypeKind::Buffer: case TypeKind::Struct:
        return receiver;
      default:
        return nullopt;
    }
  }

  namespace {

    // ── the builtins that apply to any receiver ─────────────────────────────────────────────────────────────────────────────────────────────────
    // `len`, `count` and `hash` give an i64 whatever the receiver is; `push` gives the receiver's own type (an `array` refuses it); `copy` is a builtin for
    // the containers and structs that can be copied (and a call of a function named `copy` for the rest); `with`, `has`, `add`, `remove`, `freeze` and
    // `thaw` are the operations of the three collection kinds (CheckCollections.cpp). The receiver-specific builtins (`rows`, `tag`, `atomicLoad`, `resume`, ...) are
    // registered by the areas that have those receivers.

    ExprPtr lengthOf(MethodCall& call) {
      return BuiltinOf(call, Builtin::Len, IntType(64));
    }

    ExprPtr hashOf(MethodCall& call) {
      return BuiltinOf(call, Builtin::Hash, IntType(64));
    }

    // `push` on a collection that cannot change is a problem node; otherwise the receiver's type comes back (`xs = xs.push(1)` and `xs.push(1)` both work).
    ExprPtr pushOf(MethodCall& call) {
      if (const Optional<String> why = CollectionProblem(TextOf(call.check, call.name), call.receiver->type)) {
        return Unsupported(*why, call.at);
      }

      return BuiltinOf(call, Builtin::Push, call.receiver->type);
    }

    ExprPtr copyOf(MethodCall& call) {
      const Optional<TypePtr> type = CopyResult(call.receiver->type);

      return type ? BuiltinOf(call, Builtin::Copy, *type) : nullptr;
    }

    // `with`, `has`, `add`, `freeze`, `thaw`: a problem node if the collection's kind forbids it, otherwise the operation and the type it gives.
    ExprPtr collectionOp(MethodCall& call, Builtin op) {
      const StringView name = TextOf(call.check, call.name);

      if (const Optional<String> why = CollectionProblem(name, call.receiver->type)) {
        return Unsupported(*why, call.at);
      }

      const Optional<TypePtr> type = CollectionResult(name, call.receiver->type);

      return type ? BuiltinOf(call, op, *type) : nullptr;
    }

    ExprPtr withOf(MethodCall& call) {
      return collectionOp(call, Builtin::With);
    }

    ExprPtr hasOf(MethodCall& call) {
      return collectionOp(call, Builtin::Has);
    }

    ExprPtr addOf(MethodCall& call) {
      return collectionOp(call, Builtin::Add);
    }

    ExprPtr removeOf(MethodCall& call) {
      return collectionOp(call, Builtin::Remove);
    }

    // `freeze` and `thaw` are both a copy, into the other kind of collection.
    ExprPtr freezeOf(MethodCall& call) {
      return collectionOp(call, Builtin::Copy);
    }

    struct BuiltinRow {
      const char* name;
      BuiltinHandler handler;
    };

    const BuiltinRow kCommonBuiltins[] = {{"len", lengthOf}, {"count", lengthOf}, {"hash", hashOf}, {"push", pushOf}, {"copy", copyOf}, {"with", withOf},
      {"has", hasOf}, {"add", addOf}, {"remove", removeOf}, {"freeze", freezeOf}, {"thaw", freezeOf}};

  }  // namespace

  void RegisterCommonBuiltins(Registry& registry) {
    for (const BuiltinRow& row : kCommonBuiltins) {
      AddMethod(registry, row.name, kAnyKind, row.handler);
    }

    for (const char* form : {"push", "len", "hash", "copy"}) {
      AddFunctionForm(registry, form);
    }
  }

}  // namespace polyir::front
