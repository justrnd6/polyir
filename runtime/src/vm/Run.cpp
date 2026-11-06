// The dispatch loop of the bytecode VM (src/vm): runs one frame's instructions, makes and runs calls (functions, closures, externs,
// interface methods), unwinds a `throw` to the handler covering it, and drives coroutines. Everything a value-producing instruction can
// fail with travels as an Outcome: a throw unwinds inside the frame that holds a handler for it and otherwise ends the frame, a trap
// ends the run, a suspension ends the frame with its pc already past the yield so the next resume carries on from there.
// The semantics (trap texts, evaluation order, what a coroutine's `.resume()` answers) were matched to the Nina-written reference VM
// (removed 2026-10-28, see core/docs/language/vm-proposal.md); "the oracle" in the comments below means that reference behaviour.
// runtime/tests/vm_vs_native.sh now holds this VM to the native build.
#include "polyir/Std.hpp"
#include "vm/Machine.hpp"

namespace polyir::vm {

  namespace {

    // Past this many nested calls a runaway recursion is reported as a trap instead of overrunning the host's stack.
    constexpr int kMaxDepth = 20000;

    // ── the operand stack ──

    void push(Frame& frame, Value v) {
      frame.stack.push_back(move(v));
    }

    Value pop(Frame& frame) {
      if (frame.stack.empty()) {
        return VoidValue();
      }

      Value v = move(frame.stack.back());
      frame.stack.pop_back();

      return v;
    }

    Value& top(Frame& frame) {
      return frame.stack.back();
    }

    // The last `n` pushed values, in the order they were pushed (the first argument first).
    Vector<Value> popN(Frame& frame, int32_t n) {
      const size_t first = frame.stack.size() - static_cast<size_t>(n);
      Vector<Value> out;
      out.reserve(static_cast<size_t>(n));

      for (size_t i = first; i < frame.stack.size(); i++) {
        out.push_back(move(frame.stack[i]));
      }

      frame.stack.resize(first);

      return out;
    }

    // ── small outcomes ──

    // The instruction did its work and the frame carries on with the next one.
    Outcome next() {
      return VoidOutcome();
    }

    Outcome suspended(Value v) {
      Outcome outcome;
      outcome.flow = Flow::Suspend;
      outcome.value = move(v);

      return outcome;
    }

    Outcome doneCase() {
      return ValueOutcome(VariantValue(1, VoidValue()));
    }

    Outcome notFunction() {
      return Trap("calling a value that is not a function");
    }

    // A call to an extern nothing supplies; preflight declines such a program first, so this is only a guard.
    Outcome unbound(const String& name) {
      return UnboundExternTrap(name);
    }

    // A call's outcome, as the calling frame sees it: a value is pushed and the frame carries on. A throw or a trap is handed to the
    // loop.
    Outcome called(Frame& frame, Outcome out) {
      if (!IsValue(out)) {
        return out;
      }

      push(frame, move(out.value));

      return next();
    }

    const Shape& shape(const Machine& m, int32_t index) {
      return m.program->shapes[static_cast<size_t>(index)];
    }

    const Value& constant(const Machine& m, int32_t index) {
      return m.program->constants[static_cast<size_t>(index)];
    }

    // ── frames ──

    Frame frameFor(int32_t function, const Code& code) {
      Frame frame;
      frame.function = function;
      frame.slots.resize(static_cast<size_t>(max(code.slots, code.captures + code.parameters)));

      return frame;
    }

    void bindArgs(Frame& frame, int32_t first, Vector<Value>& args) {
      for (size_t i = 0; i < args.size(); i++) {
        frame.slots[static_cast<size_t>(first) + i] = move(args[i]);
      }
    }

    Outcome runCode(Machine& m, const Code& code, Frame& frame);

    // One nested run: the depth guard turns runaway recursion into a trap before the host's own stack gives out.
    Outcome runGuarded(Machine& m, const Code& code, Frame& frame) {
      if (m.depth >= kMaxDepth) {
        return Trap("stack overflow");
      }

      m.depth++;
      Outcome out = runCode(m, code, frame);
      m.depth--;

      return out;
    }

    // A plain call never suspends: a yield reaching its frame is outside any coroutine (the oracle traps the same way).
    Outcome trapStrayYield(Outcome out) {
      if (out.flow == Flow::Suspend) {
        return Trap("a yield outside a coroutine");
      }

      return out;
    }

    // Calling a coroutine function runs none of it: the frame waits, parameters bound, for the first `.resume()`.
    Value newCoroutine(Machine& m, Frame frame) {
      HeapObject object;
      object.kind = ObjectKind::Coroutine;
      object.coroutine = make_shared<Coroutine>();
      object.coroutine->frame = move(frame);

      return RefValue(Alloc(m.heap, move(object)));
    }

    // A closure body's captured boxes are its first slots, its parameters follow. The closure record is taken by value: the heap may grow
    // while the body runs, which would move the object it lives in.
    Outcome callClosure(Machine& m, Closure closure, Vector<Value> args) {
      const Code& code = m.program->functions[static_cast<size_t>(closure.function)];
      Frame frame = frameFor(closure.function, code);
      bindArgs(frame, 0, closure.captured);
      bindArgs(frame, code.captures, args);

      return trapStrayYield(runGuarded(m, code, frame));
    }

    bool isFunctionIndex(const Machine& m, int64_t index) {
      return index >= 0 && index < static_cast<int64_t>(m.program->functions.size());
    }

    // ── externs ──

    const ExternInfo* externNamed(const Machine& m, const String& name) {
      for (const ExternInfo& e : m.program->externs) {
        if (e.name == name) {
          return &e;
        }
      }

      return nullptr;
    }

    // The VM's own bindings first (the host layer, putchar, libm), then a loaded `import native` library, in the oracle's order.
    Outcome callExternInfo(Machine& m, const ExternInfo* e, const String& name, const Vector<Value>& args) {
      if (BindsExtern(name)) {
        return CallBoundExtern(m, name, args);
      }

      if (e != nullptr && HasForeign(m, e->symbol)) {
        return CallForeign(m, *e, e->symbol, args);
      }

      return unbound(name);
    }

    Outcome callExtern(Machine& m, int32_t index, const Vector<Value>& args) {
      const ExternInfo& e = m.program->externs[static_cast<size_t>(index)];

      return callExternInfo(m, &e, e.name, args);
    }

    // ── interfaces ──

    HeapObject* structOf(Machine& m, const Value& v) {
      HeapObject* object = ObjectOf(m.heap, v);

      return object != nullptr && object->kind == ObjectKind::Struct ? object : nullptr;
    }

    // `Struct_method`: the checker's member naming, which the receiver's own struct name picks; "" when the slot is out of range.
    String methodTarget(const Machine& m, const HeapObject& receiver, int32_t interfaceIndex, int64_t slot) {
      const InterfaceInfo& info = m.program->interfaces[static_cast<size_t>(interfaceIndex)];

      if (slot < 0 || slot >= static_cast<int64_t>(info.methods.size())) {
        return "";
      }

      return receiver.name + "_" + info.methods[static_cast<size_t>(slot)];
    }

    // The receiver goes back in front and the vtable slot (args[0]) is dropped: the slot's place is reused for the receiver.
    Outcome callInterface(Machine& m, int32_t interfaceIndex, Value receiver, Vector<Value> args) {
      const HeapObject* object = structOf(m, receiver);

      if (object == nullptr || args.empty()) {
        return Trap("an interface call on a value that is not a struct");
      }

      const String target = methodTarget(m, *object, interfaceIndex, IntOf(args[0]));

      if (target.empty()) {
        return Trap("'" + object->name + "' has no method in that interface slot");
      }

      args[0] = move(receiver);

      return CallNamed(m, target, move(args));
    }

    // ── globals ──

    bool isBound(const Machine& m, int32_t g) {
      return g >= 0 && g < static_cast<int32_t>(m.globals.size()) && m.globals[static_cast<size_t>(g)].kind == ValueKind::Ref;
    }

    // A global read before its initializer ran (or one no initializer produced) is unbound, as the oracle reported it.
    Outcome unboundGlobal(const Machine& m, int32_t g) {
      return Trap("'" + m.program->globals[static_cast<size_t>(g)] + "' is not bound");
    }

    Outcome loadGlobal(Machine& m, Frame& frame, int32_t g) {
      if (!isBound(m, g)) {
        return unboundGlobal(m, g);
      }

      push(frame, BoxGet(m.heap, m.globals[static_cast<size_t>(g)].number));

      return next();
    }

    Outcome storeGlobal(Machine& m, Frame& frame, int32_t g) {
      if (!isBound(m, g)) {
        return unboundGlobal(m, g);
      }

      BoxSet(m.heap, m.globals[static_cast<size_t>(g)].number, pop(frame));

      return next();
    }

    // ── exceptions ──

    // The innermost handler covering `at`: handler ranges nest, so the narrowest one is the nearest `try`.
    const Handler* handlerFor(const Code& code, int32_t at) {
      const Handler* best = nullptr;

      for (const Handler& h : code.handlers) {
        if (at < h.from || at >= h.to) {
          continue;
        }

        if (best == nullptr || h.to - h.from < best->to - best->from) {
          best = &h;
        }
      }

      return best;
    }

    // The thrown code lands in the handler's slot (a named catch's variable, or the hidden slot a cleanup rethrows from). A handler
    // without a slot (-1, which Compile.cpp never emits) would leave the code on the cleared stack instead.
    void enterHandler(Frame& frame, const Handler& h, int64_t thrownCode) {
      frame.stack.clear();

      if (h.slot >= 0) {
        frame.slots[static_cast<size_t>(h.slot)] = I64Value(thrownCode);
      } else {
        push(frame, I64Value(thrownCode));
      }

      frame.pc = h.catchPc;
    }

    // Whether a throw at `at` stays in this frame.
    bool unwind(const Code& code, Frame& frame, int32_t at, int64_t thrownCode) {
      const Handler* h = handlerFor(code, at);

      if (h == nullptr) {
        return false;
      }

      enterHandler(frame, *h, thrownCode);

      return true;
    }

    // ── the instructions, one handler per group of opcodes (see `execute` below) ──

    // The first group, part 4 of 4: pushing Void or None.
    Outcome pushVoidOrNoneOp(Frame& frame, const Instruction& instruction) {
      switch (instruction.op) {
        case Op::PushVoid: {
          push(frame, VoidValue());
          return next();
        }
        case Op::PushNone: {
          push(frame, NoneValue());
          return next();
        }
        default:
          return Trap("unknown instruction");
      }
    }

    // The first group, part 3 of 4: pushing a constant.
    Outcome pushOp(Machine& m, Frame& frame, const Instruction& instruction) {
      switch (instruction.op) {
        case Op::PushConst: {
          push(frame, constant(m, instruction.a));
          return next();
        }
        case Op::PushString: {
          push(frame, StrValue(m.heap, TextOf(constant(m, instruction.a))));
          return next();
        }
        default:
          return pushVoidOrNoneOp(frame, instruction);
      }
    }

    // The first group, part 2 of 4: the operand stack itself and the globals.
    Outcome stackOp(Machine& m, Frame& frame, const Instruction& instruction) {
      switch (instruction.op) {
        case Op::Pop: {
          pop(frame);
          return next();
        }
        case Op::Dup: {
          push(frame, top(frame));
          return next();
        }
        case Op::LoadGlobal:
          return loadGlobal(m, frame, instruction.a);
        case Op::StoreGlobal:
          return storeGlobal(m, frame, instruction.a);
        default:
          return pushOp(m, frame, instruction);
      }
    }

    Outcome boxSlot(Machine& m, Frame& frame, int32_t slot) {
      Value held = move(frame.slots[static_cast<size_t>(slot)]);
      frame.slots[static_cast<size_t>(slot)] = RefValue(NewBox(m.heap, move(held)));

      return next();
    }

    // A label parameter's slot (or a closure's capture of one). A label that was fallen into has unbound parameters (as in the
    // oracle). So the unbound marker left there reads and writes as a missing name does.
    Value& boundCell(Machine& m, Frame& frame, const Instruction& instruction) {
      Value& slot = frame.slots[static_cast<size_t>(instruction.a)];

      return instruction.c == 1 ? ObjectAt(m.heap, slot.number).cell : slot;
    }

    Outcome notBound(const Machine& m, const Instruction& instruction) {
      return Trap("'" + TextOf(constant(m, instruction.b)) + "' is not bound");
    }

    Outcome loadBound(Machine& m, Frame& frame, const Instruction& instruction) {
      const Value& cell = boundCell(m, frame, instruction);

      if (IsUnbound(cell)) {
        return notBound(m, instruction);
      }

      push(frame, cell);

      return next();
    }

    // A boxed parameter's cell is in the heap: the write barrier first.
    Outcome storeBound(Machine& m, Frame& frame, const Instruction& instruction) {
      Value& cell = boundCell(m, frame, instruction);

      if (IsUnbound(cell)) {
        return notBound(m, instruction);
      }

      if (instruction.c == 1) {
        WriteBarrier(m.heap, cell);
      }

      cell = pop(frame);

      return next();
    }

    // A plain slot holds its value; a boxed one holds the box's heap id.
    Outcome loadSlot(Machine& m, Frame& frame, const Instruction& instruction) {
      const Value& slot = frame.slots[static_cast<size_t>(instruction.a)];
      push(frame, instruction.op == Op::Load ? slot : BoxGet(m.heap, slot.number));

      return next();
    }

    Outcome storeSlot(Machine& m, Frame& frame, const Instruction& instruction) {
      Value& slot = frame.slots[static_cast<size_t>(instruction.a)];

      if (instruction.op == Op::Store) {
        slot = pop(frame);
      } else {
        BoxSet(m.heap, slot.number, pop(frame));
      }

      return next();
    }

    Outcome boundOp(Machine& m, Frame& frame, const Instruction& instruction) {
      return instruction.op == Op::LoadBound ? loadBound(m, frame, instruction) : storeBound(m, frame, instruction);
    }

    // The first group (PushConst..Dup), part 1 of 4: a function's slots.
    Outcome slotOp(Machine& m, Frame& frame, const Instruction& instruction) {
      switch (instruction.op) {
        case Op::Load: case Op::LoadBoxed:
          return loadSlot(m, frame, instruction);
        case Op::Store: case Op::StoreBoxed:
          return storeSlot(m, frame, instruction);
        case Op::BoxSlot:
          return boxSlot(m, frame, instruction.a);
        case Op::LoadBound: case Op::StoreBound:
          return boundOp(m, frame, instruction);
        default:
          return stackOp(m, frame, instruction);
      }
    }

    Outcome binary(Machine& m, Frame& frame, const Instruction& instruction) {
      const Value right = pop(frame);
      const Value left = pop(frame);

      return called(frame, BinaryOp(m, static_cast<BinOp>(instruction.a), shape(m, instruction.b), shape(m, instruction.c), left, right));
    }

    Outcome unary(Machine& m, Frame& frame, const Instruction& instruction) {
      const Value v = pop(frame);

      return called(frame, UnaryOp(static_cast<UnOp>(instruction.a), shape(m, instruction.b), shape(m, instruction.c), v));
    }

    // The second group (Fit..Unary): conversions and operators.
    Outcome operatorOp(Machine& m, Frame& frame, const Instruction& instruction) {
      switch (instruction.op) {
        case Op::Fit: {
          top(frame) = FitTo(top(frame), shape(m, instruction.a));
          return next();
        }
        case Op::Convert: {
          top(frame) = ConvertTo(top(frame), shape(m, instruction.a));
          return next();
        }
        case Op::Binary:
          return binary(m, frame, instruction);
        case Op::Unary:
          return unary(m, frame, instruction);
        default:
          return Trap("unknown instruction");
      }
    }

    int32_t targetOf(const JumpTable& table, int64_t picked) {
      for (size_t k = 0; k < table.values.size(); k++) {
        if (table.values[k] == picked) {
          return table.targets[k];
        }
      }

      return table.otherwise;
    }

    // A conditional jump: it goes to its target when the condition it pops is `wanted`.
    Outcome jumpWhen(Frame& frame, const Instruction& instruction, bool wanted) {
      if (Truthy(pop(frame)) == wanted) {
        frame.pc = instruction.a;
      }

      return next();
    }

    // The third group (Jump..Switch): moving the pc.
    Outcome jumpOp(const Code& code, Frame& frame, const Instruction& instruction) {
      switch (instruction.op) {
        case Op::Jump: {
          frame.pc = instruction.a;
          return next();
        }
        case Op::JumpIfFalse:
          return jumpWhen(frame, instruction, false);
        case Op::JumpIfTrue:
          return jumpWhen(frame, instruction, true);
        case Op::Switch: {
          frame.pc = targetOf(code.tables[static_cast<size_t>(instruction.a)], IntOf(pop(frame)));
          return next();
        }
        default:
          return Trap("unknown instruction");
      }
    }

    Outcome callIndirect(Machine& m, Frame& frame, int32_t count) {
      Vector<Value> args = popN(frame, count);
      const Value callee = pop(frame);

      return called(frame, CallValue(m, callee, move(args)));
    }

    Outcome callInterfaceInstruction(Machine& m, Frame& frame, const Instruction& instruction) {
      Vector<Value> args = popN(frame, instruction.b);
      Value receiver = pop(frame);

      return called(frame, callInterface(m, instruction.a, move(receiver), move(args)));
    }

    Outcome makeClosure(Machine& m, Frame& frame, const Instruction& instruction) {
      HeapObject object;
      object.kind = ObjectKind::Closure;
      object.closure.function = instruction.a;
      object.closure.captured = popN(frame, instruction.b);
      push(frame, RefValue(Alloc(m.heap, move(object))));

      return next();
    }

    // A function value carries its name (what `show` prints). An extern's number is -1 - its index (so -1, -2, ...). A call through
    // the value sees the negative number and finds the extern instead of a function.
    Value functionValueOf(const Machine& m, int32_t function) {
      return FuncValue(function, m.program->functions[static_cast<size_t>(function)].name);
    }

    Value externValueOf(const Machine& m, int32_t externIndex) {
      return FuncValue(-1 - externIndex, m.program->externs[static_cast<size_t>(externIndex)].name);
    }

    Outcome funcRef(Machine& m, Frame& frame, const Instruction& instruction) {
      if (instruction.op == Op::FuncRef) {
        push(frame, functionValueOf(m, instruction.a));
      } else {
        push(frame, externValueOf(m, instruction.a));
      }

      return next();
    }

    // A call by index: a declared function or an extern, its arguments popped.
    Outcome callDirect(Machine& m, Frame& frame, const Instruction& instruction) {
      if (instruction.op == Op::Call) {
        return called(frame, CallFunction(m, instruction.a, popN(frame, instruction.b)));
      }

      return called(frame, callExtern(m, instruction.a, popN(frame, instruction.b)));
    }

    // A function as a value: a closure over the captures pushed, or a bare reference.
    Outcome functionValue(Machine& m, Frame& frame, const Instruction& instruction) {
      if (instruction.op == Op::MakeClosure) {
        return makeClosure(m, frame, instruction);
      }

      return funcRef(m, frame, instruction);
    }

    // The fourth group (Call..ExternRef): calls and function values.
    Outcome callOp(Machine& m, Frame& frame, const Instruction& instruction) {
      switch (instruction.op) {
        case Op::Call: case Op::CallExtern:
          return callDirect(m, frame, instruction);
        case Op::CallIndirect:
          return callIndirect(m, frame, instruction.b);
        case Op::CallInterface:
          return callInterfaceInstruction(m, frame, instruction);
        case Op::MakeClosure: case Op::FuncRef: case Op::ExternRef:
          return functionValue(m, frame, instruction);
        default:
          return Trap("unknown instruction");
      }
    }

    // The value is popped; the struct stays pushed for the next field (a struct literal fills its fields in sequence).
    Outcome setField(Machine& m, Frame& frame, int32_t field) {
      Value v = pop(frame);
      HeapObject* object = structOf(m, top(frame));

      if (object == nullptr) {
        return Trap("assigning a field of a value that is not a struct");
      }

      WriteBarrier(m.heap, object->items[static_cast<size_t>(field)]);
      object->items[static_cast<size_t>(field)] = move(v);

      return next();
    }

    Outcome getField(Machine& m, Frame& frame, int32_t field) {
      const Value s = pop(frame);
      const HeapObject* object = structOf(m, s);

      if (object == nullptr) {
        return Trap("reading a field of a value that is not a struct");
      }

      push(frame, object->items[static_cast<size_t>(field)]);

      return next();
    }

    // The write pushes nothing: an assignment is a statement.
    Outcome setIndex(Machine& m, Frame& frame, const Instruction& instruction) {
      Value v = pop(frame);
      const Vector<Value> indices = popN(frame, instruction.a);
      const Value container = pop(frame);
      Outcome out = WriteIndex(m, container, indices, shape(m, instruction.b), move(v));

      return IsValue(out) ? next() : out;
    }

    Outcome getIndex(Machine& m, Frame& frame, const Instruction& instruction) {
      const Vector<Value> indices = popN(frame, instruction.a);
      const Value container = pop(frame);

      return called(frame, ReadIndex(m, container, indices, shape(m, instruction.b), shape(m, instruction.c)));
    }

    Outcome newContainer(Machine& m, Frame& frame, const Instruction& instruction) {
      const Vector<Value> sizes = popN(frame, instruction.b);
      push(frame, NewContainerOf(m, shape(m, instruction.a), sizes));

      return next();
    }

    Outcome wrapTop(Frame& frame, const Instruction& instruction) {
      Value v = pop(frame);

      if (instruction.op == Op::Variant) {
        push(frame, VariantValue(instruction.a, move(v)));
      } else {
        push(frame, EitherValue(instruction.a == 1, move(v)));
      }

      return next();
    }

    // Debug markers and releases do nothing (their operands were not even evaluated: the compiler emits PushVoid for them).
    bool isNoOp(Builtin op) {
      return op == Builtin::Loc || op == Builtin::Var || op == Builtin::Free || op == Builtin::FreeBox;
    }

    Outcome builtinOp(Machine& m, Frame& frame, const Instruction& instruction) {
      const Builtin op = static_cast<Builtin>(instruction.a);
      Vector<Value> args = popN(frame, instruction.b);
      Value receiver = pop(frame);

      if (isNoOp(op)) {
        return called(frame, VoidOutcome());
      }

      if (op == Builtin::CoroutineYield) {
        return suspended(move(receiver));
      }

      return called(frame, CallBuiltin(m, op, shape(m, instruction.c), move(receiver), move(args)));
    }

    // Reads and writes through a field or an index.
    Outcome accessOp(Machine& m, Frame& frame, const Instruction& instruction) {
      switch (instruction.op) {
        case Op::SetField:
          return setField(m, frame, instruction.a);
        case Op::GetField:
          return getField(m, frame, instruction.a);
        case Op::SetIndex:
          return setIndex(m, frame, instruction);
        case Op::GetIndex:
          return getIndex(m, frame, instruction);
        default:
          return Trap("unknown instruction");
      }
    }

    // A struct of its defaults, or an array of the values popped.
    Outcome newObject(Machine& m, Frame& frame, const Instruction& instruction) {
      if (instruction.op == Op::NewStruct) {
        push(frame, NewStructOf(m, instruction.a));
      } else {
        push(frame, NewArray(m.heap, popN(frame, instruction.a)));
      }

      return next();
    }

    // The fifth group (NewStruct..Builtin): objects and builtins; field and index access are handed on to accessOp.
    Outcome objectOp(Machine& m, Frame& frame, const Instruction& instruction) {
      switch (instruction.op) {
        case Op::NewStruct: case Op::NewArray:
          return newObject(m, frame, instruction);
        case Op::NewContainer:
          return newContainer(m, frame, instruction);
        case Op::Variant: case Op::Either:
          return wrapTop(frame, instruction);
        case Op::Builtin:
          return builtinOp(m, frame, instruction);
        default:
          return accessOp(m, frame, instruction);
      }
    }

    // The rest of the last group (exceptions and coroutines): Throw throws the value it pops, and Trap stops the program with its message.
    Outcome throwOrTrap(Machine& m, Frame& frame, const Instruction& instruction) {
      if (instruction.op == Op::Throw) {
        return Thrown(IntOf(pop(frame)));
      }

      return Trap(TextOf(constant(m, instruction.a)));  // Op::Trap, the only opcode left
    }

    // One instruction that does not end the frame. Bytecode.hpp declares the opcodes in groups (constants and slots, operators,
    // jumps, calls, objects, then exceptions and coroutines). The opcodes that end the frame (Return, ReturnVoid, Yield, Yielded,
    // Halt) never get here: stepCode handles them first.
    Outcome execute(Machine& m, const Code& code, Frame& frame, const Instruction& instruction) {
      // Comparing with the last opcode each handler takes (Dup, Unary, Switch, ExternRef, Builtin) picks the group's handler. The
      // handler switches on the opcode itself; a group with many opcodes hands its `default:` on to a second handler for the rest
      // of the group.
      if (instruction.op <= Op::Dup) {
        return slotOp(m, frame, instruction);
      }

      if (instruction.op <= Op::Unary) {
        return operatorOp(m, frame, instruction);
      }

      if (instruction.op <= Op::Switch) {
        return jumpOp(code, frame, instruction);
      }

      if (instruction.op <= Op::ExternRef) {
        return callOp(m, frame, instruction);
      }

      if (instruction.op <= Op::Builtin) {
        return objectOp(m, frame, instruction);
      }

      // Only Throw and Trap are left (the other opcodes of the last group end the frame).
      return throwOrTrap(m, frame, instruction);
    }

    // Return, ReturnVoid, Halt, Yield and Yielded end the frame; everything else is an `execute`.
    bool endsFrame(Op op) {
      return op == Op::Return || op == Op::ReturnVoid || op == Op::Halt || op == Op::Yield || op == Op::Yielded;
    }

    Outcome finish(Frame& frame, Op op) {
      if (op == Op::Return) {
        return ValueOutcome(pop(frame));
      }

      if (op == Op::Yield || op == Op::Yielded) {
        return suspended(pop(frame));
      }

      return VoidOutcome();
    }

    // Runs one instruction. Returns the frame's outcome when the frame ends (a return, a yield, a trap, or a throw no handler here
    // catches). Returns nothing when the frame goes on, including after a throw that a handler of this frame caught.
    Optional<Outcome> stepCode(Machine& m, const Code& code, Frame& frame) {
      const int32_t at = frame.pc++;
      const Instruction& instruction = code.instructions[static_cast<size_t>(at)];

      if (endsFrame(instruction.op)) {
        return finish(frame, instruction.op);
      }

      Outcome step = execute(m, code, frame, instruction);

      if (step.flow == Flow::Value || (step.flow == Flow::Throw && unwind(code, frame, at, step.thrownCode))) {
        return {};
      }

      return step;
    }

    // Between two instructions every live reference is in a root: a running frame's slots or stack, a global, a pinned value. So
    // this is where a collection may run (Collector.cpp). It ends the frame only with the `out of memory` trap.
    Optional<Outcome> safePoint(Machine& m) {
      if (!IsCollectionDue(m)) {
        return {};
      }

      Outcome collected = CollectGarbage(m);

      if (IsValue(collected)) {
        return {};
      }

      return collected;
    }

    // Steps through the frame's instructions until one ends the frame; falling off the end returns void.
    Outcome runInstructions(Machine& m, const Code& code, Frame& frame) {
      const int32_t end = static_cast<int32_t>(code.instructions.size());

      while (frame.pc >= 0 && frame.pc < end) {
        if (Optional<Outcome> stopped = safePoint(m)) {
          return *stopped;
        }

        if (Optional<Outcome> ended = stepCode(m, code, frame)) {
          return *ended;
        }
      }

      return VoidOutcome();
    }

    // The frame is one of the collector's roots for as long as it runs. Every way out of runInstructions passes LeaveFrame, since
    // nothing in the loop throws a C++ exception.
    Outcome runCode(Machine& m, const Code& code, Frame& frame) {
      EnterFrame(m, frame);
      Outcome out = runInstructions(m, code, frame);
      LeaveFrame(m);

      return out;
    }

    // ── coroutines ──

    // The instruction a suspended frame stopped after: `yielded()` waits for an answer, a plain `yield` does not.
    bool asksAnswer(const Code& code, const Frame& frame) {
      const Instruction& instruction = code.instructions[static_cast<size_t>(frame.pc - 1)];

      return instruction.op == Op::Yielded || (instruction.op == Op::Builtin && static_cast<Builtin>(instruction.a) == Builtin::CoroutineYield);
    }

    Outcome stopped(Coroutine& co, const Code& code, Value v) {
      co.phase = asksAnswer(code, co.frame) ? CoroutinePhase::AtYielded : CoroutinePhase::AtYield;

      return ValueOutcome(VariantValue(0, move(v)));
    }

    // Runs the coroutine's frame once more: case 0 with the yielded value, case 1 when the body is over. A throw or a trap passes
    // through and finishes it.
    Outcome drive(Machine& m, Coroutine& co) {
      co.phase = CoroutinePhase::Ready;
      const Code& code = m.program->functions[static_cast<size_t>(co.frame.function)];
      Outcome out = runGuarded(m, code, co.frame);

      if (out.flow == Flow::Suspend) {
        return stopped(co, code, move(out.value));
      }

      co.phase = CoroutinePhase::Finished;

      return IsValue(out) ? doneCase() : out;
    }

    Outcome notCoroutine() {
      return Trap("a coroutine operation on a value that is not a coroutine");
    }

    SharedPtr<Coroutine> coroutineOf(Machine& m, const Value& v) {
      const HeapObject* object = ObjectOf(m.heap, v);

      if (object == nullptr || object->kind != ObjectKind::Coroutine) {
        return nullptr;
      }

      return object->coroutine;
    }

    Outcome drivePinned(Machine& m, const Value& coroutine, Coroutine& co) {
      PinValue(m, coroutine);
      Outcome out = drive(m, co);
      UnpinValue(m);

      return out;
    }

    // A global's initializer is a tiny function of its own; its value goes into a fresh box every read and write of the global shares.
    bool initGlobal(Machine& m, size_t g) {
      const Code& code = m.program->globalInits[g];
      Frame frame = frameFor(-1, code);
      Outcome out = trapStrayYield(runGuarded(m, code, frame));

      if (!IsValue(out)) {
        return false;
      }

      m.globals[g] = RefValue(NewBox(m.heap, move(out.value)));

      return true;
    }

    // A coroutine's parameters go into their boxes now, at the call (the oracle boxed them there), not when the frame first runs.
    void boxParameters(Machine& m, Frame& frame, const Code& code) {
      for (int32_t parameter = 0; parameter < code.parameters; parameter++) {
        boxSlot(m, frame, parameter);
      }
    }

    // A function value's number: a function's index, or (negative) an extern's, as externValueOf made it.
    Outcome callFunctionValue(Machine& m, int64_t number, Vector<Value> args) {
      if (number >= 0) {
        return CallFunction(m, static_cast<int32_t>(number), move(args));
      }

      const auto externIndex = static_cast<size_t>(-1 - number);

      if (externIndex >= m.program->externs.size()) {
        return notFunction();
      }

      return callExtern(m, static_cast<int32_t>(externIndex), args);
    }

  }  // namespace

  Outcome CallFunction(Machine& m, int32_t function, Vector<Value> args) {
    if (!isFunctionIndex(m, function)) {
      return notFunction();
    }

    const Code& code = m.program->functions[static_cast<size_t>(function)];
    Frame frame = frameFor(function, code);
    bindArgs(frame, 0, args);

    if (!code.isCoroutine) {
      return trapStrayYield(runGuarded(m, code, frame));
    }

    boxParameters(m, frame, code);

    return ValueOutcome(newCoroutine(m, move(frame)));
  }

  Outcome CallValue(Machine& m, const Value& callee, Vector<Value> args) {
    if (callee.kind == ValueKind::Func) {
      return callFunctionValue(m, callee.number, move(args));
    }

    const HeapObject* object = ObjectOf(m.heap, callee);

    if (object == nullptr || object->kind != ObjectKind::Closure || !isFunctionIndex(m, object->closure.function)) {
      return notFunction();
    }

    return callClosure(m, object->closure, move(args));
  }

  Outcome CallNamed(Machine& m, const String& name, Vector<Value> args) {
    const auto found = m.program->functionIndex.find(name);

    if (found != m.program->functionIndex.end()) {
      return CallFunction(m, found->second, move(args));
    }

    return callExternInfo(m, externNamed(m, name), name, args);
  }

  Outcome RunFrame(Machine& m, Frame& frame) {
    if (!isFunctionIndex(m, frame.function)) {
      return notFunction();
    }

    return runCode(m, m.program->functions[static_cast<size_t>(frame.function)], frame);
  }

  // Resumes the coroutine: its frame runs once more (`drive`). It is found through its reference and held by its shared pointer,
  // never by an `HeapObject&`. Running it may allocate, and a growing heap moves every cell.
  Outcome ResumeCoroutine(Machine& m, const Value& coroutine) {
    const SharedPtr<Coroutine> co = coroutineOf(m, coroutine);

    if (co == nullptr) {
      return notCoroutine();
    }

    if (co->phase == CoroutinePhase::Finished) {
      return doneCase();
    }

    // The coroutine's frame is written without barriers while it runs, as any frame is. So while the collector marks, every value
    // in it turns grey before it runs (what the barrier would have done for each of those writes).
    WriteBarrierAll(m.heap, ObjectAt(m.heap, coroutine.number));

    if (co->phase == CoroutinePhase::AtYielded) {
      push(co->frame, BoolValue(false));
    }

    // The reference is pinned while the coroutine runs, so a collection inside it keeps the coroutine (and with it the frame being
    // run). It does so even when the program itself no longer refers to it.
    return drivePinned(m, coroutine, *co);
  }

  Outcome CloseCoroutine(Machine& m, const Value& coroutine) {
    const SharedPtr<Coroutine> co = coroutineOf(m, coroutine);

    if (co == nullptr) {
      return notCoroutine();
    }

    if (co->phase != CoroutinePhase::AtYielded) {
      return VoidOutcome();
    }

    WriteBarrierAll(m.heap, ObjectAt(m.heap, coroutine.number));
    push(co->frame, BoolValue(true));
    Outcome out = drivePinned(m, coroutine, *co);

    return IsValue(out) ? VoidOutcome() : out;
  }

  // Every global in order, each one's failure named. The libraries are already loaded (Preflight.cpp), so an initializer may call an
  // imported extern.
  Outcome Prepare(Machine& m) {
    m.globals.assign(m.program->globals.size(), VoidValue());

    for (size_t g = 0; g < m.program->globalInits.size(); g++) {
      if (!initGlobal(m, g)) {
        return Trap("initializing global '" + m.program->globals[g] + "' failed");
      }
    }

    return VoidOutcome();
  }

}  // namespace polyir::vm
