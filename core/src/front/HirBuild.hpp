// Constructors for the HIR nodes the checker makes: one small function per node of the Nina `HExpr`, so the checker reads like the Nina checker
// (`HExpr.HBinary(code, l, r, ty, at)` is `BinaryNode(code, l, r, ty, at)`). A node is a shared, immutable `Expr`; `at` is where the Nina checker
// attached it (a token's position, or `NoSpan()`).
#pragma once

#include "front/Ast.hpp"
#include "polyir/Hir.hpp"

namespace polyir::front {

  Span SpanAt(Pos pos);

  Span NoSpan();

  // The number a literal's text spells, as the Nina `toInt() else 0` reads it: digits only, wrapping modulo 2^64.
  int64_t ParseInt(StringView text);

  // The number a float literal's text spells, as the Nina `toDouble() else 0.0` reads it.
  double ParseDouble(StringView text);

  // The character code of a `'c'` literal: the Nina `codeAt(1)`, the character after the opening quote.
  int64_t CharCode(StringView text);

  // The value of a string literal: its quotes taken off and its escapes decoded (`StripQuotes`, Strings.cpp `LiteralValue`).
  String StripQuotes(StringView text);

  // The pieces of `text` between `separator`s (always at least one; empty pieces stay).
  Vector<String> SplitOn(const String& text, char separator);

  SharedPtr<Expr> NewExpr(ExprKind kind, TypePtr type, Span at);

  ExprPtr ConstInt(int64_t value, TypePtr type, Span at);

  ExprPtr ConstFloat(double value, TypePtr type, Span at);

  ExprPtr ConstBool(bool value, TypePtr type, Span at);

  ExprPtr ConstString(String text, TypePtr type, Span at);

  ExprPtr NamedRef(ExprKind kind, String name, TypePtr type, Span at);  // Local, LoadSlot, Global or FuncRef

  ExprPtr BinaryNode(BinOp op, ExprPtr left, ExprPtr right, TypePtr type, Span at);

  ExprPtr UnaryNode(UnOp op, ExprPtr operand, TypePtr type, Span at);

  ExprPtr CastNode(ExprKind kind, ExprPtr operand, TypePtr type, Span at);  // Cast or Coerce

  ExprPtr CallNode(String name, Vector<ExprPtr> args, TypePtr type, Span at);

  ExprPtr IndirectCallNode(ExprPtr callee, Vector<ExprPtr> args, TypePtr type, Span at);

  ExprPtr BuiltinNode(Builtin op, ExprPtr receiver, Vector<ExprPtr> args, TypePtr type, Span at);

  ExprPtr StructLitNode(String name, Vector<int> indices, Vector<ExprPtr> values, TypePtr type, Span at);

  ExprPtr FieldGetNode(ExprPtr base, int index, TypePtr type, Span at);

  ExprPtr IndexGetNode(ExprPtr base, Vector<ExprPtr> indices, TypePtr type, Span at);

  ExprPtr ContainerNode(ExprKind kind, Vector<ExprPtr> operands, TypePtr type, Span at);  // VecLit or NewContainer

  // A construct the checker could not translate, or a mistake: `what` is the message the diagnostics report at `at`.
  ExprPtr Unsupported(String what, Span at);

  StmtPtr NewStmt(StmtKind kind, Span at);

}  // namespace polyir::front
