// Optionals, eithers and variants. A value is a heap block of two cells: the case number, then the value of that case. An optional has the value as
// case 0 and the absence as case 1; an either has `ok` as case 0 and `err` as case 1; a variant's cases are numbered as its type lists them.
#include "polyir/Std.hpp"
#include "emit/llvm/LlvmEmitter.hpp"

namespace polyir::llvm {

  Value SumValue(FunctionEmitter& emitter, const TypePtr& type, int tag, const Value* payload) {
    const Value block = Allocate(emitter, "16", Layout::References);
    Store(emitter, Value{to_string(tag), MakeInt(64)}, WordAddress(emitter, block.text, 0));

    if (payload != nullptr) {
      Store(emitter, *payload, WordAddress(emitter, block.text, 1));
    }

    return Value{block.text, type};
  }

  Value SumTag(FunctionEmitter& emitter, const Value& sum) {
    return LoadFrom(emitter, sum.text, MakeInt(64));
  }

  // The value of case `tag`; reading the wrong case stops the program.
  Value SumPayload(FunctionEmitter& emitter, const Value& sum, const TypePtr& caseType, int tag) {
    const Value actual = SumTag(emitter, sum);
    TrapUnless(emitter, Instruction(emitter, "icmp eq i64 " + actual.text + ", " + to_string(tag)), "stopped, the value is not the case that was read (an empty optional, an error, another variant case)");

    return LoadFrom(emitter, WordAddress(emitter, sum.text, 1), caseType);
  }

  Value EitherLit(FunctionEmitter& emitter, const Expr& expr) {
    const bool first = expr.boolValue;

    if (expr.type->kind == TypeKind::Optional && !first) {
      return SumValue(emitter, expr.type, 1, nullptr);
    }

    const Value payload = CasePayload(emitter, expr, first ? expr.type->element : expr.type->second);

    return SumValue(emitter, expr.type, first ? 0 : 1, &payload);
  }

  Value VariantLit(FunctionEmitter& emitter, const Expr& expr) {
    const Value payload = CasePayload(emitter, expr, expr.type->parameters.at(static_cast<size_t>(expr.fieldIndex)));

    return SumValue(emitter, expr.type, expr.fieldIndex, &payload);
  }

  // The value of a case, brought to the case's declared type (`eitherOk<double, i64>(3)`: the literal is an i64, the case a double).
  Value CasePayload(FunctionEmitter& emitter, const Expr& expr, const TypePtr& caseType) {
    return Fit(emitter, EmitExpr(emitter, *expr.operands[0]), caseType);
  }

  namespace {

    Value flag(const String& text) {
      return Value{text, MakeBool()};
    }

    bool isSumBuiltin(Builtin op) {
      return op == Builtin::VariantTag || op == Builtin::VariantIsCase || op == Builtin::VariantUnwrap || op == Builtin::EitherIsOk ||
             op == Builtin::EitherIsErr || op == Builtin::EitherUnwrapOk || op == Builtin::EitherUnwrapErr || op == Builtin::OptionIsSome ||
             op == Builtin::OptionIsNone || op == Builtin::OptionUnwrap;
    }

    bool isSumType(const TypePtr& type) {
      return type->kind == TypeKind::Optional || type->kind == TypeKind::Either || type->kind == TypeKind::Variant;
    }

  }  // namespace

  Value TagIs(FunctionEmitter& emitter, const Value& sum, int tag) {
    return flag(Instruction(emitter, "icmp eq i64 " + SumTag(emitter, sum).text + ", " + to_string(tag)));
  }

  // `Free` of a sum gives its block back (a hint to the collector).
  Value FreedSum(FunctionEmitter& emitter, const Expr& expr) {
    FreeHint(emitter, EmitExpr(emitter, *expr.operands[0]).text);

    return Value{"", MakeVoid()};
  }

  bool SumBuiltin(FunctionEmitter& emitter, const Expr& expr, Value& out) {
    const bool frees = expr.builtin == Builtin::Free && isSumType(expr.operands[0]->type);

    if (!frees && !isSumBuiltin(expr.builtin)) {
      return false;
    }

    out = frees ? FreedSum(emitter, expr) : SumOperation(emitter, expr, EmitExpr(emitter, *expr.operands[0]));

    return true;
  }

  Value SumOperation(FunctionEmitter& emitter, const Expr& expr, const Value& sum) {
    switch (expr.builtin) {
      case Builtin::VariantTag:
        return SumTag(emitter, sum);
      case Builtin::VariantIsCase:
        return TagIs(emitter, sum, static_cast<int>(expr.operands[1]->intValue));
      case Builtin::VariantUnwrap:
        return SumPayload(emitter, sum, expr.type, static_cast<int>(expr.operands[1]->intValue));
      default:
        return EitherOrOption(emitter, expr.builtin, sum);
    }
  }

  // The builtins of eithers and optionals: which case it is, and the value of a case.
  Value EitherOrOption(FunctionEmitter& emitter, Builtin op, const Value& sum) {
    const bool isOk = op == Builtin::EitherIsOk || op == Builtin::OptionIsSome;
    const bool isErr = op == Builtin::EitherIsErr || op == Builtin::OptionIsNone;

    if (isOk || isErr) {
      return TagIs(emitter, sum, isOk ? 0 : 1);
    }

    const bool ok = op == Builtin::EitherUnwrapOk || op == Builtin::OptionUnwrap;

    return ok ? SumPayload(emitter, sum, sum.type->element, 0) : SumPayload(emitter, sum, sum.type->second, 1);
  }

}  // namespace polyir::llvm
