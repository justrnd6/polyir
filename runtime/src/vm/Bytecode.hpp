// The bytecode the VM runs (src/vm): a flat stack machine per function. `CompileProgram` turns a checked HIR Module into it
// at load time (milliseconds): every local is a numbered slot, every function and label a number, so running does no
// name lookup. Control flow is jumps; a `try` is a handler table entry; a coroutine is a frame kept alive between
// resumes. The .pbc file stays the portable form (it is serialized HIR); this is derived from it.
#pragma once

#include "polyir/Std.hpp"
#include "polyir/Hir.hpp"
#include "vm/VmValues.hpp"

namespace polyir::vm {

  // One instruction: an opcode and up to three immediates. What they mean is listed with each opcode.
  // The opcodes come in groups, and the order matters twice. Listing.cpp's table of mnemonics is indexed by the opcode's number. And
  // Run.cpp's `execute` picks a group's handler by comparing with Dup, Unary, Switch, ExternRef and Builtin (the last opcode each handler
  // takes), then tests for Throw, and treats what is left as Trap; the opcodes that end a frame (Return, ReturnVoid, Yield, Yielded,
  // Halt) are handled before `execute`. So a new opcode goes before its group's comparison point.
  enum class Op : uint8_t {
    // ── constants and slots ──
    PushConst,  // a: constant index
    PushString,  // a: constant index of a string — a fresh identity every time (`===` on strings compares identities)
    PushVoid,
    PushNone,
    Load,  // a: slot — the slot's value
    LoadBoxed,  // a: slot holding a box — the box's value
    Store,  // a: slot — pops into the slot
    StoreBoxed,  // a: slot holding a box — pops into the box
    BoxSlot,  // a: slot — the slot's value moves into a fresh box, the slot now holds the box
    LoadBound,  // a: slot, b: constant index of its name, c: 1 when the slot holds a box — Load/LoadBoxed that traps "'NAME' is not bound" on the unbound marker
    StoreBound,  // a: slot, b: constant index of its name, c: 1 when the slot holds a box — Store/StoreBoxed with the same check
    LoadGlobal,  // a: global index
    StoreGlobal,  // a: global index
    Pop,
    Dup,
    // ── conversions and operators ──
    Fit,  // a: type-shape index — the top converted to the slot type it is stored in (narrowing, signedness, kind)
    Convert,  // a: type-shape index — a cast or coercion
    Binary,  // a: BinOp, b: operand type-shape, c: result type-shape — pops right then left
    Unary,  // a: UnOp, b: operand type-shape, c: result type-shape
    // ── jumps ──
    Jump,  // a: target pc
    JumpIfFalse,  // a: target pc — pops the condition
    JumpIfTrue,  // a: target pc — pops the condition
    Switch,  // a: table index — pops the subject; the table maps values to pcs with a default
    // ── calls ──
    Call,  // a: function index, b: argument count — pops the arguments, pushes the result (void for a void function)
    CallExtern,  // a: extern index, b: argument count
    CallIndirect,  // b: argument count — pops the arguments then the callee (a func or closure value)
    CallInterface,  // a: interface index, b: argument count (receiver, slot, args...) — the receiver's struct picks the method
    MakeClosure,  // a: code index, b: capture count — pops the captured boxes/values
    FuncRef,  // a: function index — a function value
    ExternRef,  // a: extern index — a function value that calls the extern
    Return,  // pops the result
    ReturnVoid,
    // ── objects ──
    NewStruct,  // a: struct index — a struct of defaults
    SetField,  // a: field index — pops the value then the struct (the struct stays pushed)
    GetField,  // a: field index — pops the struct
    SetIndex,  // a: index count, b: base type-shape — pops the value, the indices, then the container
    GetIndex,  // a: index count, b: base type-shape, c: element type-shape — pops the indices then the container
    NewArray,  // a: element count — pops the elements
    NewContainer,  // a: type-shape index, b: size argument count — pops the sizes
    Variant,  // a: tag — wraps the top
    Either,  // a: 1 for ok, 0 for err — wraps the top
    Builtin,  // a: Builtin, b: argument count (after the receiver), c: type-shape of the receiver (or the result, by op)
    // ── exceptions and coroutines ──
    Throw,  // pops the code
    Yield,  // pops the value; the coroutine suspends and `resume` gets case 0 of it
    Yielded,  // pops the value; suspends; on resume the answer (was the coroutine closed?) is pushed
    Trap,  // a: constant index of the message
    Halt
  };

  // An opcode and three plain integers whose meaning depends on the opcode (listed beside each Op above): a table index, a slot,
  // a pc, a count, an enum value (BinOp, UnOp, Builtin), a tag or a 1/0 flag. An opcode that needs fewer leaves the rest 0.
  struct Instruction {
    Op op = Op::Halt;
    int32_t a = 0;
    int32_t b = 0;
    int32_t c = 0;
  };

  // What the runtime needs to know of a type: enough to fit, convert, compare and build containers. One per distinct type.
  struct Shape {
    TypePtr type;  // the full type, for the few operations that look further (container defaults, struct names)
    TypeKind kind = TypeKind::Void;
    int bits = 64;
    bool isSigned = true;
    bool isFloat = false;
  };

  // A `switch`'s jumps: case values to pcs, and where everything else goes.
  struct JumpTable {
    Vector<int64_t> values;
    Vector<int32_t> targets;
    int32_t otherwise = 0;
  };

  // A `try`: the pcs it covers (`from` up to, not including, `to`), where its catch starts, and the slot the thrown code lands in
  // (a named catch's variable, or a hidden slot a cleanup rethrows from).
  struct Handler {
    int32_t from = 0;
    int32_t to = 0;
    int32_t catchPc = 0;
    int32_t slot = -1;
  };

  // One compiled function (a closure body is one too; its captures are its first slots). Parameters, `var`s and loop counters live in
  // heap boxes, so a closure shares them and an assignment is seen through them; a `let`, a label parameter and a caught code are
  // plain values.
  struct Code {
    String name;
    int32_t parameters = 0;
    int32_t captures = 0;  // leading slots a closure's captures land in (the enclosing function's boxes, or plain values)
    int32_t slots = 0;
    bool isCoroutine = false;
    bool returnsVoid = false;
    Vector<Instruction> instructions;
    Vector<Handler> handlers;
    Vector<JumpTable> tables;
  };

  struct StructLayout {
    String name;
    Vector<Value> defaults;  // what a fresh struct's fields hold
    Vector<TypePtr> fieldTypes;
  };

  struct ExternInfo {
    String name;
    String symbol;  // the C symbol an `import native` library supplies (the name itself when unbound)
    Vector<TypePtr> parameters;
    TypePtr result;
  };

  struct InterfaceInfo {
    String name;
    Vector<String> methods;  // in vtable slot order
  };

  struct Program {
    Vector<Code> functions;  // declared functions first (by module order), then closure bodies
    Map<String, int32_t> functionIndex;
    Vector<Value> constants;
    Vector<Shape> shapes;
    Vector<StructLayout> structs;
    Map<String, int32_t> structIndex;
    Vector<ExternInfo> externs;
    Vector<InterfaceInfo> interfaces;
    Vector<String> globals;  // names, in index order
    Vector<Code> globalInits;  // one small function per global: its initializer
    Vector<String> nativeLibraries;  // `import native "z"` modules, to load before running
  };

  // Compiles the module; throws RuntimeError on a construct the VM cannot run.
  Program CompileProgram(const Module& module);

  // What a label's parameter slot holds when the label was reached by falling through (only a goto binds a label's parameters).
  // It is a void no expression produces, so a LoadBound/StoreBound of it traps as an unbound name does.
  inline Value UnboundMarker() {
    Value v;
    v.number = -1;

    return v;
  }

  inline bool IsUnbound(const Value& v) {
    return v.kind == ValueKind::Void && v.number == -1;
  }

}  // namespace polyir::vm
