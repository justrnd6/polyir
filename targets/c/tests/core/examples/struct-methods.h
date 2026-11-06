#pragma once

#include <setjmp.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

typedef struct Point Point;
typedef struct Circle Circle;
typedef struct Pair Pair;

struct Point {
  double x;
  double y;
};

struct Circle {
  double r;
};

struct Pair {
  int64_t a;
  int64_t b;
};

static Point* Point_new(double x, double y);
static bool Point_equal(Point* a, Point* b);
static uint64_t Point_hash(Point* a);
static double Point_dist(Point* self);

static Circle* Circle_new(double r);
static bool Circle_equal(Circle* a, Circle* b);
static uint64_t Circle_hash(Circle* a);
static double Circle_dist(Circle* self);

static double distSum(double px, double py, double r);

static Pair* Pair_new(int64_t a, int64_t b);
static bool Pair_equal(Pair* a, Pair* b);
static uint64_t Pair_hash(Pair* a);
static int64_t Pair_sum(Pair* self);
static int64_t Pair_doubledSum(Pair* self);

static int64_t doubledSumCheck(int64_t a, int64_t b);

static Point* Point_new(double x, double y) {
  Point* made = gcAlloc(sizeof(Point));
  made->x = x;
  made->y = y;
  return made;
}

static bool Point_equal(Point* a, Point* b) {
  if (a == b || a == NULL || b == NULL) {
    return a == b;
  }

  bool same = true;
  same = same && a->x == b->x;
  same = same && a->y == b->y;
  return same;
}

static uint64_t Point_hash(Point* a) {
  uint64_t hash = 17;
  hash = hash * 31 + hashBytes(&a->x, sizeof a->x);
  hash = hash * 31 + hashBytes(&a->y, sizeof a->y);
  return hash;
}

static double Point_dist(Point* self) {
  return self->x * self->x + self->y * self->y;
}

static Circle* Circle_new(double r) {
  Circle* made = gcAlloc(sizeof(Circle));
  made->r = r;
  return made;
}

static bool Circle_equal(Circle* a, Circle* b) {
  if (a == b || a == NULL || b == NULL) {
    return a == b;
  }

  bool same = true;
  same = same && a->r == b->r;
  return same;
}

static uint64_t Circle_hash(Circle* a) {
  uint64_t hash = 17;
  hash = hash * 31 + hashBytes(&a->r, sizeof a->r);
  return hash;
}

static double Circle_dist(Circle* self) {
  return self->r * self->r;
}

static double distSum(double px, double py, double r) {
  Point* p = Point_new(px, py);
  Circle* c = Circle_new(r);
  return Point_dist(p) + Circle_dist(c);
}

static Pair* Pair_new(int64_t a, int64_t b) {
  Pair* made = gcAlloc(sizeof(Pair));
  made->a = a;
  made->b = b;
  return made;
}

static bool Pair_equal(Pair* a, Pair* b) {
  if (a == b || a == NULL || b == NULL) {
    return a == b;
  }

  bool same = true;
  same = same && a->a == b->a;
  same = same && a->b == b->b;
  return same;
}

static uint64_t Pair_hash(Pair* a) {
  uint64_t hash = 17;
  hash = hash * 31 + (uint64_t) a->a;
  hash = hash * 31 + (uint64_t) a->b;
  return hash;
}

static int64_t Pair_sum(Pair* self) {
  return self->a + self->b;
}

static int64_t Pair_doubledSum(Pair* self) {
  return Pair_sum(self) * 2;
}

static int64_t doubledSumCheck(int64_t a, int64_t b) {
  Pair* p = Pair_new(a, b);
  return Pair_doubledSum(p);
}
