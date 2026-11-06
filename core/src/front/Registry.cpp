// The checker's handler tables (Registry.hpp). Each area of the checker (CheckExpr.cpp, CheckStrings.cpp, ...) has a Register function
// that fills in the handlers it owns; makeRegistry calls them all once, in the order of kAreas, and the checker looks handlers up here.
#include "front/Registry.hpp"

namespace polyir::front {

  namespace {

    // The areas, in the order their handlers are asked. A new area adds its entry here.
    using RegisterArea = void (*)(Registry&);

    const RegisterArea kAreas[] = {
      RegisterCoreExprs, RegisterCoreStmts, RegisterCommonBuiltins, RegisterClosures, RegisterSums, RegisterConstructors, RegisterShapes, RegisterLists, RegisterIfValues,
      RegisterStrings, RegisterMatches,
    };

    Registry makeRegistry() {
      Registry registry;

      for (const RegisterArea area : kAreas) {
        area(registry);
      }

      return registry;
    }

  }  // namespace

  const Registry& SharedRegistry() {
    static const Registry built = makeRegistry();

    return built;
  }

  void AddExpr(Registry& registry, AstExprKind kind, ExprHandler handler) {
    registry.exprHandlers[static_cast<size_t>(kind)] = handler;
  }

  void AddStmt(Registry& registry, AstStmtKind kind, StmtHandler handler) {
    registry.stmtHandlers[static_cast<size_t>(kind)] = handler;
  }

  namespace {

    // The symbol of a handler's name. The registry is made once and shared by every check (SharedRegistry). So its names go in the
    // process-wide interner, the one ParseSource uses unless it is given another.
    Symbol handlerName(const char* name) {
      return Intern(GlobalInterner(), name);
    }

  }  // namespace

  // A name can have several handlers (one per kind of receiver, say); they are asked in the order they were added.
  void AddMethod(Registry& registry, const char* name, KindMask receivers, BuiltinHandler handler) {
    const Symbol key = handlerName(name);

    if (!Contains(registry.methods, key)) {
      Insert(registry.methods, key, Vector<Registry::Entry>());
    }

    Find(registry.methods, key)->push_back(Registry::Entry{receivers, handler});
  }

  void AddFunctionForm(Registry& registry, const char* name) {
    Insert(registry.functionForms, handlerName(name), true);
  }

  void AddFree(Registry& registry, const char* name, FreeHandler handler) {
    Insert(registry.freeHandlers, handlerName(name), handler);
  }

  FreeHandler FreeHandlerFor(const Registry& registry, Symbol name) {
    const FreeHandler* found = Find(registry.freeHandlers, name);

    return found == nullptr ? nullptr : *found;
  }

  namespace {

    // The entry's handler, when it takes receivers of this kind; null otherwise.
    ExprPtr tryEntry(const Registry::Entry& entry, KindMask receiverBit, MethodCall& call) {
      if ((entry.receivers & receiverBit) == 0) {
        return nullptr;
      }

      return entry.handler(call);
    }

  }  // namespace

  // The first handler of `call.name` that takes this kind of receiver and makes a node; null when none does (a handler may decline).
  ExprPtr Method(const Registry& registry, MethodCall& call) {
    const Vector<Registry::Entry>* entries = Find(registry.methods, call.name);

    if (entries == nullptr) {
      return nullptr;
    }

    const KindMask bit = KindBit(call.receiver->type->kind);

    for (const Registry::Entry& entry : *entries) {
      if (ExprPtr made = tryEntry(entry, bit, call)) {
        return made;
      }
    }

    return nullptr;
  }

}  // namespace polyir::front
