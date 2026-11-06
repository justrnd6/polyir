// Native memory: the bytes a `ptr<T>` points at when the pointer came from C (`malloc`, `calloc`, a library an `import native` names)
// rather than from the VM's own heap. The VM holds such a pointer as a plain whole number, the address C returned, so the garbage
// collector (Collector.cpp) sees only an integer and never follows it. Nothing here ever puts a heap reference into that memory: only
// numbers and addresses are stored there, the values C itself can read.
//
// `p[i]` reads or writes the element at address + i × size(T) with T's width, the getelementptr + load/store the LLVM build emits
// (emit/llvm/LlvmLists.cpp, PointerAddress). Like native code the VM trusts the address: C handed it out and the program owns what it
// points at, so neither the address nor the index is checked; only a null address traps, where native code would crash. The atomics
// (`atomicLoad`, `atomicStore`, `atomicRmw`, `cmpxchg`, `fence`) work on the same memory with the host's atomic instructions, returning
// what emit/llvm/LlvmAtomics.cpp returns: the old value for `atomicRmw` and `cmpxchg`.
//
// What native memory holds is what C has: whole numbers (an i1 as one byte, 0 or 1), f32 (four bytes, read back into the VM's double),
// f64, and pointers (eight bytes). The host is little-endian (arm64, x86-64), so a value's low bytes come first.
#include "polyir/Std.hpp"
#include "vm/Machine.hpp"

#include <bit>
#include <cmath>

namespace polyir::vm {

  namespace {

    using std::bit_cast;
    using std::fmax;
    using std::fmin;

    // ── the shape of an element ──

    // How many bytes a load or store of T touches; 0 when T is not something native memory holds.
    int storeBytes(const Type& t) {
      if (t.kind == TypeKind::Int || t.kind == TypeKind::UInt) {
        return (t.bits + 7) / 8;
      }

      if (t.kind == TypeKind::Float) {
        return t.bits == 32 || t.bits == 64 ? t.bits / 8 : 0;
      }

      return t.kind == TypeKind::Pointer ? 8 : 0;
    }

    // How far apart the elements sit: LLVM's allocation size, the store size rounded up to a power of two (an i24 takes 4 bytes).
    int strideOf(int bytes) {
      int stride = 1;

      while (stride < bytes) {
        stride *= 2;
      }

      return stride;
    }

    // The bits of T a value keeps: a pointer is a 64-bit address.
    int widthOf(const Type& t) {
      return t.kind == TypeKind::Pointer ? 64 : t.bits;
    }

    const Type& elementOf(const Shape& pointer) {
      return *pointer.type->element;
    }

    Outcome notNative(const Type& t) {
      return Trap("native memory holds numbers and pointers; the VM cannot keep a " + TypeText(t) + " there");
    }

    Outcome nullAddress() {
      return Trap("reading or writing through a null pointer");
    }

    // ── bits and values ──

    // The low `bits` bits, zero-extended: what a store of an iN writes (an i1 true is the byte 1).
    UnsignedWide masked(UnsignedWide raw, int bits) {
      if (bits >= 128) {
        return raw;
      }

      return raw & ((UnsignedWide{1} << bits) - 1);
    }

    UnsignedWide floatBits(double v, int bits) {
      if (bits == 32) {
        return bit_cast<uint32_t>(static_cast<float>(v));
      }

      return bit_cast<uint64_t>(v);
    }

    double floatFrom(UnsignedWide raw, int bits) {
      if (bits == 32) {
        return bit_cast<float>(static_cast<uint32_t>(raw));
      }

      return bit_cast<double>(static_cast<uint64_t>(raw));
    }

    // The value the bits of a T stand for, as the VM carries it: a pointer is its address, an unsigned 64-bit number.
    Value decoded(UnsignedWide raw, const Type& t) {
      switch (t.kind) {
        case TypeKind::Int:
          return t.bits == 1 ? BoolValue((raw & 1) != 0) : IntValue(static_cast<Wide>(raw), t.bits, true);
        case TypeKind::UInt:
          return IntValue(static_cast<Wide>(raw), t.bits, false);
        case TypeKind::Float:
          return FloatValue(floatFrom(raw, t.bits), t.bits);
        default:
          return IntValue(static_cast<Wide>(raw), 64, false);
      }
    }

    bool isNumber(const Value& v) {
      return v.kind == ValueKind::Int || v.kind == ValueKind::Float || v.kind == ValueKind::Bool;
    }

    // The bits a value is stored as; none for a string or a heap object, which have no address C could use.
    Optional<UnsignedWide> encoded(const Value& v, const Type& t) {
      if (!isNumber(v)) {
        return nullopt;
      }

      if (t.kind == TypeKind::Float) {
        return floatBits(FloatOf(v), t.bits);
      }

      if (v.kind == ValueKind::Bool) {
        return UnsignedWide{v.flag ? 1u : 0u};
      }

      return masked(static_cast<UnsignedWide>(WideOf(v)), widthOf(t));
    }

    // ── plain loads and stores ──

    bool isNull(const Value& address) {
      return IntOf(address) == 0;
    }

    unsigned char* addressOf(const Value& address) {
      return reinterpret_cast<unsigned char*>(static_cast<uintptr_t>(IntOf(address)));
    }

    // Element `index` of the block at `address`; a negative index steps back, as getelementptr's signed offset does.
    unsigned char* elementAt(const Value& address, const Value& index, int stride) {
      const auto offset = static_cast<uintptr_t>(IntOf(index)) * static_cast<uintptr_t>(stride);

      return addressOf(address) + offset;
    }

    UnsignedWide loaded(const unsigned char* at, int bytes) {
      UnsignedWide raw = 0;
      memcpy(&raw, at, static_cast<size_t>(bytes));

      return raw;
    }

    void store(unsigned char* at, int bytes, UnsignedWide raw) {
      memcpy(at, &raw, static_cast<size_t>(bytes));
    }

    Outcome readElement(unsigned char* at, const Type& t) {
      return ValueOutcome(decoded(loaded(at, storeBytes(t)), t));
    }

    Outcome writeElement(unsigned char* at, const Type& t, const Value& v) {
      const Optional<UnsignedWide> raw = encoded(v, t);

      if (!raw) {
        return Trap("only numbers and pointers can be stored in native memory");
      }

      store(at, storeBytes(t), *raw);

      return VoidOutcome();
    }

    // ── atomic loads and exchanges, one width at a time ──
    // `Word` is the unsigned integer of the element's width (uint8_t to uint64_t): the compiler's `__atomic_*` builtins (GCC and
    // Clang) act on exactly that many bytes, here always sequentially consistent (__ATOMIC_SEQ_CST). Templates only so that one
    // function serves the four widths.

    template <typename Word>
    UnsignedWide atomicLoadOf(void* at) {
      return __atomic_load_n(static_cast<Word*>(at), __ATOMIC_SEQ_CST);
    }

    // A strong compare-and-swap: on success `expected` keeps the old bits, on failure it becomes the bits found.
    template <typename Word>
    bool exchangeOf(void* at, UnsignedWide& expected, UnsignedWide desired) {
      Word seen = static_cast<Word>(expected);
      const bool isWeak = false;
      const bool swapped = __atomic_compare_exchange_n(static_cast<Word*>(at), &seen, static_cast<Word>(desired), isWeak, __ATOMIC_SEQ_CST,
                                                       __ATOMIC_SEQ_CST);
      expected = seen;

      return swapped;
    }

    bool isAtomicWidth(int bytes) {
      return bytes == 1 || bytes == 2 || bytes == 4 || bytes == 8;
    }

    UnsignedWide atomicLoad(void* at, int bytes) {
      switch (bytes) {
        case 1:
          return atomicLoadOf<uint8_t>(at);
        case 2:
          return atomicLoadOf<uint16_t>(at);
        case 4:
          return atomicLoadOf<uint32_t>(at);
        default:
          return atomicLoadOf<uint64_t>(at);
      }
    }

    bool exchanged(void* at, int bytes, UnsignedWide& expected, UnsignedWide desired) {
      switch (bytes) {
        case 1:
          return exchangeOf<uint8_t>(at, expected, desired);
        case 2:
          return exchangeOf<uint16_t>(at, expected, desired);
        case 4:
          return exchangeOf<uint32_t>(at, expected, desired);
        default:
          return exchangeOf<uint64_t>(at, expected, desired);
      }
    }

    // A read-modify-write: the new bits made from the old, retried until no other thread wrote in between. Returns the old bits.
    // Every ordering the program names is served as sequentially consistent, the strongest, which is allowed wherever a weaker one is.
    UnsignedWide updated(void* at, int bytes, const Function<UnsignedWide(UnsignedWide)>& change) {
      UnsignedWide old = atomicLoad(at, bytes);

      while (!exchanged(at, bytes, old, change(old))) {
        continue;
      }

      return old;
    }

    // ── what atomicRmw computes ──

    bool isIntegerRmw(const String& op) {
      static const Vector<String> ops = {"xchg", "add", "sub", "and", "nand", "or", "xor", "max", "min", "umax", "umin"};

      return find(ops.begin(), ops.end(), op) != ops.end();
    }

    bool isFloatRmw(const String& op) {
      return op == "xchg" || op == "fadd" || op == "fsub" || op == "fmax" || op == "fmin";
    }

    bool knownRmw(const String& op, const Type& t) {
      return t.kind == TypeKind::Float ? isFloatRmw(op) : isIntegerRmw(op);
    }

    // `add`, `sub`, `and`, `nand`, `or`, `xor`, `xchg`: the same on the bits whatever the signedness (two's complement).
    UnsignedWide bitwise(const String& op, UnsignedWide old, UnsignedWide v) {
      if (op == "add") {
        return old + v;
      }

      if (op == "sub") {
        return old - v;
      }

      if (op == "and") {
        return old & v;
      }

      if (op == "nand") {
        return ~(old & v);
      }

      if (op == "or") {
        return old | v;
      }

      return op == "xor" ? old ^ v : v;
    }

    Wide signedOf(UnsignedWide raw, int bits) {
      return WrapInt(static_cast<Wide>(raw), bits, true);
    }

    // `max`, `min` compare as signed numbers, `umax`, `umin` as unsigned ones; the rest are bitwise.
    UnsignedWide integerRmw(const String& op, UnsignedWide old, UnsignedWide v, int bits) {
      if (op == "max") {
        return signedOf(old, bits) < signedOf(v, bits) ? v : old;
      }

      if (op == "min") {
        return signedOf(v, bits) < signedOf(old, bits) ? v : old;
      }

      if (op == "umax") {
        return masked(old, bits) < masked(v, bits) ? v : old;
      }

      if (op == "umin") {
        return masked(v, bits) < masked(old, bits) ? v : old;
      }

      return bitwise(op, old, v);
    }

    double floatOp(const String& op, double a, double b) {
      if (op == "fadd") {
        return a + b;
      }

      if (op == "fsub") {
        return a - b;
      }

      if (op == "fmax") {
        return fmax(a, b);
      }

      return op == "fmin" ? fmin(a, b) : b;
    }

    UnsignedWide combined(const String& op, const Type& t, UnsignedWide old, UnsignedWide v) {
      if (t.kind != TypeKind::Float) {
        return masked(integerRmw(op, old, v, widthOf(t)), widthOf(t));
      }

      return floatBits(floatOp(op, floatFrom(old, t.bits), floatFrom(v, t.bits)), t.bits);
    }

    // ── the atomic builtins ──

    Outcome atomicLoadOp(void* at, const Type& t) {
      return ValueOutcome(decoded(atomicLoad(at, storeBytes(t)), t));
    }

    Outcome atomicStoreOp(void* at, const Type& t, const Value& v) {
      const Optional<UnsignedWide> raw = encoded(v, t);

      if (!raw) {
        return Trap("only numbers and pointers can be stored in native memory");
      }

      updated(at, storeBytes(t), [&](UnsignedWide) { return *raw; });

      return VoidOutcome();
    }

    Outcome atomicRmwOp(void* at, const Type& t, const String& op, const Value& v) {
      const Optional<UnsignedWide> operand = encoded(v, t);

      if (!knownRmw(op, t) || !operand) {
        return Trap("atomicRmw: '" + op + "' is not an operation on " + TypeText(t));
      }

      const UnsignedWide old = updated(at, storeBytes(t), [&](UnsignedWide was) { return combined(op, t, was, *operand); });

      return ValueOutcome(decoded(old, t));
    }

    // The strong form: the result is the old value, so success is `result == expected`.
    Outcome cmpxchgOp(void* at, const Type& t, const Value& expected, const Value& replacement) {
      Optional<UnsignedWide> seen = encoded(expected, t);
      const Optional<UnsignedWide> desired = encoded(replacement, t);

      if (!seen || !desired) {
        return Trap("cmpxchg compares and stores numbers and pointers only");
      }

      exchanged(at, storeBytes(t), *seen, *desired);

      return ValueOutcome(decoded(*seen, t));
    }

    Outcome atomicOn(void* at, Builtin op, const Type& t, const Vector<Value>& args) {
      switch (op) {
        case Builtin::AtomicLoad:
          return atomicLoadOp(at, t);
        case Builtin::AtomicStore:
          return atomicStoreOp(at, t, ArgumentAt(args, 0));
        case Builtin::AtomicRmw:
          return atomicRmwOp(at, t, TextOf(ArgumentAt(args, 0)), ArgumentAt(args, 1));
        default:
          return cmpxchgOp(at, t, ArgumentAt(args, 0), ArgumentAt(args, 1));
      }
    }

    // The fence orders memory around it; its value is the 0 the LLVM build gives it.
    Outcome fenced() {
      __atomic_thread_fence(__ATOMIC_SEQ_CST);

      return ValueOutcome(I64Value(0));
    }

    Outcome atomicThrough(Builtin op, const Shape& pointer, const Value& address, const Vector<Value>& args) {
      if (!IsNativeAddress(pointer, address)) {
        return Trap(String(BuiltinName(op)) + " needs a raw pointer from C (malloc, calloc, a library)");
      }

      const Type& t = elementOf(pointer);

      if (!isAtomicWidth(storeBytes(t))) {
        return Trap(String(BuiltinName(op)) + " on a " + TypeText(t) + " is not supported by the interpreter");
      }

      if (isNull(address)) {
        return nullAddress();
      }

      return atomicOn(addressOf(address), op, t, args);
    }

  }  // namespace

  bool IsNativeAddress(const Shape& base, const Value& container) {
    const bool isPointer = base.type != nullptr && base.type->kind == TypeKind::Pointer && base.type->element != nullptr;

    return isPointer && container.kind == ValueKind::Int;
  }

  Outcome ReadNative(const Value& address, const Vector<Value>& indices, const Shape& base) {
    const Type& t = elementOf(base);

    if (storeBytes(t) == 0) {
      return notNative(t);
    }

    if (isNull(address)) {
      return nullAddress();
    }

    return readElement(elementAt(address, ArgumentAt(indices, 0), strideOf(storeBytes(t))), t);
  }

  Outcome WriteNative(const Value& address, const Vector<Value>& indices, const Shape& base, const Value& v) {
    const Type& t = elementOf(base);

    if (storeBytes(t) == 0) {
      return notNative(t);
    }

    if (isNull(address)) {
      return nullAddress();
    }

    return writeElement(elementAt(address, ArgumentAt(indices, 0), strideOf(storeBytes(t))), t, v);
  }

  bool IsAtomicOp(Builtin op) {
    return op == Builtin::AtomicLoad || op == Builtin::AtomicStore || op == Builtin::AtomicRmw || op == Builtin::AtomicCmpxchg || op == Builtin::AtomicFence;
  }

  Outcome AtomicOp(Builtin op, const Shape& pointer, const Value& address, const Vector<Value>& args) {
    if (op == Builtin::AtomicFence) {
      return fenced();
    }

    return atomicThrough(op, pointer, address, args);
  }

}  // namespace polyir::vm
