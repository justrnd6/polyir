// The part of PolyIRBuilder that makes containers, sum values, interface values and calls through a value. Most of
// these are builtins: a CallBuiltin expression whose first operand is the receiver (the list, map, either, ...), made
// by `builtinNode` once the receiver's type has been checked. The checks come first in each function, then the node.
#include "polyir/Std.hpp"
#include "builder/BuilderSupport.hpp"

#include <algorithm>

namespace polyir {

  using namespace polyir::checks;

  namespace {

    TypePtr i64Type() {
      return MakeInt(64);
    }

    ExprPtr constant(int64_t value) {
      auto expr = MakeExpr(ExprKind::ConstInt, i64Type());
      expr->intValue = value;

      return expr;
    }

    void appendAll(Vector<ExprPtr>& to, Vector<ExprPtr>& from) {
      for (ExprPtr& e : from) {
        to.push_back(move(e));
      }
    }

    ExprPtr builtinNode(Builtin op, ExprPtr receiver, Vector<ExprPtr> arguments, TypePtr result) {
      auto expr = MakeExpr(ExprKind::CallBuiltin, move(result));
      expr->builtin = op;
      expr->operands.push_back(move(receiver));
      appendAll(expr->operands, arguments);

      return expr;
    }

    bool isList(const TypePtr& t) {
      const TypeKind kind = t->kind;

      return kind == TypeKind::DynamicArray || kind == TypeKind::FixedArray || kind == TypeKind::Buffer || kind == TypeKind::Vec ||
        kind == TypeKind::Sparse;
    }

    bool isMapOrSet(const TypePtr& t) {
      return t->kind == TypeKind::Hashmap || t->kind == TypeKind::Set;
    }

    bool hasLength(const TypePtr& t) {
      return isList(t) || t->kind == TypeKind::Hashmap || t->kind == TypeKind::Set || t->kind == TypeKind::String;
    }

    void requireIndices(const Vector<ExprPtr>& indices, size_t count, const String& where) {
      if (indices.size() != count) {
        Fail(where + " needs " + to_string(count) + " index(es), " + to_string(indices.size()) + " given");
      }

      for (const ExprPtr& i : indices) {
        RequireInteger(i->type, where + " index");
      }
    }

    TypePtr mapValue(const TypePtr& base, const Vector<ExprPtr>& indices) {
      if (indices.size() != 1) {
        Fail("a map is read with one key");
      }

      RequireSame(base->element, indices[0]->type, "map key");

      return base->second;
    }

    // A list takes one index; a sparse takes a row, or a row and a column.
    size_t indexCount(const TypePtr& base, const Vector<ExprPtr>& indices) {
      return base->kind == TypeKind::Sparse && !indices.empty() && indices.size() <= 2 ? indices.size() : 1;
    }

    TypePtr listElement(const TypePtr& base, const Vector<ExprPtr>& indices) {
      if (!isList(base) && base->kind != TypeKind::Pointer) {
        Fail("cannot index " + TypeText(*base));
      }

      requireIndices(indices, indexCount(base, indices), TypeText(*base));

      return base->element;
    }

    // The type `base[indices]` reads, after checking that `base` can be indexed that way.
    TypePtr elementOfIndexed(const TypePtr& base, const Vector<ExprPtr>& indices) {
      switch (base->kind) {
        case TypeKind::Hashmap:
          return mapValue(base, indices);
        case TypeKind::Grid: {
          requireIndices(indices, 2, "a grid");
          return base->element;
        }
        case TypeKind::Tensor: {
          requireIndices(indices, base->dims.size(), "a tensor");
          return base->element;
        }
        default:
          return listElement(base, indices);
      }
    }

    void requireMutable(const TypePtr& t, const String& what) {
      if (!t->isMutable) {
        Fail(what + " cannot be changed once it is made: " + TypeText(*t) + " is not mutable");
      }
    }

    // The same array, set or map type, made mutable or immutable as asked (Freeze and Thaw).
    TypePtr withMutability(const TypePtr& t, bool mutability) {
      switch (t->kind) {
        case TypeKind::DynamicArray:
          return MakeDynamicArray(t->element, mutability);
        case TypeKind::Set:
          return MakeSet(t->element, mutability);
        case TypeKind::Hashmap:
          return MakeHashmap(t->element, t->second, mutability);
        default:
          Fail("only an array, a set or a map has a mutable and an immutable form, not " + TypeText(*t));
      }
    }

    Vector<TypePtr> parameterTypes(const Signature& signature) {
      Vector<TypePtr> types;

      for (const Parameter& p : signature.parameters) {
        types.push_back(p.type);
      }

      return types;
    }

    void requireCallable(const TypePtr& type) {
      if (!IsCallable(*type)) {
        Fail("only a func or closure value can be called, not " + TypeText(*type));
      }
    }

    void requireIndirectArguments(const TypePtr& type, const Vector<ExprPtr>& arguments) {
      const auto& wanted = type->parameters;

      if (wanted.size() != arguments.size()) {
        Fail("call through " + TypeText(*type) + " has " + to_string(arguments.size()) + " arguments");
      }

      for (size_t i = 0; i < wanted.size(); i++) {
        RequireSame(wanted[i], arguments[i]->type, "argument " + to_string(i + 1) + " of a call through a value");
      }
    }

    void requireListType(const TypePtr& type) {
      if (!type->element || type->kind == TypeKind::Hashmap) {
        Fail("a list literal makes an array, a set or a vec, not " + TypeText(*type));
      }
    }

    void requireVecSize(const TypePtr& type, size_t count) {
      if (type->kind == TypeKind::Vec && type->size != static_cast<int>(count)) {
        Fail(TypeText(*type) + " takes " + to_string(type->size) + " elements");
      }
    }

    void requireElements(const TypePtr& type, const Vector<ExprPtr>& elements) {
      for (const ExprPtr& e : elements) {
        RequireSame(type->element, e->type, "element of " + TypeText(*type));
      }
    }

    // A map's `with` takes a key and a value; an array's or a set's takes one value.
    bool takesPair(const TypePtr& t) {
      return t->kind == TypeKind::Hashmap;
    }

    void requireWithable(const TypePtr& t) {
      if (t->isMutable || (t->kind != TypeKind::DynamicArray && t->kind != TypeKind::Set && !takesPair(t))) {
        Fail("with makes a changed copy of an array, map or set that cannot be changed in place, not of " + TypeText(*t));
      }
    }

    void requireWithCount(const TypePtr& t, const Vector<ExprPtr>& arguments) {
      const size_t count = takesPair(t) ? 2 : 1;

      if (arguments.size() != count) {
        Fail("with on " + TypeText(*t) + " takes " + to_string(count) + " argument(s)");
      }
    }

    void requireWithTypes(const TypePtr& t, const Vector<ExprPtr>& arguments) {
      RequireSame(t->element, arguments[0]->type, "with");

      if (takesPair(t)) {
        RequireSame(t->second, arguments[1]->type, "with");
      }
    }

    void requireCase(const TypePtr& variant, int caseIndex) {
      if (caseIndex < 0 || caseIndex >= static_cast<int>(variant->parameters.size())) {
        Fail("variant case " + to_string(caseIndex) + " does not exist in " + TypeText(*variant));
      }
    }

    const InterfaceDef& requireInterface(const Module& module, const String& name) {
      const InterfaceDef* interfaceDef = FindInterface(module, name);

      if (!interfaceDef) {
        Fail("no interface called '" + name + "'");
      }

      return *interfaceDef;
    }

    void requireImplements(const InterfaceDef& interfaceDef, const TypePtr& structType, const String& interfaceName) {
      const auto& names = interfaceDef.implementers;

      if (find(names.begin(), names.end(), structType->name) == names.end()) {
        Fail(structType->name + " does not implement " + interfaceName + " (declare it: implement)");
      }
    }

    const InterfaceDef& requireInterfaceValue(const Module& module, const TypePtr& type) {
      const InterfaceDef* interfaceDef = type->kind == TypeKind::Struct ? FindInterface(module, type->name) : nullptr;

      if (!interfaceDef) {
        Fail("a method is called through an interface value, not " + TypeText(*type));
      }

      return *interfaceDef;
    }

    // A method's slot is its position in the interface, which is also its place in every vtable.
    size_t slotOf(const InterfaceDef& interfaceDef, const String& method) {
      for (size_t slot = 0; slot < interfaceDef.methods.size(); slot++) {
        if (interfaceDef.methods[slot].name == method) {
          return slot;
        }
      }

      Fail(interfaceDef.name + " has no method " + method);
    }

    void requireMethodArguments(const InterfaceMethod& methodDef, const String& method, const Vector<ExprPtr>& arguments) {
      if (methodDef.parameters.size() != arguments.size()) {
        Fail(method + " takes " + to_string(methodDef.parameters.size()) + " argument(s)");
      }

      for (size_t i = 0; i < arguments.size(); i++) {
        RequireSame(methodDef.parameters[i], arguments[i]->type, "argument " + to_string(i + 1) + " of " + method);
      }
    }

  }  // namespace

  ExprPtr FuncRef(const PolyIRBuilder& builder, const String& function) {
    const Signature& known = RequireSignature(builder, function);
    auto expr = MakeExpr(ExprKind::FuncRef, MakeFunction(parameterTypes(known), known.result));
    expr->name = function;

    return expr;
  }

  ExprPtr Coerce(ExprPtr value, TypePtr to) {
    RequireInteger(value->type, "a coercion");
    RequireInteger(to, "a coercion");
    auto expr = MakeExpr(ExprKind::Coerce, move(to));
    expr->operands = {move(value)};

    return expr;
  }

  ExprPtr CallIndirect(ExprPtr callee, Vector<ExprPtr> arguments) {
    requireCallable(callee->type);
    requireIndirectArguments(callee->type, arguments);
    auto expr = MakeExpr(ExprKind::CallIndirect, callee->type->element);
    expr->operands.push_back(move(callee));
    appendAll(expr->operands, arguments);

    return expr;
  }

  ExprPtr Unsupported(const String& what, TypePtr type) {
    auto expr = MakeExpr(ExprKind::Unsupported, move(type));
    expr->text = what;

    return expr;
  }

  ExprPtr SlotRead(const String& name, TypePtr type) {
    auto expr = MakeExpr(ExprKind::LoadSlot, move(type));
    expr->name = name;

    return expr;
  }

  ExprPtr VecLit(TypePtr type, Vector<ExprPtr> elements) {
    requireListType(type);
    requireVecSize(type, elements.size());
    requireElements(type, elements);
    auto expr = MakeExpr(ExprKind::VecLit, move(type));
    expr->operands = move(elements);

    return expr;
  }

  ExprPtr NewContainer(TypePtr type, Vector<ExprPtr> sizes) {
    for (const ExprPtr& s : sizes) {
      RequireInteger(s->type, "a container size");
    }

    auto expr = MakeExpr(ExprKind::NewContainer, move(type));
    expr->operands = move(sizes);

    return expr;
  }

  ExprPtr Index(ExprPtr base, Vector<ExprPtr> indices) {
    auto expr = MakeExpr(ExprKind::IndexGet, elementOfIndexed(base->type, indices));
    expr->operands.push_back(move(base));
    appendAll(expr->operands, indices);

    return expr;
  }

  ExprPtr StrEq(ExprPtr a, ExprPtr b) {
    RequireKind(a->type, TypeKind::String, "strEq compares strings");
    RequireKind(b->type, TypeKind::String, "strEq compares strings");

    return builtinNode(Builtin::StrEq, move(a), {move(b)}, MakeBool());
  }

  ExprPtr Concat(ExprPtr a, ExprPtr b) {
    RequireKind(a->type, TypeKind::String, "concat joins strings");
    RequireKind(b->type, TypeKind::String, "concat joins strings");

    return builtinNode(Builtin::Concat, move(a), {move(b)}, MakeString());
  }

  ExprPtr Len(ExprPtr container) {
    if (!hasLength(container->type)) {
      Fail("len needs a list, map, set or string, not " + TypeText(*container->type));
    }

    return builtinNode(Builtin::Len, move(container), {}, i64Type());
  }

  ExprPtr Push(ExprPtr list, ExprPtr value) {
    RequireKind(list->type, TypeKind::DynamicArray, "push needs a mutarray");
    requireMutable(list->type, "an array");
    RequireSame(list->type->element, value->type, "push");
    TypePtr result = list->type;

    return builtinNode(Builtin::Push, move(list), {move(value)}, move(result));
  }

  ExprPtr Truncate(ExprPtr list, ExprPtr length) {
    RequireKind(list->type, TypeKind::DynamicArray, "truncate needs a mutarray");
    requireMutable(list->type, "an array");
    RequireSame(i64Type(), length->type, "truncate");
    TypePtr result = list->type;

    return builtinNode(Builtin::Truncate, move(list), {move(length)}, move(result));
  }

  ExprPtr Remove(ExprPtr collection, ExprPtr key) {
    if (!isMapOrSet(collection->type)) {
      Fail("remove needs a mutmap or mutset, not " + TypeText(*collection->type));
    }

    requireMutable(collection->type, "a map or set");
    RequireSame(collection->type->element, key->type, "remove");

    return builtinNode(Builtin::Remove, move(collection), {move(key)}, MakeBool());
  }

  ExprPtr Has(ExprPtr collection, ExprPtr key) {
    if (!isMapOrSet(collection->type)) {
      Fail("has needs a map or set, not " + TypeText(*collection->type));
    }

    RequireSame(collection->type->element, key->type, "has");

    return builtinNode(Builtin::Has, move(collection), {move(key)}, MakeBool());
  }

  ExprPtr SetAdd(ExprPtr set, ExprPtr member) {
    RequireKind(set->type, TypeKind::Set, "add needs a mutset");
    requireMutable(set->type, "a set");
    RequireSame(set->type->element, member->type, "add");
    TypePtr result = set->type;

    return builtinNode(Builtin::Add, move(set), {move(member)}, move(result));
  }

  ExprPtr Copy(ExprPtr container) {
    TypePtr result = container->type;

    return builtinNode(Builtin::Copy, move(container), {}, move(result));
  }

  ExprPtr Freeze(ExprPtr container) {
    TypePtr result = withMutability(container->type, false);

    return builtinNode(Builtin::Retype, move(container), {}, move(result));
  }

  ExprPtr Thaw(ExprPtr container) {
    TypePtr result = withMutability(container->type, true);

    return builtinNode(Builtin::Retype, move(container), {}, move(result));
  }

  ExprPtr With(ExprPtr collection, Vector<ExprPtr> arguments) {
    const TypePtr t = collection->type;
    requireWithable(t);
    requireWithCount(t, arguments);
    requireWithTypes(t, arguments);

    return builtinNode(Builtin::With, move(collection), move(arguments), t);
  }

  ExprPtr VariantLit(TypePtr type, int caseIndex, ExprPtr value) {
    RequireKind(type, TypeKind::Variant, "a variant value needs a variant type");
    requireCase(type, caseIndex);
    RequireSame(type->parameters[static_cast<size_t>(caseIndex)], value->type, "variant case " + to_string(caseIndex));

    auto expr = MakeExpr(ExprKind::VariantLit, move(type));
    expr->fieldIndex = caseIndex;
    expr->operands = {move(value)};

    return expr;
  }

  ExprPtr VariantTag(ExprPtr variant) {
    RequireKind(variant->type, TypeKind::Variant, "tag needs a variant");

    return builtinNode(Builtin::VariantTag, move(variant), {}, i64Type());
  }

  ExprPtr VariantIsCase(ExprPtr variant, int caseIndex) {
    RequireKind(variant->type, TypeKind::Variant, "isCase needs a variant");

    return builtinNode(Builtin::VariantIsCase, move(variant), {constant(caseIndex)}, MakeBool());
  }

  ExprPtr VariantUnwrap(ExprPtr variant, int caseIndex) {
    RequireKind(variant->type, TypeKind::Variant, "unwrapAt needs a variant");
    requireCase(variant->type, caseIndex);
    TypePtr result = variant->type->parameters[static_cast<size_t>(caseIndex)];

    return builtinNode(Builtin::VariantUnwrap, move(variant), {constant(caseIndex)}, move(result));
  }

  static ExprPtr eitherNode(bool isOk, const TypePtr& either, ExprPtr value) {
    RequireKind(either, TypeKind::Either, "ok and err make an either");
    RequireSame(isOk ? either->element : either->second, value->type, isOk ? "ok" : "err");
    auto expr = MakeExpr(ExprKind::EitherLit, either);
    expr->boolValue = isOk;
    expr->operands = {move(value)};

    return expr;
  }

  ExprPtr Ok(TypePtr either, ExprPtr value) {
    return eitherNode(true, either, move(value));
  }

  ExprPtr Err(TypePtr either, ExprPtr value) {
    return eitherNode(false, either, move(value));
  }

  // A builtin that asks an either which side it holds and gives a flag.
  static ExprPtr eitherTest(Builtin op, ExprPtr either) {
    RequireKind(either->type, TypeKind::Either, "this needs an either");

    return builtinNode(op, move(either), {}, MakeBool());
  }

  ExprPtr IsOk(ExprPtr either) {
    return eitherTest(Builtin::EitherIsOk, move(either));
  }

  ExprPtr IsErr(ExprPtr either) {
    return eitherTest(Builtin::EitherIsErr, move(either));
  }

  ExprPtr UnwrapOk(ExprPtr either) {
    RequireKind(either->type, TypeKind::Either, "this needs an either");
    TypePtr result = either->type->element;

    return builtinNode(Builtin::EitherUnwrapOk, move(either), {}, move(result));
  }

  ExprPtr UnwrapErr(ExprPtr either) {
    RequireKind(either->type, TypeKind::Either, "this needs an either");
    TypePtr result = either->type->second;

    return builtinNode(Builtin::EitherUnwrapErr, move(either), {}, move(result));
  }

  ExprPtr Some(ExprPtr value) {
    auto expr = MakeExpr(ExprKind::EitherLit, MakeOptional(value->type));
    expr->boolValue = true;
    expr->operands = {move(value)};

    return expr;
  }

  // An optional is stored as an either; `none` still needs a payload, so it carries a dummy `false`.
  ExprPtr None(TypePtr element) {
    auto expr = MakeExpr(ExprKind::EitherLit, MakeOptional(move(element)));
    expr->boolValue = false;
    expr->operands = {BoolConst(false)};

    return expr;
  }

  // A builtin that reads an optional and gives `result` (a flag, or the value it holds).
  static ExprPtr optionRead(Builtin op, ExprPtr optional, bool givesFlag) {
    RequireKind(optional->type, TypeKind::Optional, "this needs an optional");
    TypePtr result = givesFlag ? MakeBool() : optional->type->element;

    return builtinNode(op, move(optional), {}, move(result));
  }

  ExprPtr IsSome(ExprPtr optional) {
    return optionRead(Builtin::OptionIsSome, move(optional), true);
  }

  ExprPtr IsNone(ExprPtr optional) {
    return optionRead(Builtin::OptionIsNone, move(optional), true);
  }

  ExprPtr Unwrap(ExprPtr optional) {
    return optionRead(Builtin::OptionUnwrap, move(optional), false);
  }

  ExprPtr InterfaceBox(const PolyIRBuilder& builder, const String& interfaceName, ExprPtr structValue) {
    const InterfaceDef& interfaceDef = requireInterface(builder.module, interfaceName);
    RequireKind(structValue->type, TypeKind::Struct, "only a struct is boxed as an interface value");
    requireImplements(interfaceDef, structValue->type, interfaceName);

    return builtinNode(Builtin::InterfaceBox, move(structValue), {}, MakeStruct(interfaceName));
  }

  ExprPtr InterfaceCall(const PolyIRBuilder& builder, ExprPtr object, const String& method, Vector<ExprPtr> arguments) {
    const InterfaceDef& interfaceDef = requireInterfaceValue(builder.module, object->type);
    const size_t slot = slotOf(interfaceDef, method);
    const InterfaceMethod& methodDef = interfaceDef.methods[slot];
    requireMethodArguments(methodDef, method, arguments);
    // The call carries the slot as its first argument, so the emitter knows which vtable entry to call.
    arguments.insert(arguments.begin(), constant(static_cast<int64_t>(slot)));

    return builtinNode(Builtin::InterfaceCall, move(object), move(arguments), methodDef.result);
  }

  ExprPtr Resume(ExprPtr coroutine) {
    RequireKind(coroutine->type, TypeKind::Coroutine, "resume needs a coroutine");
    TypePtr result = MakeVariant({coroutine->type->element});  // case 0 holds the yielded value, case 1 means finished

    return builtinNode(Builtin::CoroutineResume, move(coroutine), {}, move(result));
  }

  ExprPtr Done(ExprPtr coroutine) {
    RequireKind(coroutine->type, TypeKind::Coroutine, "done needs a coroutine");

    return builtinNode(Builtin::CoroutineDone, move(coroutine), {}, MakeBool());
  }

  ExprPtr Yielded(ExprPtr value) {
    return builtinNode(Builtin::CoroutineYield, move(value), {}, MakeBool());
  }

  ExprPtr CloseCoroutine(ExprPtr coroutine) {
    RequireKind(coroutine->type, TypeKind::Coroutine, "close needs a coroutine");

    return builtinNode(Builtin::CoroutineClose, move(coroutine), {}, MakeVoid());
  }

  ExprPtr BuiltinCall(Builtin op, ExprPtr receiver, Vector<ExprPtr> arguments, TypePtr result) {
    return builtinNode(op, move(receiver), move(arguments), move(result));
  }

}  // namespace polyir
