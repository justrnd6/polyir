// The HIR node constructors of HirBuild.hpp, and the helpers that turn source text into values (positions, number literals).
#include "front/HirBuild.hpp"
#include "front/TypePool.hpp"
#include "front/Strings.hpp"

namespace polyir::front {

  Span SpanAt(Pos pos) {
    Span span;
    span.line = static_cast<int>(pos.line);
    span.column = static_cast<int>(pos.column);

    return span;
  }

  Span NoSpan() {
    return Span();
  }

  // Unsigned arithmetic, so a literal too big for 64 bits wraps instead of overflowing.
  int64_t ParseInt(StringView text) {
    uint64_t value = 0;

    for (const char c : text) {
      value = value * 10 + static_cast<uint64_t>(c - '0');
    }

    return static_cast<int64_t>(value);
  }

  double ParseDouble(StringView text) {
    // Correctly rounded, as the Nina standard library's String.toDouble is (one division by an exact power of ten).
    return strtod(String(text).c_str(), nullptr);
  }

  // A character of two or three UTF-8 bytes is decoded to its code point. The lead byte gives the high bits, each following byte six more.
  int64_t CharCode(StringView text) {
    const unsigned char first = static_cast<unsigned char>(text[1]);

    if (first < 0x80 || text.size() < 4) {
      return first;
    }

    if (first < 0xE0) {
      return ((first & 0x1F) << 6) | (static_cast<unsigned char>(text[2]) & 0x3F);
    }

    return ((first & 0x0F) << 12) | ((static_cast<unsigned char>(text[2]) & 0x3F) << 6) | (static_cast<unsigned char>(text[3]) & 0x3F);
  }

  String StripQuotes(StringView text) {
    return LiteralValue(text);
  }

  Vector<String> SplitOn(const String& text, char separator) {
    Vector<String> parts;

    for (size_t from = 0; from <= text.size();) {
      const size_t at = min(text.find(separator, from), text.size());
      parts.push_back(text.substr(from, at - from));
      from = at + 1;
    }

    return parts;
  }

  // A node with only its kind, type and place set; each constructor below fills in the rest.
  SharedPtr<Expr> NewExpr(ExprKind kind, TypePtr type, Span at) {
    SharedPtr<Expr> made = make_shared<Expr>();
    made->kind = kind;
    made->type = move(type);
    made->at = at;

    return made;
  }

  ExprPtr ConstInt(int64_t value, TypePtr type, Span at) {
    SharedPtr<Expr> made = NewExpr(ExprKind::ConstInt, move(type), at);
    made->intValue = value;

    return made;
  }

  ExprPtr ConstFloat(double value, TypePtr type, Span at) {
    SharedPtr<Expr> made = NewExpr(ExprKind::ConstFloat, move(type), at);
    made->floatValue = value;

    return made;
  }

  ExprPtr ConstBool(bool value, TypePtr type, Span at) {
    SharedPtr<Expr> made = NewExpr(ExprKind::ConstBool, move(type), at);
    made->boolValue = value;

    return made;
  }

  ExprPtr ConstString(String text, TypePtr type, Span at) {
    SharedPtr<Expr> made = NewExpr(ExprKind::ConstString, move(type), at);
    made->text = move(text);

    return made;
  }

  ExprPtr NamedRef(ExprKind kind, String name, TypePtr type, Span at) {
    SharedPtr<Expr> made = NewExpr(kind, move(type), at);
    made->name = move(name);

    return made;
  }

  ExprPtr BinaryNode(BinOp op, ExprPtr left, ExprPtr right, TypePtr type, Span at) {
    SharedPtr<Expr> made = NewExpr(ExprKind::Binary, move(type), at);
    made->binaryOp = op;
    made->operands = {move(left), move(right)};

    return made;
  }

  ExprPtr UnaryNode(UnOp op, ExprPtr operand, TypePtr type, Span at) {
    SharedPtr<Expr> made = NewExpr(ExprKind::Unary, move(type), at);
    made->unaryOp = op;
    made->operands = {move(operand)};

    return made;
  }

  ExprPtr CastNode(ExprKind kind, ExprPtr operand, TypePtr type, Span at) {
    SharedPtr<Expr> made = NewExpr(kind, move(type), at);
    made->operands = {move(operand)};

    return made;
  }

  ExprPtr CallNode(String name, Vector<ExprPtr> args, TypePtr type, Span at) {
    SharedPtr<Expr> made = NewExpr(ExprKind::Call, move(type), at);
    made->name = move(name);
    made->operands = move(args);

    return made;
  }

  ExprPtr IndirectCallNode(ExprPtr callee, Vector<ExprPtr> args, TypePtr type, Span at) {
    SharedPtr<Expr> made = NewExpr(ExprKind::CallIndirect, move(type), at);
    made->operands.reserve(args.size() + 1);
    made->operands.push_back(move(callee));
    made->operands.insert(made->operands.end(), args.begin(), args.end());

    return made;
  }

  ExprPtr BuiltinNode(Builtin op, ExprPtr receiver, Vector<ExprPtr> args, TypePtr type, Span at) {
    SharedPtr<Expr> made = NewExpr(ExprKind::CallBuiltin, move(type), at);
    made->builtin = op;
    made->operands.reserve(args.size() + 1);
    made->operands.push_back(move(receiver));
    made->operands.insert(made->operands.end(), args.begin(), args.end());

    return made;
  }

  ExprPtr StructLitNode(String name, Vector<int> indices, Vector<ExprPtr> values, TypePtr type, Span at) {
    SharedPtr<Expr> made = NewExpr(ExprKind::StructLit, move(type), at);
    made->name = move(name);
    made->fieldIndices = move(indices);
    made->operands = move(values);

    return made;
  }

  ExprPtr FieldGetNode(ExprPtr base, int index, TypePtr type, Span at) {
    SharedPtr<Expr> made = NewExpr(ExprKind::FieldGet, move(type), at);
    made->operands = {move(base)};
    made->fieldIndex = index;

    return made;
  }

  ExprPtr IndexGetNode(ExprPtr base, Vector<ExprPtr> indices, TypePtr type, Span at) {
    SharedPtr<Expr> made = NewExpr(ExprKind::IndexGet, move(type), at);
    made->operands.reserve(indices.size() + 1);
    made->operands.push_back(move(base));
    made->operands.insert(made->operands.end(), indices.begin(), indices.end());

    return made;
  }

  ExprPtr ContainerNode(ExprKind kind, Vector<ExprPtr> operands, TypePtr type, Span at) {
    SharedPtr<Expr> made = NewExpr(kind, move(type), at);
    made->operands = move(operands);

    return made;
  }

  ExprPtr Unsupported(String what, Span at) {
    SharedPtr<Expr> made = NewExpr(ExprKind::Unsupported, VoidType(), at);
    made->text = move(what);

    return made;
  }

  StmtPtr NewStmt(StmtKind kind, Span at) {
    StmtPtr made = make_shared<Stmt>();
    made->kind = kind;
    made->at = at;

    return made;
  }

}  // namespace polyir::front
