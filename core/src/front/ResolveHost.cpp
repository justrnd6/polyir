// `extern type`, `extern interface` and `bridge` as declarations (HirResolveHost.nina): the host type itself, and for each member an extern function
// the checker calls like any function (`list.add(x)` finds `ArrayList_i64_add`) plus a host member that says how a host spells it.
#include "front/Resolve.hpp"
#include "front/HirBuild.hpp"

namespace polyir::front {

  Vector<String> BindingTexts(DeclContext& context, Range<AstHostBinding> binding) {
    Vector<String> out;

    for (const AstHostBinding& each : context.program.hostBindings[binding]) {
      out.push_back(Str(context.session, each.target.name) + "=" + String(StripQuotes(each.text.text)));
    }

    return out;
  }

  namespace {

    bool startsWith(StringView text, StringView prefix) {
      return text.substr(0, prefix.size()) == prefix;
    }

    // The generic `extern type` declaration an item is, or null.
    const AstExternType* genericAt(const Program& program, const AstItem& item) {
      if (item.kind != ItemKind::ExternType) {
        return nullptr;
      }

      const AstExternType& decl = program.externTypes[item.index];

      return Empty(decl.typeParams) ? nullptr : &decl;
    }

    // Whether `name` is `Base_...`, a specialization of the generic type `base`.
    bool specializes(DeclContext& context, Symbol name, Symbol base) {
      return startsWith(TextOf(context.session, name), String(TextOf(context.session, base)) + "_");
    }

    // A specialization `ArrayList_i64` of the generic `extern type ArrayList<T>` has the generic declaration's name as its base.
    Symbol baseOf(DeclContext& context, Symbol name) {
      for (const Unit& unit : context.session.units) {
        for (uint32_t i = unit.first; i < unit.last; i++) {
          const AstExternType* generic = genericAt(*unit.program, unit.program->items[i]);

          if (generic != nullptr && specializes(context, name, generic->name.name)) {
            return generic->name.name;
          }
        }
      }

      return name;
    }

    // The arguments of a specialization, read back from its name (`ArrayList_i64`: `i64`).
    Vector<TypePtr> argumentsOf(DeclContext& context, Symbol name, Symbol base) {
      Vector<TypePtr> out;

      if (name == base) {
        return out;
      }

      const String rest = Str(context.session, name).substr(TextOf(context.session, base).size() + 1);

      for (const String& part : SplitOn(rest, '_')) {
        out.push_back(Named(context.session.types, Intern(context.session.names, part)));
      }

      return out;
    }

    // How a host spells the member, for the back ends.
    HostMember hostMemberOf(DeclContext& context, Symbol owner, const AstHostMember& member, const String& function) {
      const String ownerText = Str(context.session, owner);
      const String memberText = Str(context.session, member.name.name);

      return HostMember{function, ownerText, memberText, member.isStatic, Valid(member.returnType), BindingTexts(context, member.binding)};
    }

    // The member as a function the checker can call; an instance member takes the object first, as `self`.
    ExternDef externOfMember(DeclContext& context, Symbol owner, const AstHostMember& member, const String& function) {
      Vector<Parameter> params;

      if (!member.isStatic) {
        params.push_back(Parameter{"self", StructOf(context.session.types, owner)});
      }

      for (Parameter& parameter : ParametersOf(context, member.params)) {
        params.push_back(move(parameter));
      }

      return ExternDef{function, move(params), Valid(member.returnType) ? TypeOf(context, member.returnType) : VoidType()};
    }

    // A member is declared twice: as the extern function `Owner_member`, and as the host member that says how a host spells it.
    void memberDecls(DeclContext& context, Symbol owner, const AstHostMember& member) {
      const Symbol function = Mangled(context.session, owner, member.name.name);
      const String text = Str(context.session, function);
      AddExtern(context.session, function, externOfMember(context, owner, member, text));
      AddHostMember(context.session, hostMemberOf(context, owner, member, text));
    }

    // The host type of an `extern type` (or of one specialisation of a generic one, whose arguments `base` tells).
    HostType hostTypeOf(DeclContext& context, const AstExternType& decl, Symbol name, Symbol base) {
      const String nameText = Str(context.session, name);
      const String baseText = Str(context.session, base);

      return HostType{nameText, baseText, argumentsOf(context, name, base), false, BindingTexts(context, decl.binding)};
    }

    // A bridge rule's template; a rule that is not written is `$`, the value as it is.
    String ruleText(DeclContext& context, const AstBridge& decl, StringView direction) {
      for (const AstBridgeRule& rule : context.program.bridgeRules[decl.rules]) {
        if (TextOf(context.session, rule.direction.name) == direction) {
          return String(StripQuotes(rule.text.text));
        }
      }

      return "$";
    }

  }  // namespace

  // The host type, then one extern function and one host member per member. A generic extern type is declared only as its instances.
  void ResolveExternType(DeclContext& context, const AstExternType& decl) {
    if (!Empty(decl.typeParams) && context.unit.instance == kNoIndex) {
      return;
    }

    const Symbol name = Owner(context, decl.name.name);
    const Symbol base = baseOf(context, name);
    AddHostType(context.session, hostTypeOf(context, decl, name, base), HostTypeInfo{name, base});

    for (const AstHostMember& member : context.program.hostMembers[decl.members]) {
      memberDecls(context, name, member);
    }
  }

  // An interface is also an ordinary one, so a struct that claims it converts to it and calls go through it.
  void ResolveExternInterface(DeclContext& context, const AstExternInterface& decl) {
    const String name = Str(context.session, decl.name.name);
    HostType host{name, name, {}, true, BindingTexts(context, decl.binding)};
    AddHostType(context.session, move(host), HostTypeInfo{decl.name.name, decl.name.name});
    ResolveInterface(context, decl.name, decl.methods);
  }

  // A host import as the HIR keeps it; `import java` was already turned into declarations.
  void ResolveImport(DeclContext& context, const AstImport& decl) {
    if (!Known(decl.lang.pos)) {
      return;
    }

    const String target = Str(context.session, decl.lang.name);

    if (target == "java") {
      return;
    }

    const String alias = Known(decl.alias.pos) ? Str(context.session, decl.alias.name) : String();
    AddHostImport(context.session, HostImport{target, String(StripQuotes(decl.path.text)), alias});
  }

  // `bridge T <-> java "long[]" { toHost = "...", fromHost = "..." }`: how a value crosses to the host and back.
  void ResolveBridge(DeclContext& context, const AstBridge& decl) {
    const String target = Str(context.session, decl.target.name);
    const TypePtr type = TypeOf(context, decl.poly);
    Bridge bridge{type, target, String(StripQuotes(decl.host.text)), ruleText(context, decl, "toHost"), ruleText(context, decl, "fromHost")};
    AddBridge(context.session, move(bridge));
  }

}  // namespace polyir::front
