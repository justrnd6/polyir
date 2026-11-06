// A call of a generic function (CheckGenericCall.nina): `pick(1, 2)` of `func pick<T>(first: T, second: T) -> T`. Each type
// parameter is bound to the type its argument has where the parameter's type names it (`array<T>` given a `mutarray<i64>` binds
// T to i64). The call is then a call of the copy for those types, `pick_i64`. The first call that needs a copy makes it: a unit
// that reads the generic function with each type parameter standing for its type, whose body is checked after the bodies so far.
#include "front/Checker.hpp"
#include "front/Pipeline.hpp"
#include "front/Resolve.hpp"
#include "polyir/HirPrint.hpp"

namespace polyir::front {

  namespace {

    void bindFrom(const Program& program, TypeId id, const TypePtr& given, View<Ident> params, Vector<TypePtr>& bound);

    void bindAll(const Program& program, Range<TypeId> ids, const Vector<TypePtr>& given, View<Ident> params, Vector<TypePtr>& bound) {
      const View<TypeId> written = program.typeRefs[ids];

      for (size_t i = 0; i < written.size() && i < given.size(); i++) {
        bindFrom(program, written[i], given[i], params, bound);
      }
    }

    // `T` written where `given` sits binds T (the first binding wins); a type made of others binds what each of its parts names.
    void bindFrom(const Program& program, TypeId id, const TypePtr& given, View<Ident> params, Vector<TypePtr>& bound) {
      const AstType& type = program.types[id];

      for (size_t i = 0; i < params.size() && type.kind == AstTypeKind::Named; i++) {
        if (params[i].name == type.name.name && !bound[i]) {
          bound[i] = given;
        }
      }

      if (Valid(type.first) && given->element) {
        bindFrom(program, type.first, given->element, params, bound);
      }

      if (Valid(type.second) && given->second) {
        bindFrom(program, type.second, given->second, params, bound);
      }

      bindAll(program, type.types, given->parameters, params, bound);
    }

    // How a type is written in a copy's name: `i64`, `double`, `string`, `Point`, `mutarray_i64`.
    String spellingOf(const TypePtr& type) {
      String word;

      for (const char c : TypeText(type)) {
        word += isalnum(static_cast<unsigned char>(c)) ? String(1, c) : (word.empty() || word.back() == '_' ? "" : "_");
      }

      while (!word.empty() && word.back() == '_') {
        word.pop_back();
      }

      return type->kind == TypeKind::Float ? (type->bits == 32 ? "float" : "double") : word;
    }

    Symbol copyName(Checker& check, Symbol generic, const Vector<TypePtr>& bound) {
      String name = Str(check, generic);

      for (const TypePtr& type : bound) {
        name += "_" + spellingOf(type);
      }

      return Intern(check.session.names, name);
    }

    Instance instanceOf(Checker& check, const GenericFunc& generic, View<Ident> params, const Vector<TypePtr>& bound, Symbol copy) {
      Instance instance;
      instance.name = copy;
      instance.usedIn = generic.program;

      for (size_t i = 0; i < params.size(); i++) {
        instance.params.push_back(params[i].name);
        instance.types.push_back(bound[i]);
        instance.spellings.push_back(Intern(check.session.names, spellingOf(bound[i])));
        instance.named.push_back(true);
      }

      instance.parametric = IsParametric(check.session.module, Generic{.arguments = bound});

      return instance;
    }

    // The copy's unit, resolved now so the call can find its signature. Afterwards the checker's place is put back. That is the
    // instance it reads (kept by index, as the list may move), and the module, struct and file of its code.
    void makeCopy(Checker& check, const GenericFunc& generic, Instance instance) {
      Session& session = check.session;
      const long active = session.active == nullptr ? -1 : session.active - session.instances.data();
      const String inModule = session.inModule;
      const Symbol owner = session.owner;
      const uint32_t file = session.file;

      session.instances.push_back(move(instance));
      session.units.push_back(Unit{generic.program, generic.item, generic.item + 1, generic.file, static_cast<uint32_t>(session.instances.size() - 1)});
      ResolveUnit(session, static_cast<uint32_t>(session.units.size() - 1));

      session.active = active < 0 ? nullptr : &session.instances[static_cast<size_t>(active)];
      session.inModule = inModule;
      session.owner = owner;
      session.file = file;
    }

    // What a type parameter no argument names cannot be: the copy needs every type.
    Optional<String> unboundProblem(Checker& check, Ident name, View<Ident> params, const Vector<TypePtr>& bound) {
      for (size_t i = 0; i < params.size(); i++) {
        if (!bound[i]) {
          return DisplayName(check.session, name.name) + "'s type " + Str(check, params[i].name) + " cannot be worked out from its arguments";
        }
      }

      return nullopt;
    }

  }  // namespace

  namespace {

    // A generic function read once with its type parameters standing for any type: `pick_T`, the generic as declared.
    void declareGeneric(Session& session, const GenericFunc& generic) {
      const AstFuncDecl& decl = generic.program->funcs.items[generic.program->items[generic.item].index];
      String name = Str(session, decl.name.name);
      Instance instance;
      instance.usedIn = generic.program;
      instance.parametric = true;

      for (const Ident& param : generic.program->idents[decl.typeParams]) {
        name += "_" + Str(session, param.name);
        instance.params.push_back(param.name);
        instance.types.push_back(MakeParam(Str(session, param.name)));
        instance.spellings.push_back(param.name);
        instance.named.push_back(true);
      }

      instance.name = Intern(session.names, name);
      session.instances.push_back(move(instance));
      session.units.push_back(Unit{generic.program, generic.item, generic.item + 1, generic.file, static_cast<uint32_t>(session.instances.size() - 1)});
      ResolveUnit(session, static_cast<uint32_t>(session.units.size() - 1));
    }

  }  // namespace

  void DeclareGenericFunctions(Session& session) {
    const Deque<GenericFunc> generics = session.genericFuncs.values;

    for (const GenericFunc& generic : generics) {
      declareGeneric(session, generic);
    }
  }

  ExprPtr GenericCall(Checker& check, Ident name, const GenericFunc& generic, Vector<ExprPtr> values) {
    const AstFuncDecl& decl = generic.program->funcs.items[generic.program->items[generic.item].index];
    const View<Ident> params = generic.program->idents[decl.typeParams];
    const View<AstParam> written = generic.program->params[decl.params];
    Vector<TypePtr> bound(params.size());

    for (size_t i = 0; i < written.size() && i < values.size(); i++) {
      bindFrom(*generic.program, written[i].type, values[i]->type, params, bound);
    }

    if (const Optional<String> why = unboundProblem(check, name, params, bound)) {
      return Unsupported(*why, SpanAt(name.pos));
    }

    const Symbol copy = copyName(check, name.name, bound);

    if (FindFunc(DeclsOf(check), copy) == nullptr) {
      makeCopy(check, generic, instanceOf(check, generic, params, bound, copy));
    }

    return DirectCall(check, Ident{copy, name.pos}, move(values));
  }

}  // namespace polyir::front
