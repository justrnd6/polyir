// The top-level declarations: struct, interface, func, coro func, extern type / interface / func, bridge, import, global, meta.
// Each one is parsed into its own Ast record (AstStructDecl, AstExternType, AstImport, ...) and stored with AddDecl, which also records the
// program's item order. `module Geometry { ... }` holds structs, interfaces, functions and modules: each is named with its module
// (`Geometry.Point`) and remembers it. `internal` before one keeps it to the code around it.
#include "front/Parser.hpp"
#include "front/ParserSets.hpp"

namespace polyir::front {

  namespace {

    // The marks a kernel parameter can carry; the caller has checked the token is one of the four (kParamMarks).
    ParamMark markOf(TokenKind kind) {
      switch (kind) {
        case TokenKind::UNIFORM:
          return ParamMark::Uniform;
        case TokenKind::INDEX:
          return ParamMark::Index;
        case TokenKind::WRITABLE:
          return ParamMark::Writable;
        default:
          return ParamMark::Workgroup;
      }
    }

    // `global n: i64 = 42` or `= 1.5`: the literal is kept as written; its kind says how to read it.
    void parseNumberInit(Parser& parser, AstGlobal& global) {
      global.init = At(parser, K::INT_LIT) ? GlobalInit::Int : GlobalInit::Float;
      global.value = LiteralOf(parser, Take(parser));
    }

  }  // namespace

  // ── modules ──────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────

  namespace {

    bool atWord(const Parser& parser, StringView word) {
      return At(parser, K::IDENT) && TokenText(Current(parser), parser.source) == word;
    }

    bool isInternalizable(TokenKind kind) {
      return kind == K::FUNC || kind == K::CORO || kind == K::KERNEL || kind == K::STRUCT || kind == K::INTERFACE || kind == K::STATIC;
    }

  }  // namespace

  bool AtModule(const Parser& parser) {
    return atWord(parser, "module") && KindAt(parser, 1) == K::IDENT && KindAt(parser, 2) == K::LBRACE;
  }

  bool AtInternal(const Parser& parser) {
    return atWord(parser, "internal") && isInternalizable(KindAt(parser, 1));
  }

  String ModulePath(const Parser& parser) {
    String path;

    for (const String& module : parser.modules) {
      path += (path.empty() ? "" : ".") + module;
    }

    return path;
  }

  Symbol InModule(Parser& parser, const String& module, Symbol name) {
    return Intern(*parser.program.names, module + "." + String(TextOf(*parser.program.names, name)));
  }

  bool AtItemStart(const Parser& parser) {
    return AtAny(parser, kTopLevelStart) || AtModule(parser) || AtInternal(parser) || At(parser, K::AT);
  }

  // `module Name { ... }`: what is inside is parsed as at the top level, named with the module.
  void ParseModule(Parser& parser) {
    Take(parser);
    parser.modules.push_back(String(TokenText(Take(parser), parser.source)));
    Expect(parser, K::LBRACE);

    while (AtItemStart(parser)) {
      ParseTopLevel(parser);
    }

    Expect(parser, K::RBRACE);
    parser.modules.pop_back();
  }

  // ── annotations ──────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────

  namespace {

    // `@Name` or `@Name(key: value, ...)`.
    AstAnnotation parseAnnotation(Parser& parser) {
      AstAnnotation annotation;
      Expect(parser, K::AT);
      annotation.name = ExpectIdent(parser);

      if (Accept(parser, K::LPAREN)) {
        annotation.args = OptionalCommaList(parser, kIdentStart, parser.scratch.inits, parser.program.inits, [&] { return ParseFieldInit(parser); });
        Expect(parser, K::RPAREN);
      }

      return annotation;
    }

  }  // namespace

  void ParseAnnotations(Parser& parser) {
    const size_t mark = parser.scratch.annotations.size();

    while (At(parser, K::AT)) {
      parser.scratch.annotations.push_back(parseAnnotation(parser));
    }

    parser.annotations = Take(parser.program.annotations, parser.scratch.annotations, mark);
  }

  namespace {

    // What was read before a declaration, for it: an `internal` and the annotations.
    struct Pending {
      bool internal = false;
      Range<AstAnnotation> annotations;
    };

    // The pending marks, taken so a declaration read inside this one (a nested struct) does not take them.
    Pending takePending(Parser& parser) {
      const Pending pending{parser.internal, parser.annotations};
      parser.internal = false;
      parser.annotations = {};

      return pending;
    }

    // A struct, interface or function takes the module (or struct) it sits in, its outer struct and the marks read before it.
    template <typename Decl>
    void place(Parser& parser, Decl& decl, const Pending& pending) {
      decl.isInternal = pending.internal;
      decl.annotations = pending.annotations;
      decl.module = ModulePath(parser);
      decl.outer = parser.outer;

      if (!decl.module.empty()) {
        decl.name.name = InModule(parser, decl.module, decl.name.name);
      }
    }

  }  // namespace

  // A struct as an item, then what `@Builder` makes for it.
  void AddStruct(Parser& parser, AstStructDecl decl) {
    const bool builder = HasAnnotation(parser, decl.annotations, "Builder");
    const AstStructDecl kept = decl;
    AddDecl(parser, parser.program.structs, move(decl), ItemKind::Struct);

    if (builder) {
      ExpandBuilder(parser, kept);
    }
  }

  // ── the dispatch on the first token ─────────────────────────────────────────────────────────────────────────────────────────────────────

  namespace {

    // A whole `module Name { ... }` (then true), or the `internal` before a declaration, which `place` gives it.
    bool takeModuleOrInternal(Parser& parser) {
      if (AtModule(parser)) {
        ParseModule(parser);

        return true;
      }

      if (AtInternal(parser)) {
        Take(parser);
        parser.internal = true;
      }

      return false;
    }

  }  // namespace

  namespace {

    bool atAnnotatable(const Parser& parser) {
      return AtAny(parser, kFuncStart) || At(parser, K::CORO) || At(parser, K::STRUCT) || At(parser, K::INTERFACE) || AtInternal(parser);
    }

  }  // namespace

  namespace {

    // The annotations before an item (only a struct, an interface or a function takes them), then a whole module or the `internal`
    // before a declaration. True when that was the whole item.
    bool takeMarks(Parser& parser) {
      ParseAnnotations(parser);

      if (!Empty(parser.annotations) && !atAnnotatable(parser)) {
        Fail(parser, "a struct, an interface or a function after an annotation");

        return true;
      }

      return takeModuleOrInternal(parser);
    }

  }  // namespace

  void ParseTopLevel(Parser& parser) {
    if (takeMarks(parser)) {
      return;
    }

    if (AtAny(parser, kFuncStart) || At(parser, K::CORO)) {
      return ParseFuncLevel(parser);
    }

    if (At(parser, K::STRUCT) || At(parser, K::INTERFACE) || At(parser, K::BRIDGE)) {
      return ParseTypeLevel(parser);
    }

    if (At(parser, K::EXTERN)) {
      return ParseExternLevel(parser);
    }

    ParseSimpleLevel(parser);
  }

  void ParseFuncLevel(Parser& parser) {
    const Pending pending = takePending(parser);
    const ItemKind kind = At(parser, K::CORO) ? ItemKind::CoroutineFunc : ItemKind::Func;
    AstFuncDecl decl = ParseFunc(parser);
    place(parser, decl, pending);
    AddDecl(parser, parser.program.funcs.items, move(decl), kind);
  }

  void ParseTypeLevel(Parser& parser) {
    const Pending pending = takePending(parser);

    if (At(parser, K::INTERFACE)) {
      AstInterfaceDecl decl = ParseInterface(parser);
      place(parser, decl, pending);

      return AddDecl(parser, parser.program.interfaces, move(decl), ItemKind::Interface);
    }

    if (At(parser, K::BRIDGE)) {
      return AddDecl(parser, parser.program.bridges, ParseBridge(parser), ItemKind::Bridge);
    }

    AstStructDecl decl = ParseStruct(parser);
    place(parser, decl, pending);
    AddStruct(parser, move(decl));
  }

  // `extern type`, `extern interface` or `extern func`, told apart by the token after `extern` (the grammar's two predicates).
  void ParseExternLevel(Parser& parser) {
    if (KindAt(parser, 1) == K::TYPE) {
      return AddDecl(parser, parser.program.externTypes, ParseExternType(parser), ItemKind::ExternType);
    }

    if (KindAt(parser, 1) == K::INTERFACE) {
      return AddDecl(parser, parser.program.externInterfaces, ParseExternInterface(parser), ItemKind::ExternInterface);
    }

    AddDecl(parser, parser.program.externFuncs, ParseExternFunc(parser), ItemKind::ExternFunc);
  }

  void ParseSimpleLevel(Parser& parser) {
    if (At(parser, K::IMPORT)) {
      return AddDecl(parser, parser.program.imports, ParseImport(parser), ItemKind::Import);
    }

    if (At(parser, K::GLOBAL)) {
      return AddDecl(parser, parser.program.globals, ParseGlobal(parser), ItemKind::Global);
    }

    AddDecl(parser, parser.program.metas, ParseMeta(parser), ItemKind::Meta);
  }

  // ── struct ──────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────

  AstStructDecl ParseStruct(Parser& parser) {
    AstStructDecl decl;
    ParseStructHead(parser, decl);
    ParseStructBody(parser, decl);
    Accept(parser, K::SEMI);

    return decl;
  }

  void ParseStructHead(Parser& parser, AstStructDecl& decl) {
    decl.pos = PosOf(parser, Expect(parser, K::STRUCT));
    decl.name = ExpectIdent(parser);
    decl.typeParams = ParseTypeParams(parser);
    decl.primary = ParsePrimaryFields(parser);
    decl.impls = ParseImpls(parser);
  }

  namespace {

    bool atMemberStart(const Parser& parser) {
      return AtAny(parser, kStructMemberStart) || At(parser, K::STRUCT) || At(parser, K::STATIC) || At(parser, K::AT);
    }

  }  // namespace

  // `{ fields, methods, nested structs and static functions }`, or nothing. What is declared in the body is named through the struct
  // (`Point.Builder`), as a module names its declarations.
  void ParseStructBody(Parser& parser, AstStructDecl& decl) {
    if (!Accept(parser, K::LBRACE)) {
      return;
    }

    const size_t mark = parser.scratch.members.size();
    const String outer = parser.outer;
    parser.modules.push_back(String(TextOf(*parser.program.names, decl.name.name)));
    parser.outer = ModulePath(parser);

    while (atMemberStart(parser)) {
      ParseMember(parser);
    }

    parser.modules.pop_back();
    parser.outer = outer;
    decl.members = Take(parser.program.members, parser.scratch.members, mark);
    Expect(parser, K::RBRACE);
  }

  // A struct member is a method when it starts like a function, else a field. An `internal` before a method keeps it for the struct's
  // own methods.
  namespace {

    // `struct Builder { ... }` in a body: a struct of its own, named through the struct around it.
    void parseNestedStruct(Parser& parser, const Pending& pending) {
      AstStructDecl decl = ParseStruct(parser);
      place(parser, decl, pending);
      AddStruct(parser, move(decl));
    }

    // `static func builder() -> Builder { ... }`: a function the struct holds, called `Point.builder()`, with no `this`.
    void parseStatic(Parser& parser, const Pending& pending) {
      Take(parser);
      AstFuncDecl decl = ParseFunc(parser);
      decl.isStatic = true;
      place(parser, decl, pending);
      AddDecl(parser, parser.program.funcs.items, move(decl), ItemKind::Func);
    }

    void parseMethod(Parser& parser, const Pending& pending) {
      AstFuncDecl method = ParseFunc(parser);
      method.isInternal = pending.internal;
      method.annotations = pending.annotations;

      AstMember member;
      member.isMethod = true;
      member.method = Add(parser.program.funcs, method);
      parser.scratch.members.push_back(member);
    }

    // A field; annotations go before a struct, a static function or a method only.
    void parseField(Parser& parser, const Pending& pending) {
      if (!Empty(pending.annotations)) {
        return Fail(parser, "a struct, a static function or a method after an annotation");
      }

      AstMember member;
      member.field = ParseField(parser);
      parser.scratch.members.push_back(member);
    }

    // The annotations and the `internal` before a member.
    Pending memberMarks(Parser& parser) {
      ParseAnnotations(parser);

      if (AtInternal(parser)) {
        Take(parser);
        parser.internal = true;
      }

      return takePending(parser);
    }

  }  // namespace

  void ParseMember(Parser& parser) {
    const Pending pending = memberMarks(parser);

    if (At(parser, K::STRUCT) || At(parser, K::STATIC)) {
      return At(parser, K::STRUCT) ? parseNestedStruct(parser, pending) : parseStatic(parser, pending);
    }

    if (AtAny(parser, kFuncStart)) {
      return parseMethod(parser, pending);
    }

    parseField(parser, pending);
  }

  // `var? name: type`; called where the first token is not known to fit, so it checks it.
  AstField ParseField(Parser& parser) {
    RequireAny(parser, kFieldStart, kFieldStartMsg);

    AstField field;
    field.varPos = AcceptPos(parser, K::VAR);
    field.name = ExpectIdent(parser);
    Expect(parser, K::COLON);

    const size_t typeFrom = parser.position;
    field.type = ParseType(parser);
    field.typeText = TextSince(parser, typeFrom);

    if (Accept(parser, K::EQ)) {
      const size_t valueFrom = parser.position;
      field.initial = ParseExpr(parser);
      field.initialText = TextSince(parser, valueFrom);
    }

    return field;
  }

  // `<T, U>` after a struct name, or nothing.
  Range<Ident> ParseTypeParams(Parser& parser) {
    if (!Accept(parser, K::LT)) {
      return {};
    }

    const Range<Ident> names = CommaList(parser, parser.scratch.idents, parser.program.idents, [&] { return ExpectIdent(parser); });
    Expect(parser, K::GT);

    return names;
  }

  // `: Shape, Named`, the interfaces a struct claims, or nothing.
  Range<Ident> ParseImpls(Parser& parser) {
    if (!Accept(parser, K::COLON)) {
      return {};
    }

    return CommaList(parser, parser.scratch.idents, parser.program.idents, [&] { return ExpectIdent(parser); });
  }

  // `(x: f64, y: f64)` after a struct name: the fields written in the head, or nothing.
  Range<AstField> ParsePrimaryFields(Parser& parser) {
    if (!Accept(parser, K::LPAREN)) {
      return {};
    }

    const Range<AstField> fields = CommaList(parser, parser.scratch.fields, parser.program.fields, [&] { return ParseField(parser); });
    Expect(parser, K::RPAREN);

    return fields;
  }

  // ── interface ───────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────

  AstInterfaceDecl ParseInterface(Parser& parser) {
    AstInterfaceDecl decl;
    decl.pos = PosOf(parser, Expect(parser, K::INTERFACE));
    decl.name = ExpectIdent(parser);
    decl.methods = ParseInterfaceBody(parser);

    return decl;
  }

  // `{ func ... func ... }`
  Range<AstInterfaceMethod> ParseInterfaceBody(Parser& parser) {
    Expect(parser, K::LBRACE);
    const Range<AstInterfaceMethod> methods = ParseInterfaceMethods(parser);
    Expect(parser, K::RBRACE);

    return methods;
  }

  Range<AstInterfaceMethod> ParseInterfaceMethods(Parser& parser) {
    const size_t mark = parser.scratch.interfaceMethods.size();

    while (At(parser, K::FUNC)) {
      parser.scratch.interfaceMethods.push_back(ParseInterfaceMethod(parser));
    }

    return Take(parser.program.interfaceMethods, parser.scratch.interfaceMethods, mark);
  }

  AstInterfaceMethod ParseInterfaceMethod(Parser& parser) {
    AstInterfaceMethod method;
    method.name = ExpectFuncName(parser);
    ParseSignature(parser, method.params, method.returnType);

    return method;
  }

  // `func name`
  Ident ExpectFuncName(Parser& parser) {
    Expect(parser, K::FUNC);

    return ExpectIdent(parser);
  }

  // Two keywords in a row (`extern type`), answering the position of the first.
  Pos ExpectPair(Parser& parser, TokenKind first, TokenKind second) {
    const Pos pos = PosOf(parser, Expect(parser, first));
    Expect(parser, second);

    return pos;
  }

  // `(params) -> returnType`
  void ParseSignature(Parser& parser, Range<AstParam>& params, TypeId& returnType) {
    params = ParseParamList(parser);
    Expect(parser, K::ARROW);
    returnType = ParseType(parser);
  }

  // ── func, coro func, parameters ─────────────────────────────────────────────────────────────────────────────────────────────────────────

  AstFuncDecl ParseFunc(Parser& parser) {
    AstFuncDecl func;
    func.pos = PosOf(parser, Current(parser));
    func.isCoroutine = Accept(parser, K::CORO);
    func.isKernel = !func.isCoroutine && Accept(parser, K::KERNEL);

    Expect(parser, K::FUNC);
    ParseFuncTail(parser, func);

    return func;
  }

  void ParseFuncTail(Parser& parser, AstFuncDecl& func) {
    func.name = ExpectIdent(parser);
    func.typeParams = ParseTypeParams(parser);
    ParseSignature(parser, func.params, func.returnType);
    func.body = ParseBlock(parser);
  }

  // `( param, ... )`, possibly empty.
  Range<AstParam> ParseParamList(Parser& parser) {
    Expect(parser, K::LPAREN);
    const Range<AstParam> params =
      OptionalCommaList(parser, kParamStart, parser.scratch.params, parser.program.params, [&] { return ParseParam(parser); });
    Expect(parser, K::RPAREN);

    return params;
  }

  AstParam ParseParam(Parser& parser) {
    AstParam param = ParseParamHead(parser);
    param.byPos = AcceptPos(parser, K::VAL);
    param.type = ParseType(parser);

    return param;
  }

  // `mark? name:`, the part of a parameter before its type.
  AstParam ParseParamHead(Parser& parser) {
    RequireAny(parser, kParamStart, kParamStartMsg);
    AstParam param;
    ParseMark(parser, param);
    param.name = ExpectIdent(parser);
    Expect(parser, K::COLON);

    return param;
  }

  // `uniform`, `index`, `writable` or `workgroup` before a kernel parameter, or nothing.
  void ParseMark(Parser& parser, AstParam& param) {
    if (!AtAny(parser, kParamMarks)) {
      return;
    }

    param.markPos = PosOf(parser, Current(parser));
    param.mark = markOf(CurrentKind(parser));
    Take(parser);
  }

  // ── extern ──────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────

  AstExternFunc ParseExternFunc(Parser& parser) {
    AstExternFunc decl;
    decl.pos = ExpectPair(parser, K::EXTERN, K::FUNC);
    decl.name = ExpectIdent(parser);
    ParseSignature(parser, decl.params, decl.returnType);
    decl.binding = ParseOptionalBinding(parser);

    return decl;
  }

  AstExternType ParseExternType(Parser& parser) {
    AstExternType decl;
    decl.pos = ExpectPair(parser, K::EXTERN, K::TYPE);
    decl.name = ExpectIdent(parser);
    decl.typeParams = ParseTypeParams(parser);

    decl.binding = ParseOptionalBinding(parser);
    decl.members = ParseHostMembers(parser);

    return decl;
  }

  AstExternInterface ParseExternInterface(Parser& parser) {
    AstExternInterface decl;
    decl.pos = ExpectPair(parser, K::EXTERN, K::INTERFACE);
    decl.name = ExpectIdent(parser);
    decl.binding = ParseOptionalBinding(parser);
    decl.methods = ParseInterfaceBody(parser);

    return decl;
  }

  // `{ members }`, possibly absent, then the optional `;`.
  Range<AstHostMember> ParseHostMembers(Parser& parser) {
    const Range<AstHostMember> members = Accept(parser, K::LBRACE) ? ParseHostMemberBody(parser) : Range<AstHostMember>{};
    Accept(parser, K::SEMI);

    return members;
  }

  Range<AstHostMember> ParseHostMemberBody(Parser& parser) {
    const size_t mark = parser.scratch.hostMembers.size();

    while (AtAny(parser, kHostMemberStart)) {
      parser.scratch.hostMembers.push_back(ParseHostMember(parser));
    }

    Expect(parser, K::RBRACE);

    return Take(parser.program.hostMembers, parser.scratch.hostMembers, mark);
  }

  AstHostMember ParseHostMember(Parser& parser) {
    AstHostMember member;
    member.pos = PosOf(parser, Current(parser));
    member.isStatic = Accept(parser, K::STATIC);
    member.name = ExpectFuncName(parser);
    ParseHostSignature(parser, member);

    return member;
  }

  // `(params) (-> ret)? binding?`: a host member that returns nothing has no `->`.
  void ParseHostSignature(Parser& parser, AstHostMember& member) {
    member.params = ParseParamList(parser);

    if (Accept(parser, K::ARROW)) {
      member.returnType = ParseType(parser);
    }

    member.binding = ParseOptionalBinding(parser);
  }

  // `= java "x" | js "y"`, or nothing.
  Range<AstHostBinding> ParseOptionalBinding(Parser& parser) {
    if (!Accept(parser, K::EQ)) {
      return {};
    }

    return SeparatedList(
      parser, K::PIPE, parser.scratch.hostBindings, parser.program.hostBindings, [&] { return ParseHostBinding(parser); });
  }

  AstHostBinding ParseHostBinding(Parser& parser) {
    AstHostBinding binding;
    binding.target = ExpectIdent(parser);
    binding.text = ExpectLiteral(parser, K::STRING_LIT);

    return binding;
  }

  // ── bridge, import, meta, global ────────────────────────────────────────────────────────────────────────────────────────────────────────

  AstBridge ParseBridge(Parser& parser) {
    AstBridge decl;
    decl.pos = PosOf(parser, Expect(parser, K::BRIDGE));
    decl.poly = ParseType(parser);
    ParseBridgeTarget(parser, decl);
    decl.rules = ParseBridgeRules(parser);

    return decl;
  }

  // `<-> java "long[]"`
  void ParseBridgeTarget(Parser& parser, AstBridge& decl) {
    Expect(parser, K::BIARROW);
    decl.target = ExpectIdent(parser);
    decl.host = ExpectLiteral(parser, K::STRING_LIT);
  }

  Range<AstBridgeRule> ParseBridgeRules(Parser& parser) {
    if (!Accept(parser, K::LBRACE)) {
      return {};
    }

    const size_t mark = parser.scratch.bridgeRules.size();

    while (At(parser, K::IDENT)) {
      parser.scratch.bridgeRules.push_back(ParseBridgeRule(parser));
    }

    Expect(parser, K::RBRACE);

    return Take(parser.program.bridgeRules, parser.scratch.bridgeRules, mark);
  }

  AstBridgeRule ParseBridgeRule(Parser& parser) {
    AstBridgeRule rule;
    rule.direction = ExpectIdent(parser);
    Expect(parser, K::EQ);
    rule.text = ExpectLiteral(parser, K::STRING_LIT);

    return rule;
  }

  AstImport ParseImport(Parser& parser) {
    AstImport decl;
    decl.pos = PosOf(parser, Expect(parser, K::IMPORT));

    if (At(parser, K::IDENT)) {
      decl.lang = ExpectIdent(parser);
    }

    decl.path = ExpectLiteral(parser, K::STRING_LIT);

    if (At(parser, K::IDENT)) {
      ParseImportAlias(parser, decl);
    }

    return decl;
  }

  // `as name` after an import's path.
  void ParseImportAlias(Parser& parser, AstImport& decl) {
    decl.asWord = ExpectIdent(parser);
    decl.alias = ExpectIdent(parser);
  }

  AstMeta ParseMeta(Parser& parser) {
    AstMeta decl;
    decl.pos = PosOf(parser, Expect(parser, K::META));

    if (At(parser, K::IDENT)) {
      decl.target = ExpectIdent(parser);
    }

    decl.key = ExpectLiteral(parser, K::STRING_LIT);
    Expect(parser, K::EQ);
    decl.value = ExpectLiteral(parser, K::STRING_LIT);

    return decl;
  }

  AstGlobal ParseGlobal(Parser& parser) {
    AstGlobal decl;
    decl.pos = PosOf(parser, Expect(parser, K::GLOBAL));
    ParseGlobalType(parser, decl);
    ParseGlobalInit(parser, decl);

    return decl;
  }

  // `name: type =`
  void ParseGlobalType(Parser& parser, AstGlobal& decl) {
    decl.name = ExpectIdent(parser);
    Expect(parser, K::COLON);
    decl.type = ParseType(parser);
    Expect(parser, K::EQ);
  }

  // A global starts as a number literal, `true` or `false`.
  void ParseGlobalInit(Parser& parser, AstGlobal& global) {
    if (At(parser, K::INT_LIT) || At(parser, K::FLOAT_LIT)) {
      return parseNumberInit(parser, global);
    }

    global.init = At(parser, K::TRUE_) ? GlobalInit::True : GlobalInit::False;

    if (!Accept(parser, K::TRUE_) && !Accept(parser, K::FALSE_)) {
      Fail(parser, kGlobalInitMsg);
    }
  }

}  // namespace polyir::front
