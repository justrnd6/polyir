// What is wrong with the declarations of things a HOST language provides (lang/src/ExternTypes.nina `problems`): `extern type`, `extern interface`,
// the binding of an `extern func`, and `bridge`. A binding names, per target, how the host spells the thing; the declarations themselves are resolved in
// ResolveHost.cpp, and the generic extern types are specialised with the generic structs (Generics.cpp). The messages carry the file of the declaration
// (the first declaration of that name, as the Nina origins find it) when the program came from files.
#include "front/Pipeline.hpp"
#include "front/HirBuild.hpp"

namespace polyir::front {

  namespace {

    // The targets a binding can name.
    const Vector<String> kTargets = {"native", "java", "kotlin", "js", "swift", "cpp", "python", "rust", "objc", "c", "wasm"};

    String knownTargets() {
      String out;

      for (const String& target : kTargets) {
        out += (out.empty() ? "" : ", ") + target;
      }

      return out;
    }

    bool isTarget(const String& name) {
      return IsOneOf(name, kTargets);
    }

    // The name a struct, interface or host type is declared under (they share one namespace).
    Optional<Symbol> typeName(const Program& program, const AstItem& item) {
      if (item.kind == ItemKind::Struct) {
        return program.structs[item.index].name.name;
      }

      if (item.kind == ItemKind::Interface) {
        return program.interfaces[item.index].name.name;
      }

      if (item.kind == ItemKind::ExternType) {
        return program.externTypes[item.index].name.name;
      }

      if (item.kind == ItemKind::ExternInterface) {
        return program.externInterfaces[item.index].name.name;
      }

      return nullopt;
    }

    Optional<Symbol> valueName(const Program& program, const AstItem& item) {
      if (item.kind == ItemKind::Func || item.kind == ItemKind::CoroutineFunc) {
        return program.funcs.items[item.index].name.name;
      }

      if (item.kind == ItemKind::ExternFunc) {
        return program.externFuncs[item.index].name.name;
      }

      if (item.kind == ItemKind::Global) {
        return program.globals[item.index].name.name;
      }

      return nullopt;
    }

    // The name an item is declared under, for the kinds that have an origin; none for the rest.
    Optional<Symbol> originName(const Program& program, const AstItem& item) {
      const Optional<Symbol> type = typeName(program, item);

      return type ? type : valueName(program, item);
    }

    // One declaration being checked: where its messages go and the file in front of them.
    struct Report {
      const Session& session;
      Vector<String>& out;
      String file;
    };

    // One message, at `at`, with the report's file in front.
    void add(const Report& report, const String& what, Pos at) {
      const String message = DiagnosticMessage(what, SpanAt(at));
      report.out.push_back(report.file.empty() ? message : report.file + ":" + message);
    }

    // ── bindings ────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────

    // One `target "text"` of a binding: a known target, bound once, to some text.
    void oneTarget(const Report& report, const Program& program, const AstHostBinding& binding, Vector<Symbol>& seen) {
      const String target = String(TextOf(program, binding.target.name));

      if (!isTarget(target)) {
        return add(report, "unknown target '" + target + "' (known: " + knownTargets() + ")", binding.target.pos);
      }

      if (IsOneOf(binding.target.name, seen)) {
        return add(report, "target '" + target + "' is bound twice", binding.target.pos);
      }

      if (binding.text.text.size() <= 2) {
        add(report, "the " + target + " binding is empty", binding.text.pos);
      }
    }

    void bindingProblems(const Report& report, const Program& program, Range<AstHostBinding> binding) {
      Vector<Symbol> seen;

      for (const AstHostBinding& each : program.hostBindings[binding]) {
        oneTarget(report, program, each, seen);
        seen.push_back(each.target.name);
      }
    }

    void needsBinding(const Report& report, Range<AstHostBinding> binding, const char* what, Ident name, const Program& program) {
      if (!Empty(binding)) {
        return;
      }

      add(report, String(what) + " " + String(TextOf(program, name.name)) + " needs a binding, e.g. = java \"pkg.Name\"", name.pos);
    }

    // ── names: structs, interfaces and host types share one namespace ───────────────────────────────────────────────────────────────────────────────

    // How many of a unit's items declare a type called `name`.
    int typeNamesIn(const Unit& unit, Symbol name) {
      int count = 0;

      for (uint32_t i = unit.first; i < unit.last; i++) {
        count += typeName(*unit.program, unit.program->items[i]) == name ? 1 : 0;
      }

      return count;
    }

    int typeNameCount(const Session& session, Symbol name) {
      int count = 0;

      for (const Unit& unit : session.units) {
        count += typeNamesIn(unit, name);
      }

      return count;
    }

    void nameTaken(const Report& report, Ident name) {
      if (typeNameCount(report.session, name.name) < 2) {
        return;
      }

      add(report, "duplicate struct, interface or extern type '" + String(TextOf(report.session, name.name)) + "'", name.pos);
    }

    // ── extern types and interfaces ─────────────────────────────────────────────────────────────────────────────────────────────────────────────

    void repeatedParams(const Report& report, const Program& program, const AstExternType& decl) {
      Vector<Symbol> seen;
      const String owner = "extern type " + String(TextOf(program, decl.name.name));

      for (const Ident& each : program.idents[decl.typeParams]) {
        if (IsOneOf(each.name, seen)) {
          add(report, owner + " declares type parameter " + String(TextOf(program, each.name)) + " twice", each.pos);
        }

        seen.push_back(each.name);
      }
    }

    String memberTwice(const String& owner, const String& member) {
      return owner + " declares " + member + " twice (give a host overload its own name and bind it: = java \"hostName\")";
    }

    // Each member declared once, and each member's binding sound.
    void memberProblems(const Report& report, const Program& program, const AstExternType& decl) {
      Vector<Symbol> seen;
      const String owner(TextOf(program, decl.name.name));

      for (const AstHostMember& member : program.hostMembers[decl.members]) {
        if (IsOneOf(member.name.name, seen)) {
          add(report, memberTwice(owner, String(TextOf(program, member.name.name))), member.name.pos);
        }

        seen.push_back(member.name.name);
        bindingProblems(report, program, member.binding);
      }
    }

    void externTypeItem(const Report& report, const Program& program, const AstExternType& decl) {
      nameTaken(report, decl.name);
      needsBinding(report, decl.binding, "extern type", decl.name, program);
      bindingProblems(report, program, decl.binding);
      repeatedParams(report, program, decl);
      memberProblems(report, program, decl);
    }

    void repeatedMethods(const Report& report, const Program& program, const AstExternInterface& decl) {
      Vector<Symbol> seen;
      const String owner = "extern interface " + String(TextOf(program, decl.name.name));

      for (const AstInterfaceMethod& method : program.interfaceMethods[decl.methods]) {
        if (IsOneOf(method.name.name, seen)) {
          add(report, owner + " declares " + String(TextOf(program, method.name.name)) + " twice", method.name.pos);
        }

        seen.push_back(method.name.name);
      }
    }

    void externInterfaceItem(const Report& report, const Program& program, const AstExternInterface& decl) {
      nameTaken(report, decl.name);
      needsBinding(report, decl.binding, "extern interface", decl.name, program);
      bindingProblems(report, program, decl.binding);
      repeatedMethods(report, program, decl);
    }

    // ── bridges ─────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────

    void ruleProblems(const Report& report, const Program& program, const AstBridgeRule& rule, Vector<Symbol>& seen) {
      const String direction = String(TextOf(program, rule.direction.name));
      const bool isDirection = direction == "toHost" || direction == "fromHost";

      if (!isDirection) {
        return add(report, "a bridge rule is toHost or fromHost, not '" + direction + "'", rule.direction.pos);
      }

      if (IsOneOf(rule.direction.name, seen)) {
        return add(report, direction + " is written twice", rule.direction.pos);
      }

      if (rule.text.text.find('$') == StringView::npos) {
        add(report, "a bridge template needs $ for the value", rule.text.pos);
      }
    }

    void bridgeRules(const Report& report, const Program& program, const AstBridge& decl) {
      Vector<Symbol> seen;

      for (const AstBridgeRule& rule : program.bridgeRules[decl.rules]) {
        ruleProblems(report, program, rule, seen);
        seen.push_back(rule.direction.name);
      }
    }

    void bridgeItem(const Report& report, const Program& program, const AstBridge& decl) {
      const String target = String(TextOf(program, decl.target.name));

      if (!isTarget(target)) {
        add(report, "unknown target '" + target + "' (known: " + knownTargets() + ")", decl.target.pos);
      }

      if (decl.host.text.size() <= 2) {
        add(report, "a bridge needs the host type it bridges to", decl.host.pos);
      }

      bridgeRules(report, program, decl);
    }

    void externFuncItem(const Report& report, const Program& program, const AstExternFunc& decl) {
      bindingProblems(report, program, decl.binding);
    }

    // ── host imports: `import c "zlib.h"`, `import js "crc-32" as crc` ───────────────────────────────────────────────────────────────────────────────

    // The targets a host import is for (TypeScript writes the js ones; `import java` writes declarations). `native` names a library the targets polyir
    // builds itself load or link (`import native "z"`).
    const Vector<String> kImportTargets = {"native", "c", "cpp", "objc", "swift", "js", "python", "kotlin", "rust"};

    bool importsFor(const String& lang) {
      return lang == "java" || IsOneOf(lang, kImportTargets);
    }

    // Only a module of js or python is a value the program names (`crc.str(s)`); a header or crate is not.
    void aliasProblems(const Report& report, const Program& program, const AstImport& decl, const String& lang) {
      if (TextOf(program, decl.asWord.name) != "as") {
        return add(report, "expected `as` before the import's name", decl.asWord.pos);
      }

      if (lang != "js" && lang != "python") {
        add(report, "import " + lang + " takes no `as` name: only js and python name a module", decl.asWord.pos);
      }
    }

    void importItem(const Report& report, const Program& program, const AstImport& decl) {
      if (!Known(decl.lang.pos)) {
        return;
      }

      const String lang = String(TextOf(program, decl.lang.name));
      const String forWhat = ": an import is for native (a library), c, cpp, objc, swift, js, python, kotlin or rust";

      if (!importsFor(lang)) {
        return add(report, "import " + lang + forWhat, decl.lang.pos);
      }

      if (Known(decl.asWord.pos)) {
        aliasProblems(report, program, decl, lang);
      }
    }

    // The problems of an item that is an extern type, an extern interface or an extern function.
    void externItemProblems(const Report& report, const Program& program, const AstItem& item) {
      if (item.kind == ItemKind::ExternType) {
        externTypeItem(report, program, program.externTypes[item.index]);
      }

      if (item.kind == ItemKind::ExternInterface) {
        externInterfaceItem(report, program, program.externInterfaces[item.index]);
      }

      if (item.kind == ItemKind::ExternFunc) {
        externFuncItem(report, program, program.externFuncs[item.index]);
      }
    }

    // The problems of one item; a bridge has no file in front, the other kinds the file of their name's first declaration.
    void itemProblems(const Session& session, const Program& program, const AstItem& item, Vector<String>& out) {
      const Optional<Symbol> name = originName(program, item);
      const Report report{session, out, name ? FileOf(session, *name) : String()};
      externItemProblems(report, program, item);

      if (item.kind == ItemKind::Bridge) {
        bridgeItem(Report{session, out, String()}, program, program.bridges[item.index]);
      }

      if (item.kind == ItemKind::Import) {
        importItem(Report{session, out, String()}, program, program.imports[item.index]);
      }
    }

  }  // namespace

  // The file of the first declaration called `name`; empty when the program is not from files.
  String FileOf(const Session& session, Symbol name) {
    for (const Unit& unit : session.units) {
      for (uint32_t i = unit.first; i < unit.last; i++) {
        if (originName(*unit.program, unit.program->items[i]) == name) {
          return session.files[unit.file];
        }
      }
    }

    return String();
  }

  Vector<String> ExternTypeProblems(Session& session) {
    Vector<String> out;

    for (const Unit& unit : session.units) {
      for (uint32_t i = unit.first; i < unit.last; i++) {
        itemProblems(session, *unit.program, unit.program->items[i], out);
      }
    }

    return out;
  }

}  // namespace polyir::front
