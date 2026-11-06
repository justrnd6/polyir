// Arithmetic, comparison, conversion and the string built-ins of the bytecode VM. The operand type picks integer vs float and signed vs
// unsigned; an integer result is wrapped to the result type's width.
//
// The semantics were matched to the Nina-written reference VM (removed 2026-10-28, see core/docs/language/vm-proposal.md); "the oracle"
// in the comments below means that reference behaviour, and runtime/tests/vm_vs_native.sh now holds the VM to the native build. The
// oracle emulated two's-complement arithmetic with bit lists because Nina has no bitwise operators; here the same values come out of
// native 128-bit arithmetic on the canonical payload (wrapping add/sub/mul, unsigned compare and divide, shift counts taken modulo the
// width), then the result is wrapped to its type's width. Every trap message is the oracle's, word for word.
#include "polyir/Std.hpp"
#include "vm/Machine.hpp"

#include <cmath>

namespace polyir::vm {

  namespace {

    using std::floor;

    // ── shapes ──

    bool isWhole(const Shape& s) {
      return s.kind == TypeKind::Int || s.kind == TypeKind::UInt;
    }

    bool isUnsignedShape(const Shape& s) {
      return s.kind == TypeKind::UInt;
    }

    bool isFloatShape(const Shape& s) {
      return s.kind == TypeKind::Float;
    }

    // A non-integer type counts as 64 bits wide, as the oracle measured widths.
    int widthOf(const Shape& s) {
      return isWhole(s) ? s.bits : 64;
    }

    // ── wrapping arithmetic (the host's signed overflow is undefined; unsigned wraps) ──

    UnsignedWide bitsOf(Wide v) {
      return static_cast<UnsignedWide>(v);
    }

    Wide fromBits(UnsignedWide v) {
      return static_cast<Wide>(v);
    }

    Wide wrappingAdd(Wide a, Wide b) {
      return fromBits(bitsOf(a) + bitsOf(b));
    }

    Wide wrappingSub(Wide a, Wide b) {
      return fromBits(bitsOf(a) - bitsOf(b));
    }

    Wide wrappingMul(Wide a, Wide b) {
      return fromBits(bitsOf(a) * bitsOf(b));
    }

    constexpr int WIDE_BITS = 128;

    constexpr double WIDE_LIMIT = 170141183460469231731687303715884105728.0;  // 2^127: the first float a signed 128-bit slot cannot hold

    constexpr double INT64_LIMIT = 9223372036854775808.0;  // 2^63: the first float an i64 cannot hold

    // ── structural comparison: strings, references, function values and sums ──

    // A value whose `==` is structural rather than numeric: everything that is not a number, a bool or void. A variant, an optional
    // and an either compare by case and contents, the way the LLVM emitter's eq_variant_* helpers do.
    bool hasStructuralEquality(const Value& v) {
      return v.kind != ValueKind::Int && v.kind != ValueKind::Float && v.kind != ValueKind::Bool && v.kind != ValueKind::Void;
    }

    bool isEqualityOrIdentity(BinOp op) {
      return op == BinOp::Identical || op == BinOp::NotIdentical || op == BinOp::Eq || op == BinOp::Ne;
    }

    // `===` on strings compares identities (addresses in the backends), never characters; a stamp of -1 is identical to nothing.
    bool sameStamp(const Value& a, const Value& b) {
      return b.kind == ValueKind::Str && a.text->stamp >= 0 && a.text->stamp == b.text->stamp;
    }

    bool identical(const Value& a, const Value& b) {
      return a.kind == ValueKind::Str ? sameStamp(a, b) : ShallowEq(a, b);
    }

    bool structuralCompare(Machine& m, BinOp op, const Value& a, const Value& b) {
      switch (op) {
        case BinOp::Identical:
          return identical(a, b);
        case BinOp::NotIdentical:
          return !identical(a, b);
        case BinOp::Eq:
          return DeepEq(m.heap, a, b);
        default:
          return !DeepEq(m.heap, a, b);
      }
    }

    // A struct with an `equals` method of its own decides what `==` and `!=` mean for it. The LLVM build's eq_<type> helper does
    // the same, calling `<Struct>_equals`. The answer is the function's index, or -1 when the value is not such a struct.
    int32_t equalsMethod(Machine& m, const Value& a) {
      const HeapObject* o = ObjectOf(m.heap, a);

      if (o == nullptr || o->kind != ObjectKind::Struct) {
        return -1;
      }

      const auto found = m.program->functionIndex.find(o->name + "_equals");

      return found == m.program->functionIndex.end() ? -1 : found->second;
    }

    Outcome byEqualsMethod(Machine& m, int32_t method, BinOp op, const Value& a, const Value& b) {
      Outcome out = CallFunction(m, method, {a, b});

      if (IsValue(out) && op == BinOp::Ne) {
        out.value = BoolValue(!Truthy(out.value));
      }

      return out;
    }

    bool isEquality(BinOp op) {
      return op == BinOp::Eq || op == BinOp::Ne;
    }

    // ── ordering ──

    // An ordering comparison (Eq and Ne are decided elsewhere). A template so the same four cases serve signed and unsigned
    // integers and doubles: `Number` is whichever of those the caller compares.
    template <typename Number>
    bool compared(BinOp op, Number a, Number b) {
      switch (op) {
        case BinOp::Lt:
          return a < b;
        case BinOp::Le:
          return a <= b;
        case BinOp::Gt:
          return a > b;
        default:
          return a >= b;
      }
    }

    // ── floats ──

    bool floatCompared(BinOp op, double a, double b) {
      if (op == BinOp::Eq) {
        return a == b;
      }

      if (op == BinOp::Ne) {
        return a != b;
      }

      return compared(op, a, b);
    }

    // A float result is always an f64 here, as the oracle made it.
    Outcome floatArith(BinOp op, double a, double b) {
      switch (op) {
        case BinOp::Add:
          return ValueOutcome(FloatValue(a + b, 64));
        case BinOp::Sub:
          return ValueOutcome(FloatValue(a - b, 64));
        case BinOp::Mul:
          return ValueOutcome(FloatValue(a * b, 64));
        case BinOp::Div: case BinOp::Rem:
          return ValueOutcome(FloatValue(op == BinOp::Div ? a / b : fmod(a, b), 64));  // frem = C's fmod
        default:
          return Trap(String("the operator ") + BinOpName(op) + " is not defined on floats");
      }
    }

    Outcome floats(BinOp op, double a, double b) {
      if (IsComparison(op)) {
        return ValueOutcome(BoolValue(floatCompared(op, a, b)));
      }

      return floatArith(op, a, b);
    }

    // ── integer tests (the operators whose result is a Bool) ──

    bool isIntTest(BinOp op) {
      return IsComparison(op) || op == BinOp::Identical || op == BinOp::NotIdentical || op == BinOp::LogAnd || op == BinOp::LogOr;
    }

    // An unsigned payload is zero-extended (and a U128's top bit is its own), so the unsigned reading of the bits orders it.
    bool intTest(BinOp op, Wide a, Wide b, bool unsigned_) {
      switch (op) {
        case BinOp::Eq: case BinOp::Identical:
          return a == b;
        case BinOp::Ne: case BinOp::NotIdentical:
          return a != b;
        case BinOp::LogAnd:
          return a != 0 && b != 0;
        case BinOp::LogOr:
          return a != 0 || b != 0;
        default:
          return unsigned_ ? compared(op, bitsOf(a), bitsOf(b)) : compared(op, a, b);
      }
    }

    // ── integer math ──

    // A shift count is taken modulo the operand's width, as every backend does (docs/safety-checks.md).
    Wide shiftCount(BinOp op, Wide b, const Shape& operand) {
      if (op != BinOp::Shl && op != BinOp::Shr) {
        return b;
      }

      const Wide width = widthOf(operand);

      return ((b % width) + width) % width;
    }

    // Bits fall off the top (the result's width cuts them off); a shift past the payload is zero.
    Wide shiftedLeft(Wide a, Wide n) {
      if (n <= 0) {
        return a;
      }

      if (n >= WIDE_BITS) {
        return 0;
      }

      return fromBits(bitsOf(a) << static_cast<int>(n));
    }

    // Logical for an unsigned operand (zero fills in), arithmetic for a signed one (the sign bit fills in).
    Wide shiftedRight(Wide a, Wide n, bool unsigned_) {
      if (n <= 0) {
        return a;
      }

      if (n >= WIDE_BITS) {
        return unsigned_ || a >= 0 ? 0 : -1;
      }

      return unsigned_ ? fromBits(bitsOf(a) >> static_cast<int>(n)) : a >> static_cast<int>(n);
    }

    Wide bitwise(BinOp op, Wide a, Wide b, bool unsigned_) {
      switch (op) {
        case BinOp::And:
          return a & b;
        case BinOp::Or:
          return a | b;
        case BinOp::Xor:
          return a ^ b;
        case BinOp::Shl:
          return shiftedLeft(a, b);
        default:
          return shiftedRight(a, b, unsigned_);
      }
    }

    // Empty = trap: division by zero. The signed minimum divided by -1 wraps (to itself, remainder 0), as the native build does.
    Optional<Wide> divided(BinOp op, Wide a, Wide b, bool unsigned_) {
      if (b == 0) {
        return nullopt;
      }

      if (unsigned_) {
        return fromBits(op == BinOp::Div ? bitsOf(a) / bitsOf(b) : bitsOf(a) % bitsOf(b));
      }

      if (b == -1) {
        return op == BinOp::Div ? wrappingSub(0, a) : 0;
      }

      return op == BinOp::Div ? a / b : a % b;
    }

    Optional<Wide> intMath(BinOp op, Wide a, Wide b, bool unsigned_) {
      switch (op) {
        case BinOp::Add:
          return wrappingAdd(a, b);
        case BinOp::Sub:
          return wrappingSub(a, b);
        case BinOp::Mul:
          return wrappingMul(a, b);
        case BinOp::Div: case BinOp::Rem:
          return divided(op, a, b, unsigned_);
        default:
          return bitwise(op, a, b, unsigned_);
      }
    }

    // The result wrapped to the result type's width; its signedness is the operand's, as in the oracle.
    Value fitted(Wide v, const Shape& result, bool unsigned_) {
      const int bits = widthOf(result);

      return IntValue(v, bits, !unsigned_);
    }

    Outcome ints(BinOp op, Wide a, Wide b, const Shape& operand, const Shape& result) {
      const bool unsigned_ = isUnsignedShape(operand);

      if (isIntTest(op)) {
        return ValueOutcome(BoolValue(intTest(op, a, b, unsigned_)));
      }

      const Optional<Wide> v = intMath(op, a, shiftCount(op, b, operand), unsigned_);

      if (!v) {
        return Trap("division by zero");
      }

      return ValueOutcome(fitted(*v, result, unsigned_));
    }

    // ── conversion ──

    // A magnitude as an i64 the way the oracle's float-to-Int conversion landed on arm64: past the range it saturates, NaN becomes 0.
    int64_t saturated(double magnitude) {
      if (!(magnitude >= 0.0)) {
        return 0;
      }

      if (magnitude >= INT64_LIMIT) {
        return INT64_MAX;
      }

      return static_cast<int64_t>(magnitude);
    }

    // Truncation toward zero, done on the magnitude so both signs floor the same way the oracle did.
    Wide truncated(double f) {
      if (f >= 0.0) {
        return saturated(floor(f));
      }

      return wrappingSub(0, saturated(floor(0.0 - f)));
    }

    // A float for a 128-bit slot: truncated exactly while it fits (2^127), as LLVM's fptosi/fptoui to i128 does; NaN becomes 0.
    Wide truncatedWide(double f) {
      if (!(f > -WIDE_LIMIT && f < WIDE_LIMIT)) {
        return 0;
      }

      return static_cast<Wide>(f);
    }

    // `v` as a whole number for a slot `bits` wide.
    Wide whole(const Value& v, int bits) {
      if (v.kind != ValueKind::Float) {
        return WideOf(v);
      }

      return bits > 64 ? truncatedWide(v.real) : truncated(v.real);
    }

    Value refit(const Value& v, bool same, const Shape& slot) {
      return same ? v : ConvertTo(v, slot);
    }

    // An integer landing in a typed slot: `i1` is a Bool slot whose values are already what it holds; the same width and signedness stay.
    Value fitInt(const Value& v, const Shape& slot) {
      switch (slot.kind) {
        case TypeKind::Int:
          return refit(v, slot.bits == 1 || (slot.bits == v.bits && v.isSigned), slot);
        case TypeKind::UInt:
          return refit(v, slot.bits == v.bits && !v.isSigned, slot);
        case TypeKind::Float:
          return ConvertTo(v, slot);
        default:
          return v;
      }
    }

    Value fitFloat(const Value& v, const Shape& slot) {
      return isWhole(slot) ? ConvertTo(v, slot) : v;
    }

    // ── text ──

    const Value& argAt(const Vector<Value>& args, size_t i) {
      static const Value missing;

      return i < args.size() ? args[i] : missing;
    }

    // The character with the byte's code, as UTF-8 (two bytes from 128 up, like every target).
    String charText(int64_t code) {
      const int64_t c = static_cast<int64_t>(WrapInt(code, 8, false));

      if (c < 128) {
        return String(1, static_cast<char>(c));
      }

      return String{static_cast<char>(192 + c / 64), static_cast<char>(128 + c % 64)};
    }

    // A bool is a word, a char (`i8`) its one-character string, any other number its decimal digits.
    String signedText(const Value& v, int bits) {
      if (bits == 1) {
        return Truthy(v) ? "true" : "false";
      }

      if (bits == 8) {
        return charText(IntOf(v));
      }

      return WideText(WideOf(v), true);
    }

    String unsignedText(const Value& v, int bits) {
      if (bits == 8) {
        return charText(IntOf(v));
      }

      return WideText(WideOf(v), false);
    }

    // A string keeps its identity; anything else becomes a new string.
    Outcome strOf(Machine& m, const Shape& receiver, const Value& v) {
      switch (receiver.kind) {
        case TypeKind::String:
          return ValueOutcome(v);
        case TypeKind::Int:
          return ValueOutcome(StrValue(m.heap, signedText(v, receiver.bits)));
        case TypeKind::UInt:
          return ValueOutcome(StrValue(m.heap, unsignedText(v, receiver.bits)));
        default:
          return Trap("str of a value that is not a whole number, bool, char or string");
      }
    }

    // `at` kept inside [0, len(s)].
    int64_t clamped(int64_t at, const String& s) {
      const int64_t n = static_cast<int64_t>(s.size());

      if (at < 0) {
        return 0;
      }

      return at > n ? n : at;
    }

    // The byte at `i` as a signed `i8`, 0 outside the string.
    Outcome charAt(const String& s, int64_t i) {
      if (i < 0 || i >= static_cast<int64_t>(s.size())) {
        return ValueOutcome(IntValue(0, 8, true));
      }

      const int64_t byte = static_cast<unsigned char>(s[static_cast<size_t>(i)]);

      return ValueOutcome(IntValue(byte, 8, true));
    }

    // `args` are the two ends, both clamped into the string.
    String sliced(const String& s, const Vector<Value>& args) {
      const int64_t start = clamped(IntOf(argAt(args, 0)), s);
      const int64_t end = clamped(IntOf(argAt(args, 1)), s);

      if (end <= start) {
        return "";
      }

      return s.substr(static_cast<size_t>(start), static_cast<size_t>(end - start));
    }

    // `args` is the text looked for, then where to start; -1 when absent.
    int64_t found(const String& s, const Vector<Value>& args) {
      const size_t from = static_cast<size_t>(clamped(IntOf(argAt(args, 1)), s));
      const size_t at = s.find(TextOf(argAt(args, 0)), from);

      return at == String::npos ? -1 : static_cast<int64_t>(at);
    }

    Outcome printed(Machine& m, const String& text) {
      *m.out << text << "\n";

      return VoidOutcome();
    }

    // The built-ins that take the receiver's text: a slice and a find make a fresh string / an i64, a print writes a line.
    Outcome onText(Machine& m, Builtin op, const String& s, const Vector<Value>& args) {
      switch (op) {
        case Builtin::Slice:
          return ValueOutcome(StrValue(m.heap, sliced(s, args)));
        case Builtin::Find:
          return ValueOutcome(I64Value(found(s, args)));
        case Builtin::Print:
          return printed(m, s);
        case Builtin::Fail:
          return Trap(s);
        default:
          return Trap(String("'") + BuiltinName(op) + "' is not a string builtin");
      }
    }

  }  // namespace

  Outcome BinaryOp(Machine& m, BinOp op, const Shape& operand, const Shape& result, const Value& left, const Value& right) {
    if (isEquality(op) && equalsMethod(m, left) >= 0) {
      return byEqualsMethod(m, equalsMethod(m, left), op, left, right);
    }

    if (hasStructuralEquality(left) && isEqualityOrIdentity(op)) {
      return ValueOutcome(BoolValue(structuralCompare(m, op, left, right)));
    }

    if (isFloatShape(operand)) {
      return floats(op, FloatOf(left), FloatOf(right));
    }

    return ints(op, WideOf(left), WideOf(right), operand, result);
  }

  // Any operator but LogNot on a float operand is a negation; BitNot flips every bit (xor with -1).
  Outcome UnaryOp(UnOp op, const Shape& operand, const Shape& result, const Value& v) {
    if (op == UnOp::LogNot) {
      return ValueOutcome(BoolValue(!Truthy(v)));
    }

    if (isFloatShape(operand)) {
      return ValueOutcome(FloatValue(0.0 - FloatOf(v), 64));
    }

    const Wide a = WideOf(v);
    const bool unsigned_ = isUnsignedShape(operand);

    if (op == UnOp::Neg) {
      return ValueOutcome(fitted(wrappingSub(0, a), result, unsigned_));
    }

    return ValueOutcome(fitted(a ^ -1, result, unsigned_));
  }

  Value ConvertTo(const Value& v, const Shape& to) {
    switch (to.kind) {
      case TypeKind::Int:
        return IntValue(whole(v, to.bits), to.bits, true);
      case TypeKind::UInt:
        return IntValue(whole(v, to.bits), to.bits, false);
      case TypeKind::Float:
        return FloatValue(FloatOf(v), to.bits);
      default:
        return v;
    }
  }

  Value FitTo(const Value& v, const Shape& slot) {
    if (v.kind == ValueKind::Int) {
      return fitInt(v, slot);
    }

    if (v.kind == ValueKind::Float) {
      return fitFloat(v, slot);
    }

    return v;
  }

  // The low `bits` bits of `v`, sign-extended for a signed slot and zero-extended for an unsigned one (the canonical form).
  Wide WrapInt(Wide v, int bits, bool isSigned) {
    if (bits >= WIDE_BITS) {
      return v;
    }

    if (bits <= 0) {
      return 0;
    }

    const UnsignedWide mask = (UnsignedWide{1} << bits) - 1;
    const UnsignedWide low = bitsOf(v) & mask;
    const bool topSet = ((low >> (bits - 1)) & 1) != 0;

    return fromBits(topSet && isSigned ? low | ~mask : low);
  }

  Outcome TextBuiltin(Machine& m, Builtin op, const Shape& receiver, const Value& v, const Vector<Value>& args) {
    if (op == Builtin::StrOf) {
      return strOf(m, receiver, v);
    }

    if (op == Builtin::CharAt) {
      return charAt(TextOf(v), IntOf(argAt(args, 0)));
    }

    return onText(m, op, TextOf(v), args);
  }

  // djb2 over the bytes, wrapping at 64 bits: the hash the LLVM backend's polyir_hash_string gives a string.
  int64_t HashText(const String& text) {
    uint64_t h = 5381;

    for (const char c : text) {
      h = h * 33 + static_cast<unsigned char>(c);
    }

    return fromBits(h);
  }

}  // namespace polyir::vm
