// A struct field that holds a container may be left out of a construction (CheckStructDefaults.nina): it starts empty, as a container declared with no
// value does (`let xs: mutarray<i64>`). So `StringBuilder()` (or `StringBuilder {}`) builds a builder whose lists are empty. A field with a default
// (`label: string = ""`) left out takes its default. Any other field left out stays as before (the diagnose step reports what is missing).
#include "front/Checker.hpp"

namespace polyir::front {

  namespace {

    bool isGiven(const Vector<int>& given, int index) {
      return find(given.begin(), given.end(), index) != given.end();
    }

    // A copy of a field's default constant, at the construction.
    ExprPtr defaultCopy(const ExprPtr& initial, Span at) {
      SharedPtr<Expr> copy = make_shared<Expr>(*initial);
      copy->at = at;

      return copy;
    }

    // The field's default when it has one, else the field made empty when its type is a container (FreshStorage); nothing for any other.
    void addLeftOut(Checker& check, const StructInfo& info, int index, FieldInits& inits, Span at) {
      const bool hasDefault = static_cast<size_t>(index) < info.fieldDefaults.size() && info.fieldDefaults[index];
      ExprPtr value = hasDefault ? defaultCopy(info.fieldDefaults[index], at) : FreshStorage(check, info.fieldTypes[index], at);

      if (!value) {
        return;
      }

      inits.indices.push_back(index);
      inits.values.push_back(move(value));
    }

  }  // namespace

  // The given fields, then each field that was not given: its default, or empty for a container.
  void FillStructDefaults(Checker& check, const StructInfo* info, FieldInits& inits, Span at) {
    if (info == nullptr || IsInterface(DeclsOf(check), info->name)) {
      return;
    }

    const Vector<int> given = inits.indices;

    for (int i = 0; i < static_cast<int>(info->fieldTypes.size()); i++) {
      if (!isGiven(given, i)) {
        addLeftOut(check, *info, i, inits, at);
      }
    }
  }

  // `Name()` with no arguments, when `Name` is a struct: the struct with every container field empty. Null when `Name` is no struct.
  ExprPtr EmptyConstruction(Checker& check, Symbol structName, Span at) {
    const StructInfo* info = FindStruct(DeclsOf(check), structName);

    if (info == nullptr || IsInterface(DeclsOf(check), structName)) {
      return nullptr;
    }

    FieldInits inits;
    FillStructDefaults(check, info, inits, at);

    return StructLitNode(Str(check, structName), move(inits.indices), move(inits.values), StructOf(Types(check), structName), at);
  }

}  // namespace polyir::front
