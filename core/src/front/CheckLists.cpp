// List literals (CheckLists.nina): `["cmake", "-S", dir]` is a new `mutarray` of the elements' type with each element pushed in order -- the same HIR as
// `mutarray<string>().push("cmake").push("-S").push(dir)`, so every back end already handles it. Where a list of T is expected (a typed `let`, an
// argument, a `return`), each element is checked as a T: `let shapes: array<Shape> = [Square(side: 2.0), Circle(r: 1.0)]` boxes both into Shapes.
#include "front/Checker.hpp"
#include "polyir/HirPrint.hpp"

namespace polyir::front {

  namespace {

    // The first element whose type differs from the first one's, or null. A list literal has at least one element: the parser rejects `[]`.
    const Expr* mismatched(const Vector<ExprPtr>& values) {
      const String first = TypeText(values[0]->type);

      for (const ExprPtr& value : values) {
        if (TypeText(value->type) != first) {
          return value.get();
        }
      }

      return nullptr;
    }

    // A new mutarray with each value pushed in order.
    ExprPtr pushed(Vector<ExprPtr> values, const TypePtr& type, Span at) {
      ExprPtr list = ContainerNode(ExprKind::NewContainer, {}, type, at);

      for (ExprPtr& value : values) {
        list = BuiltinNode(Builtin::Push, move(list), {move(value)}, type, at);
      }

      return list;
    }

    // The list of the checked elements: every element must have the first one's type.
    ExprPtr listOf(Vector<ExprPtr> values, Span at) {
      const TypePtr element = values[0]->type;  // there is a first element: the parser rejects `[]` (an empty list has no type to give)

      if (const Expr* other = mismatched(values)) {
        return Unsupported("a list's elements share one type: " + TypeText(element) + " and " + TypeText(other->type), at);
      }

      return pushed(move(values), MakeDynamicArray(element, true), at);
    }

    // `[a, b, c]`.
    ExprPtr listLiteral(Checker& check, const AstExpr& node) {
      return listOf(CheckAll(check, node.args), SpanAt(node.pos));
    }

  }  // namespace

  ExprPtr ListLiteralExpecting(Checker& check, const AstExpr& node, const TypePtr& expected) {
    Vector<ExprPtr> values;

    for (const ExprId id : check.program->exprRefs[node.args]) {
      values.push_back(CheckExprExpecting(check, id, expected->element));
    }

    return listOf(move(values), SpanAt(node.pos));
  }

  void RegisterLists(Registry& registry) {
    AddExpr(registry, AstExprKind::ListLit, listLiteral);
  }

}  // namespace polyir::front
