// Monomorphization (lang/src/Generics.nina): `Box<T>` becomes one concrete struct `Box_i64` for each distinct list of type arguments the program
// writes, with its methods specialised. Discovery scans the declared types the way the Nina pass does (parameters, results, fields, the types of `let`,
// `var` and label parameters in a body and its `if` / `for` / `try` blocks; not the expressions), takes each use once by its mangled name, and keeps the
// ones whose arguments name no type parameter. Each becomes an `Instance` and a Unit of its own, appended LAST (the specializations print after the
// program's other declarations); the checker reads that unit's generic declaration with every type parameter standing for its argument.
// A generic `extern type ArrayList<T>` used as `ArrayList<i64>` gets its own copy `ArrayList_i64`, members included, the same way.
#include "front/Pipeline.hpp"
#include "front/Resolve.hpp"

namespace polyir::front {

  namespace {

    // One written `Name<Args>`: where, and the name it mangles to.
    struct Use {
      const Program* program;
      TypeId type;
      Symbol mangled;
      String module;  // the module the use is written in, which names the generic declaration it means
    };

    // The walk that finds the uses: the session it reads, and the uses found so far.
    struct Discovery {
      Session& session;
      Vector<Use> uses{};
    };

    Vector<Use> discoverUses(Discovery& discovery);

    void typeIn(Discovery& discovery, const Program& program, TypeId id);

    void childrenIn(Discovery& discovery, const Program& program, const AstType& type);

    void paramsIn(Discovery& discovery, const Program& program, Range<AstParam> params);

    void blockIn(Discovery& discovery, const Program& program, const AstBlock& block);

    void blocksIn(Discovery& discovery, const Program& program, const AstStmt& stmt);

    void stmtIn(Discovery& discovery, const Program& program, StmtId id);

    void funcIn(Discovery& discovery, const Program& program, FuncId id);

    void structIn(Discovery& discovery, const Program& program, const AstStructDecl& decl);

    void membersIn(Discovery& discovery, const Program& program, const AstStructDecl& decl);

    void interfaceIn(Discovery& discovery, const Program& program, const AstInterfaceDecl& decl);

    void declIn(Discovery& discovery, const Program& program, const AstItem& item);

    void callableIn(Discovery& discovery, const Program& program, const AstItem& item);

    void typeIn(Discovery& discovery, const Program& program, TypeId id) {
      if (!Valid(id)) {
        return;
      }

      const AstType& type = program.types[id];

      if (type.kind == AstTypeKind::Generic) {
        discovery.uses.push_back(Use{&program, id, MangledGenericName(discovery.session, program, type), discovery.session.inModule});
      }

      childrenIn(discovery, program, type);
    }

    // A use, then the types written inside it: the lists first (parameters, cases, arguments), then the one or two it wraps.
    void childrenIn(Discovery& discovery, const Program& program, const AstType& type) {
      for (const TypeId each : program.typeRefs[type.types]) {
        typeIn(discovery, program, each);
      }

      typeIn(discovery, program, type.first);
      typeIn(discovery, program, type.second);
    }

    void paramsIn(Discovery& discovery, const Program& program, Range<AstParam> params) {
      for (const AstParam& param : program.params[params]) {
        typeIn(discovery, program, param.type);
      }
    }

    void blockIn(Discovery& discovery, const Program& program, const AstBlock& block) {
      for (const StmtId id : program.stmtRefs[block.stmts]) {
        stmtIn(discovery, program, id);
      }
    }

    // The blocks of an `if` or a `try`, and of each arm of a `match`.
    void blocksIn(Discovery& discovery, const Program& program, const AstStmt& stmt) {
      blockIn(discovery, program, stmt.body);
      blockIn(discovery, program, stmt.other);

      for (const AstArm& arm : program.arms[stmt.arms]) {
        blockIn(discovery, program, arm.body);
      }
    }

    void stmtIn(Discovery& discovery, const Program& program, StmtId id) {
      const AstStmt& stmt = program.stmts[id];

      if (stmt.kind == AstStmtKind::Let || stmt.kind == AstStmtKind::Var) {
        typeIn(discovery, program, stmt.type);
      }

      if (stmt.kind == AstStmtKind::Label) {
        paramsIn(discovery, program, stmt.params);
      }

      if (stmt.kind == AstStmtKind::For) {
        blockIn(discovery, program, stmt.body);
      }

      if (stmt.kind == AstStmtKind::If || stmt.kind == AstStmtKind::Try || stmt.kind == AstStmtKind::Match) {
        blocksIn(discovery, program, stmt);
      }
    }

    void funcIn(Discovery& discovery, const Program& program, FuncId id) {
      const AstFuncDecl& decl = program.funcs[id];
      paramsIn(discovery, program, decl.params);
      typeIn(discovery, program, decl.returnType);
      blockIn(discovery, program, decl.body);
    }

    void membersIn(Discovery& discovery, const Program& program, const AstStructDecl& decl) {
      for (const AstMember& member : program.members[decl.members]) {
        if (member.isMethod) {
          funcIn(discovery, program, member.method);
        } else {
          typeIn(discovery, program, member.field.type);
        }
      }
    }

    void structIn(Discovery& discovery, const Program& program, const AstStructDecl& decl) {
      for (const AstField& field : program.fields[decl.primary]) {
        typeIn(discovery, program, field.type);
      }

      membersIn(discovery, program, decl);
    }

    void interfaceIn(Discovery& discovery, const Program& program, const AstInterfaceDecl& decl) {
      for (const AstInterfaceMethod& method : program.interfaceMethods[decl.methods]) {
        paramsIn(discovery, program, method.params);
        typeIn(discovery, program, method.returnType);
      }
    }

    void declIn(Discovery& discovery, const Program& program, const AstItem& item) {
      if (item.kind == ItemKind::Struct) {
        structIn(discovery, program, program.structs[item.index]);
      }

      if (item.kind == ItemKind::Interface) {
        interfaceIn(discovery, program, program.interfaces[item.index]);
      }

      if (item.kind == ItemKind::Global) {
        typeIn(discovery, program, program.globals[item.index].type);
      }
    }

    void callableIn(Discovery& discovery, const Program& program, const AstItem& item) {
      if (item.kind == ItemKind::Func || item.kind == ItemKind::CoroutineFunc) {
        funcIn(discovery, program, FuncId{item.index});
      }

      if (item.kind != ItemKind::ExternFunc) {
        return;
      }

      paramsIn(discovery, program, program.externFuncs[item.index].params);
      typeIn(discovery, program, program.externFuncs[item.index].returnType);
    }

    // Every use in the program, in the order the declarations write them.
    Vector<Use> discoverUses(Discovery& discovery) {
      for (const Unit& unit : discovery.session.units) {
        for (uint32_t i = unit.first; i < unit.last; i++) {
          EnterItem(discovery.session, unit, unit.program->items[i]);
          declIn(discovery, *unit.program, unit.program->items[i]);
          callableIn(discovery, *unit.program, unit.program->items[i]);
        }
      }

      return move(discovery.uses);
    }

    // ── which uses become instances ─────────────────────────────────────────────────────────────────────────────────────────────────────────────────

    // The type parameters a struct, extern type or generic function item declares.
    Range<Ident> declaredParameters(const Program& program, const AstItem& item) {
      if (item.kind == ItemKind::Struct) {
        return program.structs[item.index].typeParams;
      }

      if (item.kind == ItemKind::Func) {
        return program.funcs.items[item.index].typeParams;
      }

      if (item.kind == ItemKind::ExternType) {
        return program.externTypes[item.index].typeParams;
      }

      return Range<Ident>();
    }

    void parameterNamesIn(const Unit& unit, Set<Symbol>& names) {
      const Program& program = *unit.program;

      for (uint32_t i = unit.first; i < unit.last; i++) {
        for (const Ident& each : program.idents[declaredParameters(program, program.items[i])]) {
          names.insert(each.name);
        }
      }
    }

    // Every type parameter name any generic struct or extern type of the program declares.
    Set<Symbol> typeParameterNames(const Session& session) {
      Set<Symbol> names;

      for (const Unit& unit : session.units) {
        parameterNamesIn(unit, names);
      }

      return names;
    }

    bool allConcrete(const Program& program, Range<TypeId> ids, const Set<Symbol>& parameters);

    // A type argument that still names a type parameter only arises from scanning a generic declaration's own text; such a use is skipped.
    bool isConcrete(const Program& program, TypeId id, const Set<Symbol>& parameters) {
      const AstType& type = program.types[id];

      if (type.kind == AstTypeKind::Named) {
        return parameters.count(type.name.name) == 0;
      }

      return type.kind != AstTypeKind::Generic || allConcrete(program, type.types, parameters);
    }

    bool allConcrete(const Program& program, Range<TypeId> ids, const Set<Symbol>& parameters) {
      for (const TypeId id : program.typeRefs[ids]) {
        if (!isConcrete(program, id, parameters)) {
          return false;
        }
      }

      return true;
    }

    // Adds `name` to `seen`; true when it was not there before.
    bool addIfNew(Set<Symbol>& seen, Symbol name) {
      return seen.insert(name).second;  // insert answers false when the name was already there
    }

    // Whether the use's type arguments name no type parameter.
    bool hasConcreteArguments(const Use& use, const Set<Symbol>& parameters) {
      return allConcrete(*use.program, use.program->types[use.type].types, parameters);
    }

    // Each mangled name once (the first use), then only the ones with concrete arguments.
    Vector<Use> concreteUses(const Session& session, const Vector<Use>& uses) {
      Set<Symbol> seen;
      const Set<Symbol> parameters = typeParameterNames(session);
      Vector<Use> out;

      for (const Use& use : uses) {
        // addIfNew runs first, for every use, so a later use of a name is skipped even when its first use was not concrete.
        if (addIfNew(seen, use.mangled) && hasConcreteArguments(use, parameters)) {
          out.push_back(use);
        }
      }

      return out;
    }

    // ── making an instance ──────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────

    // Type parameter `parameter` stands for `argument` in the instance.
    void bindArgument(Session& session, const Use& use, Instance& instance, Ident parameter, TypeId argument) {
      instance.params.push_back(parameter.name);
      instance.usedIn = use.program;
      instance.arguments.push_back(argument);

      instance.types.push_back(ResolveType(session, *use.program, argument));
      instance.spellings.push_back(Intern(session.names, MangledTypeText(session, *use.program, argument)));
      instance.named.push_back(use.program->types[argument].kind == AstTypeKind::Named);
    }

    // The instance for a use: each declared type parameter bound to the argument the use writes in its place.
    Instance instanceOf(Session& session, const Program& declared, Range<Ident> parameters, const Use& use) {
      Instance instance;
      instance.name = use.mangled;
      const View<Ident> names = declared.idents[parameters];
      const View<TypeId> arguments = use.program->typeRefs[use.program->types[use.type].types];

      for (size_t i = 0; i < names.size() && i < arguments.size(); i++) {
        bindArgument(session, use, instance, names[i], arguments[i]);
      }

      return instance;
    }

    // Appends the unit that reads item `item` of `declared` as `instance`.
    void addInstanceUnit(Session& session, const Program& declared, uint32_t item, Instance instance) {
      session.instances.push_back(move(instance));
      const uint32_t at = static_cast<uint32_t>(session.instances.size() - 1);
      session.units.push_back(Unit{&declared, item, item + 1, 0, at});
    }

    // Where a generic declaration is: its program and item; no program when there is none.
    struct Declared {
      const Program* program = nullptr;
      uint32_t item = 0;
    };

    bool isFound(const Declared& declared) {
      return declared.program != nullptr;
    }

    // Whether the item is a declaration of `kind` called `name` (an extern type must also have `arity` type parameters).
    bool declares(const Program& program, const AstItem& item, ItemKind kind, Symbol name, size_t arity) {
      if (item.kind != kind) {
        return false;
      }

      if (kind == ItemKind::Struct) {
        return program.structs[item.index].name.name == name;
      }

      return program.externTypes[item.index].name.name == name && program.externTypes[item.index].typeParams.count == arity;
    }

    Declared findIn(const Unit& unit, ItemKind kind, Symbol name, size_t arity) {
      for (uint32_t i = unit.first; i < unit.last; i++) {
        if (declares(*unit.program, unit.program->items[i], kind, name, arity)) {
          return Declared{unit.program, i};
        }
      }

      return Declared();
    }

    // The first item of the given kind, in the program's order, that is named `name` (and, for a host type, has `arity` type parameters).
    Declared findDeclared(const Session& session, ItemKind kind, Symbol name, size_t arity) {
      for (const Unit& unit : session.units) {
        const Declared hit = findIn(unit, kind, name, arity);

        if (isFound(hit)) {
          return hit;
        }
      }

      return Declared();
    }

    Range<Ident> parametersOf(const Declared& declared) {
      return declaredParameters(*declared.program, declared.program->items[declared.item]);
    }

    // The unit of a use's instance: of the generic struct of that name, else of the generic extern type.
    void specialize(Session& session, const Use& use) {
      const AstType& type = use.program->types[use.type];
      Declared declared = findDeclared(session, ItemKind::Struct, InScopeOf(session, use.module, type.name.name), type.types.count);

      if (!isFound(declared)) {
        declared = findDeclared(session, ItemKind::ExternType, type.name.name, type.types.count);
      }

      if (!isFound(declared)) {
        return;
      }

      addInstanceUnit(session, *declared.program, declared.item, instanceOf(session, *declared.program, parametersOf(declared), use));
    }

    // ── the generics as declared: read once with their type parameters as `Param` types ──────────────────────────────────────────────────────────

    // Whether every type argument of the use is a bare type parameter (`Box<T>` written in a generic's own text).
    bool hasParameterArguments(const Use& use, const Set<Symbol>& parameters) {
      for (const TypeId id : use.program->typeRefs[use.program->types[use.type].types]) {
        const AstType& type = use.program->types[id];

        if (type.kind != AstTypeKind::Named || parameters.count(type.name.name) == 0 || !Empty(type.sizes)) {
          return false;
        }
      }

      return true;
    }

    // The uses written with type parameters, each name once.
    Vector<Use> parameterUses(const Session& session, const Vector<Use>& uses) {
      Set<Symbol> seen;
      const Set<Symbol> parameters = typeParameterNames(session);
      Vector<Use> out;

      for (const Use& use : uses) {
        if (hasParameterArguments(use, parameters) && addIfNew(seen, use.mangled)) {
          out.push_back(use);
        }
      }

      return out;
    }

    // An instance whose arguments are type parameters, `Param` types named `arguments` (`Box_T`, each T standing for any type).
    Instance parametricInstance(Session& session, const Program& declared, Range<Ident> parameters, Symbol name, const Vector<Symbol>& arguments) {
      Instance instance;
      instance.name = name;
      instance.parametric = true;
      instance.usedIn = &declared;
      const View<Ident> names = declared.idents[parameters];

      for (size_t i = 0; i < names.size() && i < arguments.size(); i++) {
        instance.params.push_back(names[i].name);
        instance.types.push_back(MakeParam(Str(session, arguments[i])));
        instance.spellings.push_back(arguments[i]);
        instance.named.push_back(true);
      }

      return instance;
    }

    // `Box<U>` in a generic's text: the generic struct read with U as a `Param`, so that generic is checked for every type as well.
    void specializeParametric(Session& session, const Use& use, Set<Symbol>& made) {
      const AstType& type = use.program->types[use.type];
      const Declared declared = findDeclared(session, ItemKind::Struct, InScopeOf(session, use.module, type.name.name), type.types.count);
      Vector<Symbol> arguments;

      for (const TypeId id : use.program->typeRefs[type.types]) {
        arguments.push_back(use.program->types[id].name.name);
      }

      if (isFound(declared) && addIfNew(made, use.mangled)) {
        addInstanceUnit(session, *declared.program, declared.item, parametricInstance(session, *declared.program, parametersOf(declared), use.mangled, arguments));
      }
    }

    // `struct Box<T>` read once as `Box_T`, its own parameters standing for any type (unless a use already made it).
    void declaredStruct(Session& session, const Program& program, uint32_t item, Set<Symbol>& made) {
      const AstStructDecl& decl = program.structs[program.items[item].index];
      String name = Str(session, decl.name.name);
      Vector<Symbol> arguments;

      for (const Ident& param : program.idents[decl.typeParams]) {
        name += "_" + Str(session, param.name);
        arguments.push_back(param.name);
      }

      if (addIfNew(made, Intern(session.names, name))) {
        addInstanceUnit(session, program, item, parametricInstance(session, program, decl.typeParams, Intern(session.names, name), arguments));
      }
    }

    // Every generic struct the program declares, as declared.
    void declaredGenerics(Session& session, Set<Symbol>& made) {
      const size_t units = session.units.size();

      for (size_t u = 0; u < units; u++) {
        const Unit unit = session.units[u];

        for (uint32_t i = unit.first; i < unit.last && unit.instance == kNoIndex; i++) {
          const AstItem& item = unit.program->items[i];

          if (item.kind == ItemKind::Struct && !Empty(unit.program->structs[item.index].typeParams)) {
            declaredStruct(session, *unit.program, i, made);
          }
        }
      }
    }

  }  // namespace

  void Monomorphize(Session& session) {
    Discovery discovery{session};
    const Vector<Use> uses = discoverUses(discovery);

    for (const Use& use : concreteUses(session, uses)) {
      specialize(session, use);
    }

    Set<Symbol> made;

    for (const Use& use : parameterUses(session, uses)) {
      specializeParametric(session, use, made);
    }

    declaredGenerics(session, made);
  }

}  // namespace polyir::front
