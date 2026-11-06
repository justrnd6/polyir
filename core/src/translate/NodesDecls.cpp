// The program's declarations as constructs (Nodes.hpp): each module once, holding its declarations in the order they were written;
// a struct with its fields, methods, static functions and the structs declared in it; a generic written once as declared.
#include "translate/Nodes.hpp"

namespace polyir::translate {

  namespace {

    // A module's items, and the modules declared in it, kept in the order each first appears.
    struct ModuleBlock {
      Vector<Node> items;
      Vector<String> inner;   // the paths of the modules inside it, in order
      Vector<size_t> places;  // where in `items` each inner module goes
    };

    using Blocks = Map<String, ModuleBlock>;

    Node functionNode(NodeContext& context, const FuncDef& def, const String& construct, size_t from);

    Node structNode(NodeContext& context, const StructDef& def);

    // ── parts shared by declarations ──

    Node paramNode(NodeContext& context, const String& name, const TypePtr& type) {
      Node node = MakeNode("param");
      AddText(node, "name", PartKind::Name, name);
      AddNode(node, "type", PartKind::Type, TypeNode(context, type));

      return node;
    }

    // The parameters from `from` on (1 leaves out a method's `this`).
    Vector<Node> paramNodes(NodeContext& context, const Vector<Parameter>& parameters, size_t from) {
      Vector<Node> nodes;

      for (size_t i = from; i < parameters.size(); i++) {
        nodes.push_back(paramNode(context, parameters[i].name, parameters[i].type));
      }

      return nodes;
    }

    Vector<Node> typeParamNodes(NodeContext& context, const Generic& generic) {
      Vector<Node> nodes;

      for (const String& param : generic.params) {
        nodes.push_back(TypeNode(context, MakeParam(param)));
      }

      return nodes;
    }

    // `@Doc(text: "a point")` as `annotation(name, args)`, each argument `arg(name, value)`.
    Node annotationNode(const Annotation& annotation) {
      Node node = MakeNode("annotation");
      AddText(node, "name", PartKind::Name, annotation.name);
      Vector<Node> args;

      for (const auto& [name, value] : annotation.args) {
        Node arg = MakeNode("arg");
        AddText(arg, "name", PartKind::Name, name);
        AddText(arg, "value", PartKind::Text, value);
        args.push_back(arg);
      }

      AddNodes(node, "args", PartKind::List, args);

      return node;
    }

    void addAnnotations(Node& node, const Vector<Annotation>& annotations) {
      Vector<Node> nodes;

      for (const Annotation& annotation : annotations) {
        nodes.push_back(annotationNode(annotation));
      }

      if (!nodes.empty()) {
        AddNodes(node, "annotations", PartKind::Fact, nodes);
      }
    }

    // A declaration's annotations, and the fact `internal` when only the code around it uses it.
    void addPlaceFacts(Node& node, const Vector<Annotation>& annotations, bool internal) {
      addAnnotations(node, annotations);

      if (internal) {
        AddFact(node, "internal");
      }
    }

    // ── functions ──

    Vector<String> parameterNames(const FuncDef& def) {
      Vector<String> names;

      for (const Parameter& parameter : def.parameters) {
        names.push_back(parameter.name);
      }

      return names;
    }

    // What a coroutine yields: `coro<T>`'s T.
    TypePtr yieldsOf(const FuncDef& def) {
      const bool yields = def.result && def.result->kind == TypeKind::Coroutine;

      return yields ? def.result->element : def.result;
    }

    // The body's statements, made knowing the function and what its `return` (or a coroutine's `yield`) gives.
    Vector<Node> bodyNodes(NodeContext& context, const FuncDef& def) {
      const StmtList* around = context.function;
      const TypePtr result = context.result;
      context.function = &def.body;
      context.result = def.isCoroutine ? yieldsOf(def) : def.result;
      context.boxed = BoxedNames(def.body);

      const Vector<String> params = parameterNames(def);
      context.guarded = CountTries(def.body) > 0;
      context.inCoroutine = def.isCoroutine;
      context.capturedHere = def.isCoroutine ? Set<String>(params.begin(), params.end()) : Set<String>{};
      StartFunction(context, params);

      Vector<Node> body = HasLabels(def.body) ? FlowNodes(context, def.body) : StatementNodes(context, def.body);
      context.function = around;
      context.result = result;

      return body;
    }

    // `func`, `method` or `static`; a coroutine is `coro` and a kernel `kernel`, whatever it is in.
    String constructOf(const FuncDef& def, const String& construct) {
      if (def.stage != KernelStage::None) {
        return "kernel";
      }

      return def.isCoroutine ? "coro" : construct;
    }

    // Whether method `name` of `owner` is one of an interface it implements (Kotlin writes `override` before it).
    bool implementsMethod(const Module& module, const StructDef& owner, const String& name) {
      for (const InterfaceDef& interface : module.interfaces) {
        const bool named = any_of(interface.methods.begin(), interface.methods.end(), [&](const InterfaceMethod& method) { return method.name == name; });

        if (named && Implements(module, owner, interface)) {
          return true;
        }
      }

      return false;
    }

    // The facts of a function: the globals it changes, its annotations, whether it implements an interface's method and whether it
    // can throw. A member's `owner` is its struct by its whole path, as C++ defines a method as `Point::size`.
    void addFunctionFacts(NodeContext& context, Node& node, const FuncDef& def, const StructDef* owner, const String& name) {
      AddNameFact(node, "globals", AssignedNames(def.body, true));
      addPlaceFacts(node, def.annotations, def.internal);

      if (owner != nullptr && implementsMethod(*context.module, *owner, name)) {
        AddFact(node, "overrides");
      }

      if (context.throwing.count(def.name) > 0) {
        AddFact(node, "throws");
      }

      const String ownerName = owner != nullptr ? owner->name : def.outer;
      AddNameFact(node, "owner", ownerName.empty() ? Vector<String>{} : Vector<String>{ownerName});
    }

    // The function's body, made knowing what its `return` (a coroutine's `yield`) gives and what it can throw.
    Node functionBody(NodeContext& context, const FuncDef& def) {
      const TypePtr result = def.isCoroutine ? yieldsOf(def) : def.result;
      const bool throws = context.throwing.count(def.name) > 0;

      return BodyNode(context, bodyNodes(context, def), result, AssignedNames(def.body, true), {}, throws);
    }

    // Where the function came from: its statements' spans are read in its own file (an imported one's, when it has an origin).
    void addFunctionSpan(NodeContext& context, Node& node, const FuncDef& def) {
      context.spanFile = def.origin;
      AddSpan(context, node, def.at);
    }

    Node functionNode(NodeContext& context, const FuncDef& def, const String& construct, size_t from) {
      Node node = MakeNode(constructOf(def, construct));
      const StructDef* owner = OwnerOf(*context.module, def);
      addFunctionSpan(context, node, def);

      if (node.construct == "kernel") {
        AddText(node, "stage", PartKind::Text, StageName(def.stage));
      }

      const String name = owner != nullptr ? MethodName(def, *owner) : NameDeclared(context, DeclaredName(context, def.name, def.generic));
      AddText(node, "name", PartKind::Name, name);

      // A function's type parameters, none for one that is not generic, so `function {name}<{typeParams}>` writes both.
      if (node.construct == "func" && owner == nullptr) {
        AddNodes(node, "typeParams", PartKind::Type, typeParamNodes(context, def.generic));
      }

      AddNodes(node, "params", PartKind::List, paramNodes(context, def.parameters, from));
      AddNode(node, node.construct == "coro" ? "yields" : "result", PartKind::Type, TypeNode(context, node.construct == "coro" ? yieldsOf(def) : def.result));
      AddNodes(node, "body", PartKind::Block, OneNode(functionBody(context, def)));
      addFunctionFacts(context, node, def, owner, name);

      return node;
    }

    // ── structs ──

    Node fieldNode(NodeContext& context, const Field& field) {
      Node node = MakeNode("field");
      AddText(node, "name", PartKind::Name, field.name);
      AddNode(node, "type", PartKind::Type, TypeNode(context, field.type));

      if (field.initial) {
        AddNode(node, "value", PartKind::Value, ValueFor(context, field.initial, field.type));
      }

      if (field.isMutable) {
        AddFact(node, "isMutable");
      }

      return node;
    }

    // The interfaces the struct says it implements, as types.
    Vector<Node> implementsNodes(NodeContext& context, const StructDef& def) {
      Vector<Node> nodes;

      for (const InterfaceDef& interface : context.module->interfaces) {
        if (Implements(*context.module, def, interface)) {
          nodes.push_back(TypeNode(context, MakeStruct(interface.name)));
        }
      }

      return nodes;
    }

    // The methods, static functions and nested structs, made inside the struct: names read from there.
    void addMembers(NodeContext& context, Node& node, const StructDef& def) {
      Vector<Node> methods;
      Vector<Node> statics;
      Vector<Node> nested;

      for (const FuncDef* method : MethodsOf(*context.module, def)) {
        methods.push_back(functionNode(context, *method, "method", 1));
      }

      for (const FuncDef* function : StaticsOf(*context.module, def, true)) {
        statics.push_back(functionNode(context, *function, "static", 0));
      }

      for (const StructDef* inner : NestedIn(*context.module, def, true)) {
        nested.push_back(structNode(context, *inner));
      }

      AddNodes(node, "methods", PartKind::Block, methods);
      AddNodes(node, "nested", PartKind::Block, nested);
      AddNodes(node, "statics", PartKind::Block, statics);
    }

    void addFields(NodeContext& context, Node& node, const StructDef& def) {
      Vector<Node> fields;

      for (const Field& field : def.fields) {
        fields.push_back(fieldNode(context, field));
      }

      AddNodes(node, "fields", PartKind::List, fields);
    }

    bool isThis(const ExprPtr& e) {
      return e && (e->kind == ExprKind::Local || e->kind == ExprKind::LoadSlot) && e->name == "this";
    }

    // Whether a method of `def` hands `this` on (returns it, passes it, stores it), not only reads its fields or calls its methods.
    bool sharesThis(const Module& module, const StructDef& def) {
      size_t uses = 0;
      size_t receivers = 0;

      for (const FuncDef* method : MethodsOf(module, def)) {
        CountIn(method->body, [&](const Expr& e) {
          const bool receives = (e.kind == ExprKind::FieldGet || e.kind == ExprKind::Call) && !e.operands.empty() && isThis(e.operands[0]);
          uses += (e.kind == ExprKind::Local || e.kind == ExprKind::LoadSlot) && e.name == "this" ? 1 : 0;
          receivers += receives ? 1 : 0;

          return false;
        });
      }

      return uses > receivers;
    }

    // The interfaces it implements, whether it is a map's key, whether a field is `var`, and its annotations.
    void addStructFacts(NodeContext& context, Node& node, const StructDef& def) {
      const Vector<Node> implemented = implementsNodes(context, def);

      if (!implemented.empty()) {
        AddNodes(node, "implements", PartKind::Type, implemented);
      }

      if (context.keyStructs.count(def.name) > 0) {
        AddFact(node, "isKey");
      }

      if (any_of(def.fields.begin(), def.fields.end(), [](const Field& field) { return field.isMutable; })) {
        AddFact(node, "isMutable");
      }

      if (sharesThis(*context.module, def)) {
        AddFact(node, "sharesThis");
      }

      addPlaceFacts(node, def.annotations, def.internal);
    }

    // The struct's methods that implement one of `face`'s, by their names.
    Vector<Node> methodsOf(const Node& node, const InterfaceDef& face) {
      Vector<Node> methods;
      const NodePart* all = PartNamed(node, "methods");

      for (const Node& method : all != nullptr ? all->nodes : Vector<Node>{}) {
        const String name = PartNamed(method, "name")->text;
        const bool implements = any_of(face.methods.begin(), face.methods.end(), [&](const InterfaceMethod& m) { return m.name == name; });

        if (implements) {
          methods.push_back(method);
        }
      }

      return methods;
    }

    // The fact `traits`: `trait(name, methods)` for each interface the struct implements, with the methods that implement it. A
    // target that implements each interface apart uses it (Rust's `impl Shape for ...`).
    void addTraits(NodeContext& context, Node& node, const StructDef& def) {
      Vector<Node> traits;

      for (const InterfaceDef& face : context.module->interfaces) {
        if (Implements(*context.module, def, face)) {
          traits.push_back(MakeNode("trait"));
          AddText(traits.back(), "name", PartKind::Name, RelativeName(context, face.name));
          AddNodes(traits.back(), "methods", PartKind::Block, methodsOf(node, face));
          AddNameFact(traits.back(), "owner", {def.name});
        }
      }

      if (!traits.empty()) {
        AddNodes(node, "traits", PartKind::Fact, traits);
      }
    }

    Node structNode(NodeContext& context, const StructDef& def) {
      Node node = MakeNode("struct");
      AddText(node, "name", PartKind::Name, NameDeclared(context, DeclaredName(context, def.name, def.generic)));
      AddNodes(node, "typeParams", PartKind::Type, typeParamNodes(context, def.generic));

      const String around = context.scope;
      context.scope = def.name;
      addFields(context, node, def);
      addMembers(context, node, def);
      addTraits(context, node, def);

      context.scope = around;

      addStructFacts(context, node, def);

      return node;
    }

    // ── other declarations ──

    // `p0, p1, ...`: the parameters of an interface method, which the HIR keeps no names for.
    Vector<Node> argumentNames(size_t count) {
      Vector<Node> nodes;

      for (size_t i = 0; i < count; i++) {
        nodes.push_back(MakeNode("name"));
        AddText(nodes.back(), "name", PartKind::Name, "p" + to_string(i));
      }

      return nodes;
    }

    // `implementer(name, method, args)` for each struct implementing the interface. A target with no dispatch of its own (C) calls
    // the struct's method by its name, with the arguments `p0, p1`.
    Vector<Node> implementerNodes(NodeContext& context, const InterfaceDef& face, const InterfaceMethod& method) {
      Vector<Node> nodes;

      for (const StructDef& def : context.module->structs) {
        if (!Implements(*context.module, def, face)) {
          continue;
        }

        Node node = MakeNode("implementer");
        AddText(node, "name", PartKind::Name, def.name);
        AddText(node, "method", PartKind::Name, method.name);
        AddNodes(node, "args", PartKind::List, argumentNames(method.parameters.size()));
        nodes.push_back(node);
      }

      return nodes;
    }

    // `signature(name, params, result)`, with the fact `throws` when an implementation can throw.
    Node signatureNode(NodeContext& context, const InterfaceDef& def, const InterfaceMethod& method) {
      Node signature = MakeNode("signature");
      AddText(signature, "name", PartKind::Name, method.name);
      Vector<Node> params;

      for (size_t i = 0; i < method.parameters.size(); i++) {
        params.push_back(paramNode(context, "p" + to_string(i), method.parameters[i]));
      }

      AddNodes(signature, "params", PartKind::List, params);
      AddNode(signature, "result", PartKind::Type, TypeNode(context, method.result));

      if (context.throwing.count(def.name + "." + method.name) > 0) {
        AddFact(signature, "throws");
      }

      AddNodes(signature, "implementers", PartKind::Fact, implementerNodes(context, def, method));
      AddNameFact(signature, "owner", {def.name});

      return signature;
    }

    // `interface Shape { func area() -> double }`: each method's parameters are numbered, as the HIR keeps no names for them.
    Node interfaceNode(NodeContext& context, const InterfaceDef& def) {
      Node node = MakeNode("interface");
      AddText(node, "name", PartKind::Name, NameDeclared(context, def.name));
      Vector<Node> methods;

      for (const InterfaceMethod& method : def.methods) {
        methods.push_back(signatureNode(context, def, method));
      }

      AddNodes(node, "methods", PartKind::Block, methods);
      addPlaceFacts(node, def.annotations, def.internal);

      return node;
    }

    // An interface the target's host language has (`extern interface Runnable = java "java.lang.Runnable"`) is not declared again.
    void addInterface(NodeContext& context, ModuleBlock& block, const InterfaceDef& def) {
      String binding;

      if (BoundHostType(context, def.name, binding) == nullptr) {
        block.items.push_back(interfaceNode(context, def));
      }
    }

    // Whether the target reaches extern `name` through a host binding of its own (`js=...`): then it declares nothing.
    bool isBound(const NodeContext& context, const String& name) {
      return any_of(context.module->hostMembers.begin(), context.module->hostMembers.end(),
                    [&](const HostMember& member) { return member.function == name && BindingIn(context, member.bindings); });
    }

    Node externNode(NodeContext& context, const ExternDef& def) {
      Node node = MakeNode("extern");
      AddText(node, "name", PartKind::Name, def.name);
      AddNodes(node, "params", PartKind::List, paramNodes(context, def.parameters, 0));
      AddNode(node, "result", PartKind::Type, TypeNode(context, def.result));

      return node;
    }

    Node globalNode(NodeContext& context, const GlobalDef& def) {
      Node node = MakeNode("global");
      AddText(node, "name", PartKind::Name, def.name);
      AddNode(node, "type", PartKind::Type, TypeNode(context, def.type));
      AddNode(node, "value", PartKind::Value, ValueFor(context, def.initializer, def.type));

      return node;
    }

    // `import js "crc-32" as crc`: `import(module, alias)`.
    Node importNode(const HostImport& import) {
      Node node = MakeNode("import");
      AddText(node, "module", PartKind::Text, import.module);

      if (!import.alias.empty()) {
        AddText(node, "alias", PartKind::Text, import.alias);
      }

      return node;
    }

    // The target's imports, the externs and the globals, written first.
    // The externs the file's own code calls, each declared in every file that calls it.
    void addExterns(NodeContext& context, Vector<Node>& nodes) {
      const Set<String> named = OwnNames(context);

      for (const ExternDef& def : context.module->externs) {
        if (!isBound(context, def.name) && named.count(def.name) > 0) {
          nodes.push_back(externNode(context, def));
        }
      }
    }

    Vector<Node> headNodes(NodeContext& context) {
      Vector<Node> nodes;

      for (const HostImport& import : context.module->hostImports) {
        if (find(context.bindingWords.begin(), context.bindingWords.end(), import.target) != context.bindingWords.end()) {
          nodes.push_back(importNode(import));
        }
      }

      addExterns(context, nodes);

      for (const GlobalDef& def : context.module->globals) {
        if (def.origin.empty()) {
          nodes.push_back(globalNode(context, def));
        }
      }

      return nodes;
    }

    // ── the modules ──

    Vector<String> pathParts(const String& path) {
      Vector<String> parts;
      IStringStream in(path);

      for (String part; getline(in, part, '.');) {
        parts.push_back(part);
      }

      return parts;
    }

    // The module at `path`, and each module around it, noted in its outer module the first time it is seen.
    ModuleBlock& blockAt(Blocks& blocks, const String& path) {
      String outer;

      for (const String& part : pathParts(path)) {
        const String inner = outer.empty() ? part : outer + "." + part;
        ModuleBlock& around = blocks[outer];

        if (blocks.count(inner) == 0) {
          around.inner.push_back(inner);
          around.places.push_back(around.items.size());
        }

        blocks[inner];
        outer = inner;
      }

      return blocks[path];
    }

    // A module's items, its inner modules each as a `module(name, items)` where it first appeared.
    Vector<Node> itemsOf(Blocks& blocks, const String& path) {
      ModuleBlock& block = blocks[path];
      Vector<Node> items;

      for (size_t i = 0, next = 0; i <= block.items.size(); i++) {
        for (; next < block.inner.size() && block.places[next] == i; next++) {
          Node module = MakeNode("module");
          AddText(module, "name", PartKind::Name, OwnName(block.inner[next]));
          AddNodes(module, "items", PartKind::Block, itemsOf(blocks, block.inner[next]));
          items.push_back(module);
        }

        if (i < block.items.size()) {
          items.push_back(move(block.items[i]));
        }
      }

      return items;
    }

    // The generic a declaration is a copy of, when that generic is written once ("" for any other).
    String writtenGenericOf(const NodeContext& context, const DeclRef& ref) {
      const Module& module = *context.module;

      if (ref.kind == DeclKind::Struct) {
        const String& of = module.structs[ref.index].generic.of;

        return context.generics.structs.count(of) > 0 ? of : "";
      }

      const FuncDef* def = ref.kind == DeclKind::Function ? &module.functions[ref.index] : nullptr;
      const bool written = def != nullptr && OwnerOf(module, *def) == nullptr && context.generics.functions.count(def->generic.of) > 0;

      return written ? def->generic.of : "";
    }

    // Generic `of` as declared, in its module, the first time one of its copies is reached.
    void addGeneric(NodeContext& context, Blocks& blocks, Set<String>& added, const String& of) {
      const auto def = context.generics.structs.find(of);
      const auto function = context.generics.functions.find(of);
      const bool declaredElsewhere = def != context.generics.structs.end() ? !def->second->origin.empty() : !function->second->origin.empty();
      const bool imported = declaredElsewhere || context.provided.count(of) > 0;

      if (!added.insert(of).second || imported) {
        return;
      }

      if (def != context.generics.structs.end()) {
        context.scope = def->second->module;
        return blockAt(blocks, def->second->module).items.push_back(structNode(context, *def->second));
      }

      context.scope = function->second->module;
      blockAt(blocks, function->second->module).items.push_back(functionNode(context, *function->second, "func", 0));
    }

    void addDeclaration(NodeContext& context, Blocks& blocks, Set<String>& added, const DeclRef& ref) {
      if (const String of = writtenGenericOf(context, ref); !of.empty()) {
        return addGeneric(context, blocks, added, of);
      }

      const Optional<String> place = PlaceOf(*context.module, ref);

      if (!place || IsImported(context, ref)) {
        return;
      }

      context.scope = *place;
      ModuleBlock& block = blockAt(blocks, *place);

      if (ref.kind == DeclKind::Struct) {
        block.items.push_back(structNode(context, context.module->structs[ref.index]));
      } else if (ref.kind == DeclKind::Interface) {
        addInterface(context, block, context.module->interfaces[ref.index]);
      } else {
        block.items.push_back(functionNode(context, context.module->functions[ref.index], "func", 0));
      }
    }

    // The structs used as a map's key or a set's element anywhere in the program.
    void noteKeys(NodeContext& context, const TypePtr& type) {
      const bool keyed = type && (type->kind == TypeKind::Hashmap || type->kind == TypeKind::Set);

      if (keyed && type->element && type->element->kind == TypeKind::Struct) {
        context.keyStructs.insert(type->element->name);
      }
    }

    void findKeyStructs(NodeContext& context) {
      for (const FuncDef& function : context.module->functions) {
        CountIn(function.body, [&](const Expr& e) {
          noteKeys(context, e.type);

          return false;
        });
      }

      for (const StructDef& def : context.module->structs) {
        for (const Field& field : def.fields) {
          noteKeys(context, field.type);
        }
      }
    }

    // The declarations in the order they were written, each in its module's block, a generic written once at its first copy.
    Blocks declarationBlocks(NodeContext& context) {
      Blocks blocks;
      Set<String> added;

      for (const DeclRef& ref : DeclarationOrder(*context.module)) {
        addDeclaration(context, blocks, added, ref);
      }

      for (const auto& [of, def] : context.generics.structs) {
        addGeneric(context, blocks, added, of);
      }

      for (const auto& [of, def] : context.generics.functions) {
        addGeneric(context, blocks, added, of);
      }

      return blocks;
    }

    // The names a library shows: its declarations at the top that are not `internal`, and its modules.
    Vector<String> exportedNames(const Vector<Node>& declared) {
      const Set<String> shown = {"func", "coro", "kernel", "struct", "interface", "global", "module"};
      Vector<String> names;

      for (const Node& item : declared) {
        const NodePart* name = PartNamed(item, "name");

        if (shown.count(item.construct) > 0 && name != nullptr && PartNamed(item, "internal") == nullptr) {
          names.push_back(name->text);
        }
      }

      return names;
    }

    // The fact `main` with main's result type, so a rule can ask for it (`main: void`), when the file has its own `main`.
    // A file without one is a library: the facts `library`, and `exports` when it shows any names.
    void addFileFacts(NodeContext& context, Node& program, const Vector<Node>& declared) {
      const Module& module = *context.module;
      const FuncDef* main = FindFunction(module, "main");
      // The file's name is a name like any other, so a target's own word (`Math` in Java) is written apart from it (`Math_`).
      AddNameFact(program, "name", {FileClassName(module, module.file)});

      if (main != nullptr && main->origin.empty()) {
        return AddNode(program, "main", PartKind::Fact, TypeNode(context, main->result ? main->result : MakeVoid()));
      }

      AddFact(program, "library");
      AddNameFact(program, "exports", exportedNames(declared));
    }

  }  // namespace

  Vector<Node> OneNode(Node node) {
    Vector<Node> nodes;
    nodes.push_back(move(node));

    return nodes;
  }

  // One `return` of a value is a body a page may write as an expression.
  Node BodyNode(NodeContext& context, Vector<Node> statements, const TypePtr& result, const Vector<String>& globals,
    const Vector<String>& outer, bool throws) {
    Node node = MakeNode("body");
    const bool expression = statements.size() == 1 && statements[0].construct == "return" && PartNamed(statements[0], "value") != nullptr;
    AddNodes(node, "statements", PartKind::Block, move(statements));
    AddNode(node, "result", PartKind::Type, TypeNode(context, result ? result : MakeVoid()));

    AddNameFact(node, "globals", globals);
    AddNameFact(node, "outer", outer);
    throws ? AddFact(node, "throws") : void();
    expression ? AddFact(node, "expression") : void();

    return node;
  }

  // The uses of imported files, the head (imports, externs, globals), then the declarations in their modules. The `support` part
  // is filled when the rules are known.
  Node ProgramNode(NodeContext& context) {
    findKeyStructs(context);
    context.throwing = ThrowingFunctions(*context.module);
    Blocks blocks = declarationBlocks(context);
    context.scope = "";

    Vector<Node> items = UseNodes(context);
    Vector<Node> head = headNodes(context);
    Vector<Node> declared = itemsOf(blocks, "");
    Node node = MakeNode("program");
    addFileFacts(context, node, declared);

    items.insert(items.end(), make_move_iterator(head.begin()), make_move_iterator(head.end()));
    items.insert(items.end(), make_move_iterator(declared.begin()), make_move_iterator(declared.end()));
    node.parts.insert(node.parts.begin(), NodePart{"items", PartKind::Block, "", move(items)});
    node.parts.insert(node.parts.begin(), NodePart{"support", PartKind::Block, "", {}});

    return node;
  }

}  // namespace polyir::translate
