// The checker's extension points. The checker walks the tree by looking each node's kind up in a table, and a method call by looking the receiver's
// kind and the method's name up in another, so a new language construct or builtin is a new entry in a table, in a file of its own, not an edit to
// the checker's core.
//
//   expression kind   AddExpr(AstExprKind::VecLit, handler)           handler: ExprPtr (Checker&, const AstExpr&)
//   statement kind    AddStmt(AstStmtKind::Try, handler)              handler: StmtPtr (Checker&, const AstStmt&)  (binds and unbinds names itself)
//   builtin method    AddMethod("len", kindsOf(...), handler)         handler: ExprPtr (MethodCall&), null when `name` is not a builtin for this
//                                                                     receiver (the call then falls through to the next handler, and last to a call of
//                                                                     the function `name` with the receiver first)
//   function form     AddFunctionForm("len")                          `len(x, ...)` means `x.len(...)`
//   free builtin      AddFree("fence", handler)                       a builtin called by name with no receiver
//
// `SharedRegistry()` is built once per process (Registry.cpp): one line there per area, calling that area's `RegisterXxx(Registry&)`.
#pragma once

#include "front/Session.hpp"

namespace polyir::front {

  struct Checker;

  using ExprHandler = ExprPtr (*)(Checker& check, const AstExpr& node);
  using StmtHandler = StmtPtr (*)(Checker& check, const AstStmt& node);

  // `recv.name(args)`, or `name(recv, args)` for a function form. `args` are already checked; a handler may take them (move) or read them.
  struct MethodCall {
    Checker& check;
    Symbol name;
    ExprPtr receiver;
    Vector<ExprPtr>& args;
    Span at;
  };

  using BuiltinHandler = ExprPtr (*)(MethodCall& call);
  using FreeHandler = ExprPtr (*)(Checker& check, Vector<ExprPtr>& args, Span at);

  // A set of receiver type kinds a builtin handler applies to.
  using KindMask = uint32_t;
  constexpr KindMask kAnyKind = 0xFFFFFFFFu;

  // The handler tables. An expression or statement kind indexes its handler directly; a method name finds its list of handlers.
  struct Registry {
    // One handler of a method name, and the receiver kinds it takes.
    struct Entry {
      KindMask receivers;
      BuiltinHandler handler;
    };

    ExprHandler exprHandlers[64] = {};  // indexed by AstExprKind (64 leaves room for every kind)
    StmtHandler stmtHandlers[16] = {};  // indexed by AstStmtKind (16 leaves room for every kind)
    SymbolMap<Vector<Entry>> methods;
    SymbolMap<bool> functionForms;  // the names that may also be called as `name(receiver, args)`
    SymbolMap<FreeHandler> freeHandlers;
  };

  const Registry& SharedRegistry();

  void AddExpr(Registry& registry, AstExprKind kind, ExprHandler handler);

  void AddStmt(Registry& registry, AstStmtKind kind, StmtHandler handler);

  void AddMethod(Registry& registry, const char* name, KindMask receivers, BuiltinHandler handler);

  void AddFunctionForm(Registry& registry, const char* name);

  void AddFree(Registry& registry, const char* name, FreeHandler handler);

  ExprHandler ExprHandlerFor(const Registry& registry, AstExprKind kind);

  StmtHandler StmtHandlerFor(const Registry& registry, AstStmtKind kind);

  ExprPtr Method(const Registry& registry, MethodCall& call);           // the first handler for (receiver kind, name) that answers; null when none does

  bool IsFunctionForm(const Registry& registry, Symbol name);

  FreeHandler FreeHandlerFor(const Registry& registry, Symbol name);       // null when `name` is not a free builtin

  // The registration functions, one per area: each `Register...` function below is defined in the file that handles its area.
  void RegisterCoreExprs(Registry& registry);       // CheckExpr.cpp, CheckNames.cpp

  void RegisterCoreStmts(Registry& registry);       // CheckStmt.cpp, CheckFlow.cpp

  void RegisterCommonBuiltins(Registry& registry);  // CheckBuiltins.cpp: len, count, hash, push, copy, with, has, add, freeze, thaw

  void RegisterNames(Registry& registry);           // CheckNames.cpp (called by RegisterCoreExprs)

  void RegisterFlowStmts(Registry& registry);       // CheckFlow.cpp (called by RegisterCoreStmts)

  void RegisterClosures(Registry& registry);        // CheckClosure.cpp: closure literals

  void RegisterSums(Registry& registry);            // CheckSums.cpp: variant / either / optional constructors and builtins, resume / done

  void RegisterConstructors(Registry& registry);     // CheckVec.cpp: vec[...], mutarray<T>(), map<K,V>(), ...

  void RegisterShapes(Registry& registry);           // CheckShapes.cpp: rows, cols, dim, capacity, shuffle, atomics, fence

  void RegisterLists(Registry& registry);            // CheckLists.cpp: list literals [a, b]

  void RegisterIfValues(Registry& registry);         // CheckIfValue.cpp: if (c) a else b

  void RegisterMatches(Registry& registry);          // CheckMatch.cpp: match, and the switch of blocks

  void RegisterStrings(Registry& registry);          // CheckStrings.cpp: str, slice, find, truncate

  // The bit of one kind in a KindMask.
  constexpr KindMask KindBit(TypeKind kind) {
    return 1u << static_cast<unsigned>(kind);
  }

  inline ExprHandler ExprHandlerFor(const Registry& registry, AstExprKind kind) {
    return registry.exprHandlers[static_cast<size_t>(kind)];
  }

  inline StmtHandler StmtHandlerFor(const Registry& registry, AstStmtKind kind) {
    return registry.stmtHandlers[static_cast<size_t>(kind)];
  }

  inline bool IsFunctionForm(const Registry& registry, Symbol name) {
    return Contains(registry.functionForms, name);
  }

}  // namespace polyir::front
