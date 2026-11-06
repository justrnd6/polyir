// What the builder's source files share: making an expression, reporting a mistake, and the checks every typed operation starts with.
#pragma once

#include "polyir/Std.hpp"
#include "polyir/Builder.hpp"

namespace polyir::checks {

  inline SharedPtr<Expr> MakeExpr(ExprKind kind, TypePtr type) {
    auto expr = make_shared<Expr>();
    expr->kind = kind;
    expr->type = move(type);

    return expr;
  }

  [[noreturn]] inline void Fail(const String& message) {
    throw BuildError(message);
  }

  inline void RequireSame(const TypePtr& a, const TypePtr& b, const String& where) {
    if (!SameType(a, b)) {
      Fail(where + ": " + TypeText(*a) + " and " + TypeText(*b) + " are different types");
    }
  }

  // A vector is a number when its elements are: arithmetic on it works on each element.
  inline TypePtr ScalarOf(const TypePtr& t) {
    return t->kind == TypeKind::Vec ? t->element : t;
  }

  inline void RequireNumber(const TypePtr& t, const String& where) {
    if (!IsNumber(*ScalarOf(t))) {
      Fail(where + " needs a number, not " + TypeText(*t));
    }
  }

  inline void RequireInteger(const TypePtr& t, const String& where) {
    if (!IsInteger(*ScalarOf(t))) {
      Fail(where + " needs an integer, not " + TypeText(*t));
    }
  }

  inline void RequireBoolean(const TypePtr& t, const String& where) {
    if (!IsBoolean(*t)) {
      Fail(where + " needs a flag (i1), not " + TypeText(*t));
    }
  }

  inline const Signature& RequireSignature(const PolyIRBuilder& builder, const String& function) {
    const Signature* known = SignatureOf(builder, function);

    if (!known) {
      Fail("no function called '" + function + "' is declared");
    }

    return *known;
  }

  inline void RequireKind(const TypePtr& t, TypeKind kind, const String& what) {
    if (t->kind != kind) {
      Fail(what + ", not " + TypeText(*t));
    }
  }

}  // namespace polyir::checks
