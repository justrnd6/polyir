// Expressions (CheckExpr.nina): the tree walk. Each kind of node has a handler in the registry; this file has the core ones -- operators, unary
// operators, parentheses, literals and `val` -- and CheckNames.cpp the names, calls and struct literals. The checker mirrors the Nina compiler,
// quirks included (every integer literal is i64, comparisons give i1). Anything not translated becomes a problem node (`Unsupported`), so a half-finished
// body is visible rather than silently wrong.
#include "front/Checker.hpp"
#include "front/Strings.hpp"

namespace polyir::front {

  namespace {

    ExprPtr unsupportedForm(Checker&, const AstExpr&) {
      return Unsupported("expression form", NoSpan());
    }

  }  // namespace

  // Dispatches on the node's kind; a kind nothing is registered for is `?expression form?`.
  ExprPtr CheckExpr(Checker& check, ExprId id) {
    const AstExpr& node = Node(check, id);

    if (check.depth >= kMaxExpressionDepth) {
      return Unsupported("expression is nested too deeply", SpanAt(node.pos));
    }

    const DepthGuard guard(check.depth);
    const ExprHandler handler = ExprHandlerFor(check.registry, node.kind);

    if (handler == nullptr) {
      return unsupportedForm(check, node);
    }

    return handler(check, node);
  }

  // `val x`: an independent copy of a reference value (a struct, or a container: `CopyResult` says which types copy). Any other value -- a number, a
  // string, a pointer -- is already independent, and is its own copy.
  ExprPtr ValCopy(ExprPtr value) {
    const Optional<TypePtr> type = CopyResult(value->type);

    if (!type) {
      return value;
    }

    const Span at = value->at;

    return BuiltinNode(Builtin::Copy, move(value), {}, *type, at);
  }

  namespace {

    // ── binary operators ────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────

    // The operator node, unless `==` or `===` cannot compare these values.
    ExprPtr operatorNode(Checker& check, BinOp code, ExprPtr left, ExprPtr right, Span at) {
      if (const Optional<String> why = EqualityProblem(check, code, *left)) {
        return Unsupported(*why, at);
      }

      const TypePtr type = ResultType(code, left->type);

      return BinaryNode(code, move(left), move(right), type, at);
    }

    // `a + b` on strings or mutarrays is a concatenation; otherwise both operands must be of one kind.
    ExprPtr binary(Checker& check, const AstExpr& node) {
      ExprPtr left = CheckExpr(check, node.first);
      ExprPtr right = CheckExpr(check, node.second);
      const Span at = SpanAt(node.opPos);

      if (node.op == Operator::Add && IsConcatType(left->type)) {
        return BuiltinNode(Builtin::Concat, left, {move(right)}, left->type, at);
      }

      if (ExprPtr problem = OperandsProblem(left, right)) {
        return problem;
      }

      if (ExprPtr problem = AnyTypeProblem(left, String("`") + OperatorText(node.op) + "`", at)) {
        return problem;
      }

      return operatorNode(check, BinOpOf(node.op), move(left), move(right), at);
    }

    // ── unary operators ─────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────

    // `!x` gives a flag (i1); `-x` and `~x` give the operand's own type.
    ExprPtr unaryOf(Checker& check, const AstExpr& node, UnOp code) {
      ExprPtr operand = CheckExpr(check, node.first);
      const TypePtr type = code == UnOp::LogNot ? IntType(1) : operand->type;
      const Span at = operand->at;

      if (ExprPtr problem = AnyTypeProblem(operand, code == UnOp::Neg ? "`-`" : (code == UnOp::LogNot ? "`!`" : "`~`"), at)) {
        return problem;
      }

      return UnaryNode(code, move(operand), type, at);
    }

    ExprPtr negate(Checker& check, const AstExpr& node) {
      return unaryOf(check, node, UnOp::Neg);
    }

    ExprPtr logNot(Checker& check, const AstExpr& node) {
      return unaryOf(check, node, UnOp::LogNot);
    }

    ExprPtr bitNot(Checker& check, const AstExpr& node) {
      return unaryOf(check, node, UnOp::BitNot);
    }

    ExprPtr valueCopy(Checker& check, const AstExpr& node) {
      return ValCopy(CheckExpr(check, node.first));
    }

    ExprPtr paren(Checker& check, const AstExpr& node) {
      return CheckExpr(check, node.first);
    }

    ExprPtr postfix(Checker& check, const AstExpr& node) {
      return CheckPostfix(check, node);
    }

    // ── literals ────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────

    ExprPtr intLiteral(Checker&, const AstExpr& node) {
      return ConstInt(ParseInt(node.literal.text), IntType(64), SpanAt(node.literal.pos));
    }

    ExprPtr floatLiteral(Checker&, const AstExpr& node) {
      return ConstFloat(ParseDouble(node.literal.text), FloatType(64), SpanAt(node.literal.pos));
    }

    ExprPtr stringLiteral(Checker&, const AstExpr& node) {
      return ConstString(StripQuotes(node.literal.text), StringType(), SpanAt(node.literal.pos));
    }

    ExprPtr charLiteral(Checker&, const AstExpr& node) {
      return ConstInt(CharValue(node.literal.text), IntType(8), SpanAt(node.literal.pos));
    }

    ExprPtr trueLiteral(Checker&, const AstExpr&) {
      return ConstBool(true, IntType(1), NoSpan());
    }

    ExprPtr falseLiteral(Checker&, const AstExpr&) {
      return ConstBool(false, IntType(1), NoSpan());
    }

    // The handler of one expression kind.
    struct ExprRow {
      AstExprKind kind;
      ExprHandler handler;
    };

    const ExprRow kCoreExprs[] = {{AstExprKind::Binary, binary}, {AstExprKind::Neg, negate}, {AstExprKind::LogNot, logNot}, {AstExprKind::BitNot, bitNot},
      {AstExprKind::ValCopy, valueCopy}, {AstExprKind::Paren, paren}, {AstExprKind::Postfix, postfix}, {AstExprKind::Int, intLiteral},
      {AstExprKind::Float, floatLiteral}, {AstExprKind::String, stringLiteral}, {AstExprKind::Char, charLiteral}, {AstExprKind::True, trueLiteral},
      {AstExprKind::False, falseLiteral}};

  }  // namespace

  void RegisterCoreExprs(Registry& registry) {
    for (const ExprRow& row : kCoreExprs) {
      AddExpr(registry, row.kind, row.handler);
    }

    RegisterNames(registry);
  }

}  // namespace polyir::front
