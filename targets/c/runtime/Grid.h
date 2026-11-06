#ifndef POLYIR_GRID_H
#define POLYIR_GRID_H

// A grid, matrix or tensor: its sizes, and its cells in one row-major block. `GRID_AT(g, T, i, j)` is a cell, to read or to
// write; a cell outside it throws -1.
typedef struct Grid {
  int64_t dims[8];
  int64_t rank;
  size_t size;
  char* cells;
} Grid;

static Grid* gridNew(size_t size, const void* zero, int64_t rank, const int64_t* dims) {
  Grid* grid = gcAlloc(sizeof(Grid));
  int64_t count = 1;
  grid->rank = rank;
  grid->size = size;

  for (int64_t i = 0; i < rank; i++) {
    grid->dims[i] = dims[i];
    count *= dims[i];
  }

  grid->cells = gcAlloc(count * size);

  for (int64_t i = 0; i < count; i++) {
    memcpy(grid->cells + i * size, zero, size);
  }

  return grid;
}

static void* gridAt(Grid* grid, const int64_t* indices) {
  int64_t at = 0;

  for (int64_t i = 0; i < grid->rank; i++) {
    if (indices[i] < 0 || indices[i] >= grid->dims[i]) {
      checkFailed(-1, "index out of range");
    }

    at = at * grid->dims[i] + indices[i];
  }

  return grid->cells + at * grid->size;
}

static Grid* gridCopy(Grid* from) {
  Grid* grid = copyBytes(from, sizeof(Grid));
  int64_t count = 1;

  for (int64_t i = 0; i < from->rank; i++) {
    count *= from->dims[i];
  }

  grid->cells = copyBytes(from->cells, count * from->size);

  return grid;
}

#define GRID_AT(grid, T, ...) (*(T*) gridAt((grid), (int64_t[]){__VA_ARGS__}))
#define GRID_NEW(T, zero, ...) gridNew(sizeof(T), (T[]){zero}, sizeof((int64_t[]){__VA_ARGS__}) / sizeof(int64_t), (int64_t[]){__VA_ARGS__})
#endif
