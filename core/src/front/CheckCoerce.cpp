// When an argument needs converting to a parameter's type (CheckCoerce.nina). A call is checked argument by argument (Fitted in CheckInterface.cpp):
// each one is boxed and type-checked against its parameter first, then CoerceAll fixes the width. An integer of another width, or a float where a
// double is wanted (or the other way round), gets an explicit ExprKind::Coerce node; any other argument is left as it is (a real mismatch has
// already been reported by Expecting).
#include "front/Checker.hpp"

namespace polyir::front {

  namespace {

    // Both integers (of any width), or both floating point (float or double).
    bool sameFamily(const Type& a, const Type& b) {
      return (IsInteger(a) && IsInteger(b)) || (IsFloat(a) && IsFloat(b));
    }

    // `value` as `target`: wrapped in an ExprKind::Coerce node when the two are different types of the same family, otherwise unchanged.
    ExprPtr coerced(ExprPtr value, const TypePtr& target) {
      if (SameType(value->type, target) || !sameFamily(*value->type, *target)) {
        return value;
      }

      const Span at = value->at;

      return CastNode(ExprKind::Coerce, move(value), target, at);
    }

  }  // namespace

  // Each argument coerced to the parameter it fills; arguments beyond the last parameter pass through unchanged.
  Vector<ExprPtr> CoerceAll(Vector<ExprPtr> values, const Vector<TypePtr>& params) {
    for (size_t i = 0; i < values.size() && i < params.size(); i++) {
      values[i] = coerced(move(values[i]), params[i]);
    }

    return values;
  }

}  // namespace polyir::front
