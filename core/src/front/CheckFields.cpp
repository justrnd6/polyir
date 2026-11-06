// Which fields of a struct can change (CheckFields.nina). A field declared `var` can be assigned (`p.x = 5`) and its struct is a reference type; the
// other fields are fixed once the struct is made, and a struct with no `var` field is an immutable value. An assignment to a fixed field is reported
// with both lists, so the message says what can change.
#include "front/Checker.hpp"

namespace polyir::front {

  namespace {

    // The names of the fields that can change (`isMutable`) or of those that cannot, joined by commas.
    String joinNames(const Checker& check, const StructInfo& info, bool isMutable) {
      String out;

      for (size_t i = 0; i < info.fieldNames.size(); i++) {
        if (info.fieldMutable[i] != isMutable) {
          continue;
        }

        out += (out.empty() ? "" : ", ") + Str(check, info.fieldNames[i]);
      }

      return out;
    }

    // "Point can change: n, total; fixed: id", or the advice for a struct with nothing mutable.
    String fieldSummary(const Checker& check, const StructInfo& info) {
      const String name = Str(check, info.name);
      const String changing = joinNames(check, info, true);

      if (!changing.empty()) {
        return name + " can change: " + changing + "; fixed: " + joinNames(check, info, false);
      }

      const String example = "`var " + Str(check, info.fieldNames[0]) + ": T`";

      return name + " has no var fields, so it is an immutable value (declare a field " + example + " to make it assignable)";
    }

  }  // namespace

  // Why `target` cannot be assigned, or nothing when it can (or is not a field).
  Optional<String> FieldProblem(Checker& check, const Expr& target) {
    if (target.kind != ExprKind::FieldGet || target.operands[0]->type->kind != TypeKind::Struct) {
      return nullopt;
    }

    const StructInfo* info = FindStruct(DeclsOf(check), SymbolOf(check, target.operands[0]->type->name));

    if (info == nullptr || info->fieldMutable[target.fieldIndex]) {
      return nullopt;
    }

    return "cannot assign field " + Str(check, info->fieldNames[target.fieldIndex]) + " of " + Str(check, info->name) + ": it is not a var field. " +
      fieldSummary(check, *info);
  }

}  // namespace polyir::front
