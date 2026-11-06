#ifndef POLYIR_SPARSE_H
#define POLYIR_SPARSE_H

// A sparse grid keeps the cells written (rows, columns, values); a read finds the last write, else the zero.
typedef struct Sparse {
  List* rows;
  List* cols;
  List* values;
  void* zero;
} Sparse;

static Sparse* sparseNew(size_t size, const void* zero) {
  Sparse* sparse = gcAlloc(sizeof(Sparse));
  sparse->rows = listNew(sizeof(int64_t));
  sparse->cols = listNew(sizeof(int64_t));
  sparse->values = listNew(size);
  sparse->zero = copyBytes(zero, size);

  return sparse;
}

static void* sparseGet(Sparse* sparse, int64_t row, int64_t col) {
  void* found = sparse->zero;

  for (int64_t i = 0; i < sparse->values->count; i++) {
    if (LIST_AT(sparse->rows, i, int64_t) == row && LIST_AT(sparse->cols, i, int64_t) == col) {
      found = listAt(sparse->values, i);
    }
  }

  return found;
}

static void sparseSet(Sparse* sparse, int64_t row, int64_t col, const void* value) {
  listPush(sparse->rows, &row);
  listPush(sparse->cols, &col);
  listPush(sparse->values, value);
}

static Sparse* sparseCopy(Sparse* from) {
  Sparse* sparse = copyBytes(from, sizeof(Sparse));
  sparse->rows = listCopy(from->rows);
  sparse->cols = listCopy(from->cols);
  sparse->values = listCopy(from->values);

  return sparse;
}
#endif
