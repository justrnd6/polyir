// `if (c) a else b` as a value (CheckIfValue.nina), worked out just before the statement that holds it, into a hidden
// variable that the expression then reads:
//
//   var ifValue_L_C: T
//   if (c) { ifValue_L_C = a } else { ifValue_L_C = b }
//
// Only the branch taken is worked out: what a branch needs before it (a nested `if` value, say) goes inside that branch.
// Both branches have one type; where a type is expected (a typed `let`, an argument) each is checked as it. `return if (c) a
// else b` needs no variable: it is the `if`, each branch returning its value (and an `else if` chain an `if` in the else).
#include "front/Checker.hpp"
#include "polyir/HirPrint.hpp"

namespace polyir::front {

  namespace {

    // A branch's value, with what checking it hoisted put in front of it in `body` instead of before the whole statement.
    ExprPtr branchValue(Checker& check, ExprId id, const TypePtr* expected, StmtList& body) {
      StmtList outer = move(check.hoisted);
      check.hoisted.clear();
      ExprPtr value = expected != nullptr ? CheckExprExpecting(check, id, *expected) : CheckExpr(check, id);
      body = move(check.hoisted);
      check.hoisted = move(outer);

      return value;
    }

    // `name = value`, the last statement of a branch.
    StmtPtr assigned(const String& name, const TypePtr& type, ExprPtr value) {
      StmtPtr made = NewStmt(StmtKind::Assign, value->at);
      made->target = NamedRef(ExprKind::LoadSlot, name, type, value->at);
      made->value = move(value);

      return made;
    }

    // The type the value has: the expected one, else the first branch's, which the second must share.
    Optional<String> branchProblem(const ExprPtr& then, const ExprPtr& otherwise, const TypePtr* expected) {
      if (expected == nullptr && TypeText(then->type) != TypeText(otherwise->type)) {
        return "both branches of an `if` value have one type: " + TypeText(then->type) + " and " + TypeText(otherwise->type);
      }

      const TypePtr& type = expected != nullptr ? *expected : then->type;

      return type->kind == TypeKind::Void ? Optional<String>("an `if` value needs a value in each branch") : nullopt;
    }

    // `var ifValue_L_C: T` and the `if` that sets it go before the statement; the value is the variable.
    ExprPtr hoistedValue(Checker& check, const AstExpr& node, ExprPtr condition, StmtList thenBody, StmtList elseBody, const TypePtr& type) {
      const String name = "ifValue_" + to_string(node.pos.line) + "_" + to_string(node.pos.column);
      StmtPtr slot = NewStmt(StmtKind::Var, condition->at);
      slot->name = name;
      slot->type = type;

      StmtPtr choice = NewStmt(StmtKind::If, condition->at);
      choice->value = move(condition);
      choice->body = move(thenBody);
      choice->elseBody = move(elseBody);

      check.hoisted.push_back(move(slot));
      check.hoisted.push_back(move(choice));

      return NamedRef(ExprKind::LoadSlot, name, type, SpanAt(node.pos));
    }

    ExprPtr ifValue(Checker& check, const AstExpr& node, const TypePtr* expected) {
      ExprPtr condition = CheckExpr(check, node.first);
      StmtList thenBody;
      StmtList elseBody;
      ExprPtr then = branchValue(check, node.second, expected, thenBody);
      ExprPtr otherwise = branchValue(check, node.third, expected, elseBody);

      if (const Optional<String> why = branchProblem(then, otherwise, expected)) {
        return Unsupported(*why, SpanAt(node.pos));
      }

      const TypePtr type = expected != nullptr ? *expected : then->type;
      const String name = "ifValue_" + to_string(node.pos.line) + "_" + to_string(node.pos.column);
      thenBody.push_back(assigned(name, type, Expecting(type, move(then))));
      elseBody.push_back(assigned(name, type, Expecting(type, move(otherwise))));

      return hoistedValue(check, node, move(condition), move(thenBody), move(elseBody), type);
    }

    ExprPtr ifValueAsWritten(Checker& check, const AstExpr& node) {
      return ifValue(check, node, nullptr);
    }

  }  // namespace

  ExprPtr IfValueExpecting(Checker& check, const AstExpr& node, const TypePtr& expected) {
    return ifValue(check, node, &expected);
  }

  namespace {

    // One branch of `return if ...`: another `if` value is an `if` of its own; any other value is returned as the function's type.
    StmtList returnedBranch(Checker& check, ExprId id) {
      if (Node(check, id).kind == AstExprKind::IfValue) {
        return StmtList{ReturnIfValue(check, Node(check, id))};
      }

      StmtList body;
      ExprPtr value = branchValue(check, id, &check.returnType, body);
      StmtPtr made = NewStmt(StmtKind::Return, value->at);
      made->value = move(value);
      body.push_back(move(made));

      return body;
    }

  }  // namespace

  StmtPtr ReturnIfValue(Checker& check, const AstExpr& node) {
    ExprPtr condition = CheckExpr(check, node.first);
    StmtPtr choice = NewStmt(StmtKind::If, condition->at);
    choice->value = move(condition);
    choice->body = returnedBranch(check, node.second);
    choice->elseBody = returnedBranch(check, node.third);

    return choice;
  }

  void RegisterIfValues(Registry& registry) {
    AddExpr(registry, AstExprKind::IfValue, ifValueAsWritten);
  }

}  // namespace polyir::front
