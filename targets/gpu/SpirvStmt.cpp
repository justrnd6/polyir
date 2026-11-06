// HIR statements as SPIR-V blocks (SpirvStmt.nina). Control flow in SPIR-V is structured: before a conditional branch the writer says where the paths
// meet again (`OpSelectionMerge`), and a loop names its exit and its continue block (the step run between rounds) in `OpLoopMerge`.
#include "gpu/SpirvLower.hpp"

namespace polyir {

  namespace {

    // The scope with these names added at the back, so a lookup finds them first.
    SpvScope withBindings(SpvScope scope, const SpvScope& added) {
      scope.insert(scope.end(), added.begin(), added.end());

      return scope;
    }

    // The loop's body block: the statements, then on to the continue block unless they already left it (a `return`).
    void loopBody(SpvBody& body, const SpvScope& inner, const StmtList& statements, Id inside, Id step, const TypePtr& returns) {
      Begin(body.function, inside);
      LowerBlock(body, inner, statements, returns);

      if (IsOpen(body.function)) {
        Branch(body.function, step);
      }
    }

    // The continue block: the carried variables advanced, back to the header; then the code after the loop starts at `exit`.
    void loopStep(SpvBody& body, const SpvScope& inner, const SpvScope& carried, const Stmt& s, Id step, Id header, Id exit) {
      Begin(body.function, step);
      Advance(body, inner, carried, s);
      Branch(body.function, header);
      Begin(body.function, exit);
    }

  }  // namespace

  // The statements in order, each seeing the names bound before it; the names in scope afterwards.
  SpvScope LowerBlock(SpvBody& body, SpvScope scope, const StmtList& stmts, const TypePtr& returns) {
    for (const StmtPtr& s : stmts) {
      scope = LowerStmt(body, move(scope), *s, returns);
    }

    return scope;
  }

  SpvScope LowerStmt(SpvBody& body, SpvScope scope, const Stmt& s, const TypePtr& returns) {
    if (s.kind == StmtKind::Let || s.kind == StmtKind::Var) {
      return Declaration(body, move(scope), s);
    }

    if (s.kind == StmtKind::Assign) {
      Assign(body, scope, s);
    } else if (s.kind == StmtKind::ExprStmt) {
      LowerExpr(body, scope, *s.value);
    } else {
      LowerFlow(body, scope, s, returns);
    }

    return scope;
  }

  void LowerFlow(SpvBody& body, const SpvScope& scope, const Stmt& s, const TypePtr& returns) {
    if (s.kind == StmtKind::Return && s.value) {
      return ReturnValue(body.function, ExprTo(body, scope, *s.value, returns));
    }

    if (s.kind == StmtKind::If) {
      return IfStmt(body, scope, s, returns);
    }

    if (s.kind == StmtKind::For) {
      return ForStmt(body, scope, s, returns);
    }

    ReportProblem(body.module, "a statement the SPIR-V writer does not handle");
  }

  // `let`: the value computed once and named. `var`: a local variable, started at the given value (or zero).
  SpvScope Declaration(SpvBody& body, SpvScope scope, const Stmt& s) {
    if (s.kind == StmtKind::Let) {
      return withBindings(scope, {SpvBinding{s.name, ExprTo(body, scope, *s.value, s.type), s.type, false}});
    }

    const Id slot = Variable(body.function, SpvType(body.module, s.type));
    const Id start = StartValue(body, scope, s);
    Store(body.function, slot, start);

    return withBindings(move(scope), {SpvBinding{s.name, slot, s.type, true}});
  }

  Id StartValue(SpvBody& body, const SpvScope& scope, const Stmt& s) {
    return s.value ? ExprTo(body, scope, *s.value, s.type) : SpvConstant(body.module, s.type, 0.0);
  }

  // `x = v` stores to a variable; `out[k] = v` stores to a slot of a writable array or of workgroup memory.
  void Assign(SpvBody& body, const SpvScope& scope, const Stmt& s) {
    const Expr& target = *s.target;

    if (target.kind == ExprKind::LoadSlot || target.kind == ExprKind::Local) {
      return AssignVariable(body, scope, target.name, *s.value);
    }

    if (target.kind == ExprKind::IndexGet) {
      return AssignSlot(body, scope, *target.operands[0], *target.operands[1], *s.value);
    }

    ReportProblem(body.module, "an assignment the SPIR-V writer does not handle");
  }

  void AssignVariable(SpvBody& body, const SpvScope& scope, const String& name, const Expr& value) {
    for (auto b = scope.rbegin(); b != scope.rend(); ++b) {
      if (b->name == name) {
        return Store(body.function, b->id, ExprTo(body, scope, value, b->type));
      }
    }

    ReportProblem(body.module, "the name " + name + " is not in scope");
  }

  void AssignSlot(SpvBody& body, const SpvScope& scope, const Expr& base, const Expr& position, const Expr& value) {
    const SpvArray* array = ArrayOf(body, base);

    if (array == nullptr) {
      return ReportProblem(body.module, "only a writable array or workgroup memory can be written to");
    }

    const Id slot = ExprTo(body, scope, position, SpvInt32());
    const Id stored = ExprTo(body, scope, value, array->element);
    StoreSlot(body, *array, slot, stored);
  }

  // The condition, where the paths meet, a branch, the two arms, and the meeting block. An arm that ended in a `return` is already closed.
  void IfStmt(SpvBody& body, const SpvScope& scope, const Stmt& s, const TypePtr& returns) {
    const Id test = ExprTo(body, scope, *s.value, SpvFlag());
    const Id thenBlock = Fresh(body.module), elseBlock = Fresh(body.module), merge = Fresh(body.module);
    SelectionMerge(body.function, merge);
    BranchIf(body.function, test, thenBlock, elseBlock);

    Arm(body, scope, thenBlock, s.body, merge, returns);
    Arm(body, scope, elseBlock, s.elseBody, merge, returns);
    Begin(body.function, merge);
  }

  void Arm(SpvBody& body, const SpvScope& scope, Id label, const StmtList& statements, Id merge, const TypePtr& returns) {
    Begin(body.function, label);
    LowerBlock(body, scope, statements, returns);

    if (IsOpen(body.function)) {
      Branch(body.function, merge);
    }
  }

  // `for (i = 0, s = 0; cond; i = i + 1, s = s + i) { body }`: each carried name is a variable started at its first value. The loop is a header (the
  // structure marker), the condition (leave or enter the body), the body, and a continue block. The continue block works out every new value
  // from the OLD ones.
  void ForStmt(SpvBody& body, const SpvScope& scope, const Stmt& s, const TypePtr& returns) {
    const SpvScope carried = CarriedVariables(body, scope, s);
    const SpvScope inner = withBindings(scope, carried);

    const Id header = Fresh(body.module), test = Fresh(body.module), inside = Fresh(body.module), step = Fresh(body.module), exit = Fresh(body.module);
    LoopHeader(body.function, header, exit, step, test);
    BranchIf(body.function, ExprTo(body, inner, *s.value, SpvFlag()), inside, exit);

    loopBody(body, inner, s.body, inside, step, returns);
    loopStep(body, inner, carried, s, step, header, exit);
  }

  SpvScope CarriedVariables(SpvBody& body, const SpvScope& scope, const Stmt& s) {
    SpvScope out;

    for (size_t i = 0; i < s.names.size(); i++) {
      out.push_back(CarriedVariable(body, scope, s.names[i], *s.initializers[i]));
    }

    return out;
  }

  // One carried name: a variable typed like its first value and started at it (computed in the scope around the loop).
  SpvBinding CarriedVariable(SpvBody& body, const SpvScope& scope, const String& name, const Expr& init) {
    const Id slot = Variable(body.function, SpvType(body.module, init.type));
    Store(body.function, slot, ExprTo(body, scope, init, init.type));

    return SpvBinding{name, slot, init.type, true};
  }

  // Every new value is computed from the old ones first, then all are stored.
  void Advance(SpvBody& body, const SpvScope& scope, const SpvScope& carried, const Stmt& s) {
    Words next;

    for (size_t i = 0; i < carried.size(); i++) {
      next.push_back(ExprTo(body, scope, *s.steps[i], carried[i].type));
    }

    for (size_t i = 0; i < carried.size(); i++) {
      Store(body.function, carried[i].id, next[i]);
    }
  }

}  // namespace polyir
