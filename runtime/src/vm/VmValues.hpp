// The values and heap of the bytecode VM (src/vm): what a running polyir program holds. The model was matched to the Nina-written
// reference VM (removed 2026-10-28, see core/docs/language/vm-proposal.md); runtime/tests/vm_vs_native.sh now holds the VM to the
// native build. Scalars are carried inline, everything with identity (structs, containers, closures, boxes, coroutines) lives in
// the heap and is carried as a reference. Integers are carried in 128 bits (`Wide`) with their declared width and signedness beside them, always in
// canonical form — sign-extended when signed, zero-extended when unsigned — so I128/U128 hold their full value and unsigned
// division, comparison and narrowing follow the HIR's rules, not the host's.
#pragma once

#include "polyir/Std.hpp"
#include "polyir/Hir.hpp"

namespace polyir::vm {

  // An integer's payload: wide enough for I128/U128; narrower integers sit in it extended to canonical form.
  using Wide = __int128;
  using UnsignedWide = unsigned __int128;

  enum class ValueKind : uint8_t { Void, Int, Float, Bool, Str, Func, Ref, Some, None, Variant, Either };

  // A string's text with its identity: `===` compares addresses in the backends, so every string-producing expression gets
  // a fresh stamp from the heap's stamp counter (NewStamp); -1 is "no identity" (defaults, conversions). A stamp is only a
  // number, not a heap cell: strings take no room in the heap, and their text is freed by C++ when the last value holding it goes.
  struct Text {
    String chars;
    int64_t stamp = -1;
  };

  struct Value;
  using ValuePtr = SharedPtr<Value>;

  struct Value {
    ValueKind kind = ValueKind::Void;
    int bits = 64;  // Int: the declared width; Float: 32 or 64
    bool isSigned = true;  // Int
    bool flag = false;  // Bool; Either: isOk
    Wide number = 0;  // Int (canonical for its width); Ref: the heap id; Func: the function index, or -1 - the extern index; Variant: the tag
    double real = 0;  // Float
    SharedPtr<const Text> text;  // Str; Func: the function's name (what `show` prints)
    ValuePtr inner;  // Some, Variant, Either: the wrapped value
  };

  // What one heap object holds. `kind` says which parts mean something: a struct its `name` and `items` (the fields); a list
  // `items`; a set `items` (the members); a map `items` (the keys), `values` and `capacity`; a sparse `items` (the keys, one
  // per write) and `values`; a box `cell`; a closure `closure`; a coroutine `coroutine`. A free cell is one the collector
  // reclaimed (Collector.cpp): it holds nothing and waits on the heap's free list for the next allocation.
  enum class ObjectKind : uint8_t { Struct, Array, Map, Set, Sparse, Box, Closure, Coroutine, Free };

  struct Closure {
    int function = -1;  // which compiled function (a closure body) runs: an index into Program::functions
    Vector<Value> captured;  // the boxes (and values) the body reads, in its capture order
  };

  struct Coroutine;

  struct HeapObject {
    ObjectKind kind = ObjectKind::Box;
    String name;
    Vector<Value> items;
    Vector<Value> values;
    int64_t capacity = 0;  // a mutmap's bucket count (0 when untracked)
    Value cell;
    Closure closure;
    SharedPtr<Coroutine> coroutine;
  };

  // What a cycle of the collector is doing (Collector.cpp): nothing, marking, or sweeping.
  enum class Phase : uint8_t { Idle, Marking, Sweeping };

  // A grey object: reached, and its values looked at up to `next` (a long list is looked at over several slices).
  struct Grey {
    int64_t id = 0;
    size_t next = 0;
  };

  // Where the collector's cycle stands between two slices (Collector.cpp). It lives in the heap because allocation (which colours each new
  // object) and the write barrier take only the heap.
  struct Marking {
    Phase phase = Phase::Idle;
    Vector<uint8_t> marked;  // one per cell, as long as `cells`: reached in this cycle (grey or black)
    Deque<Grey> worklist;  // the grey objects (a deque: it grows without copying what it holds, which would be a pause of its own)
    size_t sweepAt = 0;  // the sweep has passed every cell before this one ...
    size_t sweepEnd = 0;  // ... of the cells there were when the marking ended
    int64_t markedObjects = 0;  // what the cycle has marked so far: at the end of the marking, what survives
  };

  // The heap: an object's id is its index in `cells`, and a reference is that id, so a cell never moves. The garbage collector
  // (Collector.cpp) empties the cells nothing can reach any more and lists them in `freeCells`; Alloc fills those first and grows
  // `cells` only when none is free. (`Free`/`FreeBox` in a program are only hints: the collector decides.)
  struct Heap {
    Vector<HeapObject> cells;
    Deque<int64_t> freeCells;  // ids of emptied cells, the next allocations' homes (a deque: a sweep that frees a million cells adds them without copying)
    int64_t objects = 0;  // cells in use: `cells` minus `freeCells`, what the collector's threshold and ceiling count
    int64_t nextStamp = 0;  // the next string identity (NewStamp)
    Marking marking;
  };

  // ── the write barrier (Collector.cpp, docs/memory.md Part 3) ──
  // While the collector marks in slices, the program runs between them. Before a store overwrites a value inside an object, the value it
  // overwrites turns grey (ShadeValue). So whatever was in the heap when the cycle began is marked before it can disappear. The rest of
  // the time the barrier is one comparison.
  void ShadeValue(Heap& heap, const Value& v);

  void ShadeObject(Heap& heap, const HeapObject& o);  // every value of the object, before they are moved or removed all at once

  // A new object's colour (Collector.cpp): marked while a cycle marks, or while the sweep has yet to reach its cell; unmarked otherwise.
  void ColourNewObject(Heap& heap, int64_t id);

  inline void WriteBarrier(Heap& heap, const Value& overwritten) {
    if (heap.marking.phase == Phase::Marking) {
      ShadeValue(heap, overwritten);
    }
  }

  inline void WriteBarrierAll(Heap& heap, const HeapObject& o) {
    if (heap.marking.phase == Phase::Marking) {
      ShadeObject(heap, o);
    }
  }

  // ── making values ──
  Wide WrapInt(Wide v, int bits, bool isSigned);  // `v` reduced to `bits` bits: the value an `iN`/`uN` slot holds

  Value IntValue(Wide v, int bits, bool isSigned);  // `v` reduced to `bits` bits, in canonical form

  Value FloatValue(double v, int bits);

  Value BoolValue(bool b);

  Value StrValue(Heap& heap, String text);  // a fresh identity (NewStamp)

  Value StrStamped(String text, int64_t stamp);  // an identity taken earlier (a foreign call takes its stamp before the call)

  Value StrNoIdentity(String text);  // stamp -1: a default or a conversion

  Value FuncValue(int64_t index, String name);  // a function by index, or an extern as -1 - its index

  Value RefValue(int64_t id);

  Value SomeValue(Value inner);

  Value VariantValue(int64_t tag, Value inner);

  Value EitherValue(bool isOk, Value inner);

  inline Value VoidValue() {
    return Value{};
  }

  inline Value I64Value(int64_t v) {
    return IntValue(v, 64, true);
  }

  inline Value NoneValue() {
    Value v;
    v.kind = ValueKind::None;

    return v;
  }

  // ── reading values ──
  bool Truthy(const Value& v);

  int64_t IntOf(const Value& v);  // the low 64 bits; 0 when it is not an integer (the checker guarantees the type)

  Wide WideOf(const Value& v);  // the whole 128-bit payload; 0 when it is not an integer

  String WideText(Wide v, bool isSigned);  // decimal digits: to_string has no 128-bit overload

  double FloatOf(const Value& v);

  const String& TextOf(const Value& v);  // "" when it is not a string

  bool IsNone(const Value& v);

  String Show(const Value& v);  // the text `print` and `str` give

  // A float's text: the shortest digits that read back exactly, a whole number with ".0" (`3.0`), `NaN`, `Infinity`, `-Infinity`.
  String FloatText(double v, int bits = 64);

  bool ShallowEq(const Value& a, const Value& b);  // scalars structurally, references by identity

  // The value a `var` slot of this type holds before its first assignment (scalars zero; aggregates `none`).
  Value DefaultOf(const TypePtr& type);

  // ── the heap ──
  int64_t Alloc(Heap& heap, HeapObject object);

  int64_t NewStamp(Heap& heap);  // a fresh string identity: the next number of the heap's counter

  HeapObject& ObjectAt(Heap& heap, int64_t id);

  HeapObject* ObjectOf(Heap& heap, const Value& v);  // null when `v` is not a reference

  int64_t NewBox(Heap& heap, Value v);

  Value BoxGet(Heap& heap, int64_t id);

  void BoxSet(Heap& heap, int64_t id, Value v);

  Value NewArray(Heap& heap, Vector<Value> elements);

  Value NewMap(Heap& heap, int64_t requested);

  Value NewSet(Heap& heap);

  Value NewSparse(Heap& heap);

  Value SparseKey(Heap& heap, const Vector<Value>& indices);  // `sp[i]` is `sp[i, 0]`

  int64_t KeyIndex(Heap& heap, const Vector<Value>& keys, const Value& key);  // -1 when absent

  int64_t LastKeyIndex(Heap& heap, const Vector<Value>& keys, const Value& key);  // sparse reads are last-write-wins

  bool DeepEq(Heap& heap, const Value& a, const Value& b);  // `==`: structural, following the containers

  void GrowMap(HeapObject& map);  // the table doubles when three quarters full (capacity 0 means untracked)

  // ── the arguments of a bound extern (Host.cpp, ProcessHost.cpp, NinaRuntime.cpp) ──
  const Value& ArgumentAt(const Vector<Value>& args, size_t i);  // a void Value past the end

  String WordAt(const Vector<Value>& args, size_t i);  // show() of that argument

  Vector<String> WordsAt(Heap& heap, const Vector<Value>& args, size_t i);  // a List<Text> argument's strings (none when not a list)

  Value StringList(Heap& heap, const Vector<String>& words);  // a list of fresh strings

}  // namespace polyir::vm
