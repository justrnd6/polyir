// The builtins of the bytecode VM (src/vm): containers, sums (variants, eithers, options), interface calls, coroutine operations,
// shapes and hashes, plus indexed reads and writes and fresh containers and structs. The behaviour, trap message for trap message, was
// matched to the Nina-written reference VM (removed 2026-10-28, see core/docs/language/vm-proposal.md); runtime/tests/vm_vs_native.sh
// now holds the VM to the native build.
#include "polyir/Std.hpp"
#include "vm/Machine.hpp"

namespace polyir::vm {

  namespace {

    // ── small answers ──

    Outcome count(int64_t n) {
      return ValueOutcome(I64Value(n));
    }

    Outcome flag(bool b) {
      return ValueOutcome(BoolValue(b));
    }

    Outcome fresh(Machine& m, HeapObject object) {
      return ValueOutcome(RefValue(Alloc(m.heap, move(object))));
    }

    Outcome notContainer(const String& doing) {
      return Trap(doing + " a value that is not a container");
    }

    // An argument the checker always supplies; void when it is missing, so a malformed program cannot read past the list.
    Value argAt(const Vector<Value>& args, size_t i) {
      return i < args.size() ? args[i] : VoidValue();
    }

    // An element by position; the checker keeps every slot walk and shape index in range, so the fallback is never semantics.
    // A template so it serves a vector of any item type (`T`), with `T{}` as the fallback.
    template <typename T>
    T slotAt(const Vector<T>& xs, int64_t at) {
      if (at < 0 || at >= static_cast<int64_t>(xs.size())) {
        return T{};
      }

      return xs[static_cast<size_t>(at)];
    }

    // ── finding objects by kind ──

    // The object `v` refers to when it is of this kind; null when `v` is not a reference to such an object.
    HeapObject* objectOfKind(Machine& m, const Value& v, ObjectKind kind) {
      HeapObject* o = ObjectOf(m.heap, v);

      return o != nullptr && o->kind == kind ? o : nullptr;
    }

    // A map's keys or a set's members: what `has`, `assocKey` and the slot walks index.
    Vector<Value>* keysOf(Machine& m, const Value& v) {
      HeapObject* o = ObjectOf(m.heap, v);

      if (o == nullptr || (o->kind != ObjectKind::Map && o->kind != ObjectKind::Set)) {
        return nullptr;
      }

      return &o->items;
    }

    HeapObject listOf(Vector<Value> items) {
      HeapObject list;
      list.kind = ObjectKind::Array;
      list.items = move(items);

      return list;
    }

    // Writes `value` under `key`, found at `at` (-1 when absent: the entry is appended).
    void putEntry(HeapObject& map, int64_t at, const Value& key, Value value) {
      if (at >= 0) {
        map.values[static_cast<size_t>(at)] = move(value);
        return;
      }

      map.items.push_back(key);
      map.values.push_back(move(value));
    }

    // ── classifying builtins ──

    bool isNoOp(Builtin op) {
      return op == Builtin::Loc || op == Builtin::Var || op == Builtin::Free || op == Builtin::FreeBox;
    }

    bool isCoroutineOp(Builtin op) {
      return op == Builtin::CoroutineResume || op == Builtin::CoroutineDone || op == Builtin::CoroutineClose;
    }

    bool isVariantOp(Builtin op) {
      return op == Builtin::VariantTag || op == Builtin::VariantIsCase || op == Builtin::VariantUnwrap;
    }

    bool isEitherOp(Builtin op) {
      return op == Builtin::EitherIsOk || op == Builtin::EitherIsErr || op == Builtin::EitherUnwrapOk || op == Builtin::EitherUnwrapErr;
    }

    bool isOptionOp(Builtin op) {
      return op == Builtin::OptionIsSome || op == Builtin::OptionIsNone || op == Builtin::OptionUnwrap || op == Builtin::Present;
    }

    bool isShapeOp(Builtin op) {
      return op == Builtin::Rows || op == Builtin::Cols || op == Builtin::Dim;
    }

    bool isTextOp(Builtin op) {
      return op == Builtin::StrOf || op == Builtin::CharAt || op == Builtin::Slice || op == Builtin::Find || op == Builtin::Print || op == Builtin::Fail;
    }

    // ── lengths ──

    Outcome sizeOf(const HeapObject& o) {
      switch (o.kind) {
        case ObjectKind::Array:
        case ObjectKind::Map:
        case ObjectKind::Set:
        case ObjectKind::Sparse:
          return count(static_cast<int64_t>(o.items.size()));
        default:
          return notContainer("taking the length of");
      }
    }

    Outcome length(Machine& m, const Value& v) {
      if (v.kind == ValueKind::Str) {
        return count(static_cast<int64_t>(v.text->chars.size()));
      }

      const HeapObject* o = ObjectOf(m.heap, v);

      return o != nullptr ? sizeOf(*o) : notContainer("taking the length of");
    }

    // `.capacity()`: a mutmap reports its (power-of-two) bucket count; an array-backed container reports its element count.
    Outcome capacity(Machine& m, const Value& v) {
      const HeapObject* o = ObjectOf(m.heap, v);

      if (o == nullptr) {
        return notContainer("taking the capacity of");
      }

      return o->kind == ObjectKind::Map ? count(o->capacity) : length(m, v);
    }

    // ── lists ──

    Outcome push(Machine& m, const Value& v, Value x) {
      HeapObject* o = objectOfKind(m, v, ObjectKind::Array);

      if (o == nullptr) {
        return Trap("push on a value that is not a list");
      }

      o->items.push_back(move(x));

      return ValueOutcome(v);  // `push` gives the list back: `arr = push(arr, x)` keeps working
    }

    Outcome concatLists(Machine& m, const HeapObject& left, const Value& b) {
      const HeapObject* right = objectOfKind(m, b, ObjectKind::Array);

      if (right == nullptr) {
        return Trap("concat of a non-list");
      }

      HeapObject out = listOf(left.items);  // both sides are copied before the alloc, which may move the heap's cells
      out.items.insert(out.items.end(), right->items.begin(), right->items.end());

      return fresh(m, move(out));
    }

    // A string plus anything is text; a list plus a list is a fresh list.
    Outcome concat(Machine& m, const Value& a, const Value& b) {
      if (a.kind == ValueKind::Str) {
        return ValueOutcome(StrValue(m.heap, a.text->chars + Show(b)));
      }

      const HeapObject* left = objectOfKind(m, a, ObjectKind::Array);

      if (left == nullptr) {
        return Trap("concat of a non-list");
      }

      return concatLists(m, *left, b);
    }

    // Each element dropped goes through the write barrier first.
    Outcome truncated(Machine& m, const Value& v, int64_t keep) {
      HeapObject* o = objectOfKind(m, v, ObjectKind::Array);

      if (o == nullptr) {
        return Trap("truncate on a value that is not a list");
      }

      while (!o->items.empty() && static_cast<int64_t>(o->items.size()) > keep) {
        WriteBarrier(m.heap, o->items.back());
        o->items.pop_back();
      }

      return ValueOutcome(v);
    }

    // ── copies ──

    // `copy` clones a container or a struct; a box, closure or coroutine has no copy.
    bool copyable(ObjectKind k) {
      return k != ObjectKind::Box && k != ObjectKind::Closure && k != ObjectKind::Coroutine && k != ObjectKind::Free;
    }

    Outcome copied(Machine& m, const Value& v) {
      const HeapObject* o = ObjectOf(m.heap, v);

      if (o == nullptr) {
        return ValueOutcome(v);
      }

      if (!copyable(o->kind)) {
        return Trap("copying a value that is not a container");
      }

      return fresh(m, HeapObject(*o));  // name, items, values and capacity come along; a container sets nothing else
    }

    Outcome withTrap() {
      return Trap("with on a value that is not a container");
    }

    Outcome withElement(Machine& m, HeapObject list, Value x) {
      list.items.push_back(move(x));

      return fresh(m, move(list));
    }

    // A changed copy of a map keeps its capacity as it is: only a write into the map itself grows it.
    Outcome withEntry(Machine& m, HeapObject map, const Value& key, Value value) {
      putEntry(map, KeyIndex(m.heap, map.items, key), key, move(value));

      return fresh(m, move(map));
    }

    Outcome withMember(Machine& m, HeapObject set, const Value& x) {
      if (KeyIndex(m.heap, set.items, x) < 0) {
        set.items.push_back(x);
      }

      return fresh(m, move(set));
    }

    Outcome withAdded(Machine& m, const HeapObject& o, const Vector<Value>& args) {
      switch (o.kind) {
        case ObjectKind::Array:
          return withElement(m, HeapObject(o), argAt(args, 0));
        case ObjectKind::Map:
          return withEntry(m, HeapObject(o), argAt(args, 0), argAt(args, 1));
        case ObjectKind::Set:
          return withMember(m, HeapObject(o), argAt(args, 0));
        default:
          return withTrap();
      }
    }

    // `a.with(x)` / `m.with(k, v)` / `s.with(x)`: a changed copy.
    Outcome changedCopy(Machine& m, const Value& v, const Vector<Value>& args) {
      const HeapObject* o = ObjectOf(m.heap, v);

      return o != nullptr ? withAdded(m, *o, args) : withTrap();
    }

    // ── maps and sets ──

    Outcome has(Machine& m, const Value& v, const Value& key) {
      const Vector<Value>* keys = keysOf(m, v);

      if (keys == nullptr) {
        return Trap("has on a value that is not a map or set");
      }

      return flag(KeyIndex(m.heap, *keys, key) >= 0);
    }

    Outcome addMember(Machine& m, const Value& v, const Value& member) {
      HeapObject* o = objectOfKind(m, v, ObjectKind::Set);

      if (o == nullptr) {
        return Trap("add on a value that is not a mutset");
      }

      if (KeyIndex(m.heap, o->items, member) < 0) {
        o->items.push_back(member);
      }

      return VoidOutcome();
    }

    // Slot walks index a map's keys (or a set's members) and a map's values directly.
    // The VM keeps a map's or set's entries densely (items/values, in insertion order) under a nominal `capacity`. The
    // lowering's map helpers walk Capacity slots, each AssocLive or not; a slot is live exactly when an entry sits at it.
    Outcome assocLive(Machine& m, const Value& v, int64_t slot) {
      const Vector<Value>* keys = keysOf(m, v);

      if (keys == nullptr) {
        return Trap("a slot walk on a value that is not a map or set");
      }

      return ValueOutcome(BoolValue(slot >= 0 && slot < static_cast<int64_t>(keys->size())));
    }

    Outcome assocKey(Machine& m, const Value& v, int64_t slot) {
      const Vector<Value>* keys = keysOf(m, v);

      if (keys == nullptr) {
        return Trap("a slot walk on a value that is not a map or set");
      }

      return ValueOutcome(slotAt(*keys, slot));
    }

    Outcome assocValue(Machine& m, const Value& v, int64_t slot) {
      const HeapObject* o = objectOfKind(m, v, ObjectKind::Map);

      if (o == nullptr) {
        return Trap("a slot walk on a value that is not a map");
      }

      return ValueOutcome(slotAt(o->values, slot));
    }

    Outcome removeTrap() {
      return Trap("remove on a value that is not a map or set");
    }

    // Each answers whether the key was there. Removing an entry moves every entry after it. So all of the object's values go
    // through the write barrier first (a removal costs as much as the move already does).
    Outcome removedEntry(Machine& m, HeapObject& map, const Value& key) {
      const int64_t at = KeyIndex(m.heap, map.items, key);

      if (at < 0) {
        return flag(false);
      }

      WriteBarrierAll(m.heap, map);
      map.items.erase(map.items.begin() + at);
      map.values.erase(map.values.begin() + at);

      return flag(true);
    }

    Outcome removedMember(Machine& m, HeapObject& set, const Value& key) {
      const int64_t at = KeyIndex(m.heap, set.items, key);

      if (at < 0) {
        return flag(false);
      }

      WriteBarrierAll(m.heap, set);
      set.items.erase(set.items.begin() + at);

      return flag(true);
    }

    Outcome removedKey(Machine& m, const Value& v, const Value& key) {
      HeapObject* o = ObjectOf(m.heap, v);

      if (o == nullptr) {
        return removeTrap();
      }

      if (o->kind == ObjectKind::Map) {
        return removedEntry(m, *o, key);
      }

      if (o->kind == ObjectKind::Set) {
        return removedMember(m, *o, key);
      }

      return removeTrap();
    }

    // ── interface calls ──

    Outcome notStruct() {
      return Trap("an interface call on a value that is not a struct");
    }

    const InterfaceInfo* interfaceNamed(const Program& program, const String& name) {
      for (const InterfaceInfo& interface : program.interfaces) {
        if (interface.name == name) {
          return &interface;
        }
      }

      return nullptr;
    }

    // The implementation's arguments: the receiver, then everything after the vtable slot.
    Vector<Value> withReceiver(const Value& v, const Vector<Value>& args) {
      Vector<Value> out{v};

      for (size_t i = 1; i < args.size(); i++) {
        out.push_back(args[i]);
      }

      return out;
    }

    // The receiver's own struct name picks the implementation (`Struct_method`, the checker's member naming).
    Outcome callMethod(Machine& m, const Value& v, const String& method, const Vector<Value>& args) {
      const HeapObject* o = objectOfKind(m, v, ObjectKind::Struct);

      if (o == nullptr) {
        return notStruct();
      }

      const String name = o->name + "_" + method;  // read before the call, which may move the heap's cells

      return CallNamed(m, name, withReceiver(v, args));
    }

    Outcome interfaceMethodCall(Machine& m, const String& interfaceName, const Value& v, const Vector<Value>& args) {
      const InterfaceInfo* d = interfaceNamed(*m.program, interfaceName);

      if (d == nullptr) {
        return Trap("'" + interfaceName + "' is not an interface");
      }

      return callMethod(m, v, slotAt(d->methods, IntOf(argAt(args, 0))), args);
    }

    // `interface.method(args)`: `args[0]` is the vtable slot; the receiver's static type names the interface.
    Outcome interfaceCall(Machine& m, const Shape& shape, const Value& v, const Vector<Value>& args) {
      if (shape.type == nullptr || shape.type->kind != TypeKind::Struct) {
        return Trap("an interface call on a value that is not an interface");
      }

      return interfaceMethodCall(m, shape.type->name, v, args);
    }

    // ── coroutines ──

    Outcome notCoroutine() {
      return Trap("a coroutine operation on a value that is not a coroutine");
    }

    // Resume and close run program code, so they take the coroutine's reference, not its object. The heap may grow while the
    // coroutine runs, which moves its cells (Run.cpp pins the reference for that time).
    Outcome onCoroutine(Machine& m, Builtin op, const Value& v, const Coroutine& co) {
      if (op == Builtin::CoroutineResume) {
        return ResumeCoroutine(m, v);
      }

      if (op == Builtin::CoroutineDone) {
        return flag(co.phase == CoroutinePhase::Finished);
      }

      return CloseCoroutine(m, v);
    }

    Outcome coroutineOp(Machine& m, Builtin op, const Value& v) {
      const HeapObject* o = objectOfKind(m, v, ObjectKind::Coroutine);

      if (o == nullptr || o->coroutine == nullptr) {
        return notCoroutine();
      }

      const SharedPtr<Coroutine> co = o->coroutine;

      return onCoroutine(m, op, v, *co);
    }

    // ── variants, eithers, options ──

    Outcome variantCase(Builtin op, const Value& v, int64_t wanted) {
      if (op == Builtin::VariantTag) {
        return count(v.number);
      }

      if (op == Builtin::VariantIsCase) {
        return flag(v.number == wanted);
      }

      if (v.number != wanted) {
        return Trap("unwrapping case " + to_string(wanted) + " of a variant holding case " + to_string(static_cast<int64_t>(v.number)));
      }

      return ValueOutcome(*v.inner);
    }

    Outcome variantOp(Builtin op, const Value& v, const Vector<Value>& args) {
      if (v.kind != ValueKind::Variant) {
        return Trap("a variant operation on a value that is not a variant");
      }

      return variantCase(op, v, IntOf(argAt(args, 0)));
    }

    Outcome eitherSide(Builtin op, bool isOk, const Value& value) {
      if (op == Builtin::EitherIsOk) {
        return flag(isOk);
      }

      if (op == Builtin::EitherIsErr) {
        return flag(!isOk);
      }

      if (op == Builtin::EitherUnwrapOk) {
        return isOk ? ValueOutcome(value) : Trap("unwrapOk of an err");
      }

      return isOk ? Trap("unwrapErr of an ok") : ValueOutcome(value);
    }

    Outcome eitherOp(Builtin op, const Value& v) {
      if (v.kind != ValueKind::Either) {
        return Trap("an either operation on a value that is not an either");
      }

      return eitherSide(op, v.flag, *v.inner);
    }

    // The checker builds an `optional<T>` like an `either<T, i1>` (`Some` is `ok`, `None` is `err`). An absent one read from a
    // container slot is the slot's default, `none`.
    bool present(const Value& v) {
      if (v.kind == ValueKind::None) {
        return false;
      }

      if (v.kind == ValueKind::Either) {
        return v.flag;
      }

      return true;
    }

    Outcome absent() {
      return Trap("unwrap of an absent optional");
    }

    Outcome unwrapOption(const Value& v) {
      if (v.kind == ValueKind::Some) {
        return ValueOutcome(*v.inner);
      }

      if (v.kind == ValueKind::Either) {
        return v.flag ? ValueOutcome(*v.inner) : absent();
      }

      if (v.kind == ValueKind::None) {
        return absent();
      }

      return ValueOutcome(v);  // the checker also unwraps plain values an optional slot carries
    }

    Outcome optionOp(Builtin op, const Value& v) {
      if (op == Builtin::OptionIsNone) {
        return flag(!present(v));
      }

      if (op == Builtin::OptionUnwrap) {
        return unwrapOption(v);
      }

      return flag(present(v));
    }

    // ── shapes and hashes ──

    // Rows/Cols/Dim come straight off the receiver's type.
    Outcome gridShape(Builtin op, const Type& t) {
      if (op == Builtin::Rows) {
        return count(t.size);
      }

      if (op == Builtin::Cols) {
        return count(t.cols);
      }

      return Trap("dim on a grid");
    }

    Outcome tensorShape(Builtin op, const Type& t, const Vector<Value>& args) {
      if (op != Builtin::Dim) {
        return Trap("rows/cols on a tensor");
      }

      return count(slotAt(t.dims, IntOf(argAt(args, 0))));
    }

    Outcome shapeOp(Builtin op, const Shape& shape, const Vector<Value>& args) {
      if (shape.type != nullptr && shape.type->kind == TypeKind::Grid) {
        return gridShape(op, *shape.type);
      }

      if (shape.type != nullptr && shape.type->kind == TypeKind::Tensor) {
        return tensorShape(op, *shape.type, args);
      }

      return Trap("a shape operation on a value with no shape");
    }

    Outcome hashOf(const Value& v) {
      if (v.kind == ValueKind::Int || v.kind == ValueKind::Bool) {
        return count(IntOf(v));
      }

      if (v.kind == ValueKind::Str) {
        return count(HashText(v.text->chars));
      }

      return Trap("hash is not supported by the interpreter yet for this value");
    }

    // ── dispatch ──

    // The slot walks over a map or set; what is left (shuffles) the VM does not run, and says so by the HIR printer's name.
    Outcome walkOp(Machine& m, Builtin op, const Value& v, const Vector<Value>& args) {
      switch (op) {
        case Builtin::AssocLive:
          return assocLive(m, v, IntOf(argAt(args, 0)));
        case Builtin::AssocKey:
          return assocKey(m, v, IntOf(argAt(args, 0)));
        case Builtin::AssocValue:
          return assocValue(m, v, IntOf(argAt(args, 0)));
        default:
          return Trap(String("the operation ") + BuiltinName(op) + " is not supported by the interpreter yet");
      }
    }

    // Membership, truncation and removal; the rest go on to walkOp.
    Outcome keyedOp(Machine& m, Builtin op, const Value& v, const Vector<Value>& args) {
      switch (op) {
        case Builtin::Has:
          return has(m, v, argAt(args, 0));
        case Builtin::Add:
          return addMember(m, v, argAt(args, 0));
        case Builtin::Truncate:
          return truncated(m, v, IntOf(argAt(args, 0)));
        case Builtin::Remove:
          return removedKey(m, v, argAt(args, 0));
        default:
          return walkOp(m, op, v, args);
      }
    }

    // The copies; the rest go on to keyedOp.
    Outcome copyOp(Machine& m, Builtin op, const Value& v, const Vector<Value>& args) {
      switch (op) {
        case Builtin::Copy:
          return copied(m, v);
        case Builtin::With:
          return changedCopy(m, v, args);
        default:
          return keyedOp(m, op, v, args);
      }
    }

    // Lengths, capacities and the list operations; the rest go on to copyOp.
    Outcome containerOp(Machine& m, Builtin op, const Value& v, const Vector<Value>& args) {
      switch (op) {
        case Builtin::Len:
          return length(m, v);
        case Builtin::Capacity:
          return capacity(m, v);
        case Builtin::Push:
          return push(m, v, argAt(args, 0));
        case Builtin::Concat:
          return concat(m, v, argAt(args, 0));
        default:
          return copyOp(m, op, v, args);
      }
    }

    // Options, shapes, hashes and strings; the rest go on to containerOp.
    Outcome valueOp(Machine& m, Builtin op, const Shape& shape, const Value& v, const Vector<Value>& args) {
      if (isOptionOp(op)) {
        return optionOp(op, v);
      }

      if (isShapeOp(op)) {
        return shapeOp(op, shape, args);
      }

      if (op == Builtin::Hash) {
        return hashOf(v);
      }

      if (isTextOp(op)) {
        return TextBuiltin(m, op, shape, v, args);
      }

      return containerOp(m, op, v, args);
    }

    // Atomics, variants and eithers; the rest go on to valueOp. A `yielded()` reaches here only inside a larger expression: as a
    // statement's whole value the compiler emits `Yielded` instead.
    Outcome atomicOrSumOp(Machine& m, Builtin op, const Shape& shape, const Value& v, const Vector<Value>& args) {
      if (op == Builtin::CoroutineYield) {
        return Trap("yielded() inside a larger expression is not supported by the interpreter");
      }

      if (IsAtomicOp(op)) {
        return AtomicOp(op, shape, v, args);
      }

      if (isVariantOp(op)) {
        return variantOp(op, v, args);
      }

      if (isEitherOp(op)) {
        return eitherOp(op, v);
      }

      return valueOp(m, op, shape, v, args);
    }

    // ── element positions ──

    // `[row, col]` as row * cols + col; any other number of indices: the first one as it is.
    int64_t gridAt(const Vector<Value>& indices, int cols) {
      if (indices.size() == 2) {
        return IntOf(indices[0]) * cols + IntOf(indices[1]);
      }

      return IntOf(argAt(indices, 0));
    }

    int64_t tensorAt(const Vector<Value>& indices, const Vector<int>& dims) {
      int64_t at = 0;

      for (size_t i = 0; i < indices.size(); i++) {
        at = at * slotAt(dims, static_cast<int64_t>(i)) + IntOf(indices[i]);
      }

      return at;
    }

    // A row-major flat position for a grid or tensor; one plain index otherwise.
    int64_t flatIndex(const Vector<Value>& indices, const Shape& base) {
      if (base.type != nullptr && base.type->kind == TypeKind::Grid) {
        return gridAt(indices, base.type->cols);
      }

      if (base.type != nullptr && base.type->kind == TypeKind::Tensor) {
        return tensorAt(indices, base.type->dims);
      }

      return IntOf(argAt(indices, 0));
    }

    bool inBounds(int64_t at, size_t length) {
      return at >= 0 && at < static_cast<int64_t>(length);
    }

    Outcome outOfBounds(int64_t at, size_t length) {
      return Trap("index " + to_string(at) + " is out of bounds (length " + to_string(length) + ")");
    }

    // The value at `at` among `values`, or the element type's default when the key was absent (-1).
    Outcome entry(const Vector<Value>& values, int64_t at, const Shape& element) {
      if (at < 0) {
        return ValueOutcome(element.type != nullptr ? DefaultOf(element.type) : NoneValue());
      }

      return ValueOutcome(values[static_cast<size_t>(at)]);
    }

    Outcome readElement(const HeapObject& list, const Vector<Value>& indices, const Shape& base) {
      const int64_t at = flatIndex(indices, base);

      if (!inBounds(at, list.items.size())) {
        return outOfBounds(at, list.items.size());
      }

      return ValueOutcome(list.items[static_cast<size_t>(at)]);
    }

    // Sparse reads are last-write-wins. The key is a fresh array, so the object is fetched only after that alloc.
    Outcome readSparse(Machine& m, int64_t id, const Vector<Value>& indices, const Shape& element) {
      const Value key = SparseKey(m.heap, indices);
      const HeapObject& sparse = ObjectAt(m.heap, id);

      return entry(sparse.values, LastKeyIndex(m.heap, sparse.items, key), element);
    }

    // A write into an element or an entry that is there overwrites a value: the write barrier first.
    Outcome writeElement(Machine& m, HeapObject& list, const Vector<Value>& indices, const Shape& base, Value v) {
      const int64_t at = flatIndex(indices, base);

      if (!inBounds(at, list.items.size())) {
        return outOfBounds(at, list.items.size());
      }

      WriteBarrier(m.heap, list.items[static_cast<size_t>(at)]);
      list.items[static_cast<size_t>(at)] = move(v);

      return VoidOutcome();
    }

    Outcome writeEntry(Machine& m, HeapObject& map, const Value& key, Value v) {
      const int64_t at = KeyIndex(m.heap, map.items, key);

      if (at >= 0) {
        WriteBarrier(m.heap, map.values[static_cast<size_t>(at)]);
      }

      putEntry(map, at, key, move(v));

      if (at < 0) {
        GrowMap(map);
      }

      return VoidOutcome();
    }

    // Sparse writes are append-only; `len` counts every write.
    Outcome writeSparse(Machine& m, int64_t id, const Vector<Value>& indices, Value v) {
      const Value key = SparseKey(m.heap, indices);
      HeapObject& sparse = ObjectAt(m.heap, id);
      sparse.items.push_back(key);
      sparse.values.push_back(move(v));

      return VoidOutcome();
    }

    // ── fresh containers ──

    int64_t sizeOr(const Vector<Value>& sizes, int64_t size) {
      return sizes.empty() ? size : IntOf(sizes[0]);
    }

    int64_t volume(const Vector<int>& dims) {
      int64_t out = 1;

      for (const int d : dims) {
        out *= d;
      }

      return out;
    }

    Value zeroStruct(Machine& m, const Type& t) {
      const auto found = m.program->structIndex.find(t.name);

      if (found == m.program->structIndex.end()) {
        return NoneValue();  // an unknown struct's default
      }

      return NewStructOf(m, found->second);
    }

    // A cell nobody wrote: the element's default; a struct element is a struct of defaults (as a native zeroed block reads).
    Value zeroOf(Machine& m, const TypePtr& elem) {
      if (elem != nullptr && elem->kind == TypeKind::Struct) {
        return zeroStruct(m, *elem);
      }

      return elem != nullptr ? DefaultOf(elem) : NoneValue();
    }

    Value filled(Machine& m, const TypePtr& elem, int64_t count) {
      Vector<Value> xs;

      for (int64_t i = 0; i < count; i++) {
        xs.push_back(zeroOf(m, elem));
      }

      return NewArray(m.heap, move(xs));
    }

    // The keyed containers (maps, sparse arrays, sets); a dynamic array otherwise.
    Value keyedContainer(Machine& m, const Type& t, const Vector<Value>& sizes) {
      if (t.kind == TypeKind::Hashmap) {
        return NewMap(m.heap, sizeOr(sizes, 0));
      }

      if (t.kind == TypeKind::Sparse) {
        return NewSparse(m.heap);
      }

      if (t.kind == TypeKind::Set) {
        return NewSet(m.heap);
      }

      return NewArray(m.heap, {});
    }

    // How many cells a fixed-size container starts with (a buffer's size argument wins over its declared one); -1 for a keyed one.
    int64_t cellCount(const Type& t, const Vector<Value>& sizes) {
      switch (t.kind) {
        case TypeKind::FixedArray:
          return t.size;
        case TypeKind::Vec:
          return t.size;
        case TypeKind::Grid:
          return static_cast<int64_t>(t.size) * t.cols;
        case TypeKind::Tensor:
          return volume(t.dims);
        default:
          return t.kind == TypeKind::Buffer ? sizeOr(sizes, t.size) : -1;
      }
    }

    // The fixed-size containers start full of their element's zero.
    Value containerOf(Machine& m, const Type& t, const Vector<Value>& sizes) {
      const int64_t cells = cellCount(t, sizes);

      return cells >= 0 ? filled(m, t.element, cells) : keyedContainer(m, t, sizes);
    }

    // ── reads and writes by kind of object ──

    Outcome readFrom(Machine& m, const HeapObject& o, const Value& container, const Vector<Value>& indices, const Shape& base, const Shape& element) {
      switch (o.kind) {
        case ObjectKind::Array:
          return readElement(o, indices, base);
        case ObjectKind::Map:
          return entry(o.values, KeyIndex(m.heap, o.items, argAt(indices, 0)), element);
        case ObjectKind::Sparse:
          return readSparse(m, container.number, indices, element);
        default:
          return Trap("indexing a value that is not a container");
      }
    }

    Outcome writeInto(Machine& m, HeapObject& o, const Value& container, const Vector<Value>& indices, const Shape& base, Value v) {
      switch (o.kind) {
        case ObjectKind::Array:
          return writeElement(m, o, indices, base, move(v));
        case ObjectKind::Map:
          return writeEntry(m, o, argAt(indices, 0), move(v));
        case ObjectKind::Sparse:
          return writeSparse(m, container.number, indices, move(v));
        default:
          return Trap("assigning into a value that is not a container");
      }
    }

  }  // namespace

  // The dispatch runs in steps, each handing on the builtins it does not take:
  // CallBuiltin -> atomicOrSumOp -> valueOp -> containerOp -> copyOp -> keyedOp -> walkOp.
  Outcome CallBuiltin(Machine& m, Builtin op, const Shape& shape, Value receiver, Vector<Value> args) {
    if (isNoOp(op)) {
      return VoidOutcome();
    }

    if (op == Builtin::StrEq) {
      return flag(ShallowEq(receiver, argAt(args, 0)));
    }

    if (op == Builtin::Retype || op == Builtin::InterfaceBox) {
      return ValueOutcome(move(receiver));
    }

    if (op == Builtin::InterfaceCall) {
      return interfaceCall(m, shape, receiver, args);
    }

    if (isCoroutineOp(op)) {
      return coroutineOp(m, op, receiver);
    }

    return atomicOrSumOp(m, op, shape, receiver, args);
  }

  Outcome ReadIndex(Machine& m, const Value& container, const Vector<Value>& indices, const Shape& base, const Shape& element) {
    if (IsNativeAddress(base, container)) {
      return ReadNative(container, indices, base);
    }

    const HeapObject* o = ObjectOf(m.heap, container);

    if (o == nullptr) {
      return Trap("indexing a value that is not a container");
    }

    return readFrom(m, *o, container, indices, base, element);
  }

  Outcome WriteIndex(Machine& m, const Value& container, const Vector<Value>& indices, const Shape& base, Value v) {
    if (IsNativeAddress(base, container)) {
      return WriteNative(container, indices, base, v);
    }

    HeapObject* o = ObjectOf(m.heap, container);

    if (o == nullptr) {
      return Trap("assigning into a value that is not a container");
    }

    return writeInto(m, *o, container, indices, base, move(v));
  }

  Value NewContainerOf(Machine& m, const Shape& type, const Vector<Value>& sizes) {
    if (type.type == nullptr) {
      return NewArray(m.heap, {});
    }

    return containerOf(m, *type.type, sizes);
  }

  Value NewStructOf(Machine& m, int32_t structIndex) {
    const StructLayout& layout = m.program->structs[static_cast<size_t>(structIndex)];
    HeapObject object;
    object.kind = ObjectKind::Struct;
    object.name = layout.name;
    object.items = layout.defaults;

    return RefValue(Alloc(m.heap, move(object)));
  }

}  // namespace polyir::vm
