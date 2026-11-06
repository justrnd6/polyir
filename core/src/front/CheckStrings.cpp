// The string built-ins (CheckStrings.nina): `str(x)` (a whole number, bool, char or string as text), `s.slice(from, to)`, `s.find(sub)` /
// `s.find(sub, from)`, `s[i]` and `print(x)` (any value `str` takes, as a line on standard output); and `xs.truncate(n)` on a list.
// Everything else on strings (`split`, `trim`, `join`, ...) is ordinary polyir, in the Text library (stdlib/Text.poly, added by Prelude.cpp).
#include "front/Checker.hpp"

namespace polyir::front {

  namespace {

    // The kinds `str` turns into text (a bool and a char are integers here).
    bool hasText(TypeKind kind) {
      return kind == TypeKind::Int || kind == TypeKind::UInt || kind == TypeKind::String;
    }

    // A float has no text form yet (each target would print it differently).
    ExprPtr floatHasNoText(const String& call, Span at) {
      return Unsupported(call + " takes a whole number, bool, char or string; a float has no text form yet", at);
    }

    ExprPtr strOf(MethodCall& call) {
      const TypeKind kind = call.receiver->type->kind;

      if (kind == TypeKind::Float) {
        return floatHasNoText("str(x)", call.at);
      }

      return hasText(kind) ? BuiltinOf(call, Builtin::StrOf, StringType()) : nullptr;
    }

    ExprPtr sliceOf(MethodCall& call) {
      if (call.args.size() != 2) {
        return Unsupported("s.slice(from, to) takes two positions", call.at);
      }

      return BuiltinOf(call, Builtin::Slice, StringType());
    }

    // `find` without a start searches from 0.
    ExprPtr findOf(MethodCall& call) {
      if (call.args.empty() || call.args.size() > 2) {
        return Unsupported("s.find(sub) or s.find(sub, from)", call.at);
      }

      Vector<ExprPtr> args = call.args;

      if (args.size() == 1) {
        args.push_back(ConstInt(0, IntType(64), call.at));
      }

      return BuiltinNode(Builtin::Find, call.receiver, move(args), IntType(64), call.at);
    }

    // A string as it is; any other value as its `str(value)`.
    ExprPtr textOf(const ExprPtr& value) {
      if (value->type->kind == TypeKind::String) {
        return value;
      }

      return BuiltinNode(Builtin::StrOf, value, {}, StringType(), value->at);
    }

    // `print(x)` writes a string as it is and any other value as its `str(x)`.
    ExprPtr printOf(MethodCall& call) {
      const TypeKind kind = call.receiver->type->kind;

      if (!call.args.empty()) {
        return Unsupported("print(x) takes one value", call.at);
      }

      if (kind == TypeKind::Float) {
        return floatHasNoText("print(x)", call.at);
      }

      if (!hasText(kind)) {
        return nullptr;
      }

      return BuiltinNode(Builtin::Print, textOf(call.receiver), {}, VoidType(), call.at);
    }

    // `fail(message)` stops the program with the message (a string).
    ExprPtr failOf(MethodCall& call) {
      if (!call.args.empty() || call.receiver->type->kind != TypeKind::String) {
        return Unsupported("fail(message) takes one string", call.at);
      }

      return BuiltinNode(Builtin::Fail, call.receiver, {}, VoidType(), call.at);
    }

    // `xs.truncate(n)` keeps a list's first `n` elements; the result is the list.
    ExprPtr truncateOf(MethodCall& call) {
      return BuiltinOf(call, Builtin::Truncate, call.receiver->type);
    }

  }  // namespace

  // `s[i]`: the character at byte `i`.
  ExprPtr CharAt(ExprPtr text, Vector<ExprPtr> indices) {
    const Span at = text->at;

    if (indices.size() != 1) {
      return Unsupported("a string takes one index", at);
    }

    return BuiltinNode(Builtin::CharAt, move(text), move(indices), IntType(8), at);
  }

  void RegisterStrings(Registry& registry) {
    AddMethod(registry, "str", kAnyKind, strOf);
    AddMethod(registry, "slice", KindBit(TypeKind::String), sliceOf);
    AddMethod(registry, "find", KindBit(TypeKind::String), findOf);
    AddMethod(registry, "truncate", KindBit(TypeKind::DynamicArray), truncateOf);

    AddMethod(registry, "print", kAnyKind, printOf);
    AddMethod(registry, "fail", kAnyKind, failOf);

    AddFunctionForm(registry, "str");
    AddFunctionForm(registry, "print");
    AddFunctionForm(registry, "fail");
  }

}  // namespace polyir::front
