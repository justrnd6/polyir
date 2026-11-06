// Statements (CheckStmt.nina): `let`, `var`, `if`, `for`, `return`, assignment and expression statements. Control flow that moves between labels or
// handlers (`goto`, `branch`, `switch`, `try`, `throw`, `yield`) is in CheckFlow.cpp. A handler checks one statement and updates the scope for the ones
// after it (a `let` or `var` adds a name; a `for` leaves its loop variables visible, as today); `CheckBlock` drops the names a block bound.
#include "front/Checker.hpp"
#include "front/Resolve.hpp"
#include "polyir/HirPrint.hpp"

namespace polyir::front {

  StmtPtr UnsupportedStmt(String what, Span at) {
    StmtPtr made = NewStmt(StmtKind::ExprStmt, at);
    made->value = Unsupported(move(what), at);

    return made;
  }

  TypePtr ResolveType(Checker& check, TypeId id) {
    return ResolveType(check.session, *check.program, id);
  }

  namespace {

    // Dispatches on the statement's kind; a kind nothing is registered for is `?statement?`.
    StmtPtr checkStmt(Checker& check, StmtId id) {
      const AstStmt& stmt = check.program->stmts[id];
      const StmtHandler handler = StmtHandlerFor(check.registry, stmt.kind);

      if (handler == nullptr) {
        return UnsupportedStmt("statement", NoSpan());
      }

      return handler(check, stmt);
    }

    // The checked statement, after what checking it hoisted to go before it.
    void appendChecked(Checker& check, StmtList& out, StmtId id) {
      StmtPtr made = checkStmt(check, id);

      for (StmtPtr& before : check.hoisted) {
        out.push_back(move(before));
      }

      check.hoisted.clear();
      out.push_back(move(made));
    }

  }  // namespace

  // What the statement holding the block hoisted (from its condition, say) stays outside it: the block's statements hoist their own.
  StmtList CheckBlock(Checker& check, const AstBlock& block) {
    const size_t mark = Mark(check.scope);
    StmtList outer = move(check.hoisted);
    StmtList out;
    check.hoisted.clear();

    for (const StmtId id : check.program->stmtRefs[block.stmts]) {
      appendChecked(check, out, id);
    }

    check.hoisted = move(outer);
    Reset(check.scope, mark);

    return out;
  }

  bool IsGrowable(const TypePtr& type) {
    return type->kind == TypeKind::DynamicArray || type->kind == TypeKind::Sparse || type->kind == TypeKind::Hashmap;
  }

  namespace {

    // ── let / var ───────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────

    // A `let` or `var`: positioned at the name.
    StmtPtr named(Checker& check, StmtKind kind, Ident name, const TypePtr& type, ExprPtr value) {
      StmtPtr made = NewStmt(kind, SpanAt(name.pos));
      made->name = Str(check, name.name);
      made->type = type;
      made->value = move(value);

      return made;
    }

    // A `let` named and bound with its type. A growable one (IsGrowable) is a `var` in a slot. A `push` replaces its contents, which then
    // changes what every later use of the name sees, even from inside an `if` or a loop.
    StmtPtr letBound(Checker& check, const AstStmt& stmt, const TypePtr& type, ExprPtr value) {
      const bool growable = IsGrowable(type);
      StmtPtr made = named(check, growable ? StmtKind::Var : StmtKind::Let, stmt.name, type, move(value));
      BindName(check.scope, stmt.name.name, growable ? BindingKind::Slot : BindingKind::Local, type);

      return made;
    }

    // `val b: T = a` is `let b: T = val a`: the name is fixed, and it names a copy.
    StmtPtr letWithValue(Checker& check, const AstStmt& stmt, const TypePtr& type) {
      ExprPtr value = CheckExprExpecting(check, stmt.value, type);
      value = Expecting(type, stmt.isVal ? ValCopy(move(value)) : move(value));

      return letBound(check, stmt, type, move(value));
    }

    // `let x: T` with no value: a container starts empty (a growable one in a slot, as with a value).
    StmtPtr letStorage(Checker& check, const AstStmt& stmt, const TypePtr& type) {
      ExprPtr fresh = FreshStorage(check, type, SpanAt(stmt.name.pos));
      const bool growable = fresh && IsGrowable(type);
      BindName(check.scope, stmt.name.name, growable ? BindingKind::Slot : BindingKind::Local, type);

      if (!fresh) {
        return UnsupportedStmt("let without a value", SpanAt(stmt.name.pos));
      }

      return named(check, growable ? StmtKind::Var : StmtKind::Let, stmt.name, type, move(fresh));
    }

    // ── a `let` or `var` written without a type: the name has its value's type (`let g = populated()`) ──────────────────────────────────────────

    String needsTypeOrValue(const String& name) {
      return "`" + name + "` needs a type or a value (`let " + name + ": T` or `let " + name + " = value`)";
    }

    // No value: answers null and puts the reason in `why`.
    ExprPtr noValueBecause(String reason, String& why) {
      why = move(reason);

      return nullptr;
    }

    // A name with no type is still bound (to void), so its later uses are not reported as unknown names as well.
    StmtPtr untypedName(Checker& check, const AstStmt& stmt, BindingKind kind, const String& why) {
      BindName(check.scope, stmt.name.name, kind, VoidType());

      return UnsupportedStmt(why, SpanAt(stmt.name.pos));
    }

    // The value, or (in `why`) why there is none to take a type from. A value that is already a problem is reported as itself.
    ExprPtr inferredValue(Checker& check, const AstStmt& stmt, String& why) {
      const String name = Str(check, stmt.name.name);

      if (!Valid(stmt.value)) {
        return noValueBecause(needsTypeOrValue(name), why);
      }

      ExprPtr value = CheckExpr(check, stmt.value);

      if (IsVoid(*value->type) && value->kind != ExprKind::Unsupported) {
        return noValueBecause("`" + name + "` has no type, and its value gives nothing to take one from", why);
      }

      return value;
    }

    StmtPtr inferredLet(Checker& check, const AstStmt& stmt) {
      String why;
      ExprPtr value = inferredValue(check, stmt, why);

      if (!value) {
        return untypedName(check, stmt, BindingKind::Local, why);
      }

      const TypePtr type = value->type;

      if (stmt.isVal) {
        value = ValCopy(move(value));
      }

      return letBound(check, stmt, type, move(value));
    }

    StmtPtr inferredVar(Checker& check, const AstStmt& stmt) {
      String why;
      ExprPtr value = inferredValue(check, stmt, why);

      if (!value) {
        return untypedName(check, stmt, BindingKind::Slot, why);
      }

      const TypePtr type = value->type;
      StmtPtr made = named(check, StmtKind::Var, stmt.name, type, move(value));
      BindName(check.scope, stmt.name.name, BindingKind::Slot, type);

      return made;
    }

    StmtPtr letStmt(Checker& check, const AstStmt& stmt) {
      if (!Valid(stmt.type)) {
        return inferredLet(check, stmt);
      }

      const TypePtr type = ResolveType(check, stmt.type);

      return Valid(stmt.value) ? letWithValue(check, stmt, type) : letStorage(check, stmt, type);
    }

    // A typed `var`'s first value. With no value a container starts empty (as for `let`); anything else has no value until assigned.
    ExprPtr varInit(Checker& check, const AstStmt& stmt, const TypePtr& type) {
      if (!Valid(stmt.value)) {
        return FreshStorage(check, type, SpanAt(stmt.name.pos));
      }

      return Expecting(type, CheckExprExpecting(check, stmt.value, type));
    }

    StmtPtr varStmt(Checker& check, const AstStmt& stmt) {
      if (!Valid(stmt.type)) {
        return inferredVar(check, stmt);
      }

      const TypePtr type = ResolveType(check, stmt.type);
      ExprPtr init = varInit(check, stmt, type);
      StmtPtr made = named(check, StmtKind::Var, stmt.name, type, move(init));
      BindName(check.scope, stmt.name.name, BindingKind::Slot, type);

      return made;
    }

    // ── if / for ────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────

    StmtPtr ifStmt(Checker& check, const AstStmt& stmt) {
      ExprPtr cond = CheckExpr(check, stmt.cond);
      StmtPtr made = NewStmt(StmtKind::If, cond->at);
      made->body = CheckBlock(check, stmt.body);
      made->elseBody = CheckBlock(check, stmt.other);
      made->value = move(cond);

      return made;
    }

    // Loop variables are bound as read-only values whose type is their initial value's. The condition, steps and body all see them, and so do
    // the statements after the loop.
    void bindLoopVars(Checker& check, Stmt& made, View<AstForBinding> inits) {
      for (size_t i = 0; i < inits.size(); i++) {
        made.names.push_back(Str(check, inits[i].name.name));
        BindName(check.scope, inits[i].name.name, BindingKind::Local, made.initializers[i]->type);
      }
    }

    // The initial values, then the loop variables they give types to.
    void initLoopVars(Checker& check, Stmt& made, View<AstForBinding> inits) {
      for (const AstForBinding& init : inits) {
        made.initializers.push_back(CheckExpr(check, init.value));
      }

      bindLoopVars(check, made, inits);
    }

    // An `if` value is worked out once, before its statement, so a loop's test or step (worked out every round) cannot hold one.
    ExprPtr everyRound(Checker& check, ExprId id) {
      const size_t before = check.hoisted.size();
      ExprPtr value = CheckExpr(check, id);

      if (check.hoisted.size() == before) {
        return value;
      }

      check.hoisted.resize(before);

      return Unsupported("an `if` value cannot sit in a loop's test or step: work it out in the loop's body", value->at);
    }

    // The condition, the steps and the body, which see the loop variables.
    void forTail(Checker& check, Stmt& made, const AstStmt& stmt) {
      made.value = everyRound(check, stmt.cond);

      for (const AstForBinding& step : check.program->forBindings[stmt.steps]) {
        made.steps.push_back(everyRound(check, step.value));
      }

      made.body = CheckBlock(check, stmt.body);
      made.at = made.value->at;
    }

    StmtPtr forInStmt(Checker& check, const AstStmt& stmt);

    String stepMessage(Checker& check, const AstForBinding& step, size_t position) {
      return "a for loop's steps assign its own variables, in order: `" + Str(check, step.name.name) + "` is not the loop's variable number " +
        to_string(position + 1) + " (step a variable declared before the loop in the body)";
    }

    // The steps assign the loop's own variables, in the order they are declared (`for (i = 0, j = 9; ...; i = i + 1, j = j - 1)`). A variable
    // declared before the loop is stepped in the body instead.
    Optional<String> stepProblem(Checker& check, const AstStmt& stmt) {
      const View<AstForBinding> inits = check.program->forBindings[stmt.inits];
      const View<AstForBinding> steps = check.program->forBindings[stmt.steps];

      for (size_t i = 0; i < steps.size(); i++) {
        if (i >= inits.size() || steps[i].name.name != inits[i].name.name) {
          return stepMessage(check, steps[i], i);
        }
      }

      return nullopt;
    }

    StmtPtr forStmt(Checker& check, const AstStmt& stmt) {
      if (stmt.isForIn) {
        return forInStmt(check, stmt);
      }

      if (const Optional<String> why = stepProblem(check, stmt)) {
        return UnsupportedStmt(*why, CheckExpr(check, stmt.cond)->at);
      }

      StmtPtr made = NewStmt(StmtKind::For, NoSpan());
      initLoopVars(check, *made, check.program->forBindings[stmt.inits]);
      forTail(check, *made, stmt);

      return made;
    }

    // ── for (x in gen) ──────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────
    //
    // A loop over what a coroutine yields (CheckForIn.nina), written as the resume loop it means:
    //
    //   let forGen_L_C: coro<T> = gen
    //   var forStep_L_C: variant<T> = forGen_L_C.resume()
    //   for (; forStep_L_C.isCase(0); ) { let x: T = forStep_L_C.unwrapAt(0)  body  forStep_L_C = forGen_L_C.resume() }
    //
    // The first two statements go before the loop (`check.hoisted`). A `return` in the body closes the coroutine first.

    // The names and types one loop works with.
    struct ForIn {
      String name;       // the loop variable, `x`
      TypePtr yields;    // what the coroutine yields, `T`
      String coroutine;  // the hidden local that holds the coroutine, `forGen_L_C`
      String step;       // the hidden slot that holds the last resume's result, `forStep_L_C`
      Span at;
    };

    ExprPtr coroutineOf(const ForIn& loop) {
      return NamedRef(ExprKind::Local, loop.coroutine, MakeCoroutine(loop.yields), loop.at);
    }

    TypePtr stepTypeOf(const ForIn& loop) {
      return MakeVariant({loop.yields});
    }

    ExprPtr stepOf(const ForIn& loop) {
      return NamedRef(ExprKind::LoadSlot, loop.step, stepTypeOf(loop), loop.at);
    }

    // `.resume()` carries the yielded type; the slot it is stored in is the `variant<T>` (as for a hand-written loop).
    ExprPtr resumedOf(const ForIn& loop) {
      return BuiltinNode(Builtin::CoroutineResume, coroutineOf(loop), {}, loop.yields, loop.at);
    }

    ExprPtr caseZero(const ForIn& loop) {
      return ConstInt(0, IntType(64), loop.at);
    }

    StmtPtr letNamed(String name, TypePtr type, ExprPtr value, Span at, StmtKind kind) {
      StmtPtr made = NewStmt(kind, at);
      made->name = move(name);
      made->type = move(type);
      made->value = move(value);

      return made;
    }

    // Every `return` in the body (nested blocks too, not closures: they return from themselves) closes the coroutine before it leaves.
    StmtList closingReturns(StmtList stmts, const ForIn& loop);

    StmtPtr closeBefore(const Stmt& returnStatement, const ForIn& loop);

    void closeNested(Stmt& stmt, const ForIn& loop) {
      stmt.body = closingReturns(move(stmt.body), loop);
      stmt.elseBody = closingReturns(move(stmt.elseBody), loop);

      for (StmtList& arm : stmt.caseBodies) {
        arm = closingReturns(move(arm), loop);
      }
    }

    StmtList closingReturns(StmtList stmts, const ForIn& loop) {
      StmtList out;

      for (StmtPtr& stmt : stmts) {
        if (stmt->kind == StmtKind::Return) {
          out.push_back(closeBefore(*stmt, loop));
        }

        if (stmt->kind == StmtKind::If || stmt->kind == StmtKind::For || stmt->kind == StmtKind::Try || stmt->kind == StmtKind::Select) {
          closeNested(*stmt, loop);
        }

        out.push_back(move(stmt));
      }

      return out;
    }

    // The close of the coroutine that goes before a `return`.
    StmtPtr closeBefore(const Stmt& returnStatement, const ForIn& loop) {
      StmtPtr close = NewStmt(StmtKind::ExprStmt, returnStatement.at);
      close->value = BuiltinNode(Builtin::CoroutineClose, coroutineOf(loop), {}, MakeVoid(), loop.at);

      return close;
    }

    ForIn forInParts(Checker& check, const AstStmt& stmt, TypePtr yields) {
      const String where = to_string(stmt.name.pos.line) + "_" + to_string(stmt.name.pos.column);

      return ForIn{Str(check, stmt.name.name), move(yields), "forGen_" + where, "forStep_" + where, SpanAt(stmt.name.pos)};
    }

    // The loop body with the coroutine and its step in scope (and only there).
    StmtList forInBody(Checker& check, const AstStmt& stmt, const ForIn& loop) {
      const size_t mark = Mark(check.scope);
      BindName(check.scope, SymbolOf(check, loop.coroutine), BindingKind::Local, MakeCoroutine(loop.yields));
      BindName(check.scope, SymbolOf(check, loop.step), BindingKind::Slot, stepTypeOf(loop));
      BindName(check.scope, stmt.name.name, BindingKind::Local, loop.yields);

      StmtList body = closingReturns(CheckBlock(check, stmt.body), loop);
      Reset(check.scope, mark);

      return body;
    }

    StmtList forInLoopBody(Checker& check, const AstStmt& stmt, const ForIn& loop);

    void hoistGenAndStep(Checker& check, const ForIn& loop, ExprPtr source);

    // `for (; step.isCase(0); ) { ... }`; its setup goes before it (hoisted).
    StmtPtr forInLoop(Checker& check, const AstStmt& stmt, const ForIn& parts, ExprPtr source) {
      StmtList body = forInLoopBody(check, stmt, parts);
      StmtPtr loop = NewStmt(StmtKind::For, parts.at);
      loop->value = BuiltinNode(Builtin::VariantIsCase, stepOf(parts), {caseZero(parts)}, IntType(1), parts.at);
      loop->body = move(body);
      hoistGenAndStep(check, parts, move(source));

      return loop;
    }

    // `step = gen.resume()`, the last statement of each turn.
    StmtPtr resumeStep(const ForIn& loop) {
      StmtPtr advance = NewStmt(StmtKind::Assign, loop.at);
      advance->target = stepOf(loop);
      advance->value = resumedOf(loop);

      return advance;
    }

    // `let x: T = step.unwrapAt(0)`, the body, then `step = gen.resume()`.
    StmtList forInLoopBody(Checker& check, const AstStmt& stmt, const ForIn& loop) {
      ExprPtr yielded = BuiltinNode(Builtin::VariantUnwrap, stepOf(loop), {caseZero(loop)}, loop.yields, loop.at);
      StmtList body{letNamed(loop.name, loop.yields, move(yielded), loop.at, StmtKind::Let)};

      for (StmtPtr& each : forInBody(check, stmt, loop)) {
        body.push_back(move(each));
      }

      body.push_back(resumeStep(loop));

      return body;
    }

    // `let gen: coro<T> = source` and `var step: variant<T> = gen.resume()`, before the loop.
    void hoistGenAndStep(Checker& check, const ForIn& loop, ExprPtr source) {
      check.hoisted.push_back(letNamed(loop.coroutine, MakeCoroutine(loop.yields), move(source), loop.at, StmtKind::Let));
      check.hoisted.push_back(letNamed(loop.step, stepTypeOf(loop), resumedOf(loop), loop.at, StmtKind::Var));
    }

    StmtPtr forInStmt(Checker& check, const AstStmt& stmt) {
      ExprPtr source = CheckExpr(check, stmt.value);

      if (source->type->kind != TypeKind::Coroutine) {
        return CheckForInList(check, stmt, move(source));
      }

      const TypePtr yields = source->type->element;

      return forInLoop(check, stmt, forInParts(check, stmt, yields), move(source));
    }

    // ── return / assignment / expression statements ─────────────────────────────────────────────────────────────────────────────────────────────

    // `return` with no value: in a function with no result it returns nothing (a void 0, which HirPrint shows as a bare `return`).
    StmtPtr bareReturn(Span at) {
      StmtPtr made = NewStmt(StmtKind::Return, at);
      made->value = ConstInt(0, VoidType(), at);

      return made;
    }

    StmtPtr returnStmt(Checker& check, const AstStmt& stmt) {
      if (!Valid(stmt.value)) {
        return bareReturn(SpanAt(stmt.pos));
      }

      if (Node(check, stmt.value).kind == AstExprKind::IfValue) {
        return ReturnIfValue(check, Node(check, stmt.value));
      }

      ExprPtr value = CheckExprExpecting(check, stmt.value, check.returnType);
      StmtPtr made = NewStmt(StmtKind::Return, value->at);
      made->value = move(value);

      return made;
    }

    StmtPtr assignment(ExprPtr target, ExprPtr value, Span at) {
      StmtPtr made = NewStmt(StmtKind::Assign, at);
      made->target = move(target);
      made->value = move(value);

      return made;
    }

    // `name = value`. In an assignment target a `var` or global names its STORAGE, not the value read from it. A name bound by `let`, `val` or as a
    // parameter always means the same object; only a `var` can be pointed elsewhere.
    StmtPtr assignTo(Checker& check, const AstStmt& stmt, Ident name) {
      ExprPtr target = NameRef(check, name);
      ExprPtr value = Expecting(target->type, CheckExprExpecting(check, stmt.value, target->type));

      if (target->kind == ExprKind::Local) {
        return UnsupportedStmt("cannot assign to " + target->name + ": a name from let, val or a parameter always means the same object " +
          "(declare it with var to point it at something else)", SpanAt(name.pos));
      }

      return assignment(move(target), move(value), SpanAt(name.pos));
    }

    // `xs[i] = v` / `m[k] = v` only on a collection that can change.
    StmtPtr collectionAssign(ExprPtr target, ExprPtr value) {
      const Span at = target->at;

      if (target->kind == ExprKind::IndexGet) {
        if (const Optional<String> why = AssignProblem(target->operands[0]->type)) {
          return UnsupportedStmt(*why, at);
        }
      }

      return assignment(move(target), move(value), at);
    }

    // `a[i] = v`, `p.x = v`: the target is the element's or field's storage.
    StmtPtr assignElement(Checker& check, const AstStmt& stmt) {
      ExprPtr target = CheckExpr(check, stmt.target);
      ExprPtr value = Expecting(target->type, CheckExprExpecting(check, stmt.value, target->type));

      if (const Optional<String> why = FieldProblem(check, *target)) {
        return UnsupportedStmt(*why, target->at);
      }

      return collectionAssign(move(target), move(value));
    }

    StmtPtr exprStmt(Checker& check, const AstStmt& stmt) {
      const AstExpr& target = Node(check, stmt.target);

      if (stmt.isAssign) {
        return target.kind == AstExprKind::Name ? assignTo(check, stmt, target.name) : assignElement(check, stmt);
      }

      StmtPtr made = NewStmt(StmtKind::ExprStmt, NoSpan());
      made->value = CheckExpr(check, stmt.target);
      made->at = made->value->at;

      return made;
    }

  }  // namespace

  void RegisterCoreStmts(Registry& registry) {
    AddStmt(registry, AstStmtKind::Let, letStmt);
    AddStmt(registry, AstStmtKind::Var, varStmt);

    AddStmt(registry, AstStmtKind::If, ifStmt);
    AddStmt(registry, AstStmtKind::For, forStmt);
    AddStmt(registry, AstStmtKind::Return, returnStmt);
    AddStmt(registry, AstStmtKind::Expr, exprStmt);

    RegisterFlowStmts(registry);
  }

}  // namespace polyir::front
