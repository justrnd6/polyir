// A type as a construct of a page's `## types` section (Nodes.hpp): `i64`, `string`, `array(elem)`, `map(key, value)`. A copy of
// a generic written once is `instance(name, args)` (`Box<i64>`), and a type parameter `param(name)`.
#include "translate/Nodes.hpp"

namespace polyir::translate {

  namespace {

    Node typeNamed(const String& construct, const TypePtr& type) {
      Node node = MakeNode(construct);
      node.isType = true;
      node.note = type;

      return node;
    }

    Node withParts(NodeContext& context, const String& construct, const TypePtr& type, const Vector<Pair<String, TypePtr>>& parts) {
      Node node = typeNamed(construct, type);

      for (const auto& [name, part] : parts) {
        AddNode(node, name, PartKind::Type, TypeNode(context, part));
      }

      return node;
    }

    // `vec(elem, size)`, `grid(elem, rows, cols)`: an element type and sizes as text.
    Node sizedType(NodeContext& context, const String& construct, const TypePtr& type, const Vector<Pair<String, String>>& sizes) {
      Node node = withParts(context, construct, type, {{"elem", type->element}});

      for (const auto& [name, size] : sizes) {
        AddText(node, name, PartKind::Text, size);
      }

      return node;
    }

    String dimsText(const Vector<int>& dims) {
      String text;

      for (const int dim : dims) {
        text += (text.empty() ? "" : ", ") + to_string(dim);
      }

      return text;
    }

    // A type the host language has, by the target's own binding: `host(name, binding)`, with `args` for one made with types.
    Node hostType(NodeContext& context, const TypePtr& type, const HostType& host, const String& binding) {
      Node node = typeNamed("host", type);
      AddText(node, "name", PartKind::Name, RelativeName(context, host.base.empty() ? host.name : host.base));
      AddText(node, "binding", PartKind::Text, binding);

      if (!host.typeArguments.empty()) {
        AddNodes(node, "args", PartKind::Type, TypeNodes(context, host.typeArguments));
      }

      return node;
    }

    // A struct; a copy of a generic written once is `instance(name, args)`, and so is the generic as declared (its args `param`s).
    Node structType(NodeContext& context, const TypePtr& type) {
      String binding;

      if (const HostType* host = BoundHostType(context, type->name, binding)) {
        return hostType(context, type, *host, binding);
      }

      const StructDef* def = AnyStruct(*context.module, type->name);
      const bool written = def != nullptr && context.generics.structs.count(def->generic.of) > 0;
      Node node = typeNamed(written ? "instance" : "struct", type);
      AddText(node, "name", PartKind::Name, RelativeName(context, written ? def->generic.of : type->name));

      // An interface's values are of any struct implementing it (Swift writes `any Shape`).
      if (FindInterface(*context.module, type->name) != nullptr) {
        AddFact(node, "isInterface");
      }

      if (written) {
        AddNodes(node, "args", PartKind::Type, TypeNodes(context, def->generic.arguments));
      }

      return node;
    }

    Node collectionType(NodeContext& context, const TypePtr& type) {
      const String prefix = type->isMutable ? "mut" : "";

      switch (type->kind) {
        case TypeKind::DynamicArray:
          return withParts(context, prefix + "array", type, {{"elem", type->element}});
        case TypeKind::Hashmap:
          return withParts(context, prefix + "map", type, {{"key", type->element}, {"value", type->second}});
        default:
          return withParts(context, prefix + "set", type, {{"elem", type->element}});
      }
    }

    // `closure(params, result)` and `func(params, result)`.
    Node calledType(NodeContext& context, const String& construct, const TypePtr& type) {
      Node node = typeNamed(construct, type);
      AddNodes(node, "params", PartKind::Type, TypeNodes(context, type->parameters));
      AddNode(node, "result", PartKind::Type, TypeNode(context, type->element));
      AddText(node, "arity", PartKind::Fact, to_string(type->parameters.size()));

      return node;
    }

    Node valueType(NodeContext& context, const TypePtr& type) {
      switch (type->kind) {
        case TypeKind::Optional:
          return withParts(context, "optional", type, {{"elem", type->element}});
        case TypeKind::Either:
          return withParts(context, "either", type, {{"ok", type->element}, {"err", type->second}});
        case TypeKind::Pointer:
          return withParts(context, "ptr", type, {{"elem", type->element}});
        case TypeKind::Coroutine:
          return withParts(context, "coro", type, {{"elem", type->element}});
        case TypeKind::Sparse:
          return withParts(context, "sparse", type, {{"elem", type->element}});
        default:
          return typeNamed("void", type);
      }
    }

    Node shapedType(NodeContext& context, const TypePtr& type) {
      switch (type->kind) {
        case TypeKind::Vec:
          return sizedType(context, "vec", type, {{"size", to_string(type->size)}});
        case TypeKind::Matrix:
          return sizedType(context, "matrix", type, {{"size", to_string(type->size)}});
        case TypeKind::FixedArray:
          return sizedType(context, "fixed", type, {{"size", to_string(type->size)}});
        case TypeKind::Buffer:
          return sizedType(context, "buffer", type, {{"size", to_string(type->size)}});
        case TypeKind::Grid:
          return sizedType(context, "grid", type, {{"rows", to_string(type->size)}, {"cols", to_string(type->cols)}});
        case TypeKind::Tensor:
          return sizedType(context, "tensor", type, {{"dims", dimsText(type->dims)}});
        default:
          return valueType(context, type);
      }
    }

    Node variantType(NodeContext& context, const TypePtr& type) {
      Node node = typeNamed("variant", type);
      AddNodes(node, "cases", PartKind::Type, TypeNodes(context, type->parameters));

      return node;
    }

    Node compoundType(NodeContext& context, const TypePtr& type) {
      switch (type->kind) {
        case TypeKind::Struct:
          return structType(context, type);
        case TypeKind::DynamicArray:
        case TypeKind::Hashmap:
        case TypeKind::Set:
          return collectionType(context, type);
        case TypeKind::Closure:
          return calledType(context, "closure", type);
        case TypeKind::Function:
          return calledType(context, "func", type);
        default:
          return type->kind == TypeKind::Variant ? variantType(context, type) : shapedType(context, type);
      }
    }

    // `i64`, `u8`, `bool`, `float`, `double`: a number type by its .poly name.
    Node numberType(const TypePtr& type) {
      if (type->kind == TypeKind::Float) {
        return typeNamed(type->bits == 32 ? "float" : "double", type);
      }

      const String name = type->kind == TypeKind::UInt ? "u" + to_string(type->bits) : type->bits == 1 ? "bool" : "i" + to_string(type->bits);

      return typeNamed(name, type);
    }

    Node paramType(const TypePtr& type) {
      Node node = typeNamed("param", type);
      AddText(node, "name", PartKind::Name, type->name);

      return node;
    }

  }  // namespace

  Node TypeNode(NodeContext& context, const TypePtr& type) {
    if (!type || type->kind == TypeKind::Void) {
      return typeNamed("void", type ? type : MakeVoid());
    }

    switch (type->kind) {
      case TypeKind::Param:
        return paramType(type);
      case TypeKind::Int:
      case TypeKind::UInt:
      case TypeKind::Float:
        return numberType(type);
      case TypeKind::String:
        return typeNamed("string", type);
      default:
        return compoundType(context, type);
    }
  }

  Vector<Node> TypeNodes(NodeContext& context, const Vector<TypePtr>& types) {
    Vector<Node> nodes;

    for (const TypePtr& type : types) {
      nodes.push_back(TypeNode(context, type));
    }

    return nodes;
  }

}  // namespace polyir::translate
