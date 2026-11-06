// The byte level of a WebAssembly binary (WasmBytes.hpp), used by the direct writer (WasmDirect.cpp): numbers as LEB128 (seven
// bits a byte, the top bit set on every byte but the last), doubles as their eight little-endian bytes, names and sections as their
// length followed by their bytes.
#include "wasm/WasmBytes.hpp"

namespace polyir::wasm {

  namespace {

    // Whether `byte` (the low seven bits of what is left) can be the last byte of a signed number whose remaining bits are `rest`. The
    // rest must be all sign bits (0, or -1 for a negative number). Bit 6 of the byte (0x40) must already show that sign, because a
    // reader sign-extends from bit 6 of the last byte.
    bool isLastSignedByte(int64_t rest, uint8_t byte) {
      const bool signBitSet = (byte & 0x40) != 0;

      return (rest == 0 && !signBitSet) || (rest == -1 && signBitSet);
    }

  }  // namespace

  void UnsignedLeb(Bytes& out, uint64_t value) {
    do {
      uint8_t byte = value & 0x7Fu;
      value >>= 7;
      out.push_back(value != 0 ? static_cast<uint8_t>(byte | 0x80u) : byte);
    } while (value != 0);
  }

  // Seven bits a byte, lowest first; `>>` on a negative number keeps its sign, so what is left of it ends as -1.
  void SignedLeb(Bytes& out, int64_t value) {
    bool isLast = false;

    while (!isLast) {
      const uint8_t byte = value & 0x7F;
      value >>= 7;
      isLast = isLastSignedByte(value, byte);
      out.push_back(isLast ? byte : static_cast<uint8_t>(byte | 0x80u));
    }
  }

  void DoubleBytes(Bytes& out, double value) {
    uint64_t bits;
    memcpy(&bits, &value, sizeof(bits));

    for (int i = 0; i < 8; ++i) {
      out.push_back(static_cast<uint8_t>(bits >> (8 * i)));
    }
  }

  void Append(Bytes& out, const Bytes& more) {
    out.insert(out.end(), more.begin(), more.end());
  }

  namespace {

    constexpr uint8_t kFunctionTypeTag = 0x60;
    constexpr uint8_t kSectionType = 1;
    constexpr uint8_t kSectionFunction = 3;
    constexpr uint8_t kSectionExport = 7;
    constexpr uint8_t kSectionCode = 10;

    // A section: its id, the payload's length, the payload.
    void section(Bytes& out, uint8_t id, const Bytes& payload) {
      out.push_back(id);
      UnsignedLeb(out, payload.size());
      Append(out, payload);
    }

    Bytes valueTypes(const Vector<ValueType>& types) {
      Bytes out;
      UnsignedLeb(out, types.size());

      for (ValueType type : types) {
        out.push_back(type);
      }

      return out;
    }

    Bytes typeEntry(const FunctionBinary& f) {
      Bytes out = {kFunctionTypeTag};
      Append(out, valueTypes(f.parameters));
      Append(out, valueTypes(f.results));

      return out;
    }

    Bytes counted(const Vector<Bytes>& entries) {
      Bytes out;
      UnsignedLeb(out, entries.size());

      for (const Bytes& entry : entries) {
        Append(out, entry);
      }

      return out;
    }

    Bytes lengthPrefixed(const Bytes& payload) {
      Bytes out;
      UnsignedLeb(out, payload.size());
      Append(out, payload);

      return out;
    }

    Bytes body(const FunctionBinary& f) {
      Bytes out;
      UnsignedLeb(out, f.locals.size());

      for (ValueType type : f.locals) {
        out.insert(out.end(), {1, type});  // a run of 1 local of this type
      }

      Append(out, f.code);
      out.push_back(END);

      return lengthPrefixed(out);
    }

    Bytes exportEntry(const FunctionBinary& f, size_t index) {
      Bytes out;
      UnsignedLeb(out, f.exportName.size());
      out.insert(out.end(), f.exportName.begin(), f.exportName.end());
      out.push_back(0);  // the export's kind: 0 is a function
      UnsignedLeb(out, index);

      return out;
    }

    Bytes exports(const Vector<FunctionBinary>& functions) {
      Vector<Bytes> entries;

      for (size_t i = 0; i < functions.size(); ++i) {
        if (!functions[i].exportName.empty()) {
          entries.push_back(exportEntry(functions[i], i));
        }
      }

      return entries.empty() ? Bytes{} : counted(entries);
    }

    // The function section gives each function the index of its type. Function i uses type i, because typeEntries writes one type per
    // function (no type is shared), so the entries are 0, 1, 2, ...
    Bytes typeIndexOfEachFunction(size_t count) {
      Vector<Bytes> entries;

      for (size_t i = 0; i < count; ++i) {
        Bytes entry;
        UnsignedLeb(entry, i);
        entries.push_back(entry);
      }

      return counted(entries);
    }

  }  // namespace

  // The magic number "\0asm", then the format's version, 1.
  static Bytes moduleHeader() {
    return {0x00, 0x61, 0x73, 0x6D, 0x01, 0x00, 0x00, 0x00};
  }

  static Vector<Bytes> typeEntries(const Vector<FunctionBinary>& functions) {
    Vector<Bytes> out;

    for (const FunctionBinary& f : functions) {
      out.push_back(typeEntry(f));
    }

    return out;
  }

  static Vector<Bytes> bodyEntries(const Vector<FunctionBinary>& functions) {
    Vector<Bytes> out;

    for (const FunctionBinary& f : functions) {
      out.push_back(body(f));
    }

    return out;
  }

  static Bytes sections(const Vector<FunctionBinary>& functions) {
    Bytes out;
    section(out, kSectionType, counted(typeEntries(functions)));
    section(out, kSectionFunction, typeIndexOfEachFunction(functions.size()));

    if (!exports(functions).empty()) {
      section(out, kSectionExport, exports(functions));
    }

    section(out, kSectionCode, counted(bodyEntries(functions)));

    return out;
  }

  Bytes ModuleBytes(const Vector<FunctionBinary>& functions) {
    Bytes out = moduleHeader();
    Append(out, sections(functions));

    return out;
  }

}  // namespace polyir::wasm
