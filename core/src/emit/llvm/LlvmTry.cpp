// try, catch and throw, without any unwinding tables: a throw stores its number in a global and sets a kFlag, and every call to a function of the
// program is followed by a look at that flag. When it is set the caller goes to the nearest enclosing `catch` (which clears it and reads the kNumber),
// or returns from its own function with a zero result so that its caller looks too. A program with no `throw` pays nothing for this.
#include "polyir/Std.hpp"
#include "emit/llvm/LlvmEmitter.hpp"

namespace polyir::llvm {

  namespace {

    const char* const kFlag = "@thrown";
    const char* const kNumber = "@thrown_value";

  }  // namespace

  void PassOnThrownIfAny(FunctionEmitter& emitter) {
    if (Throws(emitter.context_)) {
      PassOnThrown(emitter);
    }
  }

  // Where control goes with the flag set: the catch of the innermost `try`, or out of the function.
  void PassOnThrown(FunctionEmitter& emitter) {
    const String set = Instruction(emitter, String("load i1, ptr ") + kFlag);
    const String handler = UniqueName(emitter, "thrown");
    const String carryOn = UniqueName(emitter, "normal");
    Terminate(emitter, "br i1 " + set + ", label %" + handler + ", label %" + carryOn);

    StartBlock(emitter, handler);
    GoToHandler(emitter);
    StartBlock(emitter, carryOn);
  }

  // To the catch of the innermost `try`, or else out of the function with a zero result.
  void GoToHandler(FunctionEmitter& emitter) {
    if (!emitter.handlers_.empty()) {
      BranchTo(emitter, emitter.handlers_.back());
    } else {
      Terminate(emitter, IsVoid(*emitter.function_.result) ? String("ret void") : "ret " + LlvmTypeText(emitter.function_.result) + " " + ZeroText(emitter.function_.result));
    }
  }

  void ThrowStatement(FunctionEmitter& emitter, const Stmt& statement) {
    const Value thrown = Resized(emitter, EmitExpr(emitter, *statement.value), 64, IsSigned(*statement.value->type));
    Store(emitter, thrown, kNumber);
    Store(emitter, Value{"true", MakeBool()}, kFlag);
    GoToHandler(emitter);
  }

  void TryStatement(FunctionEmitter& emitter, const Stmt& statement) {
    const String handler = UniqueName(emitter, "catch");
    const String end = UniqueName(emitter, "try.end");

    TryBody(emitter, statement, handler);
    JumpUnlessTerminated(emitter, end);

    CatchBody(emitter, statement, handler);
    JumpUnlessTerminated(emitter, end);
    StartBlock(emitter, end);
  }

  void JumpUnlessTerminated(FunctionEmitter& emitter, const String& block) {
    if (!emitter.terminated_) {
      BranchTo(emitter, block);
    }
  }

  // The body of a `try`, with a throw inside it going to `handler`.
  void TryBody(FunctionEmitter& emitter, const Stmt& statement, const String& handler) {
    emitter.handlers_.push_back(handler);
    emitter.scopes_.emplace_back();
    EmitBlock(emitter, statement.body);
    emitter.scopes_.pop_back();
    emitter.handlers_.pop_back();
  }

  // The `catch`: clears the flag and binds the thrown number. A cleanup (no name) puts the throw back when it is done and carries on out.
  void CatchBody(FunctionEmitter& emitter, const Stmt& statement, const String& handler) {
    StartBlock(emitter, handler);
    Store(emitter, Value{"false", MakeBool()}, kFlag);
    const Value thrown = LoadFrom(emitter, kNumber, MakeInt(64));
    CatchStatements(emitter, statement, thrown);

    if (statement.name.empty() && !emitter.terminated_) {
      ThrowAgain(emitter, thrown);
    }
  }

  void CatchStatements(FunctionEmitter& emitter, const Stmt& statement, const Value& thrown) {
    emitter.scopes_.emplace_back();
    BindVariable(emitter, statement.name.empty() ? String("cleanup.thrown") : statement.name, thrown);
    EmitBlock(emitter, statement.elseBody);
    emitter.scopes_.pop_back();
  }

  void ThrowAgain(FunctionEmitter& emitter, const Value& thrown) {
    Store(emitter, thrown, kNumber);
    Store(emitter, Value{"true", MakeBool()}, kFlag);
    GoToHandler(emitter);
  }

}  // namespace polyir::llvm
