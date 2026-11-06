// Sum types (CheckContainers.nina `sumConstructor`, CheckBuiltins.nina `variantResult` / `eitherResult` / `optionalResult` / `coroutineResult`):
// `variant<A, B>(tag, x)`, `eitherOk<T, E>(x)`, `eitherErr<T, E>(e)`, `Some<T>(x)`, `None<T>()`, and the builtins that read them -- `tag`, `isCase`,
// `unwrapAt`, `isOk`, `isErr`, `unwrapOk`, `unwrapErr`, `isSome`, `isNone`, `unwrap` -- and the coroutine builtins `resume`, `done`, `close` and `yielded`.
#include "front/Checker.hpp"

namespace polyir::front {

  namespace {

    // ── constructors ────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────

    // The case types written in `variant<A, B>`.
    Vector<TypePtr> caseTypes(Checker& check, Range<TypeId> cases) {
      Vector<TypePtr> types;

      for (const TypeId id : check.program->typeRefs[cases]) {
        types.push_back(ResolveType(check, id));
      }

      return types;
    }

    // A variant or either literal: the node and its one payload.
    SharedPtr<Expr> litNode(ExprKind kind, ExprPtr payload, TypePtr type, Span at) {
      SharedPtr<Expr> made = NewExpr(kind, move(type), at);
      made->operands.push_back(move(payload));

      return made;
    }

    // The payload is checked against its declared case type, so a generic struct literal finds its specialized name.
    ExprPtr variantLiteral(Checker& check, const AstExpr& node) {
      Vector<TypePtr> types = caseTypes(check, node.types);
      const int64_t index = ParseInt(node.literal.text);

      if (index >= static_cast<int64_t>(types.size())) {
        return Unsupported("variant case out of range", SpanAt(node.literal.pos));
      }

      ExprPtr payload = CheckExprExpecting(check, node.first, types[index]);
      SharedPtr<Expr> made = litNode(ExprKind::VariantLit, move(payload), MakeVariant(move(types)), SpanAt(node.literal.pos));
      made->fieldIndex = static_cast<int>(index);

      return made;
    }

    // An either (or optional) literal: `isOk` says which side the payload is on.
    ExprPtr eitherLit(bool isOk, ExprPtr payload, TypePtr type) {
      SharedPtr<Expr> made = litNode(ExprKind::EitherLit, move(payload), move(type), NoSpan());
      made->boolValue = isOk;

      return made;
    }

    // `eitherOk<T, E>(x)` and `eitherErr<T, E>(e)`: the payload is checked against the side it is on.
    ExprPtr eitherOf(Checker& check, const AstExpr& node, bool isOk) {
      const TypePtr ok = ResolveType(check, node.type);
      const TypePtr err = ResolveType(check, node.type2);

      return eitherLit(isOk, CheckExprExpecting(check, node.first, isOk ? ok : err), MakeEither(ok, err));
    }

    ExprPtr eitherOk(Checker& check, const AstExpr& node) {
      return eitherOf(check, node, true);
    }

    ExprPtr eitherErr(Checker& check, const AstExpr& node) {
      return eitherOf(check, node, false);
    }

    // `Some<T>(x)`: the payload is checked against `T`.
    ExprPtr someOf(Checker& check, const AstExpr& node) {
      const TypePtr element = ResolveType(check, node.type);

      return eitherLit(true, CheckExprExpecting(check, node.first, element), MakeOptional(element));
    }

    // `None<T>()` carries a flag the absent case never reads.
    ExprPtr noneOf(Checker& check, const AstExpr& node) {
      return eitherLit(false, ConstBool(false, IntType(1), NoSpan()), MakeOptional(ResolveType(check, node.type)));
    }

    // ── builtins ────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────

    ExprPtr tagOf(MethodCall& call) {
      return BuiltinOf(call, Builtin::VariantTag, IntType(64));
    }

    ExprPtr isCaseOf(MethodCall& call) {
      return BuiltinOf(call, Builtin::VariantIsCase, IntType(1));
    }

    // `unwrapAt(i)`: `i` must be an integer literal; anything else reads as case 0.
    ExprPtr unwrapAtOf(MethodCall& call) {
      const Vector<TypePtr>& cases = call.receiver->type->parameters;

      if (call.args.empty() || empty(cases)) {
        return nullptr;
      }

      const bool isLiteral = call.args[0]->kind == ExprKind::ConstInt;
      const int64_t index = isLiteral ? call.args[0]->intValue : 0;

      if (index < 0 || index >= static_cast<int64_t>(cases.size())) {
        return nullptr;
      }

      return BuiltinOf(call, Builtin::VariantUnwrap, cases[index]);
    }

    ExprPtr isOkOf(MethodCall& call) {
      return BuiltinOf(call, Builtin::EitherIsOk, IntType(1));
    }

    ExprPtr isErrOf(MethodCall& call) {
      return BuiltinOf(call, Builtin::EitherIsErr, IntType(1));
    }

    ExprPtr unwrapOkOf(MethodCall& call) {
      return BuiltinOf(call, Builtin::EitherUnwrapOk, call.receiver->type->element);
    }

    ExprPtr unwrapErrOf(MethodCall& call) {
      return BuiltinOf(call, Builtin::EitherUnwrapErr, call.receiver->type->second);
    }

    ExprPtr isSomeOf(MethodCall& call) {
      return BuiltinOf(call, Builtin::OptionIsSome, IntType(1));
    }

    ExprPtr isNoneOf(MethodCall& call) {
      return BuiltinOf(call, Builtin::OptionIsNone, IntType(1));
    }

    ExprPtr unwrapOf(MethodCall& call) {
      return BuiltinOf(call, Builtin::OptionUnwrap, call.receiver->type->element);
    }

    ExprPtr resumeOf(MethodCall& call) {
      return BuiltinOf(call, Builtin::CoroutineResume, call.receiver->type->element);
    }

    ExprPtr doneOf(MethodCall& call) {
      return BuiltinOf(call, Builtin::CoroutineDone, IntType(1));
    }

    ExprPtr closeOf(MethodCall& call) {
      return BuiltinOf(call, Builtin::CoroutineClose, MakeVoid());
    }

    // `v.yielded()`: hands `v` out like `yield v`, and says whether the coroutine was abandoned (`close`) while suspended there (an i1; only in a coroutine).
    ExprPtr yieldedOf(MethodCall& call) {
      return BuiltinOf(call, Builtin::CoroutineYield, IntType(1));
    }

    // One builtin: its name, the one receiver kind it takes, and its handler.
    struct BuiltinRow {
      const char* name;
      TypeKind receiver;
      BuiltinHandler handler;
    };

    const BuiltinRow kSumBuiltins[] = {{"tag", TypeKind::Variant, tagOf}, {"tag", TypeKind::Either, tagOf}, {"isCase", TypeKind::Variant, isCaseOf},
      {"unwrapAt", TypeKind::Variant, unwrapAtOf}, {"isOk", TypeKind::Either, isOkOf}, {"isErr", TypeKind::Either, isErrOf},
      {"unwrapOk", TypeKind::Either, unwrapOkOf}, {"unwrapErr", TypeKind::Either, unwrapErrOf}, {"isSome", TypeKind::Optional, isSomeOf},
      {"isNone", TypeKind::Optional, isNoneOf}, {"unwrap", TypeKind::Optional, unwrapOf}, {"resume", TypeKind::Coroutine, resumeOf},
      {"done", TypeKind::Coroutine, doneOf}, {"close", TypeKind::Coroutine, closeOf}};

    // The handler of one constructor's expression kind.
    struct ExprRow {
      AstExprKind kind;
      ExprHandler handler;
    };

    const ExprRow kSumExprs[] = {{AstExprKind::Variant, variantLiteral}, {AstExprKind::EitherOk, eitherOk}, {AstExprKind::EitherErr, eitherErr},
      {AstExprKind::Some, someOf}, {AstExprKind::None, noneOf}};

  }  // namespace

  void RegisterSums(Registry& registry) {
    for (const ExprRow& row : kSumExprs) {
      AddExpr(registry, row.kind, row.handler);
    }

    for (const BuiltinRow& row : kSumBuiltins) {
      AddMethod(registry, row.name, KindBit(row.receiver), row.handler);
    }

    AddMethod(registry, "yielded", kAnyKind, yieldedOf);
  }

}  // namespace polyir::front
