// The generics as declared (Hir.hpp `Generic`). The front end reads each generic once with its type parameters as `Param` types (`Box_T`,
// `pick_T`), so the generic is checked for every type, not only for those a program uses. Those copies are not code: Diagnose leaves them
// out, a problem in one only says why its generic does not work for every type, and after the check they move from the module's structs
// and functions to its genericStructs and genericFunctions. LLVM, the VM and the HIR text see only the copies made for real types.
#include "front/Pipeline.hpp"

namespace polyir::front {

  namespace {

    bool mentionsParam(const Module& module, const TypePtr& type, int depth);

    bool anyMentionsParam(const Module& module, const Vector<TypePtr>& types, int depth) {
      return any_of(types.begin(), types.end(), [&](const TypePtr& each) { return mentionsParam(module, each, depth); });
    }

    // A struct copy made from `Param` types (`Box_T`) is itself one.
    bool isParametricStruct(const Module& module, const String& name, int depth) {
      const StructDef* def = FindStruct(module, name);

      return def != nullptr && anyMentionsParam(module, def->generic.arguments, depth + 1);
    }

    bool mentionsParam(const Module& module, const TypePtr& type, int depth) {
      if (!type || depth > 8) {
        return false;
      }

      if (type->kind == TypeKind::Param || (type->kind == TypeKind::Struct && isParametricStruct(module, type->name, depth))) {
        return true;
      }

      const bool inParts = mentionsParam(module, type->element, depth + 1) || mentionsParam(module, type->second, depth + 1);

      return inParts || anyMentionsParam(module, type->parameters, depth + 1);
    }

    // The struct a method belongs to, when it is one.
    StructDef* methodsStruct(Module& module, const FuncDef& function) {
      const bool isMethod = !function.parameters.empty() && function.parameters[0].name == "this" && function.parameters[0].type;

      return isMethod ? const_cast<StructDef*>(FindStruct(module, function.parameters[0].type->name)) : nullptr;
    }

    // The first problem in a parametric function says why its generic, and the struct it is a method of, do not work for every type.
    void noteFunction(Session& session, size_t index) {
      FuncDef& function = session.module.functions[index];
      const Vector<String> problems = FunctionProblems(session, index);

      if (problems.empty()) {
        return;
      }

      function.generic.notParametric = problems.front();
      StructDef* owner = methodsStruct(session.module, function);

      if (owner != nullptr && owner->generic.notParametric.empty()) {
        owner->generic.notParametric = problems.front();
      }
    }

    // Each list's declarations: the code kept in place, the parametric ones appended to `generics`; `kept` maps old places to new (-1: moved).
    // (Which are parametric is decided before any moves, as deciding looks structs up by name.)
    template <typename Decl>
    Vector<long> split(const Module& module, Vector<Decl>& items, Vector<Decl>& generics) {
      Vector<bool> parametric;
      Vector<Decl> code;
      Vector<long> kept;

      for (const Decl& item : items) {
        parametric.push_back(IsParametric(module, item.generic));
      }

      for (size_t i = 0; i < items.size(); i++) {
        kept.push_back(parametric[i] ? -1 : static_cast<long>(code.size()));
        (parametric[i] ? generics : code).push_back(move(items[i]));
      }

      items = move(code);

      return kept;
    }

    // The order without the moved declarations, the others at their new places.
    void renumber(Module& module, const Vector<long>& structs, const Vector<long>& functions) {
      Vector<DeclRef> order;

      for (DeclRef ref : module.order) {
        const bool isStruct = ref.kind == DeclKind::Struct;
        const long place = isStruct ? structs[ref.index] : (ref.kind == DeclKind::Function ? functions[ref.index] : static_cast<long>(ref.index));

        if (place >= 0) {
          ref.index = static_cast<size_t>(place);
          order.push_back(ref);
        }
      }

      module.order = move(order);
    }

    // The functions' sources follow their functions.
    void keepSources(Session& session, const Vector<long>& functions) {
      Vector<FuncSource> sources;

      for (size_t i = 0; i < functions.size() && i < session.sources.size(); i++) {
        if (functions[i] >= 0) {
          sources.push_back(session.sources[i]);
        }
      }

      session.sources = move(sources);
    }

  }  // namespace

  bool IsParametric(const Module& module, const Generic& generic) {
    return anyMentionsParam(module, generic.arguments, 0);
  }

  void NoteParametric(Session& session) {
    for (size_t i = 0; i < session.module.functions.size(); i++) {
      if (IsParametric(session.module, session.module.functions[i].generic)) {
        noteFunction(session, i);
      }
    }
  }

  void SplitGenerics(Session& session) {
    Module& module = session.module;
    const Vector<long> functions = split(module, module.functions, module.genericFunctions);
    const Vector<long> structs = split(module, module.structs, module.genericStructs);

    renumber(module, structs, functions);
    keepSources(session, functions);
  }

}  // namespace polyir::front
