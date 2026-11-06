// The problems of `interface` declarations and of the structs that claim them (`struct Circle : Shape`), checked on the syntax tree so every message points
// at the exact `line:column` of the offending name (lang/src/Interfaces.nina):
//   * an interface declares each method once, takes no fixed-size array, and its name is not also a struct's;
//   * every name after a struct's `:` is an interface (or an `extern interface`);
//   * the struct has every method of the interface with the same parameter and return types (parameter names may differ);
//   * a generic struct's claims are compared once it is specialized (the pipeline asks again after `Monomorphize`).
// Whether a value of one struct may be used where an interface is expected is decided later, on the checked program (CheckInterface.cpp).
#include "front/HirBuild.hpp"
#include "front/Pipeline.hpp"

namespace polyir::front {

  namespace {

    // Where a declaration was written: its program and the file number of its unit.
    struct Source {
      const Program* program;
      uint32_t file;
      const Instance* instance = nullptr;  // set when the unit is a generic struct made concrete
    };

    // An interface (or extern interface) declaration and where it was written.
    struct InterfaceDecl {
      Source source;
      Ident name;
      Range<AstInterfaceMethod> methods;
    };

    String join(const Vector<String>& parts, const char* separator) {
      String out;

      for (size_t i = 0; i < parts.size(); i++) {
        out += (i == 0 ? "" : separator) + parts[i];
      }

      return out;
    }

    // ── type text: two types are compared by comparing this ─────────────────────────────────────────────────────────────────────────────────────

    String typeText(const Program& program, TypeId id, const Instance* instance = nullptr);

    Vector<String> typeTexts(const Program& program, Range<TypeId> ids, const Instance* instance) {
      Vector<String> out;

      for (const TypeId id : program.typeRefs[ids]) {
        out.push_back(typeText(program, id, instance));
      }

      return out;
    }

    String wrapped(const String& head, const String& inside) {
      return head + "<" + inside + ">";
    }

    String sizesText(const Program& program, const AstType& type) {
      String out;

      for (const Literal& size : program.literals[type.sizes]) {
        out += "," + String(size.text);
      }

      return out;
    }

    // A type parameter of the instance reads as its argument; `T[4]` keeps the size when the argument is a bare name (Generics.nina applySizeToSubstituted).
    String boundText(const Program& program, const AstType& type, const Instance& instance, int at) {
      const AstType& argument = instance.usedIn->types[instance.arguments[static_cast<size_t>(at)]];
      const String text = typeText(*instance.usedIn, instance.arguments[static_cast<size_t>(at)]);

      if (Empty(type.sizes) || argument.kind != AstTypeKind::Named) {
        return text;
      }

      return String(TextOf(*instance.usedIn, argument.name.name)) + "[" + String(program.literals[type.sizes][0].text) + "]";
    }

    // A name, a generic `Name<...>` or a fixed array `Name[N]`; a type parameter of the instance reads as its argument.
    String namedText(const Program& program, const AstType& type, const Instance* instance) {
      const int bound = instance != nullptr && type.kind == AstTypeKind::Named ? IndexOf(*instance, type.name.name) : -1;

      if (bound >= 0) {
        return boundText(program, type, *instance, bound);
      }

      const String name(TextOf(program, type.name.name));

      if (type.kind == AstTypeKind::Generic) {
        return wrapped(name, join(typeTexts(program, type.types, instance), ","));
      }

      return Empty(type.sizes) ? name : name + "[" + String(program.literals[type.sizes][0].text) + "]";
    }

    // `func(A,B)->R` or `closure(A,B)->R`.
    String functionText(const Program& program, const AstType& type, const char* head, const Instance* instance) {
      const String inside = join(typeTexts(program, type.types, instance), ",");

      return String(head) + "(" + inside + ")->" + typeText(program, type.first, instance);
    }

    String pairText(const Program& program, const AstType& type, const char* head, const Instance* instance) {
      const String second = Valid(type.second) ? "," + typeText(program, type.second, instance) : String();

      return wrapped(head, typeText(program, type.first, instance) + second);
    }

    bool isFunctionKind(AstTypeKind kind) {
      return kind == AstTypeKind::Func || kind == AstTypeKind::Closure;
    }

    bool isPairKind(AstTypeKind kind) {
      return kind == AstTypeKind::Map || kind == AstTypeKind::Mutmap || kind == AstTypeKind::Either;
    }

    // The keyword of each type kind, in the order of AstTypeKind.
    const char* const kTypeKeywords[] = {"ptr", "vec", "array", "mutarray", "buffer", "grid", "tensor", "sparse", "map", "mutmap", "set", "mutset",
                                         "func", "closure", "variant", "either", "optional", "coro"};

    // A type that starts with a keyword: `ptr<...>`, `func(...)->...`, `map<...>`, `variant<...>`, ...
    String keywordText(const Program& program, const AstType& type, const Instance* instance) {
      const char* head = kTypeKeywords[static_cast<size_t>(type.kind)];

      if (type.kind == AstTypeKind::Variant) {
        return wrapped(head, join(typeTexts(program, type.types, instance), ","));
      }

      if (isFunctionKind(type.kind)) {
        return functionText(program, type, head, instance);
      }

      if (isPairKind(type.kind)) {
        return pairText(program, type, head, instance);
      }

      return wrapped(head, typeText(program, type.first, instance) + sizesText(program, type));
    }

    String typeText(const Program& program, TypeId id, const Instance* instance) {
      const AstType& type = program.types[id];
      const bool isNamed = type.kind == AstTypeKind::Generic || type.kind == AstTypeKind::Named;

      return isNamed ? namedText(program, type, instance) : keywordText(program, type, instance);
    }

    // `func area(r: f64) -> f64`, as the problem messages show a signature.
    String signature(const Program& program, Ident name, Range<AstParam> params, TypeId returnType, const Instance* instance = nullptr) {
      Vector<String> shown;

      for (const AstParam& param : program.params[params]) {
        shown.push_back(String(TextOf(program, param.name.name)) + ": " + typeText(program, param.type, instance));
      }

      return "func " + String(TextOf(program, name.name)) + "(" + join(shown, ", ") + ") -> " + typeText(program, returnType, instance);
    }

    // Same parameter types in the same order and the same return type; parameter names do not matter.
    // The interface's side is read as written; the struct's side with the struct's instance (when it is a generic made concrete).
    bool sameTypes(const AstInterfaceMethod& wanted, const Program& interfaceProgram, const AstFuncDecl& found, const Program& structProgram,
                   const Instance* instance) {
      if (wanted.params.count != found.params.count) {
        return false;
      }

      if (typeText(interfaceProgram, wanted.returnType) != typeText(structProgram, found.returnType, instance)) {
        return false;
      }

      for (uint32_t i = 0; i < wanted.params.count; i++) {
        const TypeId wantedType = interfaceProgram.params[wanted.params][i].type;

        if (typeText(interfaceProgram, wantedType) != typeText(structProgram, structProgram.params[found.params][i].type, instance)) {
          return false;
        }
      }

      return true;
    }

    // ── the rules ───────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────

    // What the rules know about the program, collected first, and the problems found.
    struct Rules {
      Session& session;
      Vector<String> messages{};
      Vector<InterfaceDecl> interfaces{};
      Vector<InterfaceDecl> externInterfaces{};
      SymbolMap<int> nameCounts{};  // how many structs and interfaces carry each name
      SymbolMap<bool> hostTypes{};  // the names of `extern type`s
    };

    void collect(Rules& rules);

    void note(Rules& rules, Source source, const AstItem& item);

    void noteInterface(Rules& rules, Source source, const AstInterfaceDecl& decl);

    void count(Rules& rules, Ident name);

    void inUnit(Rules& rules, const Unit& unit);

    const Instance* instanceOf(const Rules& rules, const Unit& unit);

    void inItem(Rules& rules, Source source, const AstItem& item);

    void problem(Rules& rules, Source source, const String& what, Pos at);

    String structName(const Rules& rules, Source source, const AstStructDecl& decl);

    String identText(Source source, Ident ident);

    void inInterface(Rules& rules, Source source, const AstInterfaceDecl& decl);

    void inMethodDecl(Rules& rules, Source source, const AstInterfaceDecl& decl, const AstInterfaceMethod& method, Vector<Symbol>& seen);

    void arrayParam(Rules& rules, Source source, const AstParam& param);

    void inStruct(Rules& rules, Source source, const AstStructDecl& decl);

    const InterfaceDecl* findIn(const Vector<InterfaceDecl>& list, Symbol wanted);

    void inClaim(Rules& rules, Source source, const AstStructDecl& decl, Ident claim);

    String notInterface(const Rules& rules, Source source, Ident claim);

    void inMethodsUnlessGeneric(Rules& rules, Source source, const AstStructDecl& decl, Ident claim, const InterfaceDecl& interface);

    const AstFuncDecl* methodNamed(Source source, const AstStructDecl& decl, Symbol wanted);

    void inMethod(Rules& rules, Source source, const AstStructDecl& decl, Ident claim, const InterfaceDecl& interface, const AstInterfaceMethod& method);

    void signatureProblem(Rules& rules, Source source, const AstStructDecl& decl, const InterfaceDecl& interface, const AstInterfaceMethod& method,
                          const AstFuncDecl& implementation);

    // The rules with every interface, extern interface, extern type and type name of the program noted.
    Rules makeRules(Session& session) {
      Rules rules{session};
      collect(rules);

      return rules;
    }

    // Every problem, unit by unit, in source order. A generic read with `Param` types (Generics.cpp) is checked by its copies for real types.
    Vector<String> problems(Rules& rules) {
      for (const Unit& unit : rules.session.units) {
        if (unit.instance == kNoIndex || !rules.session.instances[unit.instance].parametric) {
          inUnit(rules, unit);
        }
      }

      return move(rules.messages);
    }

    // The declarations as written (an instance unit is a copy of one, so it is skipped).
    void collect(Rules& rules) {
      for (const Unit& unit : rules.session.units) {
        if (unit.instance != kNoIndex) {
          continue;
        }

        for (uint32_t i = unit.first; i < unit.last; i++) {
          note(rules, Source{unit.program, unit.file}, unit.program->items[i]);
        }
      }
    }

    void noteExternInterface(Rules& rules, Source source, const AstExternInterface& decl) {
      rules.externInterfaces.push_back(InterfaceDecl{source, decl.name, decl.methods});
    }

    void note(Rules& rules, Source source, const AstItem& item) {
      const Program& program = *source.program;

      if (item.kind == ItemKind::Struct) {
        count(rules, program.structs[item.index].name);
      }

      if (item.kind == ItemKind::Interface) {
        noteInterface(rules, source, program.interfaces[item.index]);
      }

      if (item.kind == ItemKind::ExternType) {
        Insert(rules.hostTypes, program.externTypes[item.index].name.name, true);
      }

      if (item.kind == ItemKind::ExternInterface) {
        noteExternInterface(rules, source, program.externInterfaces[item.index]);
      }
    }

    void noteInterface(Rules& rules, Source source, const AstInterfaceDecl& decl) {
      count(rules, decl.name);
      rules.interfaces.push_back(InterfaceDecl{source, decl.name, decl.methods});
    }

    // One more struct or interface carries this name.
    void count(Rules& rules, Ident name) {
      int* seen = Find(rules.nameCounts, name.name);

      if (seen == nullptr) {
        Insert(rules.nameCounts, name.name, 1);
      } else {
        *seen += 1;
      }
    }

    void inUnit(Rules& rules, const Unit& unit) {
      const Source source{unit.program, unit.file, instanceOf(rules, unit)};

      for (uint32_t i = unit.first; i < unit.last; i++) {
        inItem(rules, source, unit.program->items[i]);
      }
    }

    const Instance* instanceOf(const Rules& rules, const Unit& unit) {
      return unit.instance == kNoIndex ? nullptr : &rules.session.instances[unit.instance];
    }

    void inItem(Rules& rules, Source source, const AstItem& item) {
      const Program& program = *source.program;

      if (item.kind == ItemKind::Interface) {
        inInterface(rules, source, program.interfaces[item.index]);
      }

      if (item.kind == ItemKind::Struct) {
        inStruct(rules, source, program.structs[item.index]);
      }
    }

    // One message at `at`, with the declaration's file in front.
    void problem(Rules& rules, Source source, const String& what, Pos at) {
      const String& file = rules.session.files[source.file];
      const String message = DiagnosticMessage(what, SpanAt(at));
      rules.messages.push_back(file.empty() ? message : file + ":" + message);
    }

    // A generic struct made concrete is named by its instance (`Box_i64`).
    String structName(const Rules& rules, Source source, const AstStructDecl& decl) {
      return source.instance == nullptr ? identText(source, decl.name) : Str(rules.session, source.instance->name);
    }

    String identText(Source source, Ident ident) {
      return String(TextOf(*source.program, ident.name));
    }

    void inInterface(Rules& rules, Source source, const AstInterfaceDecl& decl) {
      const int* total = Find(rules.nameCounts, decl.name.name);

      if (total != nullptr && *total >= 2) {
        problem(rules, source, "duplicate struct or interface '" + identText(source, decl.name) + "'", decl.name.pos);
      }

      Vector<Symbol> seen;

      for (const AstInterfaceMethod& method : source.program->interfaceMethods[decl.methods]) {
        inMethodDecl(rules, source, decl, method, seen);
      }
    }

    void inMethodDecl(Rules& rules, Source source, const AstInterfaceDecl& decl, const AstInterfaceMethod& method, Vector<Symbol>& seen) {
      const String twice = "interface " + identText(source, decl.name) + " declares " + identText(source, method.name) + " twice";

      if (IsOneOf(method.name.name, seen)) {
        problem(rules, source, twice, method.name.pos);
      }

      seen.push_back(method.name.name);

      for (const AstParam& param : source.program->params[method.params]) {
        arrayParam(rules, source, param);
      }
    }

    void arrayParam(Rules& rules, Source source, const AstParam& param) {
      const AstType& type = source.program->types[param.type];

      if (type.kind != AstTypeKind::Named || Empty(type.sizes)) {
        return;
      }

      const String what = "interface methods cannot take a fixed-size array (" + identText(source, param.name) + "): pass a ptr<T> or a mutarray<T>";
      problem(rules, source, what, param.name.pos);
    }

    void inStruct(Rules& rules, Source source, const AstStructDecl& decl) {
      for (const Ident& claim : source.program->idents[decl.impls]) {
        inClaim(rules, source, decl, claim);
      }
    }

    const InterfaceDecl* findIn(const Vector<InterfaceDecl>& list, Symbol wanted) {
      for (const InterfaceDecl& decl : list) {
        if (decl.name.name == wanted) {
          return &decl;
        }
      }

      return nullptr;
    }

    // `struct S : Name`: Name must be an interface or an extern interface, and S must have its methods.
    void inClaim(Rules& rules, Source source, const AstStructDecl& decl, Ident claim) {
      const InterfaceDecl* found = findIn(rules.interfaces, InScopeOf(rules.session, decl.module, claim.name));

      if (found == nullptr) {
        found = findIn(rules.externInterfaces, claim.name);
      }

      if (found != nullptr) {
        return inMethodsUnlessGeneric(rules, source, decl, claim, *found);
      }

      problem(rules, source, notInterface(rules, source, claim), claim.pos);
    }

    // What the claimed name is instead: an extern type, a struct, or nothing at all.
    String notInterface(const Rules& rules, Source source, Ident claim) {
      const String text = identText(source, claim);

      if (Contains(rules.hostTypes, claim.name) && !Contains(rules.nameCounts, claim.name)) {
        return text + " is an extern type, not an interface";
      }

      return Contains(rules.nameCounts, claim.name) ? text + " is a struct, not an interface" : "unknown interface " + text;
    }

    // A generic struct's methods are compared once it is made concrete (its types are only known then).
    void inMethodsUnlessGeneric(Rules& rules, Source source, const AstStructDecl& decl, Ident claim, const InterfaceDecl& interface) {
      if (!Empty(decl.typeParams) && source.instance == nullptr) {
        return;
      }

      for (const AstInterfaceMethod& method : interface.source.program->interfaceMethods[interface.methods]) {
        inMethod(rules, source, decl, claim, interface, method);
      }
    }

    const AstFuncDecl* methodNamed(Source source, const AstStructDecl& decl, Symbol wanted) {
      for (const AstMember& member : source.program->members[decl.members]) {
        if (member.isMethod && source.program->funcs[member.method].name.name == wanted) {
          return &source.program->funcs[member.method];
        }
      }

      return nullptr;
    }

    // The struct must have the interface's method, with the same signature.
    void inMethod(Rules& rules, Source source, const AstStructDecl& decl, Ident claim, const InterfaceDecl& interface, const AstInterfaceMethod& method) {
      const AstFuncDecl* implementation = methodNamed(source, decl, method.name.name);

      if (implementation != nullptr) {
        return signatureProblem(rules, source, decl, interface, method, *implementation);
      }

      const String needs = signature(*interface.source.program, method.name, method.params, method.returnType);
      problem(rules, source, structName(rules, source, decl) + " does not implement " + identText(source, interface.name) + ": it needs " + needs, claim.pos);
    }

    void signatureProblem(Rules& rules, Source source, const AstStructDecl& decl, const InterfaceDecl& interface, const AstInterfaceMethod& method,
                          const AstFuncDecl& implementation) {
      const Program& interfaceProgram = *interface.source.program;

      if (sameTypes(method, interfaceProgram, implementation, *source.program, source.instance)) {
        return;
      }

      const String structMethod = structName(rules, source, decl) + "." + identText(source, implementation.name);
      const String who = structMethod + " does not match " + identText(source, interface.name) + "." + identText(interface.source, method.name);
      const String expected = signature(interfaceProgram, method.name, method.params, method.returnType);
      const String found = signature(*source.program, implementation.name, implementation.params, implementation.returnType, source.instance);
      problem(rules, source, who + ": expected " + expected + ", found " + found, implementation.name.pos);
    }

  }  // namespace

  Vector<String> InterfaceProblems(Session& session) {
    Rules rules = makeRules(session);

    return problems(rules);
  }

}  // namespace polyir::front
