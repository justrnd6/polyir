// How threads cooperate, and the float remainder, as SPIR-V instructions (SpirvAtomic.nina, SpirvRemainder.nina): atomic updates of one slot of a
// `writable` array, the workgroup functions and the barrier; and C's `fmod` computed without trusting the GPU's division.
#include "gpu/KernelCommon.hpp"
#include "gpu/SpirvLower.hpp"

namespace polyir {

  namespace {

    constexpr int kGlslFAbs = 4;
    constexpr int kGlslFloor = 8;
    constexpr int64_t kScopeWorkgroup = 2;
    constexpr int64_t kAcquireReleaseWorkgroupMemory = 264;  // AcquireRelease (8) + WorkgroupMemory (256)

    // The instruction of `atomicAdd`, `atomicMin` (signed) or `atomicMax` (signed).
    int atomicCode(const String& name) {
      if (name == "atomicAdd") {
        return OpAtomicIAdd;
      }

      if (name == "atomicMin") {
        return OpAtomicSMin;
      }

      return OpAtomicSMax;
    }

  }  // namespace

  // `atomicAdd`, `atomicMin`, `atomicMax` (whole numbers) and `atomicAddFloat`: `array[slot] op= value`, giving the old value.
  Id Atomic(SpvBody& body, const SpvScope& scope, const Expr& e) {
    const SpvArray* array = ArrayOf(body, *e.operands[0]);

    if (array == nullptr) {
      return Unsupported(body, "an atomic update needs a writable array parameter");
    }

    const Id slot = ExprTo(body, scope, *e.operands[1], SpvInt32());
    const Id pointer = SlotPointer(body, *array, slot);

    if (e.name == "atomicAddFloat") {
      return AddFloat(body, pointer, ExprTo(body, scope, *e.operands[2], SpvFloat32()));
    }

    return IntegerUpdate(body, e.name, pointer, ExprTo(body, scope, *e.operands[2], SpvInt32()));
  }

  // On the whole device (scope 1) with no ordering needed (0).
  Id IntegerUpdate(SpvBody& body, const String& name, Id pointer, Id amount) {
    const int code = atomicCode(name);
    const Id type = TypeInt(body.module, 32), scopeId = Device(body), order = Relaxed(body);

    return ValueOf(body.function, code, type, {pointer, scopeId, order, amount});
  }

  // A float is added with a compare-and-swap loop on the slot's raw bits. Read them, work out the new bits, and swap them in only if the slot
  // still holds what was read. Otherwise, try again with what is there now. `seen` is the loop's variable, the last value read.
  Id AddFloat(SpvBody& body, Id pointer, Id amount) {
    const Id word = TypeUint(body.module), floatType = TypeFloat(body.module), seen = Variable(body.function, word);
    const Id scopeId = Device(body), order = Relaxed(body);
    Store(body.function, seen, ValueOf(body.function, OpAtomicLoad, word, {pointer, scopeId, order}));
    SwapLoop(body, pointer, amount, seen);

    return ValueOf(body.function, OpBitcast, floatType, {Load(body.function, word, seen)});
  }

  void SwapLoop(SpvBody& body, Id pointer, Id amount, Id seen) {
    const Id header = Fresh(body.module), attempt = Fresh(body.module), step = Fresh(body.module), merge = Fresh(body.module);
    LoopHeader(body.function, header, merge, step, attempt);
    TrySwap(body, pointer, amount, seen, merge, step);

    Begin(body.function, step);
    Branch(body.function, header);
    Begin(body.function, merge);
  }

  // One attempt: new bits from the value last seen, a compare-and-swap, and out of the loop on success.
  void TrySwap(SpvBody& body, Id pointer, Id amount, Id seen, Id merge, Id step) {
    const Id word = TypeUint(body.module), floatType = TypeFloat(body.module), before = Load(body.function, word, seen);
    const Id sum = ValueOf(body.function, OpFAdd, floatType, {ValueOf(body.function, OpBitcast, floatType, {before}), amount});
    const Id wanted = ValueOf(body.function, OpBitcast, word, {sum});

    const Id scopeId = Device(body), order = Relaxed(body), unequal = Relaxed(body);
    const Id previous = ValueOf(body.function, OpAtomicCompareExchange, word, {pointer, scopeId, order, unequal, wanted, before});
    Store(body.function, seen, previous);
    BranchIf(body.function, ValueOf(body.function, OpIEqual, TypeBool(body.module), {previous, before}), merge, step);
  }

  // `groupSize()` is 256; a thread's place in its group is its number modulo 256 and its group's number the number divided by 256.
  Id Group(SpvBody& body, const SpvScope& scope, const Expr& e) {
    const Id intType = TypeInt(body.module, 32), size = ConstInt(body.module, 32, kGroupSize);

    if (e.name == "groupSize") {
      return size;
    }

    if (e.name == "barrier") {
      return Barrier(body);
    }

    const Id thread = ExprTo(body, scope, *e.operands[0], SpvInt32());

    return ValueOf(body.function, e.name == "localIndex" ? OpSRem : OpSDiv, intType, {thread, size});
  }

  // `OpControlBarrier` on the workgroup with acquire-release ordering of workgroup memory; its value as a function is 0.
  Id Barrier(SpvBody& body) {
    const Id workgroup = ConstUint(body.module, kScopeWorkgroup);
    Effect(body.function, OpControlBarrier, {workgroup, workgroup, ConstUint(body.module, kAcquireReleaseWorkgroupMemory)});

    return ConstInt(body.module, 32, 0);
  }

  // ── the float remainder ──

  // `a % b` for floats or vectors of floats, as C's `fmod`: the sign of `a`, a size below |b|. A GPU may build `OpFRem` from an inexact division
  // (`2867.0 / 47.0` a hair under 61). So the quotient is rounded down, and a remainder that lands outside [0, |b|) is moved back in.
  Id Remainder(SpvBody& body, const TypePtr& t, Id a, Id b) {
    const Id ax = Absolute(body, t, a), ay = Absolute(body, t, b);
    const Id rough = RoughRemainder(body, t, ax, ay);
    const Id remainder = MoveIntoRange(body, t, rough, ay);

    return WithSignOf(body, t, a, remainder);
  }

  // |a| - floor(|a| / |b|) * |b|: the remainder, give or take one |b|.
  Id RoughRemainder(SpvBody& body, const TypePtr& t, Id ax, Id ay) {
    const Id type = SpvType(body.module, t);
    const Id quotient = ValueOf(body.function, OpFDiv, type, {ax, ay});
    const Id whole = ValueOf(body.function, OpExtInst, type, {body.writer.glsl, kGlslFloor, quotient});
    const Id product = ValueOf(body.function, OpFMul, type, {whole, ay});

    return ValueOf(body.function, OpFSub, type, {ax, product});
  }

  // Into [0, |b|): below 0 gets |b| added back; at |b| or more gets |b| taken off.
  Id MoveIntoRange(SpvBody& body, const TypePtr& t, Id r, Id ay) {
    const Id type = SpvType(body.module, t), zero = SpvConstant(body.module, t, 0.0), flag = FlagType(body, t);
    const Id below = ValueOf(body.function, OpFOrdLessThan, flag, {r, zero});
    const Id sum = ValueOf(body.function, OpFAdd, type, {r, ay});
    const Id raised = ValueOf(body.function, OpSelect, type, {below, sum, r});

    const Id over = ValueOf(body.function, OpFOrdGreaterThanEqual, flag, {raised, ay});
    const Id lowered = ValueOf(body.function, OpFSub, type, {raised, ay});

    return ValueOf(body.function, OpSelect, type, {over, lowered, raised});
  }

  // The remainder `r` with the sign of `a`.
  Id WithSignOf(SpvBody& body, const TypePtr& t, Id a, Id r) {
    const Id type = SpvType(body.module, t), zero = SpvConstant(body.module, t, 0.0), flag = FlagType(body, t);
    const Id negative = ValueOf(body.function, OpFOrdLessThan, flag, {a, zero});
    const Id negated = ValueOf(body.function, OpFNegate, type, {r});

    return ValueOf(body.function, OpSelect, type, {negative, negated, r});
  }

  Id Absolute(SpvBody& body, const TypePtr& t, Id x) {
    const Id type = SpvType(body.module, t);

    return ValueOf(body.function, OpExtInst, type, {body.writer.glsl, kGlslFAbs, x});
  }

  // What a comparison gives: one flag, or one per lane of a vector.
  Id FlagType(SpvBody& body, const TypePtr& t) {
    const Id flag = TypeBool(body.module);

    return SpvIsVector(t) ? TypeVector(body.module, flag, SpvLanes(t)) : flag;
  }

}  // namespace polyir
