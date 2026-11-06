// Small text helpers the LLVM compiler's parts share (see LlvmNames.hpp): how a polyir type is spelled in LLVM IR, how an integer or a float
// constant is written, and how a name or a string is quoted so LLVM reads it back exactly.
#include "polyir/Std.hpp"
#include "emit/llvm/LlvmNames.hpp"

#include <cstdio>
#include <cstring>

namespace polyir::llvm {

  namespace {

    String vectorText(const TypePtr& type) {
      return "<" + to_string(type->size) + " x " + LlvmTypeText(type->element) + ">";
    }

    bool isPlainNameChar(char c) {
      return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '.' || c == '$' || c == '-';
    }

    // `value` as the signed number an integer of `bits` bits (under 64) holds.
    int64_t wrapped(int64_t value, int bits) {
      const uint64_t mask = (uint64_t{1} << bits) - 1;
      const uint64_t low = static_cast<uint64_t>(value) & mask;
      const bool negative = (low & (uint64_t{1} << (bits - 1))) != 0;

      return negative ? static_cast<int64_t>(low) - static_cast<int64_t>(mask) - 1 : static_cast<int64_t>(low);
    }

    uint64_t bitPatternOf(double value) {
      uint64_t pattern = 0;
      memcpy(&pattern, &value, sizeof pattern);

      return pattern;
    }

    bool isPlainByte(unsigned char c) {
      return c >= 32 && c < 127 && c != '"' && c != '\\';
    }

    String escapedByte(unsigned char c) {
      char buffer[8];
      snprintf(buffer, sizeof buffer, "\\%02X", c);

      return buffer;
    }

    // Every other type is held by its address; a matrix is a GPU kernel type the LLVM back end has no type for.
    String pointerTypeText(const TypePtr& type) {
      if (type->kind == TypeKind::Matrix) {
        throw RuntimeError("a matrix is a GPU kernel type; the LLVM back end does not compile " + TypeText(*type));
      }

      return "ptr";
    }

    String plainZero(const TypePtr& type) {
      if (IsFloat(*type)) {
        return FloatText(0.0, type->bits);
      }

      if (IsBoolean(*type)) {
        return "false";
      }

      if (IsInteger(*type)) {
        return "0";
      }

      return "null";
    }

  }  // namespace

  String LlvmTypeText(const TypePtr& type) {
    switch (type->kind) {
      case TypeKind::Int: case TypeKind::UInt:
        return "i" + to_string(type->bits);
      case TypeKind::Float:
        return type->bits == 32 ? "float" : "double";
      case TypeKind::Void:
        return "void";
      case TypeKind::Vec:
        return vectorText(type);
      default:
        return pointerTypeText(type);
    }
  }

  String QuotedName(const String& name) {
    for (const char c : name) {
      if (!isPlainNameChar(c)) {
        return "\"" + name + "\"";
      }
    }

    return name;
  }

  String IntText(int64_t value, int bits) {
    if (bits == 1) {
      return value != 0 ? "true" : "false";
    }

    return to_string(bits < 64 ? wrapped(value, bits) : value);
  }

  // LLVM writes every float constant, a 32-bit one too, as the 16 hex digits of a double. It rejects one that its type cannot hold
  // exactly, so a 32-bit value is first rounded to the nearest float.
  String FloatText(double value, int bits) {
    const double exact = bits == 32 ? static_cast<double>(static_cast<float>(value)) : value;
    const uint64_t pattern = bitPatternOf(exact);
    char buffer[24];
    snprintf(buffer, sizeof buffer, "0x%016llX", static_cast<unsigned long long>(pattern));

    return buffer;
  }

  String StringBytes(const String& text) {
    String out;

    for (const unsigned char c : text) {
      out += isPlainByte(c) ? String(1, static_cast<char>(c)) : escapedByte(c);
    }

    return out;
  }

  String ZeroText(const TypePtr& type) {
    return type->kind == TypeKind::Vec ? "zeroinitializer" : plainZero(type);
  }

}  // namespace polyir::llvm
