// The SPIR-V module and function under construction (SpirvModule.nina, SpirvFn.nina): declarations made once per key, constants as words, the
// header and the piles joined in the order the format requires, and the blocks of one function.
#include "gpu/SpirvModule.hpp"

#include <cmath>

namespace polyir {

  namespace {

    constexpr int64_t kWordRange = 4294967296LL;  // 2^32
    constexpr int kDecorationBlock = 2;
    constexpr int kDecorationBufferBlock = 3;
    constexpr int kDecorationArrayStride = 6;
    constexpr int kDecorationNonWritable = 24;
    constexpr int kDecorationOffset = 35;

    // The pieces of a 32-bit IEEE-754 float: a sign bit, 8 exponent bits (biased by 127) and 23 mantissa bits.
    constexpr int64_t kSignBit = 2147483648LL;  // 2^31
    constexpr int64_t kInfinityBits = 2139095040LL;  // 0x7f800000: every exponent bit set, no mantissa
    constexpr int64_t kMantissaScale = 8388608LL;  // 2^23: one step of the exponent field
    constexpr int kExponentBias = 127;
    constexpr int kMaxBiasedExponent = 255;  // the exponent field of an infinity

    // The loop of exponentOf always takes this many steps (as the Nina writer's does): more than any float's exponent needs.
    constexpr int kExponentSteps = 200;

    void halveIfTwoOrMore(double& scaled, int& exponent) {
      if (scaled >= 2.0) {
        scaled /= 2.0;
        exponent++;
      }
    }

    void doubleIfBelowOne(double& scaled, int& exponent) {
      if (scaled < 1.0) {
        scaled *= 2.0;
        exponent--;
      }
    }

    // The exponent with 2^exponent <= magnitude < 2^(exponent+1), found by halving and doubling as the Nina writer does: each step
    // moves `scaled` (magnitude / 2^exponent) toward [1, 2).
    int exponentOf(double magnitude) {
      int exponent = 0;
      double scaled = magnitude;

      for (int step = 0; step < kExponentSteps; step++) {
        halveIfTwoOrMore(scaled, exponent);
        doubleIfBelowOne(scaled, exponent);
      }

      return exponent;
    }

    int64_t packFloat(int64_t sign, int64_t biased, int64_t mantissa) {
      if (biased <= 0) {
        return sign * kSignBit;  // too small for a normal float: a zero of that sign
      }

      if (biased >= kMaxBiasedExponent) {
        return sign * kSignBit + kInfinityBits;  // too big: an infinity of that sign
      }

      return sign * kSignBit + biased * kMantissaScale + mantissa;
    }

    // The 23 bits after the leading 1 of magnitude / 2^exponent, rounded to the nearest.
    int64_t mantissaOf(double magnitude, int exponent) {
      return static_cast<int64_t>(floor((magnitude / pow(2.0, exponent) - 1.0) * kMantissaScale + 0.5));
    }

    // 32 bits: one word (two's complement when negative). 64 bits: the low word, then the high word.
    Words intWords(int bits, int64_t value) {
      const uint64_t raw = static_cast<uint64_t>(value);

      if (bits == 64) {
        return {static_cast<int64_t>(raw & 0xffffffffULL), static_cast<int64_t>(raw >> 32)};
      }

      return {value < 0 ? value + kWordRange : value};
    }

    String joinedIds(const Words& ids) {
      String out;

      for (size_t i = 0; i < ids.size(); i++) {
        out += (i > 0 ? "," : "") + to_string(ids[i]);
      }

      return out;
    }

    // A block still open at the end of a function is closed: with a plain return in a function with no result, else as unreachable.
    void closeLastBlock(SpvFunction& function, bool isVoid) {
      if (!IsOpen(function)) {
        return;
      }

      if (isVoid) {
        ReturnNothing(function);
      } else {
        Effect(function, OpUnreachable, {});
      }
    }

  }  // namespace

  // The first word holds the instruction's length in words in its high 16 bits and the opcode in its low 16.
  Words SpvInstruction(int opcode, const Words& operands) {
    Words out{static_cast<int64_t>(operands.size() + 1) * 65536 + opcode};
    out.insert(out.end(), operands.begin(), operands.end());

    return out;
  }

  void SpvAppend(Words& pile, const Words& words) {
    pile.insert(pile.end(), words.begin(), words.end());
  }

  // The text as words, four bytes to a word with the first byte lowest, ending in at least one zero byte.
  Words SpvString(const String& text) {
    Words words(text.size() / 4 + 1, 0);

    for (size_t i = 0; i < text.size(); i++) {
      words[i / 4] += static_cast<int64_t>(static_cast<unsigned char>(text[i])) << (8 * (i % 4));
    }

    return words;
  }

  // The bits of `x` as a 32-bit float, the mantissa rounded to the nearest. A mantissa that rounds up to 2^23 carries into the
  // exponent: the next power of two, with no mantissa bits.
  int64_t SpvFloatBits(double x) {
    if (x == 0.0) {
      return 0;
    }

    const double magnitude = x < 0.0 ? -x : x;
    const int exponent = exponentOf(magnitude);
    const int64_t mantissa = mantissaOf(magnitude, exponent);
    const int64_t sign = x < 0.0 ? 1 : 0;

    if (mantissa >= kMantissaScale) {
      return packFloat(sign, exponent + 1 + kExponentBias, 0);
    }

    return packFloat(sign, exponent + kExponentBias, mantissa);
  }

  Words Concat(Words head, const Words& tail) {
    head.insert(head.end(), tail.begin(), tail.end());

    return head;
  }

  Id Known(const SpvModule& m, const String& key) {
    const auto found = m.idsByKey.find(key);

    return found == m.idsByKey.end() ? 0 : found->second;
  }

  // A type (its result ID first among the operands), declared once per key.
  Id DeclareType(SpvModule& m, const String& key, int opcode, const Words& operands) {
    if (const Id found = Known(m, key)) {
      return found;
    }

    const Id id = Fresh(m);
    SpvAppend(m.declarations, SpvInstruction(opcode, Concat({id}, operands)));

    return m.idsByKey[key] = id;
  }

  // A constant or a variable (result type, result ID, operands), declared once per key.
  Id DeclareValue(SpvModule& m, const String& key, int opcode, Id type, const Words& operands) {
    if (const Id found = Known(m, key)) {
      return found;
    }

    const Id id = Fresh(m);
    SpvAppend(m.declarations, SpvInstruction(opcode, Concat({type, id}, operands)));

    return m.idsByKey[key] = id;
  }

  // A signed integer of 32 or 64 bits (64 needs the Int64 capability).
  Id TypeInt(SpvModule& m, int bits) {
    if (bits == 64) {
      m.int64 = true;
    }

    return DeclareType(m, "int:" + to_string(bits), OpTypeInt, {bits, 1});
  }

  Id TypeVector(SpvModule& m, Id element, int count) {
    return DeclareType(m, "vec:" + to_string(element) + ":" + to_string(count), OpTypeVector, {element, count});
  }

  Id TypePointer(SpvModule& m, int storage, Id pointee) {
    return DeclareType(m, "ptr:" + to_string(storage) + ":" + to_string(pointee), OpTypePointer, {storage, pointee});
  }

  Id TypeRuntimeArray(SpvModule& m, Id element, int stride) {
    const String key = "rt:" + to_string(element);

    if (const Id found = Known(m, key)) {
      return found;
    }

    const Id id = DeclareType(m, key, OpTypeRuntimeArray, {element});
    Decorate(m, id, kDecorationArrayStride, {stride});

    return id;
  }

  Id TypeArray(SpvModule& m, Id element, Id length) {
    return DeclareType(m, "array:" + to_string(element) + ":" + to_string(length), OpTypeArray, {element, length});
  }

  Id TypeFunction(SpvModule& m, Id returns, const Words& params) {
    return DeclareType(m, "fn:" + to_string(returns) + ":" + joinedIds(params), OpTypeFunction, Concat({returns}, params));
  }

  // A struct of one runtime array laid out as a storage buffer: `Offset 0`, `BufferBlock`, and `NonWritable` when read-only.
  Id TypeBuffer(SpvModule& m, Id array, bool readOnly) {
    const String key = "buffer:" + to_string(array) + ":" + (readOnly ? "true" : "false");

    if (const Id found = Known(m, key)) {
      return found;
    }

    const Id id = DeclareType(m, key, OpTypeStruct, {array});
    Decorate(m, id, kDecorationBufferBlock, {});
    DecorateMember(m, id, 0, kDecorationOffset, {0});

    if (readOnly) {
      DecorateMember(m, id, 0, kDecorationNonWritable, {});
    }

    return id;
  }

  // The push-constant block: a struct of 32-bit members at 4-byte steps, marked `Block`.
  Id TypePushBlock(SpvModule& m, const Words& members) {
    const Id id = DeclareType(m, "push:" + joinedIds(members), OpTypeStruct, members);
    Decorate(m, id, kDecorationBlock, {});

    for (size_t i = 0; i < members.size(); i++) {
      DecorateMember(m, id, static_cast<int>(i), kDecorationOffset, {static_cast<int64_t>(4 * i)});
    }

    return id;
  }

  Id ConstBool(SpvModule& m, bool value) {
    const Id type = TypeBool(m);

    return value ? DeclareValue(m, "true", OpConstantTrue, type, {}) : DeclareValue(m, "false", OpConstantFalse, type, {});
  }

  Id ConstInt(SpvModule& m, int bits, int64_t value) {
    const Id type = TypeInt(m, bits);

    return DeclareValue(m, "ci:" + to_string(bits) + ":" + to_string(value), OpConstant, type, intWords(bits, value));
  }

  Id ConstUint(SpvModule& m, int64_t value) {
    const Id type = TypeUint(m);

    return DeclareValue(m, "cu:" + to_string(value), OpConstant, type, {value});
  }

  Id ConstFloat(SpvModule& m, double value) {
    const Id type = TypeFloat(m);

    return DeclareValue(m, "cf:" + to_string(SpvFloatBits(value)), OpConstant, type, {SpvFloatBits(value)});
  }

  void DecorateMember(SpvModule& m, Id target, int member, int decoration, const Words& operands) {
    SpvAppend(m.decorations, SpvInstruction(OpMemberDecorate, Concat({target, member, decoration}, operands)));
  }

  // `OpName`: a readable name for an ID, shown by `spirv-dis`; it changes nothing about what runs.
  void Name(SpvModule& m, Id target, const String& text) {
    SpvAppend(m.names, SpvInstruction(OpName, Concat({target}, SpvString(text))));
  }

  // The header (magic, version 1.0, generator 0, the ID bound, 0) and the capabilities (Shader; Int64 when used) come first. Then the imports,
  // the memory model (logical, GLSL450), the entry points and their execution modes. Then the names, decorations, declarations and functions.
  Words ModuleWords(const SpvModule& m, const Words& execution) {
    Words out{119734787, 65536, 0, m.nextId, 0};
    SpvAppend(out, SpvInstruction(OpCapability, {1}));

    if (m.int64) {
      SpvAppend(out, SpvInstruction(OpCapability, {11}));
    }

    SpvAppend(out, m.imports);
    SpvAppend(out, SpvInstruction(OpMemoryModel, {0, 1}));

    for (const Words* pile : {&m.entry, &execution, &m.names, &m.decorations, &m.declarations, &m.functions}) {
      SpvAppend(out, *pile);
    }

    return out;
  }

  // ── one function ──

  SpvFunction MakeSpvFunction(SpvModule& m) {
    const Id entry = Fresh(m);

    return SpvFunction{m, entry, entry, {}, {}};
  }

  void Begin(SpvFunction& function, Id label) {
    SpvAppend(function.body, SpvInstruction(OpLabel, {label}));
    function.openLabel = label;
  }

  // An instruction with no result; a block is started first when the last one was closed.
  void Effect(SpvFunction& function, int opcode, const Words& operands) {
    if (function.openLabel == 0) {
      Begin(function, Fresh(function.module));
    }

    SpvAppend(function.body, SpvInstruction(opcode, operands));
  }

  // An instruction that makes a value: result type, a new result ID, operands. Returns the ID.
  Id ValueOf(SpvFunction& function, int opcode, Id type, const Words& operands) {
    if (function.openLabel == 0) {
      Begin(function, Fresh(function.module));
    }

    const Id id = Fresh(function.module);
    SpvAppend(function.body, SpvInstruction(opcode, Concat({type, id}, operands)));

    return id;
  }

  void Close(SpvFunction& function, int opcode, const Words& operands) {
    Effect(function, opcode, operands);
    function.openLabel = 0;
  }

  // A local variable (Function storage class); returns the ID of the pointer to it.
  Id Variable(SpvFunction& function, Id type) {
    const Id pointer = TypePointer(function.module, kStorageFunction, type);
    const Id id = Fresh(function.module);
    SpvAppend(function.variables, SpvInstruction(OpVariable, {pointer, id, kStorageFunction}));

    return id;
  }

  // Writes the function into the module: header, parameters (`params` are type and ID pairs), the entry block with the variables first, the rest. A block
  // still open at the end is unreachable (`OpUnreachable`), or for a function with no result a plain return.
  void Finish(SpvFunction& function, Id functionId, Id returns, Id functionType, const Words& params, bool isVoid) {
    closeLastBlock(function, isVoid);
    Words& out = function.module.functions;
    SpvAppend(out, SpvInstruction(OpFunction, {returns, functionId, 0, functionType}));

    for (size_t i = 0; i + 1 < params.size(); i += 2) {
      SpvAppend(out, SpvInstruction(OpFunctionParameter, {params[i], params[i + 1]}));
    }

    SpvAppend(out, Concat(SpvInstruction(OpLabel, {function.entryLabel}), function.variables));
    SpvAppend(out, Concat(function.body, SpvInstruction(OpFunctionEnd, {})));
  }

}  // namespace polyir
