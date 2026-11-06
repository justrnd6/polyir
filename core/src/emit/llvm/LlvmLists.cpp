// Lists, buffers, grids, tensors and vectors, and the string operations that go with them.
//
//   mutarray / array   a heap header of three cells (length, capacity, pointer to the elements), so a copy of the value is the same list
//   buffer / T[N]      a block of N elements (the length is in the type), zeroed
//   grid / tensor      a zeroed block of rows*cols (or the product of the dimensions) elements, indexed in row-major order
//   vec<T,N>           an LLVM vector
//
// Reading outside a list, buffer, grid or tensor stops the program.
#include "polyir/Std.hpp"
#include "emit/llvm/LlvmEmitter.hpp"

namespace polyir::llvm {

  namespace {

    bool isHeaderList(const TypePtr& type) {
      return type->kind == TypeKind::DynamicArray;
    }

    // The values whose copy is the value itself: strings, closures and the sum types are never changed in place.
    bool isOwnCopy(const TypePtr& type) {
      const TypeKind k = type->kind;

      return k == TypeKind::String || k == TypeKind::Closure || k == TypeKind::Variant || k == TypeKind::Either || k == TypeKind::Optional || k == TypeKind::Coroutine;
    }

    bool isBlock(const TypePtr& type) {
      const TypeKind k = type->kind;

      return k == TypeKind::Buffer || k == TypeKind::FixedArray || k == TypeKind::Grid || k == TypeKind::Tensor;
    }

    int64_t product(const Vector<int>& dims) {
      int64_t total = 1;

      for (const int d : dims) {
        total *= d;
      }

      return total;
    }

    [[noreturn]] void notYet(const TypePtr& type) {
      Fail(TypeText(*type) + " is not supported by the LLVM compiler yet");
    }

    Value intConstant(int64_t number) {
      return Value{to_string(number), MakeInt(64)};
    }

    // The list or block that a header-list-only operation works on.
    const Value& headerList(const Value& value) {
      if (!isHeaderList(value.type)) {
        notYet(value.type);
      }

      return value;
    }

    // How many elements each index of a block runs over.
    Vector<int64_t> extentsOf(const TypePtr& type) {
      if (type->kind == TypeKind::Grid) {
        return {type->size, type->cols};
      }

      if (type->kind == TypeKind::Tensor) {
        return Vector<int64_t>(type->dims.begin(), type->dims.end());
      }

      return {type->size};
    }

    bool isContainerBuiltin(Builtin op) {
      return op == Builtin::Len || op == Builtin::Capacity || op == Builtin::Rows || op == Builtin::Cols || op == Builtin::Dim || op == Builtin::Push ||
             op == Builtin::Copy || op == Builtin::With || op == Builtin::Retype || op == Builtin::Free || op == Builtin::Truncate;
    }

  }  // namespace

  // The number of elements a block holds.
  int64_t BlockCount(const TypePtr& type) {
    switch (type->kind) {
      case TypeKind::Grid:
        return static_cast<int64_t>(type->size) * type->cols;
      case TypeKind::Tensor:
        return product(type->dims);
      default:
        return type->size;
    }
  }

  Value DataBlock(FunctionEmitter& emitter, const TypePtr& elementType, int64_t count) {
    return AllocateZeroed(emitter, to_string(count), SizeOf(emitter, elementType), LayoutOf(elementType));
  }

  // A list header with room for `capacity` elements and none in it. The header's third word is the elements' block, so the collector looks
  // inside the header; it looks inside the block when the elements are references.
  Value ListHeader(FunctionEmitter& emitter, const TypePtr& elementType, const String& capacity) {
    const Value header = Allocate(emitter, "24", Layout::References);
    Store(emitter, Value{"0", MakeInt(64)}, WordAddress(emitter, header.text, 0));
    Store(emitter, Value{capacity, MakeInt(64)}, WordAddress(emitter, header.text, 1));
    const Value data = AllocateZeroed(emitter, capacity, SizeOf(emitter, elementType), LayoutOf(elementType));
    Store(emitter, data, WordAddress(emitter, header.text, 2));

    return header;
  }

  Value ListLength(FunctionEmitter& emitter, const Value& list) {
    return LoadFrom(emitter, WordAddress(emitter, list.text, 0), MakeInt(64));
  }

  Value IndexValue(FunctionEmitter& emitter, const Value& index) {
    return Resized(emitter, index, 64, IsSigned(*index.type));
  }

  // An index outside [0, limit) stops the program (a negative one counts as a very large number).
  void BoundsCheck(FunctionEmitter& emitter, const Value& index, const String& limit) {
    TrapUnless(emitter, Instruction(emitter, "icmp ult i64 " + IndexValue(emitter, index).text + ", " + limit), "stopped, an index is out of range");
  }

  String ListAddress(FunctionEmitter& emitter, const Value& list, const Value& index) {
    BoundsCheck(emitter, index, ListLength(emitter, list).text);
    const Value data = LoadFrom(emitter, WordAddress(emitter, list.text, 2), MakePointer(list.type->element));

    return Instruction(emitter, "getelementptr " + LlvmTypeText(list.type->element) + ", ptr " + data.text + ", i64 " + IndexValue(emitter, index).text);
  }

  // `p[i]` on a bare pointer: no length is known, so nothing is checked.
  String PointerAddress(FunctionEmitter& emitter, const Value& pointer, const Value& index) {
    return Instruction(emitter, "getelementptr " + LlvmTypeText(pointer.type->element) + ", ptr " + pointer.text + ", i64 " + IndexValue(emitter, index).text);
  }

  String ElementAddress(FunctionEmitter& emitter, const Expr& expr, const Value& base, const Vector<Value>& indices) {
    if (isHeaderList(base.type)) {
      return ListAddress(emitter, base, indices[0]);
    }

    if (base.type->kind == TypeKind::Hashmap) {
      return MapWriteCell(emitter, base, indices[0]);
    }

    if (base.type->kind == TypeKind::Sparse) {
      return SparseCell(emitter, base, indices);
    }

    if (base.type->kind == TypeKind::Pointer) {
      return PointerAddress(emitter, base, indices[0]);
    }

    return BlockElement(emitter, expr, base, indices);
  }

  String BlockElement(FunctionEmitter& emitter, const Expr& expr, const Value& base, const Vector<Value>& indices) {
    if (!isBlock(base.type)) {
      notYet(base.type);
    }

    return BlockAddress(emitter, expr, base, indices);
  }

  // Where the element at `indices` of a block lives: the offset in row-major order, after each index is checked against its extent.
  String BlockAddress(FunctionEmitter& emitter, const Expr& expr, const Value& base, const Vector<Value>& indices) {
    const Vector<int64_t> extents = extentsOf(base.type);
    String offset = "0";

    for (size_t i = 0; i < indices.size(); i++) {
      offset = NextOffset(emitter, offset, extents[i], indices[i]);
    }

    return Instruction(emitter, "getelementptr " + LlvmTypeText(expr.type) + ", ptr " + base.text + ", i64 " + offset);
  }

  // The row-major offset after one more index: the offset so far times the extent, plus the index.
  String NextOffset(FunctionEmitter& emitter, const String& offset, int64_t extent, const Value& index) {
    BoundsCheck(emitter, index, to_string(extent));
    const String scaled = Instruction(emitter, "mul i64 " + offset + ", " + to_string(extent));

    return Instruction(emitter, "add i64 " + scaled + ", " + IndexValue(emitter, index).text);
  }

  // The values of the operands from number `from` on.
  Vector<Value> OperandValues(FunctionEmitter& emitter, const Expr& expr, size_t from) {
    Vector<Value> values;

    for (size_t i = from; i < expr.operands.size(); i++) {
      values.push_back(EmitExpr(emitter, *expr.operands[i]));
    }

    return values;
  }

  Value IndexGet(FunctionEmitter& emitter, const Expr& expr) {
    const Value base = EmitExpr(emitter, *expr.operands[0]);
    const Vector<Value> indices = OperandValues(emitter, expr, 1);

    if (base.type->kind == TypeKind::Vec) {
      return VectorElement(emitter, expr, base, indices[0]);
    }

    if (base.type->kind == TypeKind::Hashmap) {
      return LoadFrom(emitter, MapReadCell(emitter, base, indices[0]), expr.type);
    }

    if (base.type->kind == TypeKind::Sparse) {
      return SparseRead(emitter, expr, base, indices);
    }

    return LoadFrom(emitter, ElementAddress(emitter, expr, base, indices), expr.type);
  }

  Value VectorElement(FunctionEmitter& emitter, const Expr& expr, const Value& vector, const Value& index) {
    BoundsCheck(emitter, index, to_string(vector.type->size));

    return Value{Instruction(emitter, "extractelement " + Typed(vector) + ", i64 " + IndexValue(emitter, index).text), expr.type};
  }

  Value VecLit(FunctionEmitter& emitter, const Expr& expr) {
    const Vector<Value> items = OperandValues(emitter, expr, 0);

    if (expr.type->kind == TypeKind::Vec) {
      return VectorOf(emitter, expr.type, items);
    }

    if (expr.type->kind == TypeKind::Set) {
      return SetLiteral(emitter, expr);
    }

    return isHeaderList(expr.type) ? ListOf(emitter, expr.type, items) : BlockOf(emitter, expr.type, items);
  }

  Value VectorOf(FunctionEmitter& emitter, const TypePtr& type, const Vector<Value>& items) {
    String vector = "zeroinitializer";

    for (size_t i = 0; i < items.size(); i++) {
      vector = Instruction(emitter, "insertelement " + LlvmTypeText(type) + " " + vector + ", " + Typed(Fit(emitter, items[i], type->element)) + ", i32 " + to_string(i));
    }

    return Value{vector, type};
  }

  Value ListOf(FunctionEmitter& emitter, const TypePtr& type, const Vector<Value>& items) {
    Value list = ListHeader(emitter, type->element, to_string(items.size()));
    list.type = type;

    for (const Value& item : items) {
      ListPush(emitter, list, item);
    }

    return list;
  }

  Value BlockOf(FunctionEmitter& emitter, const TypePtr& type, const Vector<Value>& items) {
    if (!isBlock(type)) {
      notYet(type);
    }

    Value block = DataBlock(emitter, type->element, BlockCount(type));
    StoreItems(emitter, block, type->element, items);
    block.type = type;

    return block;
  }

  void StoreItems(FunctionEmitter& emitter, const Value& block, const TypePtr& elementType, const Vector<Value>& items) {
    for (size_t i = 0; i < items.size(); i++) {
      Store(emitter, items[i], Instruction(emitter, "getelementptr " + LlvmTypeText(elementType) + ", ptr " + block.text + ", i64 " + to_string(i)));
    }
  }

  Value NewContainer(FunctionEmitter& emitter, const Expr& expr) {
    const TypeKind kind = expr.type->kind;

    if (kind == TypeKind::Hashmap || kind == TypeKind::Set) {
      return NewMapFrom(emitter, expr);
    }

    return EmptyContainer(emitter, expr.type);
  }

  Value EmptyContainer(FunctionEmitter& emitter, const TypePtr& type) {
    if (type->kind == TypeKind::Sparse) {
      return EmptySparse(emitter, type);
    }

    if (!isHeaderList(type) && !isBlock(type)) {
      notYet(type);
    }

    Value made = isHeaderList(type) ? ListHeader(emitter, type->element, "0") : DataBlock(emitter, type->element, BlockCount(type));
    made.type = type;

    return made;
  }

  // Adds an element, growing the elements' room first when the list is full. The result is the list.
  Value ListPush(FunctionEmitter& emitter, const Value& list, const Value& element) {
    const Value length = ListLength(emitter, list);
    const String add = GrowIfFull(emitter, list, length);
    StartBlock(emitter, add);
    AppendElement(emitter, list, element, length);

    return list;
  }

  // Branches to a block that grows the list when it is full; both paths meet at the block named by the result.
  String GrowIfFull(FunctionEmitter& emitter, const Value& list, const Value& length) {
    const Value capacity = LoadFrom(emitter, WordAddress(emitter, list.text, 1), MakeInt(64));
    const String full = Instruction(emitter, "icmp eq i64 " + length.text + ", " + capacity.text);

    const String grow = UniqueName(emitter, "push.grow");
    const String add = UniqueName(emitter, "push.add");
    Terminate(emitter, "br i1 " + full + ", label %" + grow + ", label %" + add);
    GrowBlock(emitter, grow, add, list, capacity);

    return add;
  }

  // Doubles the room (four elements for a list with none) in a block of its own.
  void GrowBlock(FunctionEmitter& emitter, const String& name, const String& next, const Value& list, const Value& capacity) {
    StartBlock(emitter, name);
    const String bigger = BiggerCapacity(emitter, capacity);
    Reallocate(emitter, list, bigger);
    Store(emitter, Value{bigger, MakeInt(64)}, WordAddress(emitter, list.text, 1));
    BranchTo(emitter, next);
  }

  String BiggerCapacity(FunctionEmitter& emitter, const Value& capacity) {
    const String empty = Instruction(emitter, "icmp eq i64 " + capacity.text + ", 0");
    const String doubled = Instruction(emitter, "mul i64 " + capacity.text + ", 2");

    return Instruction(emitter, "select i1 " + empty + ", i64 4, i64 " + doubled);
  }

  // gc_resize is realloc, and the block keeps its place in the collector's table under its new address.
  void Reallocate(FunctionEmitter& emitter, const Value& list, const String& capacity) {
    const String bytes = Instruction(emitter, "mul i64 " + capacity + ", " + SizeOf(emitter, list.type->element));
    const Value old = LoadFrom(emitter, WordAddress(emitter, list.text, 2), MakePointer(list.type->element));
    NoteAllocation(emitter);
    const String moved = Instruction(emitter, "call ptr @gc_resize(ptr " + old.text + ", i64 " + bytes + ")");
    Store(emitter, Value{moved, MakePointer(list.type->element)}, WordAddress(emitter, list.text, 2));
  }

  void AppendElement(FunctionEmitter& emitter, const Value& list, const Value& element, const Value& length) {
    const Value data = LoadFrom(emitter, WordAddress(emitter, list.text, 2), MakePointer(list.type->element));
    HeapStore(emitter, element, Instruction(emitter, "getelementptr " + LlvmTypeText(list.type->element) + ", ptr " + data.text + ", i64 " + length.text));
    Store(emitter, Value{Instruction(emitter, "add i64 " + length.text + ", 1"), MakeInt(64)}, WordAddress(emitter, list.text, 0));
  }

  void Memcopy(FunctionEmitter& emitter, const String& to, const String& from, const String& bytes) {
    Need(emitter.context_, "declare void @llvm.memcpy.p0.p0.i64(ptr, ptr, i64, i1)");
    Line(emitter, "call void @llvm.memcpy.p0.p0.i64(ptr " + to + ", ptr " + from + ", i64 " + bytes + ", i1 false)");
  }

  Value ListCopy(FunctionEmitter& emitter, const Value& list) {
    const Value length = ListLength(emitter, list);
    Value copy = ListHeader(emitter, list.type->element, length.text);
    CopyElements(emitter, list, copy, length);
    Store(emitter, length, WordAddress(emitter, copy.text, 0));
    copy.type = list.type;

    return copy;
  }

  // `left + right` on lists: a new list holding the elements of `left` and then those of `right`.
  Value ListConcat(FunctionEmitter& emitter, const Expr& expr, const Value& left, const Value& right) {
    const Value leftLength = ListLength(emitter, left);
    const Value rightLength = ListLength(emitter, right);
    const Value total{Instruction(emitter, "add i64 " + leftLength.text + ", " + rightLength.text), MakeInt(64)};

    const Value result = ListHeader(emitter, left.type->element, total.text);
    CopyElements(emitter, left, result, leftLength);
    CopyElementsAfter(emitter, right, result, leftLength, rightLength);

    return Concatenated(emitter, expr, result, total);
  }

  Value Concatenated(FunctionEmitter& emitter, const Expr& expr, Value list, const Value& total) {
    Store(emitter, total, WordAddress(emitter, list.text, 0));
    list.type = expr.type;

    return list;
  }

  // The `count` elements of `list`, written into `to` from element `offset` on.
  void CopyElementsAfter(FunctionEmitter& emitter, const Value& list, const Value& to, const Value& offset, const Value& count) {
    const TypePtr pointee = MakePointer(list.type->element);
    const Value from = LoadFrom(emitter, WordAddress(emitter, list.text, 2), pointee);
    const Value data = LoadFrom(emitter, WordAddress(emitter, to.text, 2), pointee);
    const String at = Instruction(emitter, "getelementptr " + LlvmTypeText(list.type->element) + ", ptr " + data.text + ", i64 " + offset.text);
    Memcopy(emitter, at, from.text, Instruction(emitter, "mul i64 " + count.text + ", " + SizeOf(emitter, list.type->element)));
  }

  void CopyElements(FunctionEmitter& emitter, const Value& list, const Value& copy, const Value& length) {
    const Value from = LoadFrom(emitter, WordAddress(emitter, list.text, 2), MakePointer(list.type->element));
    const Value to = LoadFrom(emitter, WordAddress(emitter, copy.text, 2), MakePointer(list.type->element));
    Memcopy(emitter, to.text, from.text, Instruction(emitter, "mul i64 " + length.text + ", " + SizeOf(emitter, list.type->element)));
  }

  Value CopyBlock(FunctionEmitter& emitter, const Value& block) {
    const TypePtr& type = block.type;
    const String bytes = Instruction(emitter, "mul i64 " + to_string(BlockCount(type)) + ", " + SizeOf(emitter, type->element));
    Value copy = Allocate(emitter, bytes, LayoutOf(type->element));
    Memcopy(emitter, copy.text, block.text, bytes);
    copy.type = type;

    return copy;
  }

  Value StringLength(FunctionEmitter& emitter, const Value& string) {
    Need(emitter.context_, "declare i64 @strlen(ptr)");

    return Value{Instruction(emitter, "call i64 @strlen(ptr " + string.text + ")"), MakeInt(64)};
  }

  // The length of a buffer, grid, tensor or vector is part of its type.
  Value FixedLength(const TypePtr& type) {
    if (!isBlock(type) && type->kind != TypeKind::Vec) {
      notYet(type);
    }

    return intConstant(type->kind == TypeKind::Vec ? type->size : BlockCount(type));
  }

  Value LengthOf(FunctionEmitter& emitter, const Value& container) {
    const TypePtr& type = container.type;

    if (isHeaderList(type)) {
      return ListLength(emitter, container);
    }

    if (type->kind == TypeKind::Sparse) {
      return ListLength(emitter, SparseList(emitter, container, 0));
    }

    return type->kind == TypeKind::String ? StringLength(emitter, container) : FixedLength(type);
  }

  Value StringEquals(FunctionEmitter& emitter, const Value& left, const Value& right) {
    Need(emitter.context_, "declare i32 @strcmp(ptr, ptr)");
    const String order = Instruction(emitter, "call i32 @strcmp(ptr " + left.text + ", ptr " + right.text + ")");

    return Value{Instruction(emitter, "icmp eq i32 " + order + ", 0"), MakeBool()};
  }

  // `left + right` on strings: a new string holding the characters of `left` and then those of `right`.
  Value StringConcat(FunctionEmitter& emitter, const Value& left, const Value& right) {
    const String leftLength = StringLength(emitter, left).text;
    const String rightLength = StringLength(emitter, right).text;
    const Value joined = Allocate(emitter, ConcatSize(emitter, leftLength, rightLength), Layout::Data);
    Memcopy(emitter, joined.text, left.text, leftLength);
    CopyTail(emitter, joined, right, leftLength, rightLength);

    return Value{joined.text, MakeString()};
  }

  // The bytes of two strings and the terminating zero.
  String ConcatSize(FunctionEmitter& emitter, const String& leftLength, const String& rightLength) {
    const String total = Instruction(emitter, "add i64 " + leftLength + ", " + rightLength);

    return Instruction(emitter, "add i64 " + total + ", 1");
  }

  // Copies `right` (with its terminating zero) into `joined`, after the `leftLength` bytes already there.
  void CopyTail(FunctionEmitter& emitter, const Value& joined, const Value& right, const String& leftLength, const String& rightLength) {
    const String tail = Instruction(emitter, "getelementptr i8, ptr " + joined.text + ", i64 " + leftLength);
    Memcopy(emitter, tail, right.text, Instruction(emitter, "add i64 " + rightLength + ", 1"));
  }

  // The string built-ins: `hash`, the text ones (LlvmText.cpp), `==`, and `+`, which joins lists as well as strings.
  bool StringBuiltin(FunctionEmitter& emitter, const Expr& expr, Value& out) {
    if (expr.builtin == Builtin::Hash) {
      out = StringHash(emitter, EmitExpr(emitter, *expr.operands[0]));
      return true;
    }

    return TextBuiltin(emitter, expr, out) || EqualsOrConcatBuiltin(emitter, expr, out);
  }

  // `==` on strings, and `+` on strings or on lists (the checker makes both kinds of `+` a Concat).
  bool EqualsOrConcatBuiltin(FunctionEmitter& emitter, const Expr& expr, Value& out) {
    if (expr.builtin != Builtin::StrEq && expr.builtin != Builtin::Concat) {
      return false;
    }

    const Value left = EmitExpr(emitter, *expr.operands[0]);
    const Value right = EmitExpr(emitter, *expr.operands[1]);

    if (expr.builtin == Builtin::StrEq) {
      out = StringEquals(emitter, left, right);
    } else if (isHeaderList(left.type)) {
      out = ListConcat(emitter, expr, left, right);
    } else {
      out = StringConcat(emitter, left, right);
    }

    return true;
  }

  bool ContainerBuiltin(FunctionEmitter& emitter, const Expr& expr, Value& out) {
    if (!isContainerBuiltin(expr.builtin)) {
      return false;
    }

    out = ContainerOperation(emitter, expr, EmitExpr(emitter, *expr.operands[0]));

    return true;
  }

  Value ContainerOperation(FunctionEmitter& emitter, const Expr& expr, const Value& value) {
    switch (expr.builtin) {
      case Builtin::Len:
        return LengthOf(emitter, value);
      case Builtin::Capacity:
        return isHeaderList(value.type) ? LoadFrom(emitter, WordAddress(emitter, value.text, 1), MakeInt(64)) : LengthOf(emitter, value);
      case Builtin::Rows:
        return intConstant(value.type->size);
      case Builtin::Cols:
        return intConstant(value.type->cols);
      default:
        return ContainerEdit(emitter, expr, value);
    }
  }

  Value ContainerEdit(FunctionEmitter& emitter, const Expr& expr, const Value& value) {
    switch (expr.builtin) {
      case Builtin::Dim:
        return intConstant(value.type->dims.at(static_cast<size_t>(expr.operands[1]->intValue)));
      case Builtin::Push:
        return ListPush(emitter, headerList(value), EmitExpr(emitter, *expr.operands[1]));
      case Builtin::With:
        return ListWith(emitter, expr, value);
      case Builtin::Truncate:
        return ListTruncate(emitter, headerList(value), EmitExpr(emitter, *expr.operands[1]));
      default:
        return RetypeFreeOrCopy(emitter, expr, value);
    }
  }

  Value RetypeFreeOrCopy(FunctionEmitter& emitter, const Expr& expr, const Value& value) {
    if (expr.builtin == Builtin::Retype) {
      return Value{value.text, expr.type};
    }

    return expr.builtin == Builtin::Free ? FreeContainer(emitter, value) : CopyContainer(emitter, value);
  }

  namespace {

    // The number, or 0 when it is negative.
    String atLeastZero(FunctionEmitter& emitter, const String& number) {
      const String isPositive = Instruction(emitter, "icmp sgt i64 " + number + ", 0");

      return Instruction(emitter, "select i1 " + isPositive + ", i64 " + number + ", i64 0");
    }

  }  // namespace

  // Keeps the first `length` elements: the new length is the smaller of the current one and the (non-negative) request. The result is the list.
  Value ListTruncate(FunctionEmitter& emitter, const Value& list, const Value& length) {
    const String nonNegative = atLeastZero(emitter, IndexValue(emitter, length).text);
    const Value current = ListLength(emitter, list);
    const String isShorter = Instruction(emitter, "icmp ult i64 " + nonNegative + ", " + current.text);
    const Value newLength{Instruction(emitter, "select i1 " + isShorter + ", i64 " + nonNegative + ", i64 " + current.text), MakeInt(64)};
    Store(emitter, newLength, WordAddress(emitter, list.text, 0));

    return list;
  }

  // The element is worked out before the copy is made: it may call, and the copy is in no root until the push is done.
  Value ListWith(FunctionEmitter& emitter, const Expr& expr, const Value& list) {
    const Value element = EmitExpr(emitter, *expr.operands[1]);
    Value result = ListPush(emitter, ListCopy(emitter, headerList(list)), element);
    result.type = expr.type;

    return result;
  }

  Value FreeContainer(FunctionEmitter& emitter, const Value& value) {
    if (isHeaderList(value.type)) {
      FreeHint(emitter, LoadFrom(emitter, WordAddress(emitter, value.text, 2), MakePointer(value.type->element)).text);
    }

    FreeHint(emitter, value.text);

    return Value{"", MakeVoid()};
  }

  Value CopyContainer(FunctionEmitter& emitter, const Value& value) {
    if (value.type->kind == TypeKind::Struct) {
      return CopyStruct(emitter, value);
    }

    if (value.type->kind == TypeKind::Sparse) {
      return CopySparse(emitter, value);
    }

    if (isOwnCopy(value.type)) {
      return value;
    }

    if (!isHeaderList(value.type) && !isBlock(value.type)) {
      notYet(value.type);
    }

    return isHeaderList(value.type) ? ListCopy(emitter, value) : CopyBlock(emitter, value);
  }

  // A struct with a `var` field is an object: its copy is a new one holding the same field values. Any other struct is its own copy.
  Value CopyStruct(FunctionEmitter& emitter, const Value& object) {
    if (IsValueStruct(emitter, object.type) || FindInterface(ModuleOf(emitter.context_), object.type->name)) {
      return object;
    }

    const String structType = StructName(object.type->name);
    const Value fresh = Allocate(emitter, StructSize(emitter, structType), StructLayout(emitter, object.type->name));
    const String fields = Instruction(emitter, "load " + structType + ", ptr " + object.text);
    Line(emitter, "store " + structType + " " + fields + ", ptr " + fresh.text);

    return Value{fresh.text, object.type};
  }

}  // namespace polyir::llvm
