// Listing.cpp: the VM's instructions as text, to read what a program compiled to (`polyir F.poly|F.pbc --vm-listing`).
// One block per function: a header (parameters, captures, slots), one line per instruction (the pc, the mnemonic and its
// operands spelled out as names, types and constants), then its try handlers and switch tables. The mnemonics are listed
// beside the opcodes in docs/bytecode.md.
#include "polyir/Vm.hpp"
#include "polyir/Types.hpp"
#include "vm/Bytecode.hpp"

namespace polyir {

  using vm::Code;
  using vm::CompileProgram;
  using vm::Handler;
  using vm::Instruction;
  using vm::IsUnbound;
  using vm::JumpTable;
  using vm::Op;
  using vm::Program;
  using vm::Show;
  using vm::TextOf;
  using vm::Value;
  using vm::ValueKind;

  namespace {

    // How one instruction's immediates read: a pointer to one of the small functions below (`constant`, `slot`, `call`, ...), each
    // of which turns the immediates of its opcode into text.
    using Operands = String (*)(const Program&, const Instruction&);

    struct Mnemonic {
      const char* name;
      Operands operands;
    };

    // ── the names an immediate stands for ──

    String number(int64_t n) {
      return to_string(n);
    }

    // Whether `index` is a valid position in any of the Program's tables (constants, shapes, functions, ...): a template, since the
    // tables hold different kinds of items.
    template <typename Item>
    bool holds(const Vector<Item>& items, int32_t index) {
      return index >= 0 && static_cast<size_t>(index) < items.size();
    }

    // A label parameter reached by falling through holds the unbound marker (Bytecode.hpp), not a value of the program.
    String quoted(const Value& value) {
      if (IsUnbound(value)) {
        return "unbound";
      }

      return value.kind == ValueKind::Str ? "\"" + TextOf(value) + "\"" : Show(value);
    }

    String constantAt(const Program& program, int32_t index) {
      return holds(program.constants, index) ? quoted(program.constants[static_cast<size_t>(index)]) : "?";
    }

    String shapeAt(const Program& program, int32_t index) {
      return holds(program.shapes, index) ? TypeText(program.shapes[static_cast<size_t>(index)].type) : "?";
    }

    String functionAt(const Program& program, int32_t index) {
      return holds(program.functions, index) ? program.functions[static_cast<size_t>(index)].name : "?";
    }

    String externAt(const Program& program, int32_t index) {
      return holds(program.externs, index) ? program.externs[static_cast<size_t>(index)].name : "?";
    }

    String slotName(int32_t slot) {
      return "s" + number(slot);
    }

    String withArguments(const String& callee, int32_t count) {
      return callee + "/" + number(count);
    }

    // ── the operands of each opcode ──

    String nothing(const Program&, const Instruction&) {
      return "";
    }

    String constant(const Program& program, const Instruction& instruction) {
      return constantAt(program, instruction.a);
    }

    String slot(const Program&, const Instruction& instruction) {
      return slotName(instruction.a);
    }

    String nameAt(const Program& program, int32_t index) {
      return holds(program.constants, index) ? TextOf(program.constants[static_cast<size_t>(index)]) : "?";
    }

    String boundSlot(const Program& program, const Instruction& instruction) {
      return slotName(instruction.a) + (instruction.c == 1 ? " boxed" : "") + "  ; " + nameAt(program, instruction.b);
    }

    String global(const Program& program, const Instruction& instruction) {
      return holds(program.globals, instruction.a) ? program.globals[static_cast<size_t>(instruction.a)] : "?";
    }

    String shape(const Program& program, const Instruction& instruction) {
      return shapeAt(program, instruction.a);
    }

    String binary(const Program& program, const Instruction& instruction) {
      return String(BinOpName(static_cast<BinOp>(instruction.a))) + " " + shapeAt(program, instruction.b) + " -> " + shapeAt(program, instruction.c);
    }

    String unary(const Program& program, const Instruction& instruction) {
      return String(UnOpName(static_cast<UnOp>(instruction.a))) + " " + shapeAt(program, instruction.b) + " -> " + shapeAt(program, instruction.c);
    }

    String target(const Program&, const Instruction& instruction) {
      return "@" + number(instruction.a);
    }

    String table(const Program&, const Instruction& instruction) {
      return "table " + number(instruction.a);
    }

    String call(const Program& program, const Instruction& instruction) {
      return withArguments(functionAt(program, instruction.a), instruction.b);
    }

    String callExtern(const Program& program, const Instruction& instruction) {
      return withArguments(externAt(program, instruction.a), instruction.b);
    }

    String callIndirect(const Program&, const Instruction& instruction) {
      return withArguments("callee", instruction.b);
    }

    String callInterface(const Program& program, const Instruction& instruction) {
      return withArguments(holds(program.interfaces, instruction.a) ? program.interfaces[static_cast<size_t>(instruction.a)].name : "?", instruction.b);
    }

    String closure(const Program& program, const Instruction& instruction) {
      return functionAt(program, instruction.a) + " captures " + number(instruction.b);
    }

    String function(const Program& program, const Instruction& instruction) {
      return functionAt(program, instruction.a);
    }

    String externValue(const Program& program, const Instruction& instruction) {
      return externAt(program, instruction.a);
    }

    String structure(const Program& program, const Instruction& instruction) {
      return holds(program.structs, instruction.a) ? program.structs[static_cast<size_t>(instruction.a)].name : "?";
    }

    String field(const Program&, const Instruction& instruction) {
      return "field " + number(instruction.a);
    }

    String setIndex(const Program& program, const Instruction& instruction) {
      return number(instruction.a) + " index on " + shapeAt(program, instruction.b);
    }

    String getIndex(const Program& program, const Instruction& instruction) {
      return number(instruction.a) + " index on " + shapeAt(program, instruction.b) + " -> " + shapeAt(program, instruction.c);
    }

    String count(const Program&, const Instruction& instruction) {
      return number(instruction.a);
    }

    String container(const Program& program, const Instruction& instruction) {
      return shapeAt(program, instruction.a) + " sizes " + number(instruction.b);
    }

    String variantCase(const Program&, const Instruction& instruction) {
      return "case " + number(instruction.a);
    }

    String eitherSide(const Program&, const Instruction& instruction) {
      return instruction.a == 1 ? "ok" : "err";
    }

    String builtin(const Program& program, const Instruction& instruction) {
      return withArguments(BuiltinName(static_cast<Builtin>(instruction.a)), instruction.b) + " on " + shapeAt(program, instruction.c);
    }

    // One row per opcode, in the order Bytecode.hpp declares them: the table is indexed by the opcode's number (the static_assert
    // below catches a missing row). Each instruction is listed by its opcode's name in camelCase (`PushConst` is `pushConst`).
    const Mnemonic kMnemonics[] = {
      // ── constants and slots ──
      {"pushConst", constant},  // PushConst
      {"pushString", constant},  // PushString
      {"pushVoid", nothing},  // PushVoid
      {"pushNone", nothing},  // PushNone
      {"load", slot},  // Load
      {"loadBoxed", slot},  // LoadBoxed
      {"store", slot},  // Store
      {"storeBoxed", slot},  // StoreBoxed
      {"boxSlot", slot},  // BoxSlot
      {"loadBound", boundSlot},  // LoadBound
      {"storeBound", boundSlot},  // StoreBound
      {"loadGlobal", global},  // LoadGlobal
      {"storeGlobal", global},  // StoreGlobal
      {"pop", nothing},  // Pop
      {"dup", nothing},  // Dup
      // ── conversions and operators ──
      {"fit", shape},  // Fit
      {"convert", shape},  // Convert
      {"binary", binary},  // Binary
      {"unary", unary},  // Unary
      // ── jumps ──
      {"jump", target},  // Jump
      {"jumpIfFalse", target},  // JumpIfFalse
      {"jumpIfTrue", target},  // JumpIfTrue
      {"switch", table},  // Switch
      // ── calls ──
      {"call", call},  // Call
      {"callExtern", callExtern},  // CallExtern
      {"callIndirect", callIndirect},  // CallIndirect
      {"callInterface", callInterface},  // CallInterface
      {"makeClosure", closure},  // MakeClosure
      {"funcRef", function},  // FuncRef
      {"externRef", externValue},  // ExternRef
      {"return", nothing},  // Return
      {"returnVoid", nothing},  // ReturnVoid
      // ── objects ──
      {"newStruct", structure},  // NewStruct
      {"setField", field},  // SetField
      {"getField", field},  // GetField
      {"setIndex", setIndex},  // SetIndex
      {"getIndex", getIndex},  // GetIndex
      {"newArray", count},  // NewArray
      {"newContainer", container},  // NewContainer
      {"variant", variantCase},  // Variant
      {"either", eitherSide},  // Either
      {"builtin", builtin},  // Builtin
      // ── exceptions and coroutines ──
      {"throw", nothing},  // Throw
      {"yield", nothing},  // Yield
      {"yielded", nothing},  // Yielded
      {"trap", constant},  // Trap
      {"halt", nothing},  // Halt
    };

    static_assert(sizeof(kMnemonics) / sizeof(kMnemonics[0]) == static_cast<size_t>(Op::Halt) + 1, "one mnemonic per opcode");

    // ── the lines ──

    String pcText(size_t pc) {
      String digits = to_string(pc);

      while (digits.size() < 4) {
        digits = "0" + digits;
      }

      return digits;
    }

    String padded(const String& text, size_t width) {
      return text.size() >= width ? text + " " : text + String(width - text.size(), ' ');
    }

    String instructionLine(const Program& program, size_t pc, const Instruction& instruction) {
      const Mnemonic& mnemonic = kMnemonics[static_cast<size_t>(instruction.op)];
      const String operands = mnemonic.operands(program, instruction);

      return "  " + pcText(pc) + "  " + (operands.empty() ? String(mnemonic.name) : padded(mnemonic.name, 14) + operands) + "\n";
    }

    String headerLine(const String& title, const Code& c) {
      const String captures = c.captures > 0 ? ", captures " + number(c.captures) : "";
      const String kind = c.isCoroutine ? ", coroutine" : "";

      return title + "  (params " + number(c.parameters) + captures + ", slots " + number(c.slots) + kind + ")\n";
    }

    String handlerLine(const Handler& h) {
      const String into = h.slot >= 0 ? " into " + slotName(h.slot) : " (cleanup, rethrows)";

      return "  try @" + number(h.from) + "..@" + number(h.to) + " -> @" + number(h.catchPc) + into + "\n";
    }

    String tableLine(size_t index, const JumpTable& t) {
      String entries;

      for (size_t k = 0; k < t.values.size(); k++) {
        entries += number(t.values[k]) + " -> @" + number(t.targets[k]) + ", ";
      }

      return "  table " + number(static_cast<int64_t>(index)) + ": " + entries + "else @" + number(t.otherwise) + "\n";
    }

    String codeText(const Program& program, const String& title, const Code& c) {
      String out = headerLine(title, c);

      for (size_t pc = 0; pc < c.instructions.size(); pc++) {
        out += instructionLine(program, pc, c.instructions[pc]);
      }

      for (const Handler& h : c.handlers) {
        out += handlerLine(h);
      }

      for (size_t t = 0; t < c.tables.size(); t++) {
        out += tableLine(t, c.tables[t]);
      }

      return out + "\n";
    }

    String globalTitle(const Program& program, size_t index) {
      return "global " + (index < program.globals.size() ? program.globals[index] : "?");
    }

  }  // namespace

  String BytecodeListing(const Module& module) {
    const Program program = CompileProgram(module);
    String out;

    for (const Code& c : program.functions) {
      out += codeText(program, "function " + c.name, c);
    }

    for (size_t g = 0; g < program.globalInits.size(); g++) {
      out += codeText(program, globalTitle(program, g), program.globalInits[g]);
    }

    return out;
  }

}  // namespace polyir
