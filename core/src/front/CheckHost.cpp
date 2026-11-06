// Calls on a host type itself rather than on a value (CheckHost.nina): `Math.sqrt(x)`, `ArrayList.new()`. The name before the dot is not a variable or
// a function but an `extern type`; the member is the function `Owner_member` (see ResolveHost.cpp). A generic host type has no single owner:
// `ArrayList.new()` makes the `ArrayList_i64` the context asks for (`let l: ArrayList<i64> = ...`).
#include "front/Checker.hpp"

namespace polyir::front {

  namespace {

    // The host type called `name` whose own base is `base`, if there is one.
    bool hasHostType(Checker& check, Symbol name, Symbol base) {
      for (const HostTypeInfo& host : DeclsOf(check).hostTypes) {
        if (host.name == name && host.base == base) {
          return true;
        }
      }

      return false;
    }

    // `Math` names itself. `ArrayList` names the instance `ArrayList_i64` that the expected type spells.
    Optional<Symbol> ownerFor(Checker& check, Symbol name, const TypePtr* expected) {
      if (hasHostType(check, name, name)) {
        return name;
      }

      if (expected == nullptr || (*expected)->kind != TypeKind::Struct) {
        return nullopt;
      }

      const Symbol instance = SymbolOf(check, (*expected)->name);

      return hasHostType(check, instance, name) ? Optional<Symbol>(instance) : nullopt;
    }

    // The call, then whatever else the chain does with its result (`Math.sqrt(x).toI64()`).
    ExprPtr finished(Checker& check, const AstExpr& node, Symbol function, const FuncInfo& info) {
      const View<AstSuffix> suffixes = check.program->suffixes[node.suffixes];
      const Ident method = suffixes[0].name;
      ExprPtr current = CallOf(check, function, info, CheckAll(check, suffixes[0].args), SpanAt(method.pos));

      for (size_t i = 1; i < suffixes.size(); i++) {
        current = CheckSuffix(check, move(current), suffixes[i]);
      }

      return current;
    }

    // `Owner.member(args)` is a call of the function `Owner_member`.
    ExprPtr memberCall(Checker& check, const AstExpr& node, Symbol owner) {
      const Ident method = check.program->suffixes[node.suffixes][0].name;
      const Symbol function = Mangled(check.session, owner, method.name);
      const FuncInfo* info = FindFunc(DeclsOf(check), function);

      if (info != nullptr) {
        return finished(check, node, function, *info);
      }

      const String typeName = Str(check, Node(check, node.first).name.name);

      return Unsupported(typeName + " has no member " + Str(check, method.name), SpanAt(method.pos));
    }

    // A bare name that is neither a variable nor a function.
    bool isFreeName(Checker& check, const AstExpr& base) {
      if (base.kind != AstExprKind::Name) {
        return false;
      }

      return Lookup(check, base.name.name) == nullptr && FindFunc(DeclsOf(check), base.name.name) == nullptr;
    }

    // The name before the dot must be such a free name, followed by a method call.
    bool namesAHostType(Checker& check, const AstExpr& node) {
      if (node.kind != AstExprKind::Postfix) {
        return false;
      }

      if (!isFreeName(check, Node(check, node.first))) {
        return false;
      }

      return check.program->suffixes[node.suffixes][0].kind == AstSuffixKind::Method;
    }

  }  // namespace

  // The call, when `node` is exactly `Type.member(args)` followed by anything; `expected` is the type the context wants.
  ExprPtr HostStaticCall(Checker& check, const AstExpr& node, const TypePtr* expected) {
    if (!namesAHostType(check, node)) {
      return nullptr;
    }

    const Optional<Symbol> owner = ownerFor(check, Node(check, node.first).name.name, expected);

    return owner ? memberCall(check, node, *owner) : nullptr;
  }

}  // namespace polyir::front
