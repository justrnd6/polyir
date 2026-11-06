// Declarations to HIR (Resolve.hpp): step 5 of the pipeline begins here, before any function body is checked.
//
// The one idea: two passes. This file is the first -- it walks every unit's top-level items in source order and records each struct,
// interface, function signature, extern and global, both in the module being built and in the session's lookup tables. Only then are the
// bodies checked (CheckBody.cpp), which is why a function can call another one declared further down. The first part of the file turns a
// syntax type (`mutarray<i32>`) into a HIR type, one small builder per kind of type.
#include "front/Resolve.hpp"
#include "front/Kernels.hpp"
#include "front/HirBuild.hpp"

namespace polyir::front {

  namespace {

    // ── types ───────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────

    // What resolving a syntax type needs: the session (its type pool and active instance) and the program the type is written in.
    struct TypeResolver {
      Session& session;
      const Program& program;
    };

    TypePtr resolve(TypeResolver& resolver, TypeId id);

    Vector<TypePtr> resolveAll(TypeResolver& resolver, Range<TypeId> ids);

    int sizeAt(const TypeResolver& resolver, const AstType& type, size_t index);

    // The `index`-th size written in a type (`vec<f32, 4>` has the size 4).
    int sizeAt(const TypeResolver& resolver, const AstType& type, size_t index) {
      return static_cast<int>(ParseInt(resolver.program.literals[type.sizes][index].text));
    }

    // A function that makes the HIR type for one kind of syntax type (see kTypeBuilders).
    using TypeBuilder = TypePtr (*)(TypeResolver&, const AstType&);

    TypePtr ptrType(TypeResolver& resolver, const AstType& type) {
      return MakePointer(resolve(resolver, type.first));
    }

    TypePtr vecType(TypeResolver& resolver, const AstType& type) {
      return MakeVec(resolve(resolver, type.first), sizeAt(resolver, type, 0));
    }

    TypePtr arrayType(TypeResolver& resolver, const AstType& type) {
      return MakeDynamicArray(resolve(resolver, type.first), false);
    }

    TypePtr mutarrayType(TypeResolver& resolver, const AstType& type) {
      return MakeDynamicArray(resolve(resolver, type.first), true);
    }

    TypePtr bufferType(TypeResolver& resolver, const AstType& type) {
      return MakeBuffer(resolve(resolver, type.first), sizeAt(resolver, type, 0));
    }

    TypePtr gridType(TypeResolver& resolver, const AstType& type) {
      return MakeGrid(resolve(resolver, type.first), sizeAt(resolver, type, 0), sizeAt(resolver, type, 1));
    }

    TypePtr sparseType(TypeResolver& resolver, const AstType& type) {
      return MakeSparse(resolve(resolver, type.first));
    }

    TypePtr mapType(TypeResolver& resolver, const AstType& type) {
      return MakeHashmap(resolve(resolver, type.first), resolve(resolver, type.second), false);
    }

    TypePtr mutmapType(TypeResolver& resolver, const AstType& type) {
      return MakeHashmap(resolve(resolver, type.first), resolve(resolver, type.second), true);
    }

    TypePtr setType(TypeResolver& resolver, const AstType& type) {
      return MakeSet(resolve(resolver, type.first), false);
    }

    TypePtr mutsetType(TypeResolver& resolver, const AstType& type) {
      return MakeSet(resolve(resolver, type.first), true);
    }

    TypePtr funcType(TypeResolver& resolver, const AstType& type) {
      return MakeFunction(resolveAll(resolver, type.types), resolve(resolver, type.first));
    }

    TypePtr closureType(TypeResolver& resolver, const AstType& type) {
      return MakeClosure(resolveAll(resolver, type.types), resolve(resolver, type.first));
    }

    TypePtr variantType(TypeResolver& resolver, const AstType& type) {
      return MakeVariant(resolveAll(resolver, type.types));
    }

    TypePtr eitherType(TypeResolver& resolver, const AstType& type) {
      return MakeEither(resolve(resolver, type.first), resolve(resolver, type.second));
    }

    TypePtr optionalType(TypeResolver& resolver, const AstType& type) {
      return MakeOptional(resolve(resolver, type.first));
    }

    TypePtr coroutineType(TypeResolver& resolver, const AstType& type) {
      return MakeCoroutine(resolve(resolver, type.first));
    }

    TypePtr tensorType(TypeResolver& resolver, const AstType& type) {
      Vector<int> dims;

      for (size_t i = 0; i < type.sizes.count; i++) {
        dims.push_back(sizeAt(resolver, type, i));
      }

      return MakeTensor(resolve(resolver, type.first), move(dims));
    }

    TypePtr genericType(TypeResolver& resolver, const AstType& type) {
      return StructOf(resolver.session.types, MangledGenericName(resolver.session, resolver.program, type));
    }

    // `char` is `i8` and `string` a pointer-sized string; any other name is a struct. `name[N]` is a fixed array.
    // A type parameter of the instance being resolved stands for its argument; `T[N]` keeps its size only when the argument was a bare name.
    TypePtr argumentType(TypeResolver& resolver, const AstType& type, const Instance& instance, size_t at) {
      if (Empty(type.sizes) || !instance.named[at]) {
        return instance.types[at];
      }

      return MakeFixedArray(instance.types[at], sizeAt(resolver, type, 0));
    }

    TypePtr namedType(TypeResolver& resolver, const AstType& type) {
      const Instance* instance = resolver.session.active;
      const int at = instance == nullptr ? -1 : IndexOf(*instance, type.name.name);

      if (at >= 0) {
        return argumentType(resolver, type, *instance, static_cast<size_t>(at));
      }

      const TypePtr base = Named(resolver.session.types, InScope(resolver.session, type.name.name));

      return Empty(type.sizes) ? base : MakeFixedArray(base, sizeAt(resolver, type, 0));
    }

    // One builder per kind of type, in the order of AstTypeKind, so `resolve` picks the builder by indexing with the kind.
    const TypeBuilder kTypeBuilders[] = {ptrType, vecType, arrayType, mutarrayType, bufferType, gridType, tensorType, sparseType, mapType, mutmapType,
      setType, mutsetType, funcType, closureType, variantType, eitherType, optionalType, coroutineType, genericType, namedType};

    TypePtr resolve(TypeResolver& resolver, TypeId id) {
      const AstType& type = resolver.program.types[id];

      return kTypeBuilders[static_cast<size_t>(type.kind)](resolver, type);
    }

    Vector<TypePtr> resolveAll(TypeResolver& resolver, Range<TypeId> ids) {
      Vector<TypePtr> out;
      out.reserve(ids.count);

      for (const TypeId id : resolver.program.typeRefs[ids]) {
        out.push_back(resolve(resolver, id));
      }

      return out;
    }

    // How a type argument is written in a mangled name: a type parameter of the instance being resolved by its argument's spelling.
    String namedSpelling(Session& session, const AstType& type) {
      const int at = session.active == nullptr ? -1 : IndexOf(*session.active, type.name.name);

      return at < 0 ? Str(session, type.name.name) : Str(session, session.active->spellings[static_cast<size_t>(at)]);
    }

    String mangledText(Session& session, const Program& program, TypeId id) {
      const AstType& type = program.types[id];

      if (type.kind == AstTypeKind::Named) {
        return namedSpelling(session, type);
      }

      if (type.kind == AstTypeKind::Generic) {
        return Str(session, MangledGenericName(session, program, type));
      }

      return "t";
    }

  }  // namespace

  TypePtr ResolveType(Session& session, const Program& program, TypeId id) {
    TypeResolver resolver{session, program};

    return resolve(resolver, id);
  }

  String MangledTypeText(Session& session, const Program& program, TypeId id) {
    return mangledText(session, program, id);
  }

  Symbol MangledGenericName(Session& session, const Program& program, const AstType& type) {
    String name = Str(session, InScope(session, type.name.name));

    for (const TypeId argument : program.typeRefs[type.types]) {
      name += "_" + mangledText(session, program, argument);
    }

    return Intern(session.names, name);
  }

  // ── adding declarations ─────────────────────────────────────────────────────────────────────────────────────────────────────────────────────

  namespace {

    // Appends to one of the module's lists and records the declaration's place in the module's order. A template only because the module keeps
    // one list per kind of declaration.
    template <typename Item>
    size_t appendDecl(Module& module, Vector<Item>& list, Item item, DeclKind kind) {
      list.push_back(move(item));
      module.order.push_back(DeclRef{kind, list.size() - 1});

      return list.size() - 1;
    }

    FuncInfo infoOf(const Vector<Parameter>& parameters, const TypePtr& result) {
      FuncInfo info;
      info.result = result;

      for (const Parameter& parameter : parameters) {
        info.params.push_back(parameter.type);
      }

      return info;
    }

  }  // namespace

  void AddStruct(Session& session, StructDef definition, StructInfo info) {
    appendDecl(session.module, session.module.structs, move(definition), DeclKind::Struct);
    Insert(session.decls.structs, info.name, move(info));
  }

  void AddFunction(Session& session, FuncDef definition, FuncSource source) {
    Insert(session.decls.funcs, source.name, infoOf(definition.parameters, definition.result));
    appendDecl(session.module, session.module.functions, move(definition), DeclKind::Function);
    session.sources.push_back(source);
  }

  void AddExtern(Session& session, Symbol name, ExternDef definition) {
    Insert(session.decls.funcs, name, infoOf(definition.parameters, definition.result));
    appendDecl(session.module, session.module.externs, move(definition), DeclKind::Extern);
  }

  void AddGlobal(Session& session, uint32_t unit, Symbol name, GlobalDef definition) {
    session.globalUnits.push_back(unit);
    Insert(session.decls.globals, name, Binding{name, BindingKind::Global, definition.type});
    appendDecl(session.module, session.module.globals, move(definition), DeclKind::Global);
  }

  void AddInterface(Session& session, InterfaceDef definition, InterfaceInfo info) {
    appendDecl(session.module, session.module.interfaces, move(definition), DeclKind::Interface);
    Insert(session.decls.interfaces, info.name, move(info));
  }

  void AddHostType(Session& session, HostType definition, HostTypeInfo info) {
    appendDecl(session.module, session.module.hostTypes, move(definition), DeclKind::HostType);
    session.decls.hostTypes.push_back(info);
  }

  void AddBridge(Session& session, Bridge definition) {
    appendDecl(session.module, session.module.bridges, move(definition), DeclKind::Bridge);
  }

  void AddHostImport(Session& session, HostImport definition) {
    appendDecl(session.module, session.module.hostImports, move(definition), DeclKind::HostImport);
  }

  void AddHostMember(Session& session, HostMember definition) {
    appendDecl(session.module, session.module.hostMembers, move(definition), DeclKind::HostMember);
  }

  // ── parameters and functions ────────────────────────────────────────────────────────────────────────────────────────────────────────────────

  Vector<Parameter> ParametersOf(DeclContext& context, Range<AstParam> params) {
    Vector<Parameter> out;
    out.reserve(params.count);

    for (const AstParam& param : context.program.params[params]) {
      out.push_back(Parameter{Str(context.session, param.name.name), TypeOf(context, param.type)});
    }

    return out;
  }

  namespace {

    // What a declaration read in an instance unit is to its generic `declared` (`Box`): the types its parameters stand for. In the generic
    // as declared, each parameter stands for itself, and the parameters are kept too. Outside an instance it is nothing.
    Generic genericOf(DeclContext& context, Symbol declared) {
      const Instance* instance = context.session.active;
      Generic made;

      if (instance == nullptr) {
        return made;
      }

      made.of = Str(context.session, declared);
      made.arguments = instance->types;
      bool asDeclared = instance->parametric;

      for (size_t i = 0; i < instance->params.size(); i++) {
        asDeclared = asDeclared && Str(context.session, instance->params[i]) == TypeText(instance->types[i]);
      }

      for (size_t i = 0; asDeclared && i < instance->params.size(); i++) {
        made.params.push_back(Str(context.session, instance->params[i]));
      }

      return made;
    }

    // The imported file a declaration is written in, "" for the checked file's own (file 1) and the library's (file 0).
    String originOf(const DeclContext& context) {
      return context.unit.file > 1 ? context.session.files[context.unit.file] : "";
    }

    // A coroutine's declared type is what it yields, so the function itself returns a handle.
    FuncDef funcDefOf(DeclContext& context, const AstFuncDecl& decl, String name, Vector<Parameter> params, bool isCoroutine) {
      const TypePtr declared = TypeOf(context, decl.returnType);
      const TypePtr result = isCoroutine ? MakeCoroutine(declared) : declared;
      FuncDef definition{.name = move(name), .parameters = move(params), .result = result, .isCoroutine = isCoroutine, .at = SpanAt(decl.name.pos)};

      definition.module = decl.module;
      definition.internal = decl.isInternal;
      definition.outer = decl.outer;
      definition.annotations = AnnotationsOf(context, decl.annotations);
      definition.generic = genericOf(context, decl.name.name);

      definition.origin = originOf(context);

      if (decl.isKernel) {
        MarkKernel(definition, context.program, decl);
      }

      return definition;
    }

    // A top-level `func` or `coro func`, under its own name.
    // A generic function is not declared as written: each call makes a unit of its own for its types (Generics.cpp GenericCall).
    void funcItem(DeclContext& context, uint32_t index, bool isCoroutine) {
      const AstFuncDecl& decl = context.program.funcs.items[index];

      if (!Empty(decl.typeParams) && context.unit.instance == kNoIndex) {
        return;
      }

      const Symbol name = Owner(context, decl.name.name);
      FuncSource source{.unit = context.unitIndex, .decl = FuncId{index}, .name = name, .owner = Symbol(), .isMethod = false};
      FuncDef definition = funcDefOf(context, decl, Str(context.session, name), ParametersOf(context, decl.params), isCoroutine);
      AddFunction(context.session, move(definition), source);
    }

    void plainFunc(DeclContext& context, uint32_t index) {
      funcItem(context, index, false);
    }

    void coroutineFunc(DeclContext& context, uint32_t index) {
      funcItem(context, index, true);
    }

    // A method is an ordinary function named `Struct_method` with `this` prepended, so two structs can each have a method of the same name.
    // Its module is its struct's.
    void declareMethod(DeclContext& context, const AstStructDecl& owning, FuncId id) {
      const Symbol owner = Owner(context, owning.name.name);
      const AstFuncDecl& decl = context.program.funcs[id];
      const Symbol mangled = Mangled(context.session, owner, decl.name.name);
      Vector<Parameter> params{Parameter{"this", StructOf(context.session.types, owner)}};

      for (Parameter& parameter : ParametersOf(context, decl.params)) {
        params.push_back(move(parameter));
      }

      FuncSource source{.unit = context.unitIndex, .decl = id, .name = mangled, .owner = owner, .isMethod = true};
      FuncDef definition = funcDefOf(context, decl, Str(context.session, mangled), move(params), false);
      definition.module = owning.module;
      definition.generic = genericOf(context, owning.name.name);
      AddFunction(context.session, move(definition), source);
    }

    // ── structs ─────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────

    // The fields written in a struct's head, then those in its body.
    Vector<AstField> fieldsOf(const Program& program, const AstStructDecl& decl) {
      Vector<AstField> out;

      for (const AstField& field : program.fields[decl.primary]) {
        out.push_back(field);
      }

      for (const AstMember& member : program.members[decl.members]) {
        if (!member.isMethod) {
          out.push_back(member.field);
        }
      }

      return out;
    }

    // The field's default, or null; a default that is no literal of the field's type is a problem of the declaration.
    ExprPtr defaultOf(DeclContext& context, const AstField& field, const TypePtr& type) {
      String problem;
      ExprPtr value = Valid(field.initial) ? FieldDefault(context.program, field.initial, type, problem) : nullptr;
      const String& file = context.session.files[context.unit.file];

      if (!problem.empty()) {
        context.session.declProblems.push_back(file.empty() ? problem : file + ":" + problem);
      }

      return value;
    }

    void addField(DeclContext& context, StructDef& definition, StructInfo& info, const AstField& field) {
      const TypePtr resolved = TypeOf(context, field.type);
      const ExprPtr initial = defaultOf(context, field, resolved);
      definition.fields.push_back(Field{Str(context.session, field.name.name), resolved, IsVar(field), initial});

      info.fieldNames.push_back(field.name.name);
      info.fieldTypes.push_back(resolved);
      info.fieldMutable.push_back(IsVar(field));
      info.fieldDefaults.push_back(initial);
    }

    void declareStruct(DeclContext& context, const AstStructDecl& decl) {
      StructDef definition;
      StructInfo info;
      info.name = Owner(context, decl.name.name);
      definition.name = Str(context.session, info.name);

      definition.module = decl.module;
      definition.internal = decl.isInternal;
      definition.outer = decl.outer;
      definition.annotations = AnnotationsOf(context, decl.annotations);
      definition.generic = genericOf(context, decl.name.name);

      definition.origin = originOf(context);

      for (const AstField& field : fieldsOf(context.program, decl)) {
        addField(context, definition, info, field);
      }

      AddStruct(context.session, move(definition), move(info));
    }

    // A struct and then its methods. (A generic struct is not declared as written. Monomorphization replaces it with a unit of its own for each
    // use, which is read with the type parameters standing for the arguments.)
    void structItem(DeclContext& context, uint32_t index) {
      const AstStructDecl& decl = context.program.structs[index];

      if (!Empty(decl.typeParams) && context.unit.instance == kNoIndex) {
        return;
      }

      declareStruct(context, decl);

      for (const AstMember& member : context.program.members[decl.members]) {
        if (member.isMethod) {
          declareMethod(context, decl, member.method);
        }
      }
    }

    // ── interfaces ──────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────

    // Whether the struct claims the interface (`struct Circle : Shape`).
    bool declares(const Session& session, const Program& program, const AstStructDecl& decl, Symbol interfaceName) {
      for (const Ident& claim : program.idents[decl.impls]) {
        if (InScopeOf(session, decl.module, claim.name) == interfaceName) {
          return true;
        }
      }

      return false;
    }

    // The name a struct item is declared under: its own, its instance's for a unit made by monomorphization, and none for the generic text itself.
    Optional<Symbol> declaredName(const Session& session, const Unit& unit, const AstStructDecl& decl) {
      if (unit.instance != kNoIndex) {
        return session.instances[unit.instance].name;
      }

      return Empty(decl.typeParams) ? Optional<Symbol>(decl.name.name) : nullopt;
    }

    // The structs of one unit that claim the interface `name`.
    void implementersIn(const Session& session, const Unit& unit, Symbol name, Vector<Symbol>& out) {
      for (uint32_t i = unit.first; i < unit.last; i++) {
        const AstItem& item = unit.program->items[i];

        if (item.kind != ItemKind::Struct) {
          continue;
        }

        const AstStructDecl& decl = unit.program->structs[item.index];
        const Optional<Symbol> declared = declaredName(session, unit, decl);

        if (declared && declares(session, *unit.program, decl, name)) {
          out.push_back(*declared);
        }
      }
    }

    // The structs that wrote `: name` after their own name, in source order.
    Vector<Symbol> implementersOf(DeclContext& context, Symbol name) {
      Vector<Symbol> out;

      for (const Unit& unit : context.session.units) {
        implementersIn(context.session, unit, name, out);
      }

      return out;
    }

    // An interface VALUE is a struct of two pointers: the data and the method table.
    void addInterfaceValue(DeclContext& context, Ident name) {
      const TypePtr bytes = MakePointer(IntType(8));
      StructDef definition{Str(context.session, name.name), {Field{"data", bytes, false}, Field{"vtable", bytes, false}}};
      const Vector<Symbol> fieldNames{Intern(context.session.names, "data"), Intern(context.session.names, "vtable")};
      StructInfo info{.name = name.name, .fieldNames = fieldNames, .fieldTypes = {bytes, bytes}, .fieldMutable = {false, false}};
      AddStruct(context.session, move(definition), move(info));
    }

    void addMethodSignature(DeclContext& context, InterfaceDef& definition, InterfaceInfo& info, const AstInterfaceMethod& method) {
      Vector<TypePtr> params;

      for (const Parameter& parameter : ParametersOf(context, method.params)) {
        params.push_back(parameter.type);
      }

      const TypePtr result = TypeOf(context, method.returnType);
      definition.methods.push_back(InterfaceMethod{Str(context.session, method.name.name), params, result});
      info.methodNames.push_back(method.name.name);
      info.methodParams.push_back(move(params));
      info.methodResults.push_back(result);
    }

    struct InterfaceParts {
      InterfaceDef definition;
      InterfaceInfo info;
    };

    InterfaceParts interfaceParts(DeclContext& context, Ident name, Range<AstInterfaceMethod> methods) {
      InterfaceParts parts;
      parts.definition.name = Str(context.session, name.name);
      parts.info.name = name.name;

      for (const AstInterfaceMethod& method : context.program.interfaceMethods[methods]) {
        addMethodSignature(context, parts.definition, parts.info, method);
      }

      parts.info.implementers = implementersOf(context, name.name);

      for (const Symbol implementer : parts.info.implementers) {
        parts.definition.implementers.push_back(Str(context.session, implementer));
      }

      return parts;
    }

  }  // namespace

  void ResolveInterface(DeclContext& context, Ident name, Range<AstInterfaceMethod> methods) {
    addInterfaceValue(context, name);
    InterfaceParts parts = interfaceParts(context, name, methods);
    AddInterface(context.session, move(parts.definition), move(parts.info));
  }

  namespace {

    void interfaceItem(DeclContext& context, uint32_t index) {
      const AstInterfaceDecl& decl = context.program.interfaces[index];
      ResolveInterface(context, decl.name, decl.methods);

      InterfaceDef& made = context.session.module.interfaces.back();
      made.module = decl.module;
      made.internal = decl.isInternal;
      made.annotations = AnnotationsOf(context, decl.annotations);
      made.origin = originOf(context);
    }

    // ── externs, globals ────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────

    // An `extern func`. One bound to a host spelling (`= java "..."`) is also a static host member with no owner. That way a back end
    // knows how that host spells it.
    void externItem(DeclContext& context, uint32_t index) {
      const AstExternFunc& decl = context.program.externFuncs[index];
      const String name = Str(context.session, decl.name.name);
      AddExtern(context.session, decl.name.name, ExternDef{name, ParametersOf(context, decl.params), TypeOf(context, decl.returnType)});
      const Vector<String> bindings = BindingTexts(context, decl.binding);

      if (bindings.empty()) {
        return;
      }

      const HostMember member{.function = name, .owner = "", .member = name, .isStatic = true, .hasResult = true, .bindings = bindings};
      AddHostMember(context.session, member);
    }

    ExprPtr globalInit(const AstGlobal& decl, const TypePtr& type) {
      const Span at = SpanAt(decl.name.pos);

      if (decl.init == GlobalInit::Int) {
        return ConstInt(ParseInt(decl.value.text), type, at);
      }

      if (decl.init == GlobalInit::Float) {
        return ConstFloat(ParseDouble(decl.value.text), type, at);
      }

      return ConstBool(decl.init == GlobalInit::True, type, at);
    }

    void globalItem(DeclContext& context, uint32_t index) {
      const AstGlobal& decl = context.program.globals[index];
      const TypePtr resolved = TypeOf(context, decl.type);
      GlobalDef global{Str(context.session, decl.name.name), resolved, globalInit(decl, resolved), originOf(context)};
      AddGlobal(context.session, context.unitIndex, decl.name.name, move(global));
    }

    void externTypeItem(DeclContext& context, uint32_t index) {
      ResolveExternType(context, context.program.externTypes[index]);
    }

    void externInterfaceItem(DeclContext& context, uint32_t index) {
      ResolveExternInterface(context, context.program.externInterfaces[index]);
    }

    void bridgeItem(DeclContext& context, uint32_t index) {
      ResolveBridge(context, context.program.bridges[index]);
    }

    void importItem(DeclContext& context, uint32_t index) {
      ResolveImport(context, context.program.imports[index]);
    }

    // A `meta` item declares nothing.
    void ignoreItem(DeclContext&, uint32_t) {}

    using ItemResolver = void (*)(DeclContext&, uint32_t);

    // In the order of ItemKind: Struct, Interface, Func, CoroutineFunc, ExternType, ExternInterface, ExternFunc, Bridge, Import, Global, Meta.
    const ItemResolver kItemResolvers[] = {structItem, interfaceItem, plainFunc, coroutineFunc, externTypeItem, externInterfaceItem, externItem, bridgeItem,
      importItem, globalItem, ignoreItem};

  }  // namespace

  // Each item of a unit, in order; an instance unit is read with its type parameters standing for its arguments.
  void ResolveUnit(Session& session, uint32_t unitIndex) {
    const Unit& unit = session.units[unitIndex];
    session.active = unit.instance == kNoIndex ? nullptr : &session.instances[unit.instance];
    DeclContext context{session, unit, unitIndex, *unit.program};

    for (uint32_t i = unit.first; i < unit.last; i++) {
      const AstItem& item = unit.program->items[i];
      EnterItem(session, unit, item);
      kItemResolvers[static_cast<size_t>(item.kind)](context, item.index);
    }
  }

  void ResolveDeclarations(Session& session) {
    for (uint32_t i = 0; i < session.units.size(); i++) {
      ResolveUnit(session, i);
    }
  }

}  // namespace polyir::front
