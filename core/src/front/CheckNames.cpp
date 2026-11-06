// Names, calls and struct literals (CheckNames.nina). A bare name looks in scope first (locals shadow), then at functions. A call is direct (a declared
// function), indirect (a local holding a function pointer or closure), or a builtin. Unknown names and functions are marked with a problem node, not
// silently zeroed; the diagnostics report them at the name's position.
#include "front/Checker.hpp"

namespace polyir::front {

  // Reading a name: a plain local is its value, a `var` slot or a global is loaded.
  ExprPtr BindingRead(Checker& check, const Binding& binding, Span at) {
    static const ExprKind kinds[] = {ExprKind::Local, ExprKind::LoadSlot, ExprKind::Global};  // in the order of BindingKind

    return NamedRef(kinds[static_cast<size_t>(binding.kind)], Str(check, binding.name), binding.type, at);
  }

  namespace {

    // A function used as a value; its type `func(params) -> result` is made the first time it is needed.
    ExprPtr functionValue(Checker& check, Ident name, const FuncInfo& function) {
      if (!function.asValue) {
        function.asValue = MakeFunction(function.params, function.result);
      }

      return NamedRef(ExprKind::FuncRef, Str(check, name.name), function.asValue, SpanAt(name.pos));
    }

    // `Name()` for a struct is its construction with every container field empty; anything else is unknown.
    ExprPtr notAFunction(Checker& check, Ident name, const Vector<ExprPtr>& values, Span at) {
      ExprPtr made = values.empty() ? EmptyConstruction(check, name.name, at) : nullptr;

      return made ? made : Unsupported("unknown function " + Str(check, name.name), at);
    }

    ExprPtr wrongArgumentCount(Checker& check, Ident name, size_t wanted, size_t given, Span at) {
      return Unsupported(Str(check, name.name) + " takes " + to_string(wanted) + " arguments but " + to_string(given) + " were given", at);
    }

  }  // namespace

  // A bare name: a local, a global, or a function used as a value.
  ExprPtr NameRef(Checker& check, Ident name) {
    if (const Binding* found = Lookup(check, name.name)) {
      return BindingRead(check, *found, SpanAt(name.pos));
    }

    const Ident scoped{InScope(check.session, name.name), name.pos};

    if (const FuncInfo* function = FindFunc(DeclsOf(check), scoped.name)) {
      return Internal(check, scoped.name, functionValue(check, scoped, *function));
    }

    if (const Optional<Symbol> module = ModuleNamed(check.session, name.name)) {
      return Unsupported("a module is not a value: name one of its declarations (" + DisplayName(check.session, *module) + ".name)", SpanAt(name.pos));
    }

    return Unsupported("unknown name " + Str(check, name.name), SpanAt(name.pos));
  }

  // Each argument must have the kind of the parameter it fills (a struct fills an interface parameter by being boxed).
  ExprPtr CallOf(Checker& check, Symbol callee, const FuncInfo& info, Vector<ExprPtr> values, Span at) {
    return CallNode(Str(check, callee), Fitted(check, move(values), info.params), info.result, at);
  }

  ExprPtr Internal(Checker& check, Symbol name, ExprPtr value) {
    if (const Optional<String> why = InternalProblem(check.session, name)) {
      return Unsupported(*why, value->at);
    }

    return value;
  }

  // A call must pass exactly as many arguments as the function has parameters. The name is looked up from the code's module.
  ExprPtr DirectCall(Checker& check, Ident written, Vector<ExprPtr> values) {
    const Ident name{InScope(check.session, written.name), written.pos};
    const FuncInfo* function = FindFunc(DeclsOf(check), name.name);
    const Span at = SpanAt(name.pos);

    if (const GenericFunc* generic = function == nullptr ? Find(check.session.genericFuncs, name.name) : nullptr) {
      return Internal(check, name.name, GenericCall(check, name, *generic, move(values)));
    }

    if (function == nullptr) {
      return notAFunction(check, name, values, at);
    }

    if (function->params.size() == values.size()) {
      return Internal(check, name.name, CallOf(check, name.name, *function, move(values), at));
    }

    return wrongArgumentCount(check, name, function->params.size(), values.size(), at);
  }

  namespace {

    // The result type of calling through a function pointer or closure; i64 when the binding's type says nothing, as today.
    TypePtr calleeReturnType(const TypePtr& type) {
      switch (type->kind) {
        case TypeKind::Function: case TypeKind::Closure:
          return type->element;
        case TypeKind::Pointer:
          return calleeReturnType(type->element);
        default:
          return IntType(64);
      }
    }

    // A receiver that is already wrong is the problem to report (`str(missing(x))` is about `missing`).
    ExprPtr notBuiltin(Checker& check, Ident name, const ExprPtr& receiver) {
      if (receiver->kind == ExprKind::Unsupported) {
        return receiver;
      }

      return Unsupported("builtin " + Str(check, name.name), SpanAt(name.pos));
    }

    // `len(x)` is `x.len()`: the first argument is the receiver. A free builtin (`fence(order)`) has none.
    ExprPtr builtinCall(Checker& check, Ident name, Vector<ExprPtr> values) {
      const Span at = SpanAt(name.pos);

      if (FreeHandler freeHandler = FreeHandlerFor(check.registry, name.name)) {
        return freeHandler(check, values, at);
      }

      if (values.empty()) {
        return Unsupported("builtin " + Str(check, name.name), at);
      }

      Vector<ExprPtr> rest(values.begin() + 1, values.end());
      ExprPtr made = BuiltinMethod(check, name.name, values[0], rest, at);

      return made ? PushBoxed(check, move(made)) : notBuiltin(check, name, values[0]);
    }

    // A free builtin (`fence(...)`), or a builtin that may also be written as a call (`len(x)`).
    bool isBuiltinName(Checker& check, Symbol name) {
      return FreeHandlerFor(check.registry, name) != nullptr || IsFunctionForm(check.registry, name);
    }

    // A call through a local holding a function pointer or a closure.
    ExprPtr indirectCall(Checker& check, Ident name, const Binding& binding, Vector<ExprPtr> values) {
      const ExprPtr callee = BindingRead(check, binding, SpanAt(name.pos));

      return IndirectCallNode(callee, move(values), calleeReturnType(callee->type), SpanAt(name.pos));
    }

    // The arguments of a call of a declared function, each checked as its parameter's type, so `[a, b]` can fill a list of an
    // interface. A call of no known function, or with the wrong count, checks them as they are (DirectCall reports it).
    Vector<ExprPtr> argumentsFor(Checker& check, const AstExpr& node) {
      return ArgumentsFor(check, node.name.name, node.args);
    }

    // `name(args)`: a builtin, then a local (a call through it), then a declared function.
    ExprPtr callExpr(Checker& check, const AstExpr& node) {
      if (isBuiltinName(check, node.name.name)) {
        return builtinCall(check, node.name, CheckAll(check, node.args));
      }

      if (const Binding* found = Lookup(check, node.name.name)) {
        return indirectCall(check, node.name, *found, CheckAll(check, node.args));
      }

      return DirectCall(check, node.name, argumentsFor(check, node));
    }

    // The slot each initializer fills, or the problem node for the first one that fills none.
    ExprPtr fieldSlots(Checker& check, Symbol structName, const StructInfo* info, View<AstFieldInit> fields, Vector<int>& indices) {
      for (const AstFieldInit& init : fields) {
        indices.push_back(info == nullptr ? -1 : IndexOf(*info, init.name.name));

        if (indices.back() >= 0) {
          continue;
        }

        return Unsupported(Str(check, structName) + " has no field " + Str(check, init.name.name), SpanAt(init.name.pos));
      }

      return nullptr;
    }

    // Each initializer's value, checked against (and boxed to) the type of the field it fills. `info` is not null whenever there is a field
    // here: for an unknown struct, fieldSlots has already returned a problem for the first field.
    void addFieldValues(Checker& check, const StructInfo* info, View<AstFieldInit> fields, FieldInits& all) {
      for (size_t i = 0; i < fields.size(); i++) {
        all.values.push_back(BoxIfInterface(check, info->fieldTypes[all.indices[i]], CheckExpr(check, fields[i].value)));
      }
    }

    // `Name { field: value, ... }`: indices in source order (see StructLit in Hir.hpp), then each container field left out, made empty
    // (CheckStructDefaults.cpp). A struct stored in an interface-typed field is boxed.
    // A struct literal; its name is looked up from the code's module, and a internal struct is built only by the code around it.
    ExprPtr structLiteral(Checker& check, Symbol written, Span at, Range<AstFieldInit> inits) {
      const Symbol structName = InScope(check.session, written);

      if (const Optional<String> why = InternalProblem(check.session, structName)) {
        return Unsupported(*why, at);
      }

      const StructInfo* info = FindStruct(DeclsOf(check), structName);
      const View<AstFieldInit> fields = check.program->inits[inits];
      FieldInits all;

      if (ExprPtr problem = fieldSlots(check, structName, info, fields, all.indices)) {
        return problem;
      }

      addFieldValues(check, info, fields, all);
      FillStructDefaults(check, info, all, at);

      return StructLitNode(Str(check, structName), move(all.indices), move(all.values), StructOf(Types(check), structName), at);
    }

    ExprPtr structLitExpr(Checker& check, const AstExpr& node) {
      return structLiteral(check, node.name.name, SpanAt(node.name.pos), node.inits);
    }

    ExprPtr nameExpr(Checker& check, const AstExpr& node) {
      return NameRef(check, node.name);
    }

    // An interface is never what a literal builds: `let s: Shape = Circle { ... }` builds a Circle.
    Symbol structNameFor(Checker& check, Symbol written, const TypePtr& expected) {
      if (expected->kind != TypeKind::Struct) {
        return written;
      }

      const Symbol named = SymbolOf(check, expected->name);

      return IsInterface(DeclsOf(check), named) ? written : named;
    }

    // Like CheckExpr, but a struct literal takes the struct named by `expected`: `let b: Box<i64> = Box { ... }` finds the specialized `Box_i64`.
    // A list literal checks its elements as the expected list's element type.
    ExprPtr exprAsWritten(Checker& check, ExprId id, const TypePtr& expected) {
      const AstExpr& node = Node(check, id);

      if (ExprPtr call = HostStaticCall(check, node, &expected)) {
        return call;
      }

      if (node.kind == AstExprKind::ListLit && expected->kind == TypeKind::DynamicArray) {
        return ListLiteralExpecting(check, node, expected);
      }

      if (node.kind == AstExprKind::IfValue || node.kind == AstExprKind::Match) {
        return node.kind == AstExprKind::IfValue ? IfValueExpecting(check, node, expected) : MatchValueExpecting(check, node, expected);
      }

      if (node.kind != AstExprKind::StructLit) {
        return CheckExpr(check, id);
      }

      return structLiteral(check, structNameFor(check, node.name.name, expected), SpanAt(node.name.pos), node.inits);
    }

  }  // namespace

  Vector<ExprPtr> ArgumentsFor(Checker& check, Symbol written, Range<ExprId> args) {
    const FuncInfo* function = FindFunc(DeclsOf(check), InScope(check.session, written));

    if (function == nullptr || function->params.size() != args.count) {
      return CheckAll(check, args);
    }

    Vector<ExprPtr> values;

    for (const ExprId id : check.program->exprRefs[args]) {
      values.push_back(CheckExprExpecting(check, id, function->params[values.size()]));
    }

    return values;
  }

  ExprPtr CheckExprExpecting(Checker& check, ExprId id, const TypePtr& expected) {
    return BoxIfInterface(check, expected, exprAsWritten(check, id, expected));
  }

  void RegisterNames(Registry& registry) {
    AddExpr(registry, AstExprKind::Name, nameExpr);
    AddExpr(registry, AstExprKind::Call, callExpr);
    AddExpr(registry, AstExprKind::StructLit, structLitExpr);
  }

}  // namespace polyir::front
