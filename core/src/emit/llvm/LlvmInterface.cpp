// Interfaces. An interface value is a heap block of two cells: the object (the struct that was converted) and the table of its methods. The table of a
// (struct, interface) pair is a constant array of the addresses of the struct's methods, which are the functions `<Struct>_<method>` taking the struct
// first, in the order the interface lists them; a call goes through the slot number the compiler worked out.
#include "polyir/Std.hpp"
#include "emit/llvm/LlvmEmitter.hpp"

namespace polyir::llvm {

  namespace {

    TypePtr bytePointer() {
      return MakePointer(MakeInt(8));
    }

    String tableEntries(const InterfaceDef& interface, const String& implementer) {
      String entries;

      for (size_t i = 0; i < interface.methods.size(); i++) {
        entries += (i == 0 ? "" : ", ") + String("ptr @") + QuotedName(implementer + "_" + interface.methods[i].name);
      }

      return entries;
    }

    String tableDefinition(const InterfaceDef& interface, const String& implementer) {
      return VtableName(implementer, interface.name) + " = private constant [" + to_string(interface.methods.size()) + " x ptr] [" + tableEntries(interface, implementer) + "]";
    }

  }  // namespace

  Vector<String> Vtables(const Module& module) {
    Vector<String> out;

    for (const InterfaceDef& interface : module.interfaces) {
      for (const String& implementer : interface.implementers) {
        out.push_back(tableDefinition(interface, implementer));
      }
    }

    return out;
  }

  bool InterfaceBuiltin(FunctionEmitter& emitter, const Expr& expr, Value& out) {
    if (expr.builtin != Builtin::InterfaceBox && expr.builtin != Builtin::InterfaceCall) {
      return false;
    }

    const Value operand = EmitExpr(emitter, *expr.operands[0]);  // the struct to box, or the interface value to call through
    out = expr.builtin == Builtin::InterfaceBox ? InterfaceBox(emitter, expr, operand) : InterfaceCall(emitter, expr, operand);

    return true;
  }

  Value InterfaceBox(FunctionEmitter& emitter, const Expr& expr, const Value& object) {
    const Value box = Allocate(emitter, "16", Layout::References);
    Store(emitter, object, WordAddress(emitter, box.text, 0));
    Store(emitter, Value{VtableName(object.type->name, expr.type->name), bytePointer()}, WordAddress(emitter, box.text, 1));

    return Value{box.text, expr.type};
  }

  // The method in slot `slot` of the interface value's table. A method is the program's own code: calling it may reach a safe point.
  Value MethodOf(FunctionEmitter& emitter, const Value& interfaceValue, int64_t slot) {
    const Value table = LoadFrom(emitter, WordAddress(emitter, interfaceValue.text, 1), bytePointer());
    NoteCall(emitter);

    return LoadFrom(emitter, WordAddress(emitter, table.text, slot), bytePointer());
  }

  // A method call through an interface value: the method gets the object first, then the call's own arguments.
  Value InterfaceCall(FunctionEmitter& emitter, const Expr& expr, const Value& interfaceValue) {
    const Value object = LoadFrom(emitter, WordAddress(emitter, interfaceValue.text, 0), bytePointer());
    const Value method = MethodOf(emitter, interfaceValue, expr.operands[1]->intValue);
    const String rest = ArgumentList(emitter, expr, 2);
    const Value result{CallInstruction(emitter, expr.type, method.text, "ptr " + object.text + (rest.empty() ? "" : ", " + rest)), expr.type};
    PassOnThrownIfAny(emitter);

    return result;
  }

}  // namespace polyir::llvm
