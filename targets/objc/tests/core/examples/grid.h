#pragma once

#import <setjmp.h>
#import <stdbool.h>
#import <stdint.h>
#import <stdio.h>
#import <stdlib.h>
#import <string.h>

#ifndef POLYIR_CORE_H
#define POLYIR_CORE_H

// Memory: what polyir shares (a struct, a list, a map) is allocated here and never freed. `copyBytes` keeps a value on the
// heap, so an optional, a variant or a closure's environment outlives the code that made it.
static void* gcAlloc(size_t size) {
  void* memory = calloc(1, size > 0 ? size : 1);

  if (memory == NULL) {
    fputs("trap: out of memory\n", stderr);
    exit(1);
  }

  return memory;
}

static void* copyBytes(const void* from, size_t size) {
  void* to = gcAlloc(size);
  memcpy(to, from, size);

  return to;
}

// Stops the program with a message, as polyir's `fail` does.
_Noreturn static void fail(const char* message) {
  fprintf(stderr, "trap: %s\n", message);
  exit(1);
}

// `throw` and `try`: each `try` puts a handler on the stack, and a throw jumps to the latest one with its code. A check that
// fails throws polyir's code for it (-1 an index outside its container, -3 an empty optional), or stops the program when no
// `try` is there to catch it.
typedef struct Handler {
  jmp_buf jump;
  struct Handler* outer;
  int64_t code;
} Handler;

static Handler* handlers = NULL;

_Noreturn static void throwCode(int64_t code) {
  if (handlers == NULL) {
    fail("a value was thrown and nothing caught it");
  }

  handlers->code = code;
  longjmp(handlers->jump, 1);
}

_Noreturn static void checkFailed(int64_t code, const char* message) {
  if (handlers == NULL) {
    fail(message);
  }

  throwCode(code);
}

// The value an optional holds (a pointer to it).
static void* present(void* value) {
  if (value == NULL) {
    checkFailed(-3, "unwrap of an empty optional");
  }

  return value;
}

// How two values of a map's keys or a list's items compare and hash: by their bytes, unless their type has functions of its
// own (a string, a struct).
typedef bool (*SameFn)(const void* a, const void* b, size_t size);
typedef uint64_t (*HashFn)(const void* value, size_t size);

static bool sameBytes(const void* a, const void* b, size_t size) {
  return memcmp(a, b, size) == 0;
}

static uint64_t hashBytes(const void* value, size_t size) {
  uint64_t hash = 1469598103934665603u;

  for (size_t i = 0; i < size; i++) {
    hash = (hash ^ ((const unsigned char*) value)[i]) * 1099511628211u;
  }

  return hash;
}

static bool sameTextKey(const void* a, const void* b, size_t size) {
  (void) size;

  return strcmp(*(const char* const*) a, *(const char* const*) b) == 0;
}

static uint64_t hashTextKey(const void* value, size_t size) {
  (void) size;
  const char* text = *(const char* const*) value;

  return hashBytes(text, strlen(text));
}
#endif

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

static int64_t fillAndWeight(int64_t scale);
static int64_t singleCellRoundTrip(void);
static int64_t dimsCheck(void);

static int64_t fillAndWeight(int64_t scale) {
  Grid* g = GRID_NEW(int64_t, 0, 3, 4);
  for (int64_t r = 0; r < 3; r = r + 1) {
    for (int64_t c = 0; c < 4; c = c + 1) {
      GRID_AT(g, int64_t, r, c) = (r * 4 + c) * scale;
    }
  }
  int64_t total = 0;
  for (int64_t r = 0; r < 3; r = r + 1) {
    for (int64_t c = 0; c < 4; c = c + 1) {
      total = total + GRID_AT(g, int64_t, r, c) * (r + 1);
    }
  }
  return total;
}

static int64_t singleCellRoundTrip(void) {
  Grid* g = GRID_NEW(int64_t, 0, 2, 2);
  GRID_AT(g, int64_t, 0, 0) = 10;
  GRID_AT(g, int64_t, 0, 1) = 20;
  GRID_AT(g, int64_t, 1, 0) = 30;
  GRID_AT(g, int64_t, 1, 1) = 40;
  return GRID_AT(g, int64_t, 1, 0) - GRID_AT(g, int64_t, 0, 1);
}

static int64_t dimsCheck(void) {
  Grid* g = GRID_NEW(int64_t, 0, 3, 4);
  return g->dims[0] * 100 + g->dims[1];
}
