// The calls in a kernel's body, as the shader writers ask about them (HirWalk.nina `calls`, `calleeNames`, `firstArguments`; KernelGlsl.nina
// `calledKernels`). A statement's expressions are looked at first, then its nested blocks; a closure, variant or either payload is not entered.
#include "gpu/KernelCommon.hpp"

namespace polyir {

  namespace {

    void stmtCalls(const Stmt& s, Vector<const Expr*>& out);

    void blockCalls(const StmtList& stmts, Vector<const Expr*>& out) {
      for (const StmtPtr& s : stmts) {
        stmtCalls(*s, out);
      }
    }

    // HirWalk.nina's `kids` lists no sub-expressions for a closure, a variant or an either, so the walk does not enter them. The
    // shader writers' answers then match the Nina writer's.
    bool hasNoWalkedOperands(ExprKind kind) {
      return kind == ExprKind::ClosureLit || kind == ExprKind::VariantLit || kind == ExprKind::EitherLit;
    }

    void exprCalls(const Expr* e, Vector<const Expr*>& out) {
      if (e == nullptr) {
        return;
      }

      if (e->kind == ExprKind::Call) {
        out.push_back(e);
      }

      if (hasNoWalkedOperands(e->kind)) {
        return;
      }

      for (const ExprPtr& operand : e->operands) {
        exprCalls(operand.get(), out);
      }
    }

    Vector<const Expr*> forExprs(const Stmt& s) {
      Vector<const Expr*> out;

      for (const ExprPtr& e : s.initializers) {
        out.push_back(e.get());
      }

      out.push_back(s.value.get());

      for (const ExprPtr& e : s.steps) {
        out.push_back(e.get());
      }

      return out;
    }

    Vector<const Expr*> stmtExprs(const Stmt& s) {
      switch (s.kind) {
        case StmtKind::Assign:
          return {s.target.get(), s.value.get()};
        case StmtKind::For:
          return forExprs(s);
        case StmtKind::Let: case StmtKind::Var: case StmtKind::If: case StmtKind::Return: case StmtKind::ExprStmt: case StmtKind::Throw:
          return {s.value.get()};
        default:
          return {};
      }
    }

    Vector<const StmtList*> stmtBlocks(const Stmt& s) {
      if (s.kind == StmtKind::If || s.kind == StmtKind::Try) {
        return {&s.body, &s.elseBody};
      }

      if (s.kind == StmtKind::For) {
        return {&s.body};
      }

      return {};
    }

    void stmtCalls(const Stmt& s, Vector<const Expr*>& out) {
      for (const Expr* e : stmtExprs(s)) {
        exprCalls(e, out);
      }

      for (const StmtList* block : stmtBlocks(s)) {
        blockCalls(*block, out);
      }
    }

    Vector<const Expr*> callsIn(const StmtList& body) {
      Vector<const Expr*> out;
      blockCalls(body, out);

      return out;
    }

    void addMissing(Vector<String>& names, const Vector<String>& more) {
      for (const String& name : more) {
        if (!Listed(names, name)) {
          names.push_back(name);
        }
      }
    }

  }  // namespace

  Vector<String> CalleeNames(const StmtList& body) {
    Vector<String> out;

    for (const Expr* call : callsIn(body)) {
      out.push_back(call->name);
    }

    return out;
  }

  Vector<String> FirstArguments(const StmtList& body, const String& callee) {
    Vector<String> out;

    for (const Expr* call : callsIn(body)) {
      if (call->name == callee && !call->operands.empty() && call->operands[0]->kind == ExprKind::Local) {
        out.push_back(call->operands[0]->name);
      }
    }

    return out;
  }

  Vector<String> CalledKernels(const FuncDef& target, const Vector<const FuncDef*>& earlier) {
    Vector<String> names = CalleeNames(target.body);

    for (int round = 0; round < 4; round++) {
      for (const FuncDef* def : earlier) {
        if (Listed(names, def->name)) {
          addMissing(names, CalleeNames(def->body));
        }
      }
    }

    return names;
  }

  Vector<const FuncDef*> ComputeKernels(const Module& module) {
    Vector<const FuncDef*> out;

    for (const FuncDef& def : module.functions) {
      if (def.stage == KernelStage::Compute) {
        out.push_back(&def);
      }
    }

    return out;
  }

  Vector<const FuncDef*> KernelsBefore(const Vector<const FuncDef*>& kernels, size_t i) {
    return Vector<const FuncDef*>(kernels.begin(), kernels.begin() + static_cast<long>(i));
  }

}  // namespace polyir
