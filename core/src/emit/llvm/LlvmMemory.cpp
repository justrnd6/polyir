// The small pieces every kind of value shares: heap blocks made of 8-byte cells (from the garbage collector), sizes, a trap, and which names closures
// capture.
#include "polyir/Std.hpp"
#include "emit/llvm/LlvmEmitter.hpp"

#include <functional>

namespace polyir::llvm {

  namespace {

    // Visits every statement and expression below a list of statements, including the bodies of closures.
    struct Walker {
      const Function<void(const Stmt&)>& onStmt_;
      const Function<void(const Expr&)>& onExpr_;
    };

    void walkStatements(const Walker& walker, const StmtList& body);

    void walkExpressions(const Walker& walker, const Vector<ExprPtr>& exprs);

    void walkExpression(const Walker& walker, const ExprPtr& expr);

    void walkStatement(const Walker& walker, const Stmt& statement);

    void walkStatementExpressions(const Walker& walker, const Stmt& statement);

    void walkCaseExpressions(const Walker& walker, const Stmt& statement);

    void walkStatements(const Walker& walker, const StmtList& body) {
      for (const StmtPtr& child : body) {
        walkStatement(walker, *child);
      }
    }

    void walkExpressions(const Walker& walker, const Vector<ExprPtr>& exprs) {
      for (const ExprPtr& expr : exprs) {
        walkExpression(walker, expr);
      }
    }

    void walkExpression(const Walker& walker, const ExprPtr& expr) {
      if (!expr) {
        return;
      }

      walker.onExpr_(*expr);
      walkExpressions(walker, expr->operands);
      walkStatements(walker, expr->closureBody);
    }

    void walkStatement(const Walker& walker, const Stmt& statement) {
      walker.onStmt_(statement);
      walkStatementExpressions(walker, statement);
      walkStatements(walker, statement.body);
      walkStatements(walker, statement.elseBody);
    }

    void walkStatementExpressions(const Walker& walker, const Stmt& statement) {
      walkExpression(walker, statement.value);
      walkExpression(walker, statement.target);

      walkExpressions(walker, statement.initializers);
      walkExpressions(walker, statement.steps);
      walkExpressions(walker, statement.arguments);
      walkExpressions(walker, statement.elseArguments);
      walkCaseExpressions(walker, statement);
    }

    void walkCaseExpressions(const Walker& walker, const Stmt& statement) {
      for (const auto& args : statement.caseArguments) {
        walkExpressions(walker, args);
      }
    }

  }  // namespace

  Set<String> CapturedNames(const StmtList& body) {
    Set<String> names;
    const auto onExpr = [&](const Expr& expr) { names.insert(expr.captures.begin(), expr.captures.end()); };
    walkStatements(Walker{[](const Stmt&) {}, onExpr}, body);

    return names;
  }

  // An assignment (or a `for` counter, which its steps assign) names a variable.
  void NoteAssigned(const Stmt& statement, Set<String>& names) {
    if (statement.kind == StmtKind::Assign && statement.target && (statement.target->kind == ExprKind::Local || statement.target->kind == ExprKind::LoadSlot)) {
      names.insert(statement.target->name);
    }

    names.insert(statement.names.begin(), statement.names.end());
  }

  Set<String> AssignedNames(const StmtList& body) {
    Set<String> names;
    walkStatements(Walker{[&](const Stmt& statement) { NoteAssigned(statement, names); }, [](const Expr&) {}}, body);

    return names;
  }

  // Whether any function of the module throws or catches (then every call to one of them is followed by a look at the flag).
  bool ThrowsAnywhere(const Module& module) {
    bool found = false;
    const auto onStmt = [&](const Stmt& statement) { found = found || statement.kind == StmtKind::Throw || statement.kind == StmtKind::Try; };

    for (const FuncDef& definition : module.functions) {
      walkStatements(Walker{onStmt, [](const Expr&) {}}, definition.body);
    }

    return found;
  }

  String WordAddress(FunctionEmitter& emitter, const String& base, int64_t word) {
    return Instruction(emitter, "getelementptr i64, ptr " + base + ", i64 " + to_string(word));
  }

  String WordAddress(FunctionEmitter& emitter, const String& base, const String& wordIndex) {
    return Instruction(emitter, "getelementptr i64, ptr " + base + ", i64 " + wordIndex);
  }

  // Every object the compiled code makes comes from the collector's gc_allocate (runtime/Collector.c), which cuts it from its own pages. The
  // layout says whether the collector looks inside it for references.
  Value Allocate(FunctionEmitter& emitter, const String& bytes, Layout layout) {
    NoteAllocation(emitter);

    return Value{Instruction(emitter, "call ptr @gc_allocate(i64 " + bytes + ", " + LayoutOperand(layout) + ")"), MakePointer(MakeInt(8))};
  }

  Value AllocateZeroed(FunctionEmitter& emitter, const String& count, const String& elementBytes, Layout layout) {
    NoteAllocation(emitter);
    const String call = "call ptr @gc_allocate_zeroed(i64 " + count + ", i64 " + elementBytes + ", " + LayoutOperand(layout) + ")";

    return Value{Instruction(emitter, call), MakePointer(MakeInt(8))};
  }

  // `Free` of an object is a hint: the collector reclaims it once nothing reaches it, never earlier (without the collector it is a free()).
  void FreeHint(FunctionEmitter& emitter, const String& block) {
    NeedCollector(emitter.context_);
    Line(emitter, "call void @gc_free_hint(ptr " + block + ")");
  }

  // The size of a value of the type: `getelementptr` one past a null pointer.
  String SizeOf(FunctionEmitter& emitter, const TypePtr& type) {
    const String end = Instruction(emitter, "getelementptr " + LlvmTypeText(type) + ", ptr null, i32 1");

    return Instruction(emitter, "ptrtoint ptr " + end + " to i64");
  }

  // Stops the program (the way an out-of-range index or an empty optional read does) unless the flag holds.
  void TrapUnless(FunctionEmitter& emitter, const String& condition, const String& reason) {
    const String good = UniqueName(emitter, "ok");
    const String bad = UniqueName(emitter, "trap");
    Terminate(emitter, "br i1 " + condition + ", label %" + good + ", label %" + bad);

    StartBlock(emitter, bad);
    EmitTrap(emitter, reason);
    StartBlock(emitter, good);
  }

  // What was printed so far is flushed, and the reason goes to standard error before the program stops. So a crash does not lose its output.
  void EmitTrap(FunctionEmitter& emitter, const String& reason) {
    const String message = reason + "\n";
    DeclareTrapFunctions(emitter.context_);
    Instruction(emitter, "call i32 @fflush(ptr null)");
    Instruction(emitter, "call i64 @write(i32 2, ptr " + StringGlobal(emitter.context_, message) + ", i64 " + to_string(message.size()) + ")");

    Line(emitter, "call void @llvm.trap()");
    Terminate(emitter, "unreachable");
  }

  void DeclareTrapFunctions(ModuleContext& context) {
    Need(context, "declare void @llvm.trap()");
    Need(context, "declare i32 @fflush(ptr)");
    Need(context, "declare i64 @write(i32, ptr, i64)");
  }

}  // namespace polyir::llvm
