// Coroutines, on LLVM's own coroutine intrinsics.
//
//   a coroutine function   one function marked `presplitcoroutine`; clang's pipeline splits it into the part that runs at the call and the parts that
//                          run at each resume. It returns a heap block of four cells: the coroutine's handle, its yield slot, whether it was
//                          abandoned and what it is suspended at (0 nothing yet, 1 a plain yield, 2 a CoroutineYield).
//   the start              the frame is allocated, then the coroutine suspends at once, so nothing of the body runs before the first `resume`
//   `yield v`              stores `v` in the slot and suspends
//   falling off the end    the final suspend; `done` is then true (so does a `return` anywhere in the body)
//   `CoroutineYield(v)`    a `yield v` that also gives an i1: whether the coroutine was abandoned (closed) while it was suspended there
//   `CoroutineClose(co)`   abandons a coroutine that is suspended at a `CoroutineYield`: it sets the block's abandoned cell and resumes it once; one suspended
//                          at a plain `yield` (or never resumed) is just dropped, none of its code runs again
//   `resume`               runs the coroutine to its next suspend (a finished one is left alone) and gives a variant: case 0 holds the yielded value,
//                          case 1 means it has finished
#include "polyir/Std.hpp"
#include "emit/llvm/LlvmEmitter.hpp"

namespace polyir::llvm {

  namespace {

    // Resumes a coroutine unless it has finished (resuming a finished one is undefined).
    const char* const kStep = R"(define void @coro_step(ptr %handle) {
entry:
  %done = call i1 @llvm.coro.done(ptr %handle)
  br i1 %done, label %finished, label %resume
resume:
  call void @llvm.coro.resume(ptr %handle)
  br label %finished
finished:
  ret void
})";

    // Abandons a coroutine that is suspended at a `CoroutineYield` (word 3 of the block is 2): it resumes it once with the abandoned cell (word 2) set.
    const char* const kClose = R"(define void @coro_close(ptr %block) {
entry:
  %handle = load ptr, ptr %block
  %state.cell = getelementptr i64, ptr %block, i64 3
  %state = load i64, ptr %state.cell
  %live = icmp eq i64 %state, 2
  br i1 %live, label %check, label %out
check:
  %done = call i1 @llvm.coro.done(ptr %handle)
  br i1 %done, label %out, label %close
close:
  %flag.cell = getelementptr i64, ptr %block, i64 2
  store i64 1, ptr %flag.cell
  call void @llvm.coro.resume(ptr %handle)
  br label %out
out:
  ret void
})";

    const char* const kDeclarations[] = {"declare token @llvm.coro.id(i32, ptr, ptr, ptr)", "declare i64 @llvm.coro.size.i64()",
        "declare ptr @llvm.coro.begin(token, ptr)", "declare i8 @llvm.coro.suspend(token, i1)", "declare ptr @llvm.coro.free(token, ptr)",
        "declare i1 @llvm.coro.end(ptr, i1, token)", "declare i1 @llvm.coro.done(ptr)", "declare void @llvm.coro.resume(ptr)"};

    Value flag(const String& text) {
      return Value{text, MakeBool()};
    }

    // The four 8-byte cells of a coroutine's result block (the table above), and what its state cell says it is suspended at.
    const int64_t kHandleCell = 0;
    const int64_t kYieldSlotCell = 1;
    const int64_t kAbandonedCell = 2;
    const int64_t kStateCell = 3;
    const int64_t kNotSuspendedYet = 0;
    const int64_t kAtPlainYield = 1;
    const int64_t kAtCoroutineYield = 2;

  }  // namespace

  String Named(FunctionEmitter& emitter, const String& base, const String& text) {
    const String name = "%" + UniqueName(emitter, base);
    Line(emitter, name + " = " + text);

    return name;
  }

  String FunctionAttributes(const FunctionEmitter& emitter) {
    return emitter.function_.isCoroutine ? " presplitcoroutine" : "";
  }

  // A coroutine starts with the frame, its result block and the first suspend.
  void OpenCoroutine(FunctionEmitter& emitter) {
    if (!emitter.function_.isCoroutine) {
      return;
    }

    for (const char* const declaration : kDeclarations) {
      Need(emitter.context_, declaration);
    }

    emitter.coroutine_.cleanup = UniqueName(emitter, "coro.cleanup");
    emitter.coroutine_.suspend = UniqueName(emitter, "coro.suspend");
    CoroutineRamp(emitter);
    SuspendAt(emitter, false, UniqueName(emitter, "coro.start"));
  }

  // The frame is an object of the collector like any other. It holds what the body keeps across a suspend (its frame record among it). The
  // collector reaches it through the coroutine's result block. The record is linked only once the body runs (SuspendAt).
  void CoroutineFrame(FunctionEmitter& emitter) {
    emitter.coroutine_.id = Named(emitter, "id", "call token @llvm.coro.id(i32 0, ptr null, ptr null, ptr null)");
    const String size = Named(emitter, "size", "call i64 @llvm.coro.size.i64()");
    NoteAllocation(emitter);
    const String frame = Named(emitter, "frame", "call ptr @gc_allocate(i64 " + size + ", " + LayoutOperand(Layout::References) + ")");
    emitter.coroutine_.handle = Named(emitter, "hdl", "call ptr @llvm.coro.begin(token " + emitter.coroutine_.id + ", ptr " + frame + ")");
  }

  // The yield slot, and the block of the handle and the slot that the caller gets back.
  void CoroutineRamp(FunctionEmitter& emitter) {
    CoroutineFrame(emitter);
    emitter.coroutine_.slot = YieldSlot(emitter);
    emitter.coroutine_.block = Allocate(emitter, "32", Layout::References).text;  // four cells

    Store(emitter, Value{emitter.coroutine_.handle, MakePointer(MakeInt(8))}, WordAddress(emitter, emitter.coroutine_.block, kHandleCell));
    Store(emitter, Value{emitter.coroutine_.slot, MakePointer(MakeInt(8))}, WordAddress(emitter, emitter.coroutine_.block, kYieldSlotCell));
    SetCoroutineCell(emitter, emitter.coroutine_.block, kAbandonedCell, 0);
    SetCoroutineCell(emitter, emitter.coroutine_.block, kStateCell, kNotSuspendedYet);
  }

  // One value of the yielded type, zeroed; the collector looks inside when the values are references.
  String YieldSlot(FunctionEmitter& emitter) {
    const TypePtr& yielded = emitter.function_.result->element;

    return AllocateZeroed(emitter, "1", SizeOf(emitter, yielded), LayoutOf(yielded)).text;
  }

  // Suspends: continuing goes on at `next`, being destroyed goes to the cleanup, and the caller gets the result block back through `coro.suspend`.
  // The frame record is out of the chain while the coroutine is suspended, and back in once it runs again.
  void SuspendAt(FunctionEmitter& emitter, bool last, const String& next) {
    UnlinkCoroutineFrame(emitter);
    const String code = Named(emitter, "sp", String("call i8 @llvm.coro.suspend(token none, i1 ") + (last ? "true" : "false") + ")");
    Terminate(emitter, "switch i8 " + code + ", label %" + emitter.coroutine_.suspend + " [i8 0, label %" + next + " i8 1, label %" + emitter.coroutine_.cleanup + "]");
    StartBlock(emitter, next);
    LinkCoroutineFrame(emitter);
  }

  void YieldStatement(FunctionEmitter& emitter, const Stmt& statement) {
    if (!emitter.function_.isCoroutine) {
      Fail("yield belongs in a coroutine, and " + emitter.function_.name + " is not one");
    }

    HeapStore(emitter, Fit(emitter, EmitExpr(emitter, *statement.value), emitter.function_.result->element), emitter.coroutine_.slot);
    SetCoroutineCell(emitter, emitter.coroutine_.block, kStateCell, kAtPlainYield);
    SuspendAt(emitter, false, UniqueName(emitter, "coro.resume"));
  }

  // A yield that says whether it was abandoned: the abandoned cell of the block is read once the coroutine runs again.
  Value CoroutineYielded(FunctionEmitter& emitter, const Expr& expr) {
    if (!emitter.function_.isCoroutine) {
      Fail("a yield belongs in a coroutine, and " + emitter.function_.name + " is not one");
    }

    HeapStore(emitter, Fit(emitter, EmitExpr(emitter, *expr.operands[0]), emitter.function_.result->element), emitter.coroutine_.slot);
    SetCoroutineCell(emitter, emitter.coroutine_.block, kStateCell, kAtCoroutineYield);
    SuspendAt(emitter, false, UniqueName(emitter, "coro.resume"));
    // Volatile: the block is memory the optimizer believes nothing else writes while the coroutine is suspended, but `coro_close` does.
    const String cell = Instruction(emitter, "load volatile i64, ptr " + WordAddress(emitter, emitter.coroutine_.block, kAbandonedCell));

    return flag(Instruction(emitter, "icmp ne i64 " + cell + ", 0"));
  }

  // The end of a function: a coroutine finishes with its last suspend and the shared exit; any other function that falls off its end returns.
  void FinishBody(FunctionEmitter& emitter) {
    if (emitter.function_.isCoroutine) {
      EndCoroutine(emitter);
    } else if (!emitter.terminated_) {
      Terminate(emitter, IsVoid(*emitter.function_.result) ? "ret void" : "unreachable");
    }
  }

  // `return` in a coroutine ends it (value-less: what it yields went out through `yield`). It jumps to the final suspend that the end of the
  // body has.
  void LeaveCoroutine(FunctionEmitter& emitter, const Stmt& statement) {
    if (statement.value) {
      EmitExpr(emitter, *statement.value);
    }

    if (emitter.coroutine_.finish.empty()) {
      emitter.coroutine_.finish = UniqueName(emitter, "coro.finish");
    }

    BranchTo(emitter, emitter.coroutine_.finish);
  }

  void EndCoroutine(FunctionEmitter& emitter) {
    JoinFinalSuspend(emitter);

    if (!emitter.terminated_) {
      SuspendAt(emitter, true, UniqueName(emitter, "coro.final"));
      Terminate(emitter, "unreachable");
    }

    CoroutineExit(emitter);
  }

  // LLVM allows one final suspend: every `return` joins the end of the body here.
  void JoinFinalSuspend(FunctionEmitter& emitter) {
    if (emitter.coroutine_.finish.empty()) {
      return;
    }

    if (!emitter.terminated_) {
      BranchTo(emitter, emitter.coroutine_.finish);
    }

    StartBlock(emitter, emitter.coroutine_.finish);
  }

  // The cleanup gives back the frame and the slot (hints to the collector). Both ways out of the coroutine then end at `coro.suspend`, which
  // hands the caller its result block.
  void CoroutineExit(FunctionEmitter& emitter) {
    StartBlock(emitter, emitter.coroutine_.cleanup);
    const String memory = Named(emitter, "mem", "call ptr @llvm.coro.free(token " + emitter.coroutine_.id + ", ptr " + emitter.coroutine_.handle + ")");
    FreeHint(emitter, memory);
    FreeHint(emitter, emitter.coroutine_.slot);

    BranchTo(emitter, emitter.coroutine_.suspend);
    ReturnCoroutine(emitter);
  }

  void ReturnCoroutine(FunctionEmitter& emitter) {
    StartBlock(emitter, emitter.coroutine_.suspend);
    Named(emitter, "end", "call i1 @llvm.coro.end(ptr " + emitter.coroutine_.handle + ", i1 false, token none)");
    Terminate(emitter, "ret ptr " + emitter.coroutine_.block);
  }

  // ── resume and done ──────────────────────────────────────────────────────────────────────────────────────────────────

  bool CoroutineBuiltin(FunctionEmitter& emitter, const Expr& expr, Value& out) {
    const Builtin op = expr.builtin;

    if (op == Builtin::CoroutineYield) {
      out = CoroutineYielded(emitter, expr);
    } else if (op == Builtin::CoroutineClose) {
      out = CoroutineClose(emitter, EmitExpr(emitter, *expr.operands[0]));
    } else if (op == Builtin::CoroutineDone) {
      out = CoroutineDone(emitter, EmitExpr(emitter, *expr.operands[0]));
    } else if (op == Builtin::CoroutineResume) {
      out = CoroutineResume(emitter, EmitExpr(emitter, *expr.operands[0]));
    } else {
      return false;
    }

    return true;
  }

  // Sets the cell `word` of a coroutine's result block to a whole number (volatile: the caller reads these cells while the body is suspended).
  void SetCoroutineCell(FunctionEmitter& emitter, const String& block, int64_t word, int64_t value) {
    Line(emitter, "store volatile i64 " + to_string(value) + ", ptr " + WordAddress(emitter, block, word));
  }

  Value CoroutineClose(FunctionEmitter& emitter, const Value& coroutine) {
    Need(emitter.context_, kClose);
    Need(emitter.context_, "declare i1 @llvm.coro.done(ptr)");
    Need(emitter.context_, "declare void @llvm.coro.resume(ptr)");
    NoteCall(emitter);
    Line(emitter, "call void @coro_close(ptr " + coroutine.text + ")");

    return Value{"", MakeVoid()};
  }

  // The pointer held in the cell `word` of the coroutine's result block: its handle, or its yield slot.
  String LoadCoroutinePointer(FunctionEmitter& emitter, const Value& coroutine, int64_t word) {
    return LoadFrom(emitter, WordAddress(emitter, coroutine.text, word), MakePointer(MakeInt(8))).text;
  }

  Value CoroutineDone(FunctionEmitter& emitter, const Value& coroutine) {
    Need(emitter.context_, "declare i1 @llvm.coro.done(ptr)");

    return flag(Instruction(emitter, "call i1 @llvm.coro.done(ptr " + LoadCoroutinePointer(emitter, coroutine, kHandleCell) + ")"));
  }

  // Resumes, then builds the variant. The tag is 1 when the coroutine has finished, and the value in the slot is the payload of case 0.
  Value CoroutineResume(FunctionEmitter& emitter, const Value& coroutine) {
    Need(emitter.context_, kStep);
    Need(emitter.context_, "declare i1 @llvm.coro.done(ptr)");
    Need(emitter.context_, "declare void @llvm.coro.resume(ptr)");

    NoteCall(emitter);
    Line(emitter, "call void @coro_step(ptr " + LoadCoroutinePointer(emitter, coroutine, kHandleCell) + ")");
    const String finished = CoroutineDone(emitter, coroutine).text;

    return CoroutineVariant(emitter, coroutine, finished);
  }

  // The variant of one step: the tag is 1 when the coroutine has finished, and the slot's value is the payload of case 0. The checker types the call
  // as the yielded type, but what it gives is this variant.
  Value CoroutineVariant(FunctionEmitter& emitter, const Value& coroutine, const String& finished) {
    const Value yielded = LoadFrom(emitter, LoadCoroutinePointer(emitter, coroutine, kYieldSlotCell), coroutine.type->element);
    const Value block = Allocate(emitter, "16", Layout::References);  // two cells: the tag, then the payload
    Store(emitter, Value{Instruction(emitter, "zext i1 " + finished + " to i64"), MakeInt(64)}, WordAddress(emitter, block.text, 0));
    Store(emitter, yielded, WordAddress(emitter, block.text, 1));

    return Value{block.text, MakeVariant({coroutine.type->element})};
  }

}  // namespace polyir::llvm
