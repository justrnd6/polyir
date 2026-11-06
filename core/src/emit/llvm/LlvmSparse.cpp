// A sparse<T>: a block of three cells, each the header of a list (see LlvmLists.cpp): the row keys, the column keys and the values. Writing appends
// to all three, so the last entry for a (row, column) wins; reading looks from the end for the latest entry, and a cell never written reads as zero.
// `sp[i]` is `sp[i, 0]`, and the length is the number of entries written, stale ones included.
#include "polyir/Std.hpp"
#include "emit/llvm/LlvmEmitter.hpp"

namespace polyir::llvm {

  namespace {

    const char* const kZero = "@sparse_zero = internal constant [8 x i64] zeroinitializer";

    // The position of the latest entry for a (row, column), or -1.
    const char* const kFind = R"(define i64 @sparse_find(ptr %sparse, i64 %row, i64 %col) {
entry:
  %rows = load ptr, ptr %sparse
  %colsAt = getelementptr ptr, ptr %sparse, i64 1
  %cols = load ptr, ptr %colsAt
  %length = load i64, ptr %rows
  %rowsAt = getelementptr i64, ptr %rows, i64 2
  %rowData = load ptr, ptr %rowsAt
  %colsDataAt = getelementptr i64, ptr %cols, i64 2
  %colData = load ptr, ptr %colsDataAt
  br label %test
test:
  %i = phi i64 [ %length, %entry ], [ %prev, %next ]
  %more = icmp sgt i64 %i, 0
  br i1 %more, label %check, label %miss
check:
  %prev = sub i64 %i, 1
  %rowAt = getelementptr i64, ptr %rowData, i64 %prev
  %r = load i64, ptr %rowAt
  %colAt = getelementptr i64, ptr %colData, i64 %prev
  %c = load i64, ptr %colAt
  %sameRow = icmp eq i64 %r, %row
  %sameCol = icmp eq i64 %c, %col
  %both = and i1 %sameRow, %sameCol
  br i1 %both, label %hit, label %next
next:
  br label %test
hit:
  ret i64 %prev
miss:
  ret i64 -1
})";

    Value wholeNumber(const String& text) {
      return Value{text, MakeInt(64)};
    }

  }  // namespace

  // Three empty lists side by side.
  Value EmptySparse(FunctionEmitter& emitter, const TypePtr& type) {
    Value made = Allocate(emitter, "24", Layout::References);

    for (int64_t k = 0; k < 3; k++) {
      Store(emitter, ListHeader(emitter, k == 2 ? type->element : MakeInt(64), "0"), WordAddress(emitter, made.text, k));
    }

    made.type = type;

    return made;
  }

  // A new sparse over copies of the three lists.
  Value CopySparse(FunctionEmitter& emitter, const Value& sparse) {
    Value made = Allocate(emitter, "24", Layout::References);

    for (int64_t k = 0; k < 3; k++) {
      Store(emitter, ListCopy(emitter, SparseList(emitter, sparse, k)), WordAddress(emitter, made.text, k));
    }

    made.type = sparse.type;

    return made;
  }

  // One of the three lists: the row keys (0), the column keys (1) or the values (2).
  Value SparseList(FunctionEmitter& emitter, const Value& sparse, int64_t which) {
    const TypePtr element = which == 2 ? sparse.type->element : MakeInt(64);

    return LoadFrom(emitter, WordAddress(emitter, sparse.text, which), MakeDynamicArray(element, true));
  }

  // The row and the column as 64-bit numbers; a single index is a row in column 0.
  Vector<Value> SparseKeys(FunctionEmitter& emitter, const Vector<Value>& indices) {
    const Value column = indices.size() > 1 ? IndexValue(emitter, indices[1]) : wholeNumber("0");

    return {IndexValue(emitter, indices[0]), column};
  }

  // The value of the cell: the latest entry's, or zero when there is none.
  Value SparseRead(FunctionEmitter& emitter, const Expr& expr, const Value& sparse, const Vector<Value>& indices) {
    Need(emitter.context_, kFind);
    Need(emitter.context_, kZero);
    const Vector<Value> keys = SparseKeys(emitter, indices);
    const String found = Instruction(emitter, "call i64 @sparse_find(ptr " + sparse.text + ", i64 " + keys[0].text + ", i64 " + keys[1].text + ")");

    return SparseEntry(emitter, expr.type, sparse, found);
  }

  // The value at position `found` of the values, or zero when it is -1.
  Value SparseEntry(FunctionEmitter& emitter, const TypePtr& type, const Value& sparse, const String& found) {
    const Value data = LoadFrom(emitter, WordAddress(emitter, SparseList(emitter, sparse, 2).text, 2), MakePointer(type));
    const String entry = Instruction(emitter, "getelementptr " + LlvmTypeText(type) + ", ptr " + data.text + ", i64 " + found);
    const String hit = Instruction(emitter, "icmp sge i64 " + found + ", 0");

    return LoadFrom(emitter, Instruction(emitter, "select i1 " + hit + ", ptr " + entry + ", ptr @sparse_zero"), type);
  }

  // Appends an entry (the value still zero) and gives the address of its value.
  String SparseCell(FunctionEmitter& emitter, const Value& sparse, const Vector<Value>& indices) {
    const Vector<Value> keys = SparseKeys(emitter, indices);
    ListPush(emitter, SparseList(emitter, sparse, 0), keys[0]);
    ListPush(emitter, SparseList(emitter, sparse, 1), keys[1]);

    const Value values = ListPush(emitter, SparseList(emitter, sparse, 2), Value{ZeroText(sparse.type->element), sparse.type->element});
    const String last = Instruction(emitter, "sub i64 " + ListLength(emitter, values).text + ", 1");
    const Value data = LoadFrom(emitter, WordAddress(emitter, values.text, 2), MakePointer(sparse.type->element));

    return Instruction(emitter, "getelementptr " + LlvmTypeText(sparse.type->element) + ", ptr " + data.text + ", i64 " + last);
  }

}  // namespace polyir::llvm
