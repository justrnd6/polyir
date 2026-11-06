// HIR types as SPIR-V types, and constants of them (SpirvTypes.nina). Only a signed integer counts as a whole number here, as in the Nina writer
// (whose `HInt` is the signed integer); 8- and 16-bit values are computed as 32-bit.
#include "gpu/SpirvLower.hpp"

namespace polyir {

  namespace {

    bool isSignedInt(const TypePtr& t) {
      return t->kind == TypeKind::Int;
    }

    Id intType(SpvModule& m, int bits) {
      if (bits == 1) {
        return TypeBool(m);
      }

      return TypeInt(m, bits == 64 ? 64 : 32);
    }

    // A vector constant with the same number in every lane (`OpConstantComposite`).
    Id splatConstant(SpvModule& m, const TypePtr& t, double value) {
      const Id one = SpvConstant(m, SpvScalar(t), value);
      const Words lanes = SpvRepeat(one, SpvLanes(t));
      const Id type = SpvType(m, t);

      return DeclareValue(m, "splat:" + to_string(one) + ":" + to_string(lanes.size()), OpConstantComposite, type, lanes);
    }

  }  // namespace

  Id SpvType(SpvModule& m, const TypePtr& t) {
    switch (t->kind) {
      case TypeKind::Int:
        return intType(m, t->bits);
      case TypeKind::Float:
        return TypeFloat(m);
      case TypeKind::Vec:
        return TypeVector(m, SpvType(m, t->element), t->size);
      default:
        return TypeVoid(m);
    }
  }

  bool SpvIsBool(const TypePtr& t) {
    return isSignedInt(t) && t->bits == 1;
  }

  bool SpvIsFloat(const TypePtr& t) {
    return t->kind == TypeKind::Float || (t->kind == TypeKind::Vec && SpvIsFloat(t->element));
  }

  bool SpvIsVector(const TypePtr& t) {
    return t->kind == TypeKind::Vec;
  }

  bool SpvIsNumber(const TypePtr& t) {
    return (isSignedInt(t) && t->bits > 1) || t->kind == TypeKind::Float;
  }

  TypePtr SpvScalar(const TypePtr& t) {
    return SpvIsVector(t) ? t->element : t;
  }

  int SpvLanes(const TypePtr& t) {
    return SpvIsVector(t) ? t->size : 1;
  }

  int SpvBits(const TypePtr& t) {
    return isSignedInt(t) && t->bits == 64 ? 64 : 32;
  }

  TypePtr SpvPointee(const TypePtr& t) {
    return t->kind == TypeKind::Pointer ? t->element : t;
  }

  Id SpvConstant(SpvModule& m, const TypePtr& t, double value) {
    if (SpvIsVector(t)) {
      return splatConstant(m, t, value);
    }

    if (SpvIsFloat(t)) {
      return ConstFloat(m, value);
    }

    if (SpvIsBool(t)) {
      return ConstBool(m, value != 0.0);
    }

    return ConstInt(m, SpvBits(t), static_cast<int64_t>(value));
  }

  Words SpvRepeat(Id id, int count) {
    return Words(static_cast<size_t>(count), id);
  }

  const TypePtr& SpvInt32() {
    static const TypePtr kType = MakeInt(32);

    return kType;
  }

  const TypePtr& SpvFlag() {
    static const TypePtr kType = MakeInt(1);

    return kType;
  }

  const TypePtr& SpvFloat32() {
    static const TypePtr kType = MakeFloat(32);

    return kType;
  }

  const SpvArray* SpvArrayNamed(const Vector<SpvArray>& arrays, const String& name) {
    for (const SpvArray& array : arrays) {
      if (array.name == name) {
        return &array;
      }
    }

    return nullptr;
  }

}  // namespace polyir
