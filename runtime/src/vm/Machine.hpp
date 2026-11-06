// The running VM (src/vm): a machine holds the program, its heap and globals; a frame runs one function's bytecode. Everything here is a
// record; the work is in free functions, split by file: Preflight.cpp (can the VM run this program at all?), Run.cpp (the dispatch loop,
// calls, exceptions, coroutines), Ops.cpp (arithmetic, comparison, conversion, strings), Builtins.cpp (containers, sums, interfaces, shapes),
// Collector.cpp (the garbage collector: what the program can no longer reach is reclaimed), NativeMemory.cpp (`ptr<T>` indexing and atomics on memory C allocated),
// Host.cpp (externs: the few C functions the VM binds itself, the `polyir_*` host layer, and C libraries loaded for `import native`),
// ProcessHost.cpp and Commands.cpp (the std Process natives and the child processes behind them), LifecycleHost.cpp (std
// `Lifecycle.step`, handed to the embedding host), NinaRuntime.cpp (the `nina_*` ABI a
// lowered Nina program declares, with the LLVM route's semantics). tests/vm_vs_native.sh checks the VM against the LLVM build.
//
// Names used throughout the VM's files: `m` is the Machine every function takes first, `v` a Value, `object` (or `o`) a HeapObject.
#pragma once

#include "polyir/Std.hpp"
#include "polyir/Vm.hpp"
#include "vm/Bytecode.hpp"
#include "vm/VmValues.hpp"

#include <iostream>

namespace polyir::vm {

  // How an evaluation ended: with a value, with a polyir `throw` (an i64 code, travelling to the nearest `try`), with a trap (a deterministic
  // message that ends the run), or suspended at a yield (only a coroutine's frame does that; `value` is what it yielded).
  enum class Flow : uint8_t { Value, Throw, Trap, Suspend };

  struct Outcome {
    Flow flow = Flow::Value;
    Value value;  // Value: the result; Suspend: what was yielded
    int64_t thrownCode = 0;  // Throw: the code the `throw` carries
    String message;  // Trap: what the trap says
  };

  // One function's run: where it is, its slots and its operand stack.
  struct Frame {
    int32_t function = 0;  // the function this frame runs: an index into Program::functions
    int32_t pc = 0;
    Vector<Value> slots;
    Vector<Value> stack;
  };

  // Where a coroutine stands between two `.resume()`s (Run.cpp's ResumeCoroutine): not started or running, suspended at a plain
  // `yield`, suspended at `v.yielded()` (its expression waits for the answer: was the coroutine closed?), or finished.
  enum class CoroutinePhase : uint8_t { Ready, AtYield, AtYielded, Finished };

  struct Coroutine {
    Frame frame;
    CoroutinePhase phase = CoroutinePhase::Ready;
  };

  struct Library {
    String name;
    void* handle = nullptr;
  };

  // How long the collector's pauses were (Collector.cpp): their number, the longest, their sum, and how many fell in each bucket (1 µs wide
  // below 1 ms, 100 µs wide up to 100 ms, then one for the rest), from which the median and the 99th percentile are read.
  struct Pauses {
    int64_t count = 0;
    int64_t longest = 0;  // nanoseconds
    int64_t total = 0;
    Vector<int64_t> buckets;
  };

  // The garbage collector's policy and what it has done so far (Collector.cpp). The policy counts objects, not bytes: the VM cannot see
  // how much memory a C++ vector inside an object holds, but it always knows how many cells are in use. A machine gets its policy from
  // MemoryPolicy (Vm.cpp's machineFor), never from these zero defaults.
  struct Memory {
    int64_t collectAt = 0;  // a safe point does the collector's work once the heap holds more objects than this
    int64_t threshold = 0;  // between cycles: the heap that begins the next one (collectAt is then this)
    int64_t ceiling = 0;  // a cycle that leaves more objects reachable than this ends the run with a trap
    bool collectEverySafePoint = false;  // the stress mode: a whole collection between every two instructions
    bool incremental = true;  // a cycle in slices between which the program runs, or (false) each one at once
    bool verify = false;  // the incremental stress mode: a slice at every safe point, and the marks checked at the end of every marking
    int64_t slice = 0;  // the budget of one slice: values looked at, or cells swept
    int64_t cycleStart = 0;  // the objects the heap held when the cycle began
    int64_t collections = 0;  // cycles begun
    int64_t reclaimed = 0;  // objects swept, over every collection
    int64_t peakObjects = 0;  // the most objects the heap held at once (reachable or not yet collected)
    Pauses pauses;
  };

  // How a machine's collector is set up (Vm.cpp reads VmOptions and the environment into it).
  struct MemorySettings {
    int64_t ceiling = 0;
    bool collectEverySafePoint = false;
    bool incremental = true;
    bool verify = false;
    int64_t slice = 0;
  };

  struct Machine {
    const Program* program = nullptr;
    Heap heap;
    Vector<Value> globals;  // one per Program::globals, each a box reference
    Vector<Library> libraries;  // `import native` libraries, loaded before the entry point runs
    OStream* out = &cout;
    HostStep hostStep;  // VmOptions::hostStep: who runs a std `Lifecycle.step` (LifecycleHost.cpp)
    int depth = 0;  // nested calls, to turn runaway recursion into a trap
    // The collector's roots besides the globals: every frame that is running (the innermost last), and the values C++ code holds while
    // program code runs (a coroutine being resumed). Both are pushed and popped in pairs (EnterFrame/LeaveFrame, PinValue/UnpinValue).
    Vector<Frame*> runningFrames;
    Vector<Value> pinned;
    Memory memory;
  };

  inline Outcome ValueOutcome(Value v) {
    Outcome o;
    o.value = move(v);

    return o;
  }

  inline Outcome VoidOutcome() {
    return Outcome{};
  }

  inline Outcome Trap(String message) {
    Outcome o;
    o.flow = Flow::Trap;
    o.message = move(message);

    return o;
  }

  inline Outcome Thrown(int64_t code) {
    Outcome o;
    o.flow = Flow::Throw;
    o.thrownCode = code;

    return o;
  }

  inline bool IsValue(const Outcome& o) {
    return o.flow == Flow::Value;
  }

  // ── the roots the collector starts from (Collector.cpp reads them) ──

  // A frame is a root while it runs: its slots and its operand stack hold the function's locals and temporaries.
  inline void EnterFrame(Machine& m, Frame& frame) {
    m.runningFrames.push_back(&frame);
  }

  inline void LeaveFrame(Machine& m) {
    m.runningFrames.pop_back();
  }

  // A value C++ code keeps while program code runs, so a collection inside that code does not reclaim it.
  inline void PinValue(Machine& m, Value v) {
    m.pinned.push_back(move(v));
  }

  inline void UnpinValue(Machine& m) {
    m.pinned.pop_back();
  }

  // ── Collector.cpp ──
  // The policy a new machine starts with: the first collection once the heap holds the minimum threshold's worth of objects.
  Memory MemoryPolicy(const MemorySettings& settings);

  // The collector's work at a safe point: the next slice of the cycle under way (beginning one first when it is due). In the
  // stop-the-world mode it is a whole cycle at once. A value outcome, or the `out of memory` trap when more objects are still
  // reachable than the ceiling allows. Only ever called at a safe point.
  Outcome CollectGarbage(Machine& m);

  // The pause that `share` (out of 100) of the pauses were no longer than, in nanoseconds.
  int64_t PausePercentile(const Pauses& pauses, int64_t share);

  // Whether the safe point the dispatch loop is at has collecting to do. Allocation only counts; this is where the count is looked
  // at, in one comparison. During a cycle, `collectAt` is where the allocations since the last slice have earned the next one.
  inline bool IsCollectionDue(const Machine& m) {
    return m.heap.objects > m.memory.collectAt;
  }

  // ── Run.cpp ──
  // Runs `function` with `args` (parameters are the first slots). Falling off the end returns void; a goto to no label of the function traps.
  Outcome CallFunction(Machine& m, int32_t function, Vector<Value> args);

  // A call through a value: a function by index (`FuncRef`) or a closure on the heap.
  Outcome CallValue(Machine& m, const Value& callee, Vector<Value> args);

  // A call by name, as `InterfaceCall` and the host layer need: a declared function, or an extern.
  Outcome CallNamed(Machine& m, const String& name, Vector<Value> args);

  // Runs the frame from its pc until it returns, throws out, traps or suspends. A frame that suspended is resumed by calling this again
  // (after pushing the answer `Yielded` waits for, when it waits for one).
  Outcome RunFrame(Machine& m, Frame& frame);

  // `.resume()`: runs to the next yield (case 0 with the value) or the end (case 1); a throw or a trap passes through. `coroutine` is the
  // reference to the coroutine object (pinned while it runs).
  Outcome ResumeCoroutine(Machine& m, const Value& coroutine);

  // `.close()`: a coroutine waiting at `yielded()` hears `true` and runs once more; one at a plain `yield` is dropped as it is.
  Outcome CloseCoroutine(Machine& m, const Value& coroutine);

  // Evaluates every global's initializer (in order); the trap that stopped it, or a value outcome. Runs after preflight (the libraries loaded).
  Outcome Prepare(Machine& m);

  // ── Preflight.cpp ──
  // What the check before the run found: a library that could not be loaded (a trap), or the first extern nothing binds (a decline).
  struct Preflight {
    String libraryFailure;  // an `import native` library that did not load (a trap)
    String unboundExtern;   // the first extern nothing binds (a decline)
  };

  // Loads the `import native` libraries, then looks for an extern with no binding; both fields empty means the program can run.
  Preflight RunPreflight(Machine& m);

  // ── Ops.cpp ──
  Outcome BinaryOp(Machine& m, BinOp op, const Shape& operand, const Shape& result, const Value& left, const Value& right);

  Outcome UnaryOp(UnOp op, const Shape& operand, const Shape& result, const Value& v);

  Value ConvertTo(const Value& v, const Shape& to);  // an explicit cast or an implicit coercion

  Value FitTo(const Value& v, const Shape& slot);  // a value stored in a slot of the type: narrowed, re-signed or converted as the backends' typed slots do

  Outcome TextBuiltin(Machine& m, Builtin op, const Shape& receiver, const Value& v, const Vector<Value>& args);  // StrOf, CharAt, Slice, Find, Print

  int64_t HashText(const String& text);  // djb2 over the bytes, matching the LLVM backend's polyir_hash_string

  // ── Builtins.cpp ──
  // Every builtin but the ones Run.cpp handles itself (Loc, Var, Free, FreeBox: no-ops whose operands are not evaluated; CoroutineYield: the Yielded
  // instruction). `shape` is the receiver's static type (shapes, interface calls and `str` are decided by it).
  Outcome CallBuiltin(Machine& m, Builtin op, const Shape& shape, Value receiver, Vector<Value> args);

  Outcome ReadIndex(Machine& m, const Value& container, const Vector<Value>& indices, const Shape& base, const Shape& element);

  Outcome WriteIndex(Machine& m, const Value& container, const Vector<Value>& indices, const Shape& base, Value v);

  Value NewContainerOf(Machine& m, const Shape& type, const Vector<Value>& sizes);

  Value NewStructOf(Machine& m, int32_t structIndex);

  // ── NativeMemory.cpp ──
  // Raw memory: a `ptr<T>` holding an address C handed out (malloc, calloc, a library), read and written as the LLVM build does.
  // It is trusted as native code trusts it (no bounds; a null address traps). Only numbers and addresses are ever stored there,
  // never a heap reference.

  bool IsNativeAddress(const Shape& base, const Value& container);  // a pointer type whose value is an address (not a VM heap object)

  Outcome ReadNative(const Value& address, const Vector<Value>& indices, const Shape& base);

  Outcome WriteNative(const Value& address, const Vector<Value>& indices, const Shape& base, const Value& v);

  bool IsAtomicOp(Builtin op);  // atomicLoad, atomicStore, atomicRmw, cmpxchg, fence

  Outcome AtomicOp(Builtin op, const Shape& pointer, const Value& address, const Vector<Value>& args);  // atomicRmw and cmpxchg give the old value

  // ── Host.cpp ──
  // Whether `name` is an extern the VM binds itself: the `nina_*` runtime ABI (NinaRuntime.hpp), `putchar`, `puts` and the libm
  // floats the examples use. The VM also binds the `polyir_*` host layer (files, commands, the environment, the os and cpu, the
  // program's arguments).
  bool BindsExtern(const String& name);

  // Calls one of the externs the VM binds itself.
  Outcome CallBoundExtern(Machine& m, const String& name, const Vector<Value>& args);

  // The trap for a call to an extern nothing binds (Host.cpp, NinaRuntime.cpp and Run.cpp all say it the same way).
  Outcome UnboundExternTrap(const String& name);

  // Loads the `import native` libraries (a path as it is; a name as lib<name>.dylib/.so on POLYIR_LIBRARY_PATH, the system
  // folders, Homebrew's). Returns the message of the first that cannot be loaded, "" when every one loaded.
  String LoadLibraries(Machine& m, const Vector<String>& names);

  bool HasForeign(const Machine& m, const String& symbol);

  // Calls a C function of a loaded library with the extern's declared types. Whole numbers, bools and chars go as integers, f64 as
  // doubles, strings as NUL-terminated copies, pointers as addresses. It takes up to 6 integer and 8 floating-point arguments (f32
  // and f64 in any order). A struct or a container traps.
  Outcome CallForeign(Machine& m, const ExternInfo& declared, const String& symbol, const Vector<Value>& args);

}  // namespace polyir::vm
