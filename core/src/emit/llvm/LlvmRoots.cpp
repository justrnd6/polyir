// What a compiled function does for the garbage collector (docs/memory.md, Part 2; the collector is runtime/Collector.c): it keeps its
// references where the collector can find them, and it gives the collector the places where it may run.
//
// THE FRAME RECORD. A function that works with references keeps them in a block on its own stack,
//     %gc.frame = alloca { ptr, i64, [N x ptr] }     ; the caller's record, the count N, and N slots
// which it links into the chain of running functions on entry (gc_push_frame, which also zeroes the slots) and unlinks before every `ret`
// (gc_pop_frame), so the collector can walk every running function's slots from `gc_top`. The slots are
//   - every variable whose value is a reference, and the cell of every captured variable: the variable's own stack slot IS a slot of the
//     record, so storing to the variable keeps the record current at no extra cost;
//   - one slot per expression that makes or reads a reference (KeepRoot): the value is stored there as soon as it is made, so a reference
//     that is only in a register while the rest of an expression runs (`pair(makeA(), makeB())`) is still seen by a collection inside makeB.
// A slot keeps its last value until it is written again or the function returns: at worst an object lives a little longer than it must.
//
// THE WRITE BARRIER. While the collector marks in slices, the program runs between them; every store of a reference into an object
// (HeapStore) first calls gc_write_barrier with the address it writes, so the reference it overwrites is marked before it can disappear.
// Stores into fresh objects (a struct literal's fields, a list literal's elements, a copy) need none: nothing was there to lose.
//
// SAFE POINTS. A safe point is a call of gc_poll, which collects when enough has been allocated. A function that allocates has one at the
// start of its body and one at the head of every loop; allocation itself never collects, so the compiler's own sequences of allocations (a
// list's header, then its elements) never meet a collection halfway. A call of a function, a closure or an interface method may reach a
// safe point inside it, which is why the temporaries above are kept across calls too.
//
// The decision comes last (FinishedLines): the safe points stay when the function allocates, and the record stays when the function has
// references and can reach a safe point at all (one of its own, or a call). Otherwise its slots are plain stack slots and nothing else
// changes. A coroutine is the one exception to "link on entry, unlink on `ret`": its record lives in its frame and is linked each time it
// resumes and unlinked each time it suspends (LlvmCoroutine.cpp); while suspended it is found through the coroutine object.
#include "polyir/Std.hpp"
#include "emit/llvm/LlvmEmitter.hpp"

namespace polyir::llvm {

  namespace {

    // `%x = load ptr, ptr %x.addr` of a variable whose slot is in the record already needs no slot of its own.
    bool readsOwnSlot(const FunctionEmitter& emitter, const Expr& expr) {
      if (expr.kind != ExprKind::Local && expr.kind != ExprKind::LoadSlot) {
        return false;
      }

      const Slot slot = Lookup(emitter, expr.name);

      return !slot.boxed && slot.value.empty();
    }

    // A value is a reference held in a register: not a constant (`@...`, `null`) and of a type whose values are addresses.
    bool inRegister(const Value& value) {
      return value.type != nullptr && !value.text.empty() && value.text[0] == '%' && LayoutOf(value.type) == Layout::References;
    }

    String recordType(const FunctionEmitter& emitter) {
      return "{ ptr, i64, [" + to_string(emitter.roots_.slots.size()) + " x ptr] }";
    }

    String slotCount(const FunctionEmitter& emitter) {
      return to_string(emitter.roots_.slots.size());
    }

    // The line of the record's slot `k`.
    String slotAddress(const FunctionEmitter& emitter, size_t k) {
      return "  " + emitter.roots_.slots[k] + " = getelementptr " + recordType(emitter) + ", ptr %gc.frame, i32 0, i32 2, i64 " + to_string(k);
    }

    Vector<String> plainSlots(const FunctionEmitter& emitter) {
      Vector<String> lines;

      for (const String& slot : emitter.roots_.slots) {
        lines.push_back("  " + slot + " = alloca ptr");
      }

      return lines;
    }

    // The lines the decision drops: the safe points of a function that does not allocate, the record's lines of one that has no record.
    Set<size_t> droppedLines(const FunctionEmitter& emitter) {
      Set<size_t> dropped;

      if (!emitter.roots_.allocates) {
        dropped.insert(emitter.roots_.polls.begin(), emitter.roots_.polls.end());
      }

      if (!HasFrame(emitter)) {
        dropped.insert(emitter.roots_.frameLines.begin(), emitter.roots_.frameLines.end());
      }

      return dropped;
    }

  }  // namespace

  void NoteAllocation(FunctionEmitter& emitter) {
    emitter.roots_.allocates = true;
    NeedCollector(emitter.context_);
  }

  void NoteCall(FunctionEmitter& emitter) {
    emitter.roots_.calls = true;
  }

  // A variable's slot is a slot of the record when the variable holds a reference, or is captured. A captured variable's slot holds the
  // address of its cell.
  bool HoldsRoot(const FunctionEmitter& emitter, const TypePtr& type, bool boxed) {
    return Collects(emitter.context_) && (boxed || LayoutOf(type) == Layout::References);
  }

  String RootSlot(FunctionEmitter& emitter, const String& base) {
    const String slot = "%" + UniqueName(emitter, base);
    emitter.roots_.slots.push_back(slot);

    return slot;
  }

  void KeepRoot(FunctionEmitter& emitter, const Expr& expr, const Value& value) {
    if (!Collects(emitter.context_) || !inRegister(value) || readsOwnSlot(emitter, expr)) {
      return;
    }

    Store(emitter, value, RootSlot(emitter, "root"));
  }

  // A store into an object: when the value is a reference, the write barrier comes first. While it is marking, the collector greys the
  // reference about to be overwritten (docs/memory.md, Part 3). A store into a stack slot, the frame record or a global needs none: the
  // collector looked at those when its cycle began.
  void HeapStore(FunctionEmitter& emitter, const Value& value, const String& pointer) {
    if (Collects(emitter.context_) && LayoutOf(value.type) == Layout::References) {
      NeedCollector(emitter.context_);
      Line(emitter, "call void @gc_write_barrier(ptr " + pointer + ")");
    }

    Store(emitter, value, pointer);
  }

  void SafePoint(FunctionEmitter& emitter) {
    if (!Collects(emitter.context_)) {
      return;
    }

    Line(emitter, "call void @gc_poll()");
    emitter.roots_.polls.push_back(emitter.lines_.size() - 1);
  }

  void FrameLine(FunctionEmitter& emitter, const String& call) {
    Line(emitter, call);
    emitter.roots_.frameLines.push_back(emitter.lines_.size() - 1);
  }

  // A coroutine's `ret` hands back its result block at a suspend, where its record is already unlinked.
  void LeaveFrame(FunctionEmitter& emitter) {
    if (Collects(emitter.context_) && !emitter.function_.isCoroutine) {
      FrameLine(emitter, "call void @gc_pop_frame(ptr %gc.frame)");
    }
  }

  // The coroutine's frame goes along. When it resumes while the collector is marking, the collector looks at the whole frame first (the
  // coroutine writes it without barriers).
  void LinkCoroutineFrame(FunctionEmitter& emitter) {
    if (!Collects(emitter.context_)) {
      return;
    }

    FrameLine(emitter, "call void @gc_link_frame(ptr %gc.frame, ptr " + emitter.coroutine_.handle + ")");
    emitter.roots_.linked = true;
  }

  // The first suspend comes before the record was ever linked.
  void UnlinkCoroutineFrame(FunctionEmitter& emitter) {
    if (Collects(emitter.context_) && emitter.roots_.linked) {
      FrameLine(emitter, "call void @gc_unlink_frame(ptr %gc.frame)");
    }
  }

  bool HasFrame(const FunctionEmitter& emitter) {
    return Collects(emitter.context_) && !emitter.roots_.slots.empty() && (emitter.roots_.allocates || emitter.roots_.calls);
  }

  Vector<String> FrameAllocas(const FunctionEmitter& emitter) {
    if (!HasFrame(emitter)) {
      return plainSlots(emitter);
    }

    Vector<String> lines = {"  %gc.frame = alloca " + recordType(emitter)};

    for (size_t k = 0; k < emitter.roots_.slots.size(); k++) {
      lines.push_back(slotAddress(emitter, k));
    }

    return lines;
  }

  // An ordinary function links its record before its first instruction. A coroutine only fills it in, with a plain store, and links it
  // when it resumes. Before `llvm.coro.begin` the record is still on the stack, and LLVM moves what was stored into the coroutine's frame.
  Vector<String> FrameEntry(const FunctionEmitter& emitter) {
    if (!HasFrame(emitter)) {
      return {};
    }

    if (!emitter.function_.isCoroutine) {
      return {"  call void @gc_push_frame(ptr %gc.frame, i64 " + slotCount(emitter) + ")"};
    }

    const String empty = "{ ptr null, i64 " + slotCount(emitter) + ", [" + slotCount(emitter) + " x ptr] zeroinitializer }";

    return {"  store " + recordType(emitter) + " " + empty + ", ptr %gc.frame"};
  }

  // The entry block's start: the record and its slots, the function's other stack slots, then the record linked.
  Vector<String> EntryLines(const FunctionEmitter& emitter) {
    Vector<String> lines = FrameAllocas(emitter);
    lines.insert(lines.end(), emitter.allocas_.begin(), emitter.allocas_.end());
    const Vector<String> entry = FrameEntry(emitter);
    lines.insert(lines.end(), entry.begin(), entry.end());

    return lines;
  }

  Vector<String> FinishedLines(const FunctionEmitter& emitter) {
    const Set<size_t> dropped = droppedLines(emitter);
    Vector<String> lines;

    for (size_t i = 0; i < emitter.lines_.size(); i++) {
      if (dropped.count(i) == 0) {
        lines.push_back(emitter.lines_[i]);
      }
    }

    return lines;
  }

  // ── the module: the globals that hold references, and starting the collector ──

  namespace {

    bool holdsReference(const GlobalDef& global) {
      return LayoutOf(global.type) == Layout::References;
    }

    String globalAddresses(const Module& module) {
      String out;

      for (const GlobalDef& global : module.globals) {
        if (holdsReference(global)) {
          out += String(out.empty() ? "" : ", ") + "ptr @" + QuotedName(global.name);
        }
      }

      return out;
    }

    // IR added to the module that calls the collector's allocation functions (ninac's text runtime: its strings are objects too).
    bool allocatesThroughCollector(const RawIr& raw) {
      return raw.text.find("@gc_allocate") != String::npos;
    }

    // Whether anything in the module allocates: its own functions, or IR added to it that calls the collector.
    bool usesCollector(const Module& module, const ModuleContext& context) {
      if (context.used_.count("collector") != 0) {
        return true;
      }

      return any_of(module.rawIr.begin(), module.rawIr.end(), allocatesThroughCollector);
    }

    // The same IR with the collector's own definitions in it (a fragment compiled by this compiler) brings them along.
    bool definesCollector(const RawIr& raw) {
      return raw.text.find("ptr @gc_allocate(i64 %") != String::npos;
    }

  }  // namespace

  int64_t GlobalRootCount(const Module& module) {
    return count_if(module.globals.begin(), module.globals.end(), holdsReference);
  }

  String GlobalRoots(const Module& module) {
    const String count = to_string(GlobalRootCount(module));
    const String addresses = globalAddresses(module);

    return "@gc_globals = internal constant [" + count + " x ptr] " + (addresses.empty() ? "zeroinitializer" : "[" + addresses + "]");
  }

  // Added IR that allocates needs gc_allocate defined in every module. That includes a library's (where it is malloc until a `main` starts
  // the collector) and a module built without the collector (the plain allocator).
  void NeedCollectorFor(const RawIr& raw, ModuleContext& context) {
    if (allocatesThroughCollector(raw) && !definesCollector(raw)) {
      NeedCollector(context);
    }
  }

  // The program's entry point starts the collector before anything else runs, when anything in the module allocates.
  String CollectorStart(const Module& module, ModuleContext& context) {
    if (!Collects(context) || !usesCollector(module, context)) {
      return "";
    }

    NeedCollector(context);
    Need(context, GlobalRoots(module));

    return "  call void @gc_start(ptr @gc_globals, i64 " + to_string(GlobalRootCount(module)) + ")\n";
  }

}  // namespace polyir::llvm
