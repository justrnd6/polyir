// Declarations: the part of the Nina checker that turns types and top-level items into HIR (HirResolve.nina, HirResolveHost.nina, StructMethods.nina).
// `ResolveDeclarations` fills the session's module with every struct, interface, function signature, extern, global, host type and bridge in
// source order -- struct methods follow their struct as mangled `Struct_method` functions with `this` first -- and the session's tables with the
// same, so a body can then call any of them, forward or mutually recursive.
#pragma once

#include "front/Session.hpp"

namespace polyir::front {

  // What resolving the declarations of one unit needs: the session, the unit and the program it reads.
  struct DeclContext {
    Session& session;
    const Unit& unit;
    uint32_t unitIndex;
    const Program& program;
  };

  // The annotations written before a declaration (Annotations.cpp); an argument that is no literal is a problem of the declaration.
  Vector<Annotation> AnnotationsOf(DeclContext& context, Range<AstAnnotation> written);

  // A field's default (`= literal`) as a constant of its type (FieldDefaults.cpp); null with `problem` (`line:col: message`) when it is
  // no literal of that type.
  ExprPtr FieldDefault(const Program& program, ExprId id, const TypePtr& type, String& problem);

  // The HIR type of a syntax type (`mutarray<i32>`), read with the active instance's type arguments.
  TypePtr ResolveType(Session& session, const Program& program, TypeId id);

  String MangledTypeText(Session& session, const Program& program, TypeId id);  // one type argument as a mangled name writes it

  // The struct a `Name<A, B>` type means: `Name_A_B` (each argument spelled by its name; anything that is not a name is `t`).
  Symbol MangledGenericName(Session& session, const Program& program, const AstType& type);

  void ResolveDeclarations(Session& session);

  // Resolves the declarations of one unit (a generic function's copy is one, made while bodies are being checked).
  void ResolveUnit(Session& session, uint32_t unitIndex);

  // The HIR type of a syntax type written in this unit.
  TypePtr TypeOf(const DeclContext& context, TypeId id);

  Symbol Owner(const DeclContext& context, Symbol declared);

  // Parameter lists (Resolve.cpp): `names` and `types` of an AstParam range.
  Vector<Parameter> ParametersOf(DeclContext& context, Range<AstParam> params);

  // Resolves one `extern type` (ResolveHost.cpp).
  void ResolveExternType(DeclContext& context, const AstExternType& decl);

  // Resolves one `extern interface` (ResolveHost.cpp).
  void ResolveExternInterface(DeclContext& context, const AstExternInterface& decl);

  // Resolves one `bridge` (ResolveHost.cpp).
  void ResolveBridge(DeclContext& context, const AstBridge& decl);

  // Resolves one host `import` (ResolveHost.cpp).
  void ResolveImport(DeclContext& context, const AstImport& decl);

  // Resolves one `interface` (Resolve.cpp).
  void ResolveInterface(DeclContext& context, Ident name, Range<AstInterfaceMethod> methods);

  Vector<String> BindingTexts(DeclContext& context, Range<AstHostBinding> binding);  // "java=java.util.ArrayList" for each target

  // Adding a declaration: each `Add...` function below appends to the module (and its `order`). All but AddBridge, AddHostImport and
  // AddHostMember also register the name for lookup.
  void AddStruct(Session& session, StructDef definition, StructInfo info);

  void AddFunction(Session& session, FuncDef definition, FuncSource source);

  void AddExtern(Session& session, Symbol name, ExternDef definition);

  void AddGlobal(Session& session, uint32_t unit, Symbol name, GlobalDef definition);

  void AddInterface(Session& session, InterfaceDef definition, InterfaceInfo info);

  void AddHostType(Session& session, HostType definition, HostTypeInfo info);

  void AddBridge(Session& session, Bridge definition);

  void AddHostImport(Session& session, HostImport definition);

  void AddHostMember(Session& session, HostMember definition);

  inline TypePtr TypeOf(const DeclContext& context, TypeId id) {
    return ResolveType(context.session, context.program, id);
  }

  // The name a struct or extern type of this unit is declared under: its instance's for a generic made concrete, else its own.
  inline Symbol Owner(const DeclContext& context, Symbol declared) {
    return context.unit.instance == kNoIndex ? declared : context.session.instances[context.unit.instance].name;
  }

}  // namespace polyir::front
