// What one check works on and builds: the units of source being checked, the declaration tables, the module being made and the scope of the code
// being checked. Everything the Nina checker passes around as `decls: List<HDecl>` and `scope: List<HBinding>` is here, behind hash lookups.
//
//   Unit      a run of top-level items of a parsed Program, and the file it came from (a file with imports is several units: its items before the
//             import, the imported file's units, then the rest). An AST node's ids are relative to its own Program, so the checker is told which unit it
//             is in (`Enter` in Checker.hpp) before it reads a function body.
//   Decls     structs, functions (free, extern and `Struct_method`), globals, interfaces and host types by name; the first declaration of a name wins,
//             as in the Nina checker's front-to-back search.
//   Scope     the names a function body can see, newest last: bind and unbind are an append and a truncate, lookup scans from the newest.
//   Session   all of the above plus the module (the checker's output) and the interner and type pool the names and types come from.
#pragma once

#include "front/Symbols.hpp"
#include "front/TypePool.hpp"
#include "polyir/Hir.hpp"

namespace polyir::front {

  // A run of top-level items of one parsed program (see the list above).
  struct Unit {
    const Program* program = nullptr;
    uint32_t first = 0;   // the items [first, last) of program.items
    uint32_t last = 0;
    uint32_t file = 0;    // an index into Session::files; 0 is "no file" (a program checked from memory, or the library)
    uint32_t instance = kNoIndex;  // an index into Session::instances: the unit is one generic declaration made concrete (Generics.cpp)
  };

  // A generic struct or extern type made concrete for one list of type arguments: `Box<i64>` is the struct `Box_i64`. The unit that holds it reads the
  // generic declaration with each type parameter standing for its argument, so types and bodies are resolved against `Session::active`.
  struct Instance {
    Symbol name;                 // `Box_i64`
    Vector<Symbol> params;       // `T`
    Vector<TypePtr> types;       // what each parameter stands for
    Vector<Symbol> spellings;    // how each argument is written in a mangled name (`i64`, `Box_i64`, or `t`)
    const Program* usedIn = nullptr;  // the program that wrote the use, and the type arguments as it wrote them
    Vector<TypeId> arguments;
    Vector<bool> named;          // whether each argument was written as a bare name (only then does `T[4]` keep its size)
    bool parametric = false;     // its types are type parameters (`Param`): a generic as declared, checked once for every type
  };

  int IndexOf(const Instance& instance, Symbol param);  // -1 when `param` is not one of the parameters

  // ── names ───────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────

  enum class BindingKind : uint8_t { Local, Slot, Global };  // a plain value; a `var` slot (loaded on every read); a global (the same)

  // A name a function body can see, and what it stands for.
  struct Binding {
    Symbol name;
    BindingKind kind = BindingKind::Local;
    TypePtr type;
  };

  // The names in scope, newest last (see the list above).
  struct Scope {
    Vector<Binding> bindings;
  };

  size_t Mark(const Scope& scope);

  void Reset(Scope& scope, size_t mark);

  void Clear(Scope& scope);

  void BindName(Scope& scope, Symbol name, BindingKind kind, const TypePtr& type);

  const Binding* Lookup(const Scope& scope, Symbol name);

  inline size_t Mark(const Scope& scope) {
    return scope.bindings.size();
  }

  inline void Reset(Scope& scope, size_t mark) {
    scope.bindings.erase(scope.bindings.begin() + static_cast<long>(mark), scope.bindings.end());
  }

  inline void Clear(Scope& scope) {
    scope.bindings.clear();
  }

  inline void BindName(Scope& scope, Symbol name, BindingKind kind, const TypePtr& type) {
    scope.bindings.push_back(Binding{name, kind, type});
  }

  // ── declarations ────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────

  // What the checker needs to know about a struct: its fields, in declaration order, and which can change.
  struct StructInfo {
    Symbol name;
    Vector<Symbol> fieldNames;
    Vector<TypePtr> fieldTypes;
    Vector<bool> fieldMutable;
    Vector<ExprPtr> fieldDefaults{};  // each field's default (`= literal`), null for none; a literal that leaves the field out takes it
  };

  // A free function, an `extern func`, a host member or a struct method (under its mangled name).
  struct FuncInfo {
    Vector<TypePtr> params;
    TypePtr result;
    mutable TypePtr asValue;  // the function's type when it is used as a value (`func(params)->result`), made on first use
  };

  // What the checker needs to know about an interface: each method's signature, by slot, and the structs that claim it.
  struct InterfaceInfo {
    Symbol name;
    Vector<Symbol> methodNames;
    Vector<Vector<TypePtr>> methodParams;
    Vector<TypePtr> methodResults;
    Vector<Symbol> implementers;
  };

  // An `extern type` and, for a specialisation such as `ArrayList_i64`, the generic declaration it was made from (`ArrayList`).
  struct HostTypeInfo {
    Symbol name;
    Symbol base;
  };

  // The declaration tables, by name (see the list above).
  struct Decls {
    SymbolMap<StructInfo> structs;
    SymbolMap<FuncInfo> funcs;
    SymbolMap<Binding> globals;
    SymbolMap<InterfaceInfo> interfaces;
    Vector<HostTypeInfo> hostTypes;
  };

  int IndexOf(const StructInfo& info, Symbol field);  // -1 when there is no such field

  int SlotOf(const InterfaceInfo& interface, Symbol method);  // -1 when there is no such method

  bool IsImplementedBy(const InterfaceInfo& interface, Symbol structName);

  const StructInfo* FindStruct(const Decls& decls, Symbol name);

  const FuncInfo* FindFunc(const Decls& decls, Symbol name);

  const InterfaceInfo* FindInterface(const Decls& decls, Symbol name);

  bool IsInterface(const Decls& decls, Symbol name);

  inline const StructInfo* FindStruct(const Decls& decls, Symbol name) {
    return Find(decls.structs, name);
  }

  inline const FuncInfo* FindFunc(const Decls& decls, Symbol name) {
    return Find(decls.funcs, name);
  }

  inline const InterfaceInfo* FindInterface(const Decls& decls, Symbol name) {
    return Find(decls.interfaces, name);
  }

  inline bool IsInterface(const Decls& decls, Symbol name) {
    return Contains(decls.interfaces, name);
  }

  // ── the check ───────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────

  // A function body to check: which unit and declaration it is, and for a struct method the struct (`owner`, whose `this` is its first parameter).
  struct FuncSource {
    uint32_t unit = 0;
    FuncId decl;
    Symbol name;               // the name in the module: `Struct_method` for a method
    Symbol owner;              // the struct of a method (symbol 0 for a free function)
    bool isMethod = false;
  };

  // Everything one check works on and builds (see the list above).
  // Who may use an internal declaration: the code of its module (and of the modules inside it), the methods of its struct, or, for one at
  // the top level, the code of its file.
  struct InternalUse {
    String module{};       // the module's path (`Geometry`, `Outer.Inner`), for one declared in a module
    Symbol owner{};        // the struct, for a method
    uint32_t file = 0;     // the file, for one at the top level
    String display{};      // how a message names it: `Geometry.clamp`, `Point.helper`
  };

  // A generic function (`func pick<T>(...)`), kept as written: each call makes a copy for its types (Generics.cpp GenericCall).
  struct GenericFunc {
    const Program* program = nullptr;
    uint32_t item = 0;    // its item in the program
    uint32_t file = 0;
  };

  struct Session {
    Interner& names;
    TypePool types;
    Decls decls{};
    Module module{};
    Vector<Unit> units{};
    Vector<String> files{};          // files[0] is empty
    Vector<FuncSource> sources{};    // one per FuncDef of the module, in the same order
    Vector<uint32_t> globalUnits{};  // the unit of each GlobalDef of the module, in the same order
    Vector<String> declProblems{};   // what is wrong in the declarations themselves (a field default that is no literal of its type)
    Vector<Instance> instances{};    // the generic declarations made concrete (units refer to them by index)
    const Instance* active = nullptr;  // the instance whose unit is being resolved or checked, if any
    HashMap<uint64_t, Symbol> mangledNames{};  // `Struct_method` by (struct, method), so each is interned once
    SymbolMap<bool> declared{};      // every struct, interface, function, extern and global name, as the parser gave it (`Geometry.Point`)
    SymbolMap<bool> modules{};       // every module path, as its declarations' names begin with it (`Geometry`, `Outer.Inner`)
    SymbolMap<GenericFunc> genericFuncs{};  // the generic functions, by name
    SymbolMap<InternalUse> internal{}; // each internal declaration (a method by its `Struct_method` name), and who may use it
    String inModule{};               // the module of the code being resolved or checked ("" at the top level)
    Symbol owner{};                  // the struct whose method is being checked, if any
    uint32_t file = 0;               // the file of the code being resolved or checked
  };

  Session MakeSession(Interner& interner);

  // ── modules and `internal` (Modules.cpp) ──

  void CollectDeclared(Session& session);  // fills `declared`, `modules` and `internal` from every unit, before anything is resolved

  // The code of `item` (in `unit`) is what is resolved or checked next: its module and file.
  void EnterItem(Session& session, const Unit& unit, const AstItem& item);

  Symbol InScope(const Session& session, Symbol written);  // what `written` names from the current module: `Point` in Geometry is `Geometry.Point`

  Symbol InScopeOf(const Session& session, const String& module, Symbol written);  // the same, from `module`

  Optional<Symbol> ModuleNamed(const Session& session, Symbol written);  // the module `written` names from the current module, if it is one

  Optional<String> InternalProblem(const Session& session, Symbol name);  // why the current code may not use `name`, or nothing

  String DisplayName(const Session& session, Symbol name);  // how a name reads in a message: its path, `Geometry.Point`

  Symbol Mangled(Session& session, Symbol owner, Symbol method);  // `Struct_method`, made once per pair

  StringView TextOf(const Session& session, Symbol symbol);

  String Str(const Session& session, Symbol symbol);  // the text of a symbol, as a String of its own

  inline StringView TextOf(const Session& session, Symbol symbol) {
    return TextOf(session.names, symbol);
  }

  inline String Str(const Session& session, Symbol symbol) {
    return String(TextOf(session.names, symbol));
  }

}  // namespace polyir::front
