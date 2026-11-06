// The value operations of the bytecode VM (VmValues.hpp): making, reading, comparing and showing Values, and the heap they
// point into. `Show` and `FloatText` are the text every route agrees on (java21's NinaRt, cpp17's nina_f64), so a program
// prints the same on the VM as compiled.
#include "polyir/Std.hpp"
#include "vm/VmValues.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <charconv>
#include <cstring>

namespace polyir::vm {

  namespace {

    using std::isinf;
    using std::isnan;
    using std::to_chars;

    Value wrapped(ValueKind kind, Value inner) {
      Value v;
      v.kind = kind;
      v.inner = make_shared<Value>(move(inner));

      return v;
    }

    bool sameText(const Value& a, const Value& b) {
      return b.kind == ValueKind::Str && a.text->chars == b.text->chars;
    }

    // An integer equals a bool of the same number (the reference VM compared them so); a float or a bool only its own kind.
    bool scalarEq(const Value& a, const Value& b) {
      if (a.kind == ValueKind::Int) {
        return (b.kind == ValueKind::Int || b.kind == ValueKind::Bool) && WideOf(b) == a.number;
      }

      if (a.kind == ValueKind::Float) {
        return b.kind == ValueKind::Float && b.real == a.real;
      }

      return b.kind == ValueKind::Bool && b.flag == a.flag;
    }

    bool innerEq(const Value& a, const Value& b) {
      return a.kind == b.kind && ShallowEq(*a.inner, *b.inner);
    }

    bool wrappedEq(const Value& a, const Value& b) {
      switch (a.kind) {
        case ValueKind::Some:
          return innerEq(a, b);
        case ValueKind::None:
          return b.kind == ValueKind::None;
        case ValueKind::Variant:
          return innerEq(a, b) && a.number == b.number;
        case ValueKind::Either:
          return innerEq(a, b) && a.flag == b.flag;
        default:
          return b.kind == ValueKind::Void;
      }
    }

    String showScalar(const Value& v) {
      if (v.kind == ValueKind::Int) {
        return WideText(v.number, v.isSigned);
      }

      if (v.kind == ValueKind::Float) {
        return FloatText(v.real, v.bits);
      }

      return v.flag ? "true" : "false";
    }

    String showWrapped(const Value& v) {
      switch (v.kind) {
        case ValueKind::Some:
          return "some(" + Show(*v.inner) + ")";
        case ValueKind::None:
          return "none";
        case ValueKind::Variant:
          return "case" + to_string(static_cast<int64_t>(v.number)) + "(" + Show(*v.inner) + ")";
        case ValueKind::Either:
          return String(v.flag ? "ok(" : "err(") + Show(*v.inner) + ")";
        default:
          return "void";
      }
    }

    // The shortest digits that read back as exactly this value (`0.30000000000000004`, `1e+20`). An f32 is shortened as an f32, so `0.1f`
    // prints `0.1`, not the digits of the double that carries it.
    String shortestDigits(double v, int bits) {
      char digits[64];

      if (bits == 32) {
        const auto end32 = to_chars(begin(digits), end(digits), static_cast<float>(v));
        return String(digits, end32.ptr);
      }

      const auto end64 = to_chars(begin(digits), end(digits), v);

      return String(digits, end64.ptr);
    }

    // Digits with no point and no exponent are a whole number: the ".0" keeps them reading as a float.
    String withPointIfWhole(const String& digits) {
      if (digits.find_first_of(".e") != String::npos) {
        return digits;
      }

      return digits + ".0";
    }

    int64_t nextPow2(int64_t n) {
      if (n <= 0) {
        return 0;
      }

      int64_t c = 1;

      while (c < n) {
        c *= 2;
      }

      return c;
    }

    bool elementsEq(Heap& heap, const Vector<Value>& xs, const Vector<Value>& ys) {
      if (xs.size() != ys.size()) {
        return false;
      }

      for (size_t i = 0; i < xs.size(); i++) {
        if (!DeepEq(heap, xs[i], ys[i])) {
          return false;
        }
      }

      return true;
    }

    // Every key of one map is in the other with an equal value (and they have as many keys).
    bool sameEntries(Heap& heap, const HeapObject& a, const HeapObject& b) {
      if (a.items.size() != b.items.size()) {
        return false;
      }

      for (size_t i = 0; i < a.items.size(); i++) {
        const int64_t at = KeyIndex(heap, b.items, a.items[i]);

        if (at < 0 || !DeepEq(heap, a.values[i], b.values[static_cast<size_t>(at)])) {
          return false;
        }
      }

      return true;
    }

    bool sameMembers(Heap& heap, const HeapObject& a, const HeapObject& b) {
      if (a.items.size() != b.items.size()) {
        return false;
      }

      for (const Value& x : a.items) {
        if (KeyIndex(heap, b.items, x) < 0) {
          return false;
        }
      }

      return true;
    }

    // Two objects of the same kind: equal by their contents (a struct by its name too).
    bool sameObjects(Heap& heap, const HeapObject& x, const HeapObject& y) {
      switch (x.kind) {
        case ObjectKind::Struct:
          return x.name == y.name && elementsEq(heap, x.items, y.items);
        case ObjectKind::Array:
          return elementsEq(heap, x.items, y.items);
        case ObjectKind::Map:
          return sameEntries(heap, x, y);
        case ObjectKind::Set:
          return sameMembers(heap, x, y);
        default:
          return false;
      }
    }

    bool objectsEq(Heap& heap, int64_t a, int64_t b) {
      if (a == b) {
        return true;
      }

      const HeapObject& x = ObjectAt(heap, a);
      const HeapObject& y = ObjectAt(heap, b);

      return x.kind == y.kind && sameObjects(heap, x, y);
    }

    // ── allocation ──

    int64_t appendedCell(Heap& heap, HeapObject object) {
      heap.cells.push_back(move(object));

      return static_cast<int64_t>(heap.cells.size()) - 1;
    }

    // The most recently freed cell first: it is the likeliest to still be in the processor's cache.
    int64_t reusedCell(Heap& heap, HeapObject object) {
      const int64_t id = heap.freeCells.back();
      heap.freeCells.pop_back();
      heap.cells[static_cast<size_t>(id)] = move(object);

      return id;
    }

  }  // namespace

  // NaN and the infinities are spelled as java21 and cpp17 spell them (NinaRt.f64, nina_f64), so every route agrees on them.
  String FloatText(double v, int bits) {
    if (isnan(v)) {
      return "NaN";
    }

    if (isinf(v)) {
      return v > 0 ? "Infinity" : "-Infinity";
    }

    return withPointIfWhole(shortestDigits(v, bits));
  }

  Value IntValue(Wide v, int bits, bool isSigned) {
    Value out;
    out.kind = ValueKind::Int;
    out.number = WrapInt(v, bits, isSigned);
    out.bits = bits;
    out.isSigned = isSigned;

    return out;
  }

  Value FloatValue(double v, int bits) {
    Value out;
    out.kind = ValueKind::Float;
    out.real = v;
    out.bits = bits;

    return out;
  }

  Value BoolValue(bool b) {
    Value out;
    out.kind = ValueKind::Bool;
    out.flag = b;

    return out;
  }

  Value StrNoIdentity(String text) {
    Value out;
    out.kind = ValueKind::Str;
    out.text = make_shared<const Text>(Text{move(text), -1});

    return out;
  }

  Value StrStamped(String text, int64_t stamp) {
    Value out;
    out.kind = ValueKind::Str;
    out.text = make_shared<const Text>(Text{move(text), stamp});

    return out;
  }

  Value StrValue(Heap& heap, String text) {
    return StrStamped(move(text), NewStamp(heap));
  }

  Value FuncValue(int64_t index, String name) {
    Value out;
    out.kind = ValueKind::Func;
    out.number = index;
    out.text = make_shared<const Text>(Text{move(name), -1});

    return out;
  }

  Value RefValue(int64_t id) {
    Value out;
    out.kind = ValueKind::Ref;
    out.number = id;

    return out;
  }

  Value SomeValue(Value inner) {
    return wrapped(ValueKind::Some, move(inner));
  }

  Value VariantValue(int64_t tag, Value inner) {
    Value out = wrapped(ValueKind::Variant, move(inner));
    out.number = tag;

    return out;
  }

  Value EitherValue(bool isOk, Value inner) {
    Value out = wrapped(ValueKind::Either, move(inner));
    out.flag = isOk;

    return out;
  }

  bool Truthy(const Value& v) {
    if (v.kind == ValueKind::Bool) {
      return v.flag;
    }

    return v.kind == ValueKind::Int && v.number != 0;
  }

  int64_t IntOf(const Value& v) {
    return static_cast<int64_t>(WideOf(v));
  }

  Wide WideOf(const Value& v) {
    if (v.kind == ValueKind::Int) {
      return v.number;
    }

    if (v.kind == ValueKind::Bool) {
      return v.flag ? 1 : 0;
    }

    return 0;
  }

  // The digits of the magnitude, least significant first, then reversed; the sign in front for a negative signed value.
  String WideText(Wide v, bool isSigned) {
    const bool negative = isSigned && v < 0;
    UnsignedWide rest = negative ? UnsignedWide(0) - static_cast<UnsignedWide>(v) : static_cast<UnsignedWide>(v);
    String digits;

    do {
      digits += static_cast<char>('0' + static_cast<int>(rest % 10));
      rest /= 10;
    } while (rest != 0);

    return (negative ? "-" : "") + String(digits.rbegin(), digits.rend());
  }

  double FloatOf(const Value& v) {
    if (v.kind == ValueKind::Float) {
      return v.real;
    }

    if (v.kind == ValueKind::Int) {
      return v.isSigned ? static_cast<double>(v.number) : static_cast<double>(static_cast<UnsignedWide>(v.number));
    }

    return 0.0;
  }

  const String& TextOf(const Value& v) {
    static const String empty;

    return v.kind == ValueKind::Str ? v.text->chars : empty;
  }

  bool IsNone(const Value& v) {
    return v.kind == ValueKind::None;
  }

  String Show(const Value& v) {
    switch (v.kind) {
      case ValueKind::Int: case ValueKind::Float: case ValueKind::Bool:
        return showScalar(v);
      case ValueKind::Str:
        return v.text->chars;
      case ValueKind::Func:
        return "<func " + (v.text ? v.text->chars : String()) + ">";
      case ValueKind::Ref:
        return "<object #" + to_string(static_cast<int64_t>(v.number)) + ">";
      default:
        return showWrapped(v);
    }
  }

  bool ShallowEq(const Value& a, const Value& b) {
    switch (a.kind) {
      case ValueKind::Int: case ValueKind::Float: case ValueKind::Bool:
        return scalarEq(a, b);
      case ValueKind::Str:
        return sameText(a, b);
      case ValueKind::Func:
        return b.kind == ValueKind::Func && b.number == a.number;
      case ValueKind::Ref:
        return b.kind == ValueKind::Ref && b.number == a.number;
      default:
        return wrappedEq(a, b);
    }
  }

  Value DefaultOf(const TypePtr& type) {
    switch (type->kind) {
      case TypeKind::Int: case TypeKind::UInt:
        return IntValue(0, type->bits, type->kind == TypeKind::Int);
      case TypeKind::Float:
        return FloatValue(0.0, type->bits);
      case TypeKind::String:
        return StrNoIdentity("");
      case TypeKind::Void:
        return VoidValue();
      default:
        return NoneValue();
    }
  }

  // ── the heap ──

  // A cell the collector emptied is reused before the heap grows. Allocation never collects: it only counts, and the dispatch loop
  // collects at its next safe point once the count passes the threshold (Collector.cpp).
  int64_t Alloc(Heap& heap, HeapObject object) {
    heap.objects++;
    const int64_t id = heap.freeCells.empty() ? appendedCell(heap, move(object)) : reusedCell(heap, move(object));
    ColourNewObject(heap, id);

    return id;
  }

  int64_t NewStamp(Heap& heap) {
    return heap.nextStamp++;
  }

  HeapObject& ObjectAt(Heap& heap, int64_t id) {
    return heap.cells[static_cast<size_t>(id)];
  }

  HeapObject* ObjectOf(Heap& heap, const Value& v) {
    return v.kind == ValueKind::Ref ? &ObjectAt(heap, v.number) : nullptr;
  }

  int64_t NewBox(Heap& heap, Value v) {
    HeapObject box;
    box.kind = ObjectKind::Box;
    box.cell = move(v);

    return Alloc(heap, move(box));
  }

  Value BoxGet(Heap& heap, int64_t id) {
    return ObjectAt(heap, id).cell;
  }

  void BoxSet(Heap& heap, int64_t id, Value v) {
    Value& cell = ObjectAt(heap, id).cell;
    WriteBarrier(heap, cell);
    cell = move(v);
  }

  Value NewArray(Heap& heap, Vector<Value> elements) {
    HeapObject list;
    list.kind = ObjectKind::Array;
    list.items = move(elements);

    return RefValue(Alloc(heap, move(list)));
  }

  Value NewMap(Heap& heap, int64_t requested) {
    HeapObject map;
    map.kind = ObjectKind::Map;
    map.capacity = nextPow2(requested);

    return RefValue(Alloc(heap, move(map)));
  }

  Value NewSet(Heap& heap) {
    HeapObject set;
    set.kind = ObjectKind::Set;

    return RefValue(Alloc(heap, move(set)));
  }

  Value NewSparse(Heap& heap) {
    HeapObject sparse;
    sparse.kind = ObjectKind::Sparse;

    return RefValue(Alloc(heap, move(sparse)));
  }

  Value SparseKey(Heap& heap, const Vector<Value>& indices) {
    if (indices.size() == 1) {
      return NewArray(heap, {indices[0], I64Value(0)});
    }

    return NewArray(heap, indices);
  }

  int64_t KeyIndex(Heap& heap, const Vector<Value>& keys, const Value& key) {
    for (size_t i = 0; i < keys.size(); i++) {
      if (DeepEq(heap, keys[i], key)) {
        return static_cast<int64_t>(i);
      }
    }

    return -1;
  }

  int64_t LastKeyIndex(Heap& heap, const Vector<Value>& keys, const Value& key) {
    for (size_t i = keys.size(); i > 0; i--) {
      if (DeepEq(heap, keys[i - 1], key)) {
        return static_cast<int64_t>(i - 1);
      }
    }

    return -1;
  }

  bool DeepEq(Heap& heap, const Value& a, const Value& b) {
    switch (a.kind) {
      case ValueKind::Ref:
        return b.kind == ValueKind::Ref && objectsEq(heap, a.number, b.number);
      case ValueKind::Some:
        return b.kind == ValueKind::Some && DeepEq(heap, *a.inner, *b.inner);
      case ValueKind::Variant:
        return b.kind == ValueKind::Variant && b.number == a.number && DeepEq(heap, *a.inner, *b.inner);
      case ValueKind::Either:
        return b.kind == ValueKind::Either && b.flag == a.flag && DeepEq(heap, *a.inner, *b.inner);
      default:
        return ShallowEq(a, b);
    }
  }

  void GrowMap(HeapObject& map) {
    while (map.capacity > 0 && static_cast<int64_t>(map.items.size()) * 4 >= map.capacity * 3) {
      map.capacity *= 2;
    }
  }

  // ── the arguments of a bound extern ──

  const Value& ArgumentAt(const Vector<Value>& args, size_t i) {
    static const Value missing;

    return i < args.size() ? args[i] : missing;
  }

  String WordAt(const Vector<Value>& args, size_t i) {
    return Show(ArgumentAt(args, i));
  }

  Vector<String> WordsAt(Heap& heap, const Vector<Value>& args, size_t i) {
    Vector<String> words;
    HeapObject* list = ObjectOf(heap, ArgumentAt(args, i));

    if (list == nullptr || list->kind != ObjectKind::Array) {
      return words;
    }

    for (const Value& word : list->items) {
      words.push_back(Show(word));
    }

    return words;
  }

  Value StringList(Heap& heap, const Vector<String>& words) {
    Vector<Value> values;

    for (const String& word : words) {
      values.push_back(StrValue(heap, word));
    }

    return NewArray(heap, move(values));
  }

}  // namespace polyir::vm
