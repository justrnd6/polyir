// A field's default, `label: string = ""` (Resolve.hpp `FieldDefault`): a struct literal that leaves the field out takes it. The default is a
// literal of the field's type: a number (an integer one also fits a float field), a string, a char, `true` or `false`, with a `-` before a
// number. It becomes one constant, made once when the struct is declared, so a literal in another file can take it too.
#include "front/Resolve.hpp"
#include "front/HirBuild.hpp"
#include "front/Pipeline.hpp"
#include "front/Strings.hpp"
#include "polyir/HirPrint.hpp"

namespace polyir::front {

  namespace {

    bool isNumber(AstExprKind kind) {
      return kind == AstExprKind::Int || kind == AstExprKind::Float;
    }

    bool isWhole(const TypePtr& type) {
      return type->kind == TypeKind::Int || type->kind == TypeKind::UInt;
    }

    // A number literal as a constant of the field's type; null when the field holds no number.
    ExprPtr numberOf(const AstExpr& literal, bool negative, const TypePtr& type, Span at) {
      const double sign = negative ? -1.0 : 1.0;

      if (literal.kind == AstExprKind::Int && isWhole(type)) {
        return ConstInt(negative ? -ParseInt(literal.literal.text) : ParseInt(literal.literal.text), type, at);
      }

      if (type->kind == TypeKind::Float) {
        return ConstFloat(sign * ParseDouble(literal.literal.text), type, at);
      }

      return nullptr;
    }

    // A string, char or truth literal as a constant of the field's type; null when they do not fit.
    ExprPtr wordOf(const AstExpr& literal, const TypePtr& type, Span at) {
      const bool truth = literal.kind == AstExprKind::True || literal.kind == AstExprKind::False;

      if (literal.kind == AstExprKind::String && type->kind == TypeKind::String) {
        return ConstString(StripQuotes(literal.literal.text), type, at);
      }

      if (literal.kind == AstExprKind::Char && type->kind == TypeKind::Int && type->bits == 8) {
        return ConstInt(CharValue(literal.literal.text), type, at);
      }

      return truth && type->kind == TypeKind::Int && type->bits == 1 ? ConstBool(literal.kind == AstExprKind::True, type, at) : nullptr;
    }

    bool isLiteral(AstExprKind kind) {
      return isNumber(kind) || kind == AstExprKind::String || kind == AstExprKind::Char || kind == AstExprKind::True || kind == AstExprKind::False;
    }

  }  // namespace

  ExprPtr FieldDefault(const Program& program, ExprId id, const TypePtr& type, String& problem) {
    const AstExpr& written = program.exprs[id];
    const bool negative = written.kind == AstExprKind::Neg && isNumber(program.exprs[written.first].kind);
    const AstExpr& literal = negative ? program.exprs[written.first] : written;

    if (!isLiteral(literal.kind)) {
      problem = DiagnosticMessage("a field's default is a literal: a number, a string, a char, true or false", SpanAt(written.pos));

      return nullptr;
    }

    const Span at = SpanAt(literal.literal.pos);
    ExprPtr value = isNumber(literal.kind) ? numberOf(literal, negative, type, at) : wordOf(literal, type, at);
    problem = value ? "" : DiagnosticMessage("the default is no literal of the field's type, " + TypeText(type), at);

    return value;
  }

}  // namespace polyir::front
