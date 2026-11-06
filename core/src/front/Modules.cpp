// Modules and `internal` in the checker (Modules.nina): which declaration a written name stands for in the code being checked, and
// whether that code may use it.
//
// The parser names a module's declarations by their path (`Geometry.Point`). Inside `module Geometry` a bare `Point` is
// `Geometry.Point`: a name is tried in the code's own module first, then in each module around it, then at the top level. An
// `internal` declaration may be used only by the code around it: its module (and the modules inside it), its struct's methods, or,
// at the top level, its file.
#include "front/Session.hpp"

namespace polyir::front {

  namespace {

    // The module around `path` (`Outer` for `Outer.Inner`), empty at the top.
    String parentOf(const String& path) {
      const size_t at = path.rfind('.');

      return at == String::npos ? String() : path.substr(0, at);
    }

    void addModule(Session& session, const String& path) {
      for (String part = path; !part.empty(); part = parentOf(part)) {
        Insert(session.modules, Intern(session.names, part), true);
      }
    }

    // A declaration's name, its module, and whether it is internal; `owner` for a method.
    void declare(Session& session, uint32_t file, Symbol name, const String& module, bool isInternal) {
      Insert(session.declared, name, true);
      addModule(session, module);

      if (isInternal) {
        Insert(session.internal, name, InternalUse{.module = module, .owner = Symbol(), .file = module.empty() ? file : 0, .display = DisplayName(session, name)});
      }
    }

    void internalMethods(Session& session, const Program& program, const AstStructDecl& decl) {
      for (const AstMember& member : program.members[decl.members]) {
        const AstFuncDecl& method = program.funcs[member.method];

        if (member.isMethod && method.isInternal) {
          const String display = DisplayName(session, decl.name.name) + "." + Str(session, method.name.name);
          Insert(session.internal, Mangled(session, decl.name.name, method.name.name), InternalUse{.module = "", .owner = decl.name.name, .file = 0, .display = display});
        }
      }
    }

    // A function, and a generic one kept as written.
    void declareFunc(Session& session, const Unit& unit, const AstItem& item) {
      const AstFuncDecl& decl = unit.program->funcs.items[item.index];
      declare(session, unit.file, decl.name.name, decl.module, decl.isInternal);

      if (!Empty(decl.typeParams)) {
        Insert(session.genericFuncs, decl.name.name, GenericFunc{unit.program, static_cast<uint32_t>(&item - unit.program->items.data()), unit.file});
      }
    }

    void declareStruct(Session& session, const Unit& unit, const AstStructDecl& decl) {
      declare(session, unit.file, decl.name.name, decl.module, decl.isInternal);
      internalMethods(session, *unit.program, decl);
    }

    void collectItem(Session& session, const Unit& unit, const AstItem& item) {
      const Program& program = *unit.program;

      switch (item.kind) {
        case ItemKind::Struct:
          return declareStruct(session, unit, program.structs[item.index]);
        case ItemKind::Interface:
          return declare(session, unit.file, program.interfaces[item.index].name.name, program.interfaces[item.index].module, program.interfaces[item.index].isInternal);
        case ItemKind::Func:
        case ItemKind::CoroutineFunc:
          return declareFunc(session, unit, item);
        default:
          return;
      }
    }

    // Whether the current code is inside `use`'s reach.
    bool mayUse(const Session& session, const InternalUse& use) {
      if (use.owner != Symbol()) {
        return session.owner == use.owner;
      }

      if (!use.module.empty()) {
        return session.inModule == use.module || session.inModule.rfind(use.module + ".", 0) == 0;
      }

      return session.file == use.file;
    }

    // Who may use it, for a message: `code inside module Geometry`, `Point's own methods`, `code in its own file`.
    String whose(const Session& session, const InternalUse& use) {
      if (use.owner != Symbol()) {
        return DisplayName(session, use.owner) + "'s own methods";
      }

      return use.module.empty() ? String("code in its own file") : "code inside module " + DisplayName(session, Intern(session.names, use.module));
    }

  }  // namespace

  void CollectDeclared(Session& session) {
    for (const Unit& unit : session.units) {
      for (uint32_t i = unit.first; i < unit.last; i++) {
        collectItem(session, unit, unit.program->items[i]);
      }
    }
  }

  namespace {

    // Where names in an item are looked up from. For a struct that is its own path, so its fields and methods see what is declared in it.
    // For an interface or a function it is the module it is declared in; any other item is at the top level.
    String moduleOfItem(const Program& program, const AstItem& item) {
      switch (item.kind) {
        case ItemKind::Struct:
          return String(TextOf(*program.names, program.structs[item.index].name.name));
        case ItemKind::Interface:
          return program.interfaces[item.index].module;
        case ItemKind::Func:
        case ItemKind::CoroutineFunc:
          return program.funcs.items[item.index].module;
        default:
          return String();
      }
    }

  }  // namespace

  void EnterItem(Session& session, const Unit& unit, const AstItem& item) {
    session.file = unit.file;
    session.owner = Symbol();
    session.inModule = moduleOfItem(*unit.program, item);
  }

  Symbol InScopeOf(const Session& session, const String& module, Symbol written) {
    for (String path = module; !path.empty(); path = parentOf(path)) {
      const Symbol candidate = Intern(session.names, path + "." + Str(session, written));

      if (Contains(session.declared, candidate)) {
        return candidate;
      }
    }

    return written;
  }

  Symbol InScope(const Session& session, Symbol written) {
    return InScopeOf(session, session.inModule, written);
  }

  Optional<Symbol> ModuleNamed(const Session& session, Symbol written) {
    for (String path = session.inModule; !path.empty(); path = parentOf(path)) {
      const Symbol candidate = Intern(session.names, path + "." + Str(session, written));

      if (Contains(session.modules, candidate)) {
        return candidate;
      }
    }

    return Contains(session.modules, written) ? Optional<Symbol>(written) : nullopt;
  }

  Optional<String> InternalProblem(const Session& session, Symbol name) {
    const InternalUse* use = Find(session.internal, name);

    if (use == nullptr || mayUse(session, *use)) {
      return nullopt;
    }

    return use->display + " is internal: only " + whose(session, *use) + " can use it";
  }

  String DisplayName(const Session& session, Symbol name) {
    return Str(session, name);
  }

}  // namespace polyir::front
