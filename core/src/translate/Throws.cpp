// Which functions can throw (Nodes.hpp), for a target that marks them (Swift writes `throws` on one and `try` before a call of
// one). A function throws when a `throw` or a call of a function that throws is not inside a `try` that catches it. An
// interface's method throws when a method of a struct implementing it does, so a call through the interface throws too.
#include "translate/Nodes.hpp"

namespace polyir::translate {

  namespace {

    // `Shape.area` for slot `slot` of interface `name`, or "".
    String interfaceMethod(const Module& module, const String& name, int64_t slot) {
      const InterfaceDef* def = FindInterface(module, name);
      const bool known = def != nullptr && slot >= 0 && static_cast<size_t>(slot) < def->methods.size();

      return known ? def->name + "." + def->methods[static_cast<size_t>(slot)].name : "";
    }

    bool callThrows(const Module& module, const Expr& e, const Set<String>& throwing) {
      if (e.kind == ExprKind::Call) {
        return throwing.count(e.name) > 0;
      }

      const bool viaInterface = e.kind == ExprKind::CallBuiltin && e.builtin == Builtin::InterfaceCall && e.operands.size() >= 2;
      const String& face = viaInterface && e.operands[0]->type ? e.operands[0]->type->name : "";

      return viaInterface && throwing.count(interfaceMethod(module, face, e.operands[1]->intValue)) > 0;
    }

    bool exprThrows(const Module& module, const ExprPtr& e, const Set<String>& throwing) {
      return e && CountInExpr(e, [&](const Expr& each) { return callThrows(module, each, throwing); }) > 0;
    }

    bool anyThrows(const Module& module, const Vector<ExprPtr>& exprs, const Set<String>& throwing) {
      return any_of(exprs.begin(), exprs.end(), [&](const ExprPtr& e) { return exprThrows(module, e, throwing); });
    }

    // Whether the statement's own expressions call a function that throws.
    bool statementCalls(const Module& module, const Stmt& s, const Set<String>& throwing) {
      const bool own = exprThrows(module, s.value, throwing) || exprThrows(module, s.target, throwing);

      return own || anyThrows(module, s.initializers, throwing) || anyThrows(module, s.steps, throwing) || anyThrows(module, s.arguments, throwing);
    }

    bool bodyThrows(const Module& module, const StmtList& body, const Set<String>& throwing);

    // Whether an arm of a `select` throws (the other blocks are the statement's body and else).
    bool armsThrow(const Module& module, const Stmt& s, const Set<String>& throwing) {
      for (const StmtList& arm : s.caseBodies) {
        if (bodyThrows(module, arm, throwing)) {
          return true;
        }
      }

      return false;
    }

    bool bodyThrows(const Module& module, const StmtList& body, const Set<String>& throwing) {
      for (const StmtPtr& s : body) {
        const bool catches = s->kind == StmtKind::Try && !s->name.empty();
        const bool nested = (!catches && bodyThrows(module, s->body, throwing)) || bodyThrows(module, s->elseBody, throwing);

        if (s->kind == StmtKind::Throw || statementCalls(module, *s, throwing) || nested || armsThrow(module, *s, throwing)) {
          return true;
        }
      }

      return false;
    }

    // Whether a struct implementing interface `face` has a method `method` that throws.
    bool implementationThrows(const Module& module, const InterfaceDef& face, const String& method, const Set<String>& throwing) {
      for (const StructDef& def : module.structs) {
        for (const FuncDef* each : Implements(module, def, face) ? MethodsOf(module, def) : Vector<const FuncDef*>{}) {
          if (MethodName(*each, def) == method && throwing.count(each->name) > 0) {
            return true;
          }
        }
      }

      return false;
    }

    // One round over every function and interface method; true when it found another that throws.
    bool addThrowing(const Module& module, Set<String>& throwing) {
      bool added = false;

      for (const Vector<FuncDef>* functions : {&module.functions, &module.genericFunctions}) {
        for (const FuncDef& def : *functions) {
          added = (throwing.count(def.name) == 0 && bodyThrows(module, def.body, throwing) && throwing.insert(def.name).second) || added;
        }
      }

      for (const InterfaceDef& face : module.interfaces) {
        for (const InterfaceMethod& method : face.methods) {
          const String key = face.name + "." + method.name;
          added = (throwing.count(key) == 0 && implementationThrows(module, face, method.name, throwing) && throwing.insert(key).second) || added;
        }
      }

      return added;
    }

  }  // namespace

  Set<String> ThrowingFunctions(const Module& module) {
    Set<String> throwing;

    while (addThrowing(module, throwing)) {
    }

    return throwing;
  }

  bool CallThrows(const NodeContext& context, const Expr& e) {
    return callThrows(*context.module, e, context.throwing);
  }

}  // namespace polyir::translate
