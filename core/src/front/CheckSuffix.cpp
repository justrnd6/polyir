// What follows a value: `.field`, `.method(args)`, `[index]` (CheckSuffix.nina). A method call is resolved in the order: a method of an interface value,
// a numeric cast, a built-in operation for the receiver's type, then a free function called with the receiver first (UFCS; a struct's own methods are
// functions named `Struct_method`).
#include "front/Checker.hpp"

namespace polyir::front {

  // Each expression of a list, checked in order.
  Vector<ExprPtr> CheckAll(Checker& check, Range<ExprId> exprs) {
    Vector<ExprPtr> out;
    out.reserve(exprs.count);

    for (const ExprId id : check.program->exprRefs[exprs]) {
      out.push_back(CheckExpr(check, id));
    }

    return out;
  }

  namespace {

    // ── fields ──────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────

    // An interface value has methods, not fields: its two pointers are not part of the language.
    ExprPtr interfaceField(Checker& check, Symbol interfaceName, Ident field) {
      const String advice = "(call a method instead of reading " + Str(check, field.name) + ")";

      return Unsupported(Str(check, interfaceName) + " is an interface: it has methods, not fields " + advice, SpanAt(field.pos));
    }

    // `p.x`: the field's index in its struct, and its type.
    ExprPtr structField(Checker& check, ExprPtr receiver, Ident field) {
      const Symbol structName = SymbolOf(check, receiver->type->name);

      if (IsInterface(DeclsOf(check), structName)) {
        return interfaceField(check, structName, field);
      }

      const StructInfo* info = FindStruct(DeclsOf(check), structName);
      const int index = info == nullptr ? -1 : IndexOf(*info, field.name);

      if (index < 0) {
        return Unsupported("unknown field " + Str(check, field.name), SpanAt(field.pos));
      }

      return FieldGetNode(move(receiver), index, info->fieldTypes[index], SpanAt(field.pos));
    }

    ExprPtr fieldGet(Checker& check, ExprPtr receiver, Ident field) {
      if (receiver->type->kind != TypeKind::Struct) {
        return Unsupported("field " + Str(check, field.name) + " on a non-struct", SpanAt(field.pos));
      }

      return structField(check, move(receiver), field);
    }

    // ── methods ─────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────

    // The numeric conversions a method call spells: `x.toI64()`. Family: i signed, u unsigned, f float, p pointer to bytes.
    struct CastRow {
      const char* name;
      char family;
      int bits;
    };

    const CastRow kCastRows[] = {{"toI8", 'i', 8}, {"toI16", 'i', 16}, {"toI32", 'i', 32}, {"toI64", 'i', 64}, {"toU8", 'u', 8}, {"toU16", 'u', 16},
      {"toU32", 'u', 32}, {"toU64", 'u', 64}, {"toFloat", 'f', 32}, {"toDouble", 'f', 64}, {"toPtr", 'p', 8}};

    // The type a conversion gives.
    TypePtr castType(const CastRow& row) {
      if (row.family == 'i') {
        return IntType(row.bits);
      }

      if (row.family == 'u') {
        return UintType(row.bits);
      }

      if (row.family == 'f') {
        return FloatType(row.bits);
      }

      return MakePointer(IntType(row.bits));
    }

    // The conversions by name, made once.
    SymbolMap<TypePtr> makeCastTargets() {
      SymbolMap<TypePtr> built;

      for (const CastRow& row : kCastRows) {
        Insert(built, Intern(GlobalInterner(), row.name), castType(row));
      }

      return built;
    }

    const SymbolMap<TypePtr>& castTargets() {
      static const SymbolMap<TypePtr> targets = makeCastTargets();

      return targets;
    }

    // The function `x.method()` calls: `Point_method` when the receiver is a struct, else the free function `method`.
    Symbol calleeFor(Checker& check, const TypePtr& receiverType, Ident method) {
      if (receiverType->kind != TypeKind::Struct) {
        return method.name;
      }

      return Mangled(check.session, SymbolOf(check, receiverType->name), method.name);
    }

    // `p.dist()` calls `Point_dist(p)` when the receiver is a struct; otherwise `x.square()` calls the free function `square(x)` (UFCS).
    ExprPtr receiverFirstCall(Checker& check, Ident method, Vector<ExprPtr> values) {
      const Symbol callee = calleeFor(check, values[0]->type, method);

      if (const FuncInfo* function = FindFunc(DeclsOf(check), callee)) {
        return Internal(check, callee, CallOf(check, callee, *function, move(values), SpanAt(method.pos)));
      }

      return DirectCall(check, method, move(values));
    }

    ExprPtr builtinOrFunctionCall(Checker& check, ExprPtr receiver, Ident method, Vector<ExprPtr> args) {
      if (ExprPtr built = BuiltinMethod(check, method.name, receiver, args, SpanAt(method.pos))) {
        return PushBoxed(check, move(built));
      }

      Vector<ExprPtr> values{move(receiver)};

      for (ExprPtr& argument : args) {
        values.push_back(move(argument));
      }

      return receiverFirstCall(check, method, move(values));
    }

    // `x.method(args)`, tried in the order the file's header gives. (A cast takes no arguments, so they are not checked for one.)
    ExprPtr methodCall(Checker& check, ExprPtr receiver, const AstSuffix& suffix) {
      const Ident method = suffix.name;

      if (const InterfaceInfo* interface = InterfaceOf(check, receiver->type)) {
        return InterfaceCall(check, *interface, move(receiver), method, CheckAll(check, suffix.args));
      }

      if (const TypePtr* target = Find(castTargets(), method.name)) {
        ExprPtr problem = AnyTypeProblem(receiver, "`." + Str(check, method.name) + "()`", SpanAt(method.pos));

        return problem ? problem : CastNode(ExprKind::Cast, move(receiver), *target, SpanAt(method.pos));
      }

      return builtinOrFunctionCall(check, move(receiver), method, CheckAll(check, suffix.args));
    }

    // ── a module's members: `Geometry.distance(a, b)`, `Outer.Inner.f(x)`, `Geometry.f` ──

    // The member `suffix` names in the module `path`: a call (`.f(args)`), or a function as a value (`.f`).
    ExprPtr memberAt(Checker& check, Symbol path, const AstSuffix& suffix) {
      const Symbol member = Intern(check.session.names, Str(check, path) + "." + Str(check, suffix.name.name));
      const Ident name{member, suffix.name.pos};

      if (suffix.kind == AstSuffixKind::Method) {
        return DirectCall(check, name, ArgumentsFor(check, member, suffix.args));
      }

      return suffix.kind == AstSuffixKind::Field ? NameRef(check, name) : Unsupported("a module cannot be indexed", SpanAt(suffix.pos));
    }

    // How many of the suffixes name modules inside `path` (`Outer.Inner.f` has one: `Inner`), with `path` taken to the last of them.
    size_t innerModules(Checker& check, Symbol& path, View<AstSuffix> suffixes) {
      size_t at = 0;

      for (; at < suffixes.size() && suffixes[at].kind == AstSuffixKind::Field; at++) {
        const Symbol inner = Intern(check.session.names, Str(check, path) + "." + Str(check, suffixes[at].name.name));

        if (!Contains(check.session.modules, inner)) {
          break;
        }

        path = inner;
      }

      return at;
    }

    // A base that names a module (and no local) reaches its member; the suffixes after the member apply to it. Null otherwise.
    ExprPtr moduleMember(Checker& check, const AstExpr& node) {
      const AstExpr& base = check.program->exprs[node.first];
      const Optional<Symbol> module = base.kind == AstExprKind::Name && Lookup(check, base.name.name) == nullptr ? ModuleNamed(check.session, base.name.name) : nullopt;

      if (!module) {
        return nullptr;
      }

      Symbol path = *module;
      const View<AstSuffix> suffixes = check.program->suffixes[node.suffixes];
      size_t at = innerModules(check, path, suffixes);

      if (at >= suffixes.size()) {
        return Unsupported("a module is not a value: name one of its declarations (" + DisplayName(check.session, path) + ".name)", SpanAt(base.pos));
      }

      ExprPtr current = memberAt(check, path, suffixes[at]);

      for (at++; at < suffixes.size(); at++) {
        current = CheckSuffix(check, move(current), suffixes[at]);
      }

      return current;
    }

  }  // namespace

  ExprPtr CheckSuffix(Checker& check, ExprPtr receiver, const AstSuffix& suffix) {
    switch (suffix.kind) {
      case AstSuffixKind::Field:
        return fieldGet(check, move(receiver), suffix.name);
      case AstSuffixKind::Method:
        return methodCall(check, move(receiver), suffix);
      default:
        return IndexInto(move(receiver), CheckAll(check, suffix.args));
    }
  }

  // A base with suffixes: the host-type call `Math.sqrt(x)` when it is one, otherwise the base and then each suffix in turn.
  ExprPtr CheckPostfix(Checker& check, const AstExpr& node) {
    if (ExprPtr problem = BuilderChainProblem(check, node)) {
      return problem;
    }

    if (ExprPtr member = moduleMember(check, node)) {
      return member;
    }

    if (ExprPtr host = HostStaticCall(check, node, nullptr)) {
      return host;
    }

    ExprPtr current = CheckExpr(check, node.first);

    for (const AstSuffix& suffix : check.program->suffixes[node.suffixes]) {
      current = CheckSuffix(check, move(current), suffix);
    }

    return current;
  }

}  // namespace polyir::front
