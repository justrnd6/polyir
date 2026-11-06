// Interface values (CheckIface.nina). A struct that wrote `: Shape` converts to a `Shape` wherever a `Shape` is expected (`BoxIfInterface`), and
// `shape.area(args)` is a call through the value's method table (`InterfaceCall`). The interface value itself is an ordinary struct (`data`, `vtable`), so
// everything that moves structs around moves interface values too.
#include "front/Checker.hpp"

namespace polyir::front {

  // The interface behind a type, if it is one.
  const InterfaceInfo* InterfaceOf(Checker& check, const TypePtr& type) {
    if (type->kind != TypeKind::Struct) {
      return nullptr;
    }

    return FindInterface(DeclsOf(check), SymbolOf(check, type->name));
  }

  namespace {

    // A struct that implements the interface is boxed; any other is reported.
    ExprPtr boxStruct(Checker& check, const InterfaceInfo& interface, const TypePtr& expected, ExprPtr value) {
      const String source = value->type->name;
      const Span at = value->at;

      if (source == expected->name) {
        return value;
      }

      if (IsImplementedBy(interface, SymbolOf(check, source))) {
        return BuiltinNode(Builtin::InterfaceBox, move(value), {}, expected, at);
      }

      const String target = expected->name;

      return Unsupported(source + " does not implement " + target + " (declare it: struct " + source + " : " + target + ")", at);
    }

    // "Shape.area takes 1 arguments but 2 were given".
    ExprPtr wrongCount(Checker& check, const InterfaceInfo& interface, Ident method, size_t wanted, size_t given) {
      return Unsupported(Str(check, interface.name) + "." + Str(check, method.name) + " takes " + to_string(wanted) + " arguments but " + to_string(given) +
        " were given", SpanAt(method.pos));
    }

    // The call's operands are the receiver, the slot number, then the arguments fitted to the method's parameters.
    ExprPtr callSlot(Checker& check, const InterfaceInfo& interface, int slot, ExprPtr receiver, Ident method, Vector<ExprPtr> args) {
      const Vector<TypePtr>& params = interface.methodParams[slot];

      if (params.size() != args.size()) {
        return wrongCount(check, interface, method, params.size(), args.size());
      }

      Vector<ExprPtr> all{ConstInt(slot, IntType(64), SpanAt(method.pos))};

      for (ExprPtr& argument : Fitted(check, move(args), params)) {
        all.push_back(move(argument));
      }

      return BuiltinNode(Builtin::InterfaceCall, move(receiver), move(all), interface.methodResults[slot], SpanAt(method.pos));
    }

  }  // namespace

  // `value` as `expected`. Unchanged unless `expected` is an interface and `value` is a struct.
  ExprPtr BoxIfInterface(Checker& check, const TypePtr& expected, ExprPtr value) {
    const InterfaceInfo* interface = InterfaceOf(check, expected);

    if (interface == nullptr || value->type->kind != TypeKind::Struct) {
      return value;
    }

    return boxStruct(check, *interface, expected, move(value));
  }

  // `recv.method(args)` on an interface value: a call through slot `slot` of the method table.
  ExprPtr InterfaceCall(Checker& check, const InterfaceInfo& interface, ExprPtr receiver, Ident method, Vector<ExprPtr> args) {
    const int slot = SlotOf(interface, method.name);

    if (slot < 0) {
      return Unsupported(Str(check, interface.name) + " has no method " + Str(check, method.name), SpanAt(method.pos));
    }

    return callSlot(check, interface, slot, move(receiver), method, move(args));
  }

  // Each argument boxed, type-checked and width-fixed against the parameter it fills.
  Vector<ExprPtr> Fitted(Checker& check, Vector<ExprPtr> values, const Vector<TypePtr>& params) {
    for (size_t i = 0; i < values.size() && i < params.size(); i++) {
      values[i] = Expecting(params[i], BoxIfInterface(check, params[i], move(values[i])));
    }

    return CoerceAll(move(values), params);
  }

  namespace {

    TypePtr elementOfCollection(const TypePtr& type) {
      if (type->kind == TypeKind::DynamicArray || type->kind == TypeKind::Set) {
        return type->element;
      }

      return VoidType();
    }

  }  // namespace

  // `mutarray<Shape>.push(circle)`: the pushed value is boxed to the element type.
  ExprPtr PushBoxed(Checker& check, ExprPtr call) {
    const bool boxes = call->builtin == Builtin::Push || call->builtin == Builtin::With || call->builtin == Builtin::Add;

    if (call->kind != ExprKind::CallBuiltin || !boxes || call->operands.size() != 2) {
      return call;
    }

    const TypePtr element = elementOfCollection(call->operands[0]->type);

    return BuiltinNode(call->builtin, call->operands[0], {BoxIfInterface(check, element, call->operands[1])}, call->type, call->at);
  }

}  // namespace polyir::front
