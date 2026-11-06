// The checker: the state a function body is checked with, and the declarations of the functions that do the checking, grouped by the file they are in
// (each file is ported from the Nina file of the same name). Handlers (Registry.hpp) get a `Checker&`; everything they need is on it.
//
//   Checker           the unit being read, the scope, the declared return type, and the session's tables (`session.decls`, `session.types`, ...)
//   check*(...)       turn one syntax node into HIR: `CheckExpr` an expression, `CheckBlock` a block of statements, `CheckExprExpecting` an expression
//                     in a place that wants a type
//   the rest          the helpers of each Check*.nina file (ported from it; the names follow the C++ style, not always the Nina ones); a mistake in
//                     the program is never thrown: it becomes a problem node
//                     (`Unsupported(message, at)`, an `ExprKind::Unsupported`) that CheckDiagnose.cpp reports
#pragma once

#include "front/Registry.hpp"
#include "front/HirBuild.hpp"

namespace polyir::front {

  // The state a function body is checked with (see the list above).
  struct Checker {
    Session& session;
    const Registry& registry;
    const Program* program = nullptr;
    Scope scope{};
    TypePtr returnType = VoidType();   // the declared return type of the function (or closure) being checked
    int depth = 0;                     // how many expressions deep the check is (see kMaxExpressionDepth)
    StmtList hoisted{};                // statements the one just checked needs before it (a `for (x in gen)` loop's setup); CheckBlock places them
  };

  // Expressions nest as deep as the program writes them and the check recurses with them, so a limit keeps a pathological program from overflowing the stack.
  // The Nina checker overflows at about 2,500 levels; the limit is under that.
  constexpr int kMaxExpressionDepth = 2000;

  // Counts one level of expression nesting for as long as it lives.
  class DepthGuard {
    public:
      explicit DepthGuard(int& level) : level_(level) {
        level_++;
      }

      ~DepthGuard() {
        level_--;
      }

      DepthGuard(const DepthGuard&) = delete;

      DepthGuard& operator=(const DepthGuard&) = delete;

    private:
      int& level_;
  };

  void Enter(Checker& check, const Unit& unit);

  Symbol SymbolOf(Checker& check, const String& name);  // a struct type holds its name as text; the tables are keyed by Symbol

  const Binding* Lookup(const Checker& check, Symbol name);  // the scope, then the globals; null when the name means nothing

  const AstExpr& Node(const Checker& check, ExprId id);

  Decls& DeclsOf(Checker& check);

  TypePool& Types(Checker& check);

  StringView TextOf(const Checker& check, Symbol symbol);

  String Str(const Checker& check, Symbol symbol);

  inline void Enter(Checker& check, const Unit& unit) {
    check.program = unit.program;
    check.session.active = unit.instance == kNoIndex ? nullptr : &check.session.instances[unit.instance];
  }

  inline Symbol SymbolOf(Checker& check, const String& name) {
    return Intern(check.session.names, name);
  }

  inline const AstExpr& Node(const Checker& check, ExprId id) {
    return check.program->exprs[id];
  }

  inline Decls& DeclsOf(Checker& check) {
    return check.session.decls;
  }

  inline TypePool& Types(Checker& check) {
    return check.session.types;
  }

  inline StringView TextOf(const Checker& check, Symbol symbol) {
    return TextOf(check.session, symbol);
  }

  inline String Str(const Checker& check, Symbol symbol) {
    return Str(check.session, symbol);
  }

  // ── CheckExpr.cpp ───────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────
  ExprPtr CheckExpr(Checker& check, ExprId id);

  ExprPtr ValCopy(ExprPtr value);  // `val x`: an independent copy of a reference value; a plain value is its own copy

  // ── CheckNames.cpp ──────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────
  // Like CheckExpr, but a struct literal takes the struct `expected` names, and a struct converts to an interface where one is expected.
  ExprPtr CheckExprExpecting(Checker& check, ExprId id, const TypePtr& expected);

  ExprPtr NameRef(Checker& check, Ident name);

  ExprPtr DirectCall(Checker& check, Ident name, Vector<ExprPtr> values);

  // A call of the generic function `generic` (CheckGenericCall.cpp): of the copy for the types its arguments give.
  ExprPtr GenericCall(Checker& check, Ident name, const GenericFunc& generic, Vector<ExprPtr> values);

  // The arguments of a call of the function `written` names, each checked as its parameter's type when it is known.
  Vector<ExprPtr> ArgumentsFor(Checker& check, Symbol written, Range<ExprId> args);

  ExprPtr Internal(Checker& check, Symbol name, ExprPtr value);  // the value, or why the code being checked may not use the internal `name`

  ExprPtr CallOf(Checker& check, Symbol callee, const FuncInfo& info, Vector<ExprPtr> values, Span at);

  ExprPtr BindingRead(Checker& check, const Binding& binding, Span at);

  // ── CheckStructDefaults.cpp ── a construction's fields: slot indices and values, in the order they are evaluated
  struct FieldInits {
    Vector<int> indices;
    Vector<ExprPtr> values;
  };

  // `X.builder()...build()` that leaves out a required field of X: the problem, at the `.build` (CheckBuilder.cpp); null for anything else.
  ExprPtr BuilderChainProblem(Checker& check, const AstExpr& node);

  void FillStructDefaults(Checker& check, const StructInfo* info, FieldInits& inits, Span at);  // each container field not given, made empty

  ExprPtr EmptyConstruction(Checker& check, Symbol structName, Span at);                       // `Name()` for a struct, or null

  // ── CheckSuffix.cpp ─────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────
  ExprPtr CheckPostfix(Checker& check, const AstExpr& node);

  ExprPtr CheckSuffix(Checker& check, ExprPtr receiver, const AstSuffix& suffix);

  Vector<ExprPtr> CheckAll(Checker& check, Range<ExprId> exprs);

  // ── CheckOps.cpp, CheckTypes.cpp, CheckCoerce.cpp, CheckEquality.cpp, CheckFields.cpp ───────────────────────────────────────────────────────
  BinOp BinOpOf(Operator op);

  TypePtr ResultType(BinOp op, const TypePtr& left);

  bool IsConcatType(const TypePtr& type);

  Optional<String> Mismatch(const TypePtr& expected, const TypePtr& found);  // a message when both kinds are known and differ

  ExprPtr Expecting(const TypePtr& expected, ExprPtr value);                 // the value, or a problem node saying what was expected

  ExprPtr OperandsProblem(const ExprPtr& left, const ExprPtr& right);        // two operands must have the same kind: the problem node, or null

  ExprPtr AnyTypeProblem(const ExprPtr& operand, const String& what, Span at);  // `what` on a type parameter (`T`): the problem node, or null

  // Each argument converted to its parameter's integer width (or float / double) with an ExprKind::Coerce node; any other argument is unchanged.
  Vector<ExprPtr> CoerceAll(Vector<ExprPtr> values, const Vector<TypePtr>& params);

  Optional<String> EqualityProblem(Checker& check, BinOp op, const Expr& left);  // why `left op right` is not allowed

  Optional<String> FieldProblem(Checker& check, const Expr& target);              // why `target` (a field read) cannot be assigned

  // ── CheckInterface.cpp ──────────────────────────────────────────────────────────────────────────────────────────────────────────────────────
  const InterfaceInfo* InterfaceOf(Checker& check, const TypePtr& type);

  ExprPtr BoxIfInterface(Checker& check, const TypePtr& expected, ExprPtr value);

  // `[a, b, c]` where the list `expected` (an array or a mutarray) is wanted: each element checked as its element type.
  ExprPtr ListLiteralExpecting(Checker& check, const AstExpr& node, const TypePtr& expected);

  // `if (c) a else b` where `expected` is wanted (CheckIfValue.cpp): each branch checked as that type.
  ExprPtr IfValueExpecting(Checker& check, const AstExpr& node, const TypePtr& expected);

  ExprPtr MatchValueExpecting(Checker& check, const AstExpr& node, const TypePtr& expected);  // CheckMatch.cpp

  // `return if (c) a else b`: the `if` itself, each branch returning its value (CheckIfValue.cpp).
  StmtPtr ReturnIfValue(Checker& check, const AstExpr& node);

  ExprPtr InterfaceCall(Checker& check, const InterfaceInfo& interface, ExprPtr receiver, Ident method, Vector<ExprPtr> args);

  Vector<ExprPtr> Fitted(Checker& check, Vector<ExprPtr> values, const Vector<TypePtr>& params);

  ExprPtr PushBoxed(Checker& check, ExprPtr call);

  // ── CheckHost.cpp ───────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────
  // `Math.sqrt(x)`, `ArrayList.new()`: a call on a host type itself. Null when `node` is not one. `expected` is the type the context wants.
  ExprPtr HostStaticCall(Checker& check, const AstExpr& node, const TypePtr* expected);

  // ── CheckCollections.cpp, CheckBuiltins.cpp, CheckContainers.cpp ────────────────────────────────────────────────────────────────────────────
  Optional<String> CollectionProblem(StringView name, const TypePtr& receiver);   // why `recv.name(...)` is not allowed

  Optional<String> AssignProblem(const TypePtr& base);                             // why `base[i] = v` is not allowed

  Optional<TypePtr> CollectionResult(StringView name, const TypePtr& receiver);    // with / has / add / freeze / thaw

  Optional<String> KeyProblem(Checker& check, const TypePtr& type);                // why `type` cannot be a map or set key

  Optional<TypePtr> CopyResult(const TypePtr& receiver);                           // the type of `x.copy()`, when x can be copied

  // The builtin node for a method call, with the call's receiver and arguments, giving `type` (what most builtin handlers answer).
  ExprPtr BuiltinOf(MethodCall& call, Builtin op, const TypePtr& type);

  // `receiver.name(args)` as a builtin (the registered handlers), or null when it is not one; null leaves `args` as they were.
  ExprPtr BuiltinMethod(Checker& check, Symbol name, const ExprPtr& receiver, Vector<ExprPtr>& args, Span at);

  ExprPtr IndexInto(ExprPtr receiver, Vector<ExprPtr> indices);                    // `base[i, j]`

  ExprPtr FreshStorage(Checker& check, const TypePtr& type, Span at);              // `let x: T` with no value: the new container, or null

  Optional<TypePtr> ElementOf(const TypePtr& type);                                // what indexing a value of this type gives

  ExprPtr CharAt(ExprPtr text, Vector<ExprPtr> indices);                           // `s[i]` (CheckStrings.cpp)

  // ── CheckStmt.cpp, CheckFlow.cpp ────────────────────────────────────────────────────────────────────────────────────────────────────────────
  // A block's statements, each seeing the names bound by those before it; names bound inside the block do not escape it.
  StmtList CheckBlock(Checker& check, const AstBlock& block);

  StmtPtr UnsupportedStmt(String what, Span at);

  StmtPtr CheckForInList(Checker& check, const AstStmt& stmt, ExprPtr source);  // `for (x in xs)` over a list (CheckForInList.cpp)

  TypePtr ResolveType(Checker& check, TypeId id);

  // A `mutarray`, `sparse` or `mutmap`: a pointer to a header that is allocated when the container is created, whose contents grow by being replaced.
  bool IsGrowable(const TypePtr& type);

}  // namespace polyir::front
