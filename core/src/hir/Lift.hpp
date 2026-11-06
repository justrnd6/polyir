// The .poly forms found again in the HIR the front end made from them: a method in its struct, `for (x in xs)`, `for (k in m)`,
// an `if` value, a list literal, text with `${x}` holes. WritePoly (PolySource.cpp) writes them back as .poly; the translate engine
// (translate/) gives each its own construct, so a target writes them as its language does. Each function here only looks.
#pragma once

#include "polyir/Hir.hpp"

namespace polyir {

  // A walk over a table's slots, `for (key in table)`: how many statements it takes, what it walks (`source` when the front end held
  // the table in a hidden `let`), the table's and slot's names, and the statements run for each entry.
  struct WalkShape {
    size_t count = 0;
    ExprPtr source;
    String table;
    String slot;
    const StmtList* inside = nullptr;
  };

  // A hidden `let` holding what the loop after it walks, read nowhere else: a loop over a list's elements, or a walk over a table.
  struct HeldLoop {
    const Stmt* held = nullptr;
    const Stmt* loop = nullptr;
    Optional<WalkShape> walk;  // set for a walk over a table; empty for a loop over a list
  };

  // An `if` value, `if (c) a else if (d) b else e`: each test with the value it gives, and the value when no test holds.
  struct IfValue {
    Vector<Pair<ExprPtr, ExprPtr>> arms;
    ExprPtr otherwise;
  };

  // `let n: T = if (c) a else b` from a `var` and the `if` that fills it: the `var`, the value, and whether n changes later.
  struct DeclaredIf {
    const Stmt* slot = nullptr;
    IfValue value;
    bool changes = false;
  };

  // The generics written once (Hir.hpp `Generic`), each by the name of its generic: its struct or function as declared.
  struct WrittenGenerics {
    Map<String, const StructDef*> structs;
    Map<String, const FuncDef*> functions;
  };

  using ExprTest = Function<bool(const Expr&)>;

  // ── reading the HIR ──

  bool Reads(const Expr& e, const String& name);  // a read of the local or `var` slot `name`

  bool IsConstantInt(const ExprPtr& e, int64_t value);

  bool IsLiteral(const Expr& e);  // a constant number, flag or text

  bool IsCollection(const Type& t);  // a list, map or set

  size_t CountInExpr(const ExprPtr& e, const ExprTest& test);  // how many expressions in `e` pass `test`, closures included

  size_t CountIn(const StmtList& body, const ExprTest& test);

  ExprTest ReadsName(const String& name);

  bool AssignsIn(const StmtList& body, const String& name);  // whether `name` gets a new value in `body`, closures included

  bool IsHiddenName(const String& name);  // a name only a front end made: nina-lite's start with `__`

  bool IsIfValueName(const String& name);  // the front end's hidden variable for an `if` value

  // The front end's hidden variable for a `match` value (CheckMatch.cpp). It is written as the `var` and the switch or `if` chain
  // that fill it. It is never folded into an `if` value, which, checked again, would be another program.
  bool IsMatchValueName(const String& name);

  bool IsUnsetVar(const Stmt& s);  // a `var` with no value

  // ── declarations ──

  const StructDef* AnyStruct(const Module& module, const String& name);  // a struct, or a generic struct as declared (`Box_T`)

  const StructDef* OwnerOf(const Module& module, const FuncDef& function);  // the struct `function` is a method of, or null

  String MethodName(const FuncDef& function, const StructDef& owner);  // `sum` for `Point_sum`

  Vector<const FuncDef*> GenericMethodsOf(const Module& module, const StructDef& def);

  bool WorksForEveryType(const Generic& generic);

  bool HasOrder(const Module& module);  // whether the module says the order its declarations were written in

  Vector<DeclRef> DeclarationOrder(const Module& module);  // that order, or by kind (structs, interfaces, functions)

  Vector<const FuncDef*> FunctionsInOrder(const Module& module);

  Vector<const FuncDef*> MethodsOf(const Module& module, const StructDef& def);

  bool HasAnnotation(const Vector<Annotation>& annotations, const String& name);

  // A struct's static functions and nested structs; less what `@Builder` made, which reading `@Builder` makes again, unless `withBuilder`.
  Vector<const FuncDef*> StaticsOf(const Module& module, const StructDef& def, bool withBuilder = false);

  Vector<const StructDef*> NestedIn(const Module& module, const StructDef& def, bool withBuilder = false);

  bool Implements(const Module& module, const StructDef& def, const InterfaceDef& interface);

  Optional<String> PlaceOf(const Module& module, const DeclRef& ref);  // the module a declaration written on its own sits in

  WrittenGenerics FindWrittenGenerics(const Module& module, const Function<bool(const StructDef&)>& writtenOnce);

  // ── statement shapes ──

  Optional<WalkShape> CountedWalk(const Stmt& s);

  Optional<WalkShape> WalkAt(const StmtList& body, size_t i);

  String KeyName(const WalkShape& walk);  // the entry's key, from the `let k = assocKey(...)` that starts the body when there is one

  ExprTest ReadsSlotOf(const String& table, const String& slot);

  const Stmt* ListLoopAt(const StmtList& body, size_t i);  // the front end's loop over a list (3 statements), or null

  String CountedList(const Stmt& s);  // the list a counted loop over its indices reads only the element of, or ""

  Optional<HeldLoop> HeldLoopAt(const StmtList& body, size_t i);

  Optional<IfValue> IfValueOf(const Stmt& s, const String& target);  // each branch of `s` only gives `target` its value

  Optional<IfValue> ReturnIfOf(const Stmt& s);  // an `if` whose branches only return

  Optional<IfValue> InlinedAt(const StmtList& body, size_t i);  // a hidden `var` and its `if`, read once just after

  Optional<DeclaredIf> DeclaredAt(const StmtList& body, size_t i);

  // ── expression shapes ──

  Vector<ExprPtr> ListValues(const Expr& e);  // `[a, b]` as the front end makes it; empty when `e` is not one

  Vector<const Expr*> JoinedPieces(const Expr& e);  // the pieces of text joined with `+`, in order

  bool IsTextOf(const Expr& e);  // `str(x)`

  bool IsInterpolation(const Vector<const Expr*>& pieces);  // what .poly's own `"n = ${x}"` makes

  bool IsOtherKind(const Expr& e);  // a copy into the other kind of collection: `freeze()` or `thaw()`

  bool IsPlainCopy(const ExprPtr& e);  // a copy of the same kind

}  // namespace polyir
