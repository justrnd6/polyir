// The byte level of a WebAssembly binary: LEB128 numbers, the opcodes the direct writer uses, and the sections.
#pragma once

#include "polyir/Std.hpp"

namespace polyir::wasm {

  using Bytes = Vector<uint8_t>;

  enum Opcode : uint8_t {
    UNREACHABLE = 0x00, BLOCK = 0x02, LOOP = 0x03, IF = 0x04, ELSE = 0x05, END = 0x0B, BR = 0x0C, BR_IF = 0x0D, RETURN = 0x0F, CALL = 0x10,
    DROP = 0x1A, LOCAL_GET = 0x20, LOCAL_SET = 0x21, I32_CONST = 0x41, I64_CONST = 0x42, F64_CONST = 0x44,
    I32_EQZ = 0x45, I32_EQ = 0x46, I32_NE = 0x47, I64_EQZ = 0x50, I64_EQ = 0x51, I64_NE = 0x52, I64_LT_S = 0x53, I64_LT_U = 0x54,
    I64_GT_S = 0x55, I64_GT_U = 0x56, I64_LE_S = 0x57, I64_LE_U = 0x58, I64_GE_S = 0x59, I64_GE_U = 0x5A,
    F64_EQ = 0x61, F64_NE = 0x62, F64_LT = 0x63, F64_GT = 0x64, F64_LE = 0x65, F64_GE = 0x66,
    I32_AND = 0x71, I32_OR = 0x72, I32_XOR = 0x73,
    I64_ADD = 0x7C, I64_SUB = 0x7D, I64_MUL = 0x7E, I64_DIV_S = 0x7F, I64_DIV_U = 0x80, I64_REM_S = 0x81, I64_REM_U = 0x82, I64_AND = 0x83, I64_OR = 0x84,
    I64_XOR = 0x85, I64_SHL = 0x86, I64_SHR_S = 0x87, I64_SHR_U = 0x88, F64_NEG = 0x9A, F64_ADD = 0xA0, F64_SUB = 0xA1, F64_MUL = 0xA2, F64_DIV = 0xA3,
    I32_WRAP_I64 = 0xA7, I64_EXTEND_I32_U = 0xAD, I64_TRUNC_F64_S = 0xB0, F64_CONVERT_I64_S = 0xB9, F64_CONVERT_I64_U = 0xBA
  };

  enum ValueType : uint8_t { I32 = 0x7F, I64 = 0x7E, F64 = 0x7C };

  constexpr uint8_t EMPTY_BLOCK = 0x40;  // the type of a block, `if` or `loop` that leaves no value on the stack

  struct FunctionBinary {
    Vector<ValueType> parameters;
    Vector<ValueType> results;
    Vector<ValueType> locals;  // beyond the parameters
    Bytes code;                // the instructions, without the closing `end`
    String exportName;         // empty: not exported
  };

  void UnsignedLeb(Bytes& out, uint64_t value);

  void SignedLeb(Bytes& out, int64_t value);

  void DoubleBytes(Bytes& out, double value);

  void Append(Bytes& out, const Bytes& more);

  Bytes ModuleBytes(const Vector<FunctionBinary>& functions);

}  // namespace polyir::wasm
