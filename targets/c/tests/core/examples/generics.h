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

#ifndef POLYIR_EITHER_H
#define POLYIR_EITHER_H

// An `either`: whether it holds its value as ok or as an error, and the value, kept on the heap with its size.
typedef struct Either {
  bool isOk;
  void* held;
  size_t size;
} Either;

static Either makeEither(bool isOk, const void* value, size_t size) {
  return (Either){isOk, copyBytes(value, size), size};
}

// Two `either`s are the same when both are ok (or both errors) and hold the same bytes.
static bool eitherSame(Either a, Either b) {
  return a.isOk == b.isOk && a.size == b.size && memcmp(a.held, b.held, a.size) == 0;
}
#endif

typedef struct Box_i64 Box_i64;
typedef struct Box_double Box_double;
typedef struct Pair_i64_double Pair_i64_double;
typedef struct Doubler_i64 Doubler_i64;
typedef struct Container_i64 Container_i64;
typedef struct Pair_i64_i64 Pair_i64_i64;

struct Box_i64 {
  int64_t value;
};

struct Box_double {
  double value;
};

struct Pair_i64_double {
  int64_t a;
  double b;
};

struct Doubler_i64 {
  int64_t value;
};

struct Container_i64 {
  Either value;
};

struct Pair_i64_i64 {
  int64_t a;
  int64_t b;
};

static int64_t boxedI64(void);
static int64_t boxedBoth(void);
static int64_t pairOfIntAndDouble(void);
static int64_t doubledI64(void);
static int64_t containerOk(void);
static Either makePairResult(int64_t ok);
static int64_t pairResultOrErr(int64_t ok);

static Box_i64* Box_i64_new(int64_t value);
static bool Box_i64_equal(Box_i64* a, Box_i64* b);
static uint64_t Box_i64_hash(Box_i64* a);
static int64_t Box_i64_getValue(Box_i64* self);

static Box_double* Box_double_new(double value);
static bool Box_double_equal(Box_double* a, Box_double* b);
static uint64_t Box_double_hash(Box_double* a);
static double Box_double_getValue(Box_double* self);

static Pair_i64_double* Pair_i64_double_new(int64_t a, double b);
static bool Pair_i64_double_equal(Pair_i64_double* a, Pair_i64_double* b);
static uint64_t Pair_i64_double_hash(Pair_i64_double* a);
static int64_t Pair_i64_double_sumAsInt(Pair_i64_double* self);

static Doubler_i64* Doubler_i64_new(int64_t value);
static bool Doubler_i64_equal(Doubler_i64* a, Doubler_i64* b);
static uint64_t Doubler_i64_hash(Doubler_i64* a);
static int64_t Doubler_i64_doubled(Doubler_i64* self);

static Container_i64* Container_i64_new(Either value);
static bool Container_i64_equal(Container_i64* a, Container_i64* b);
static uint64_t Container_i64_hash(Container_i64* a);
static int64_t Container_i64_unwrapOrDefault(Container_i64* self, int64_t def);

static Pair_i64_i64* Pair_i64_i64_new(int64_t a, int64_t b);
static bool Pair_i64_i64_equal(Pair_i64_i64* a, Pair_i64_i64* b);
static uint64_t Pair_i64_i64_hash(Pair_i64_i64* a);
static int64_t Pair_i64_i64_sumAsInt(Pair_i64_i64* self);

static int64_t boxedI64(void) {
  Box_i64* b = Box_i64_new(42);
  return Box_i64_getValue(b);
}

static int64_t boxedBoth(void) {
  Box_i64* bi = Box_i64_new(100);
  Box_double* bd = Box_double_new(2.0);
  return Box_i64_getValue(bi) + (int64_t) Box_double_getValue(bd);
}

static int64_t pairOfIntAndDouble(void) {
  Pair_i64_double* p = Pair_i64_double_new(7, 8.5);
  return Pair_i64_double_sumAsInt(p);
}

static int64_t doubledI64(void) {
  Doubler_i64* d = Doubler_i64_new(21);
  return Doubler_i64_doubled(d);
}

static int64_t containerOk(void) {
  Container_i64* c = Container_i64_new(makeEither(true, (int64_t[]){42}, sizeof(int64_t)));
  return Container_i64_unwrapOrDefault(c, 0);
}

static Either makePairResult(int64_t ok) {
  if (ok == 0) {
    return makeEither(false, (int64_t[]){-1}, sizeof(int64_t));
  }
  return makeEither(true, (Pair_i64_i64*[]){Pair_i64_i64_new(11, 22)}, sizeof(Pair_i64_i64*));
}

static int64_t pairResultOrErr(int64_t ok) {
  Either r = makePairResult(ok);
  if (!r.isOk) {
    return (*(int64_t*) r.held);
  }
  Pair_i64_i64* p = (*(Pair_i64_i64**) r.held);
  return p->a * 100 + p->b;
}

static Box_i64* Box_i64_new(int64_t value) {
  Box_i64* made = gcAlloc(sizeof(Box_i64));
  made->value = value;
  return made;
}

static bool Box_i64_equal(Box_i64* a, Box_i64* b) {
  if (a == b || a == NULL || b == NULL) {
    return a == b;
  }

  bool same = true;
  same = same && a->value == b->value;
  return same;
}

static uint64_t Box_i64_hash(Box_i64* a) {
  uint64_t hash = 17;
  hash = hash * 31 + (uint64_t) a->value;
  return hash;
}

static int64_t Box_i64_getValue(Box_i64* self) {
  return self->value;
}

static Box_double* Box_double_new(double value) {
  Box_double* made = gcAlloc(sizeof(Box_double));
  made->value = value;
  return made;
}

static bool Box_double_equal(Box_double* a, Box_double* b) {
  if (a == b || a == NULL || b == NULL) {
    return a == b;
  }

  bool same = true;
  same = same && a->value == b->value;
  return same;
}

static uint64_t Box_double_hash(Box_double* a) {
  uint64_t hash = 17;
  hash = hash * 31 + hashBytes(&a->value, sizeof a->value);
  return hash;
}

static double Box_double_getValue(Box_double* self) {
  return self->value;
}

static Pair_i64_double* Pair_i64_double_new(int64_t a, double b) {
  Pair_i64_double* made = gcAlloc(sizeof(Pair_i64_double));
  made->a = a;
  made->b = b;
  return made;
}

static bool Pair_i64_double_equal(Pair_i64_double* a, Pair_i64_double* b) {
  if (a == b || a == NULL || b == NULL) {
    return a == b;
  }

  bool same = true;
  same = same && a->a == b->a;
  same = same && a->b == b->b;
  return same;
}

static uint64_t Pair_i64_double_hash(Pair_i64_double* a) {
  uint64_t hash = 17;
  hash = hash * 31 + (uint64_t) a->a;
  hash = hash * 31 + hashBytes(&a->b, sizeof a->b);
  return hash;
}

static int64_t Pair_i64_double_sumAsInt(Pair_i64_double* self) {
  return (int64_t) (self->a) + (int64_t) (self->b);
}

static Doubler_i64* Doubler_i64_new(int64_t value) {
  Doubler_i64* made = gcAlloc(sizeof(Doubler_i64));
  made->value = value;
  return made;
}

static bool Doubler_i64_equal(Doubler_i64* a, Doubler_i64* b) {
  if (a == b || a == NULL || b == NULL) {
    return a == b;
  }

  bool same = true;
  same = same && a->value == b->value;
  return same;
}

static uint64_t Doubler_i64_hash(Doubler_i64* a) {
  uint64_t hash = 17;
  hash = hash * 31 + (uint64_t) a->value;
  return hash;
}

static int64_t Doubler_i64_doubled(Doubler_i64* self) {
  return self->value + self->value;
}

static Container_i64* Container_i64_new(Either value) {
  Container_i64* made = gcAlloc(sizeof(Container_i64));
  made->value = value;
  return made;
}

static bool Container_i64_equal(Container_i64* a, Container_i64* b) {
  if (a == b || a == NULL || b == NULL) {
    return a == b;
  }

  bool same = true;
  same = same && memcmp(&a->value, &b->value, sizeof a->value) == 0;
  return same;
}

static uint64_t Container_i64_hash(Container_i64* a) {
  uint64_t hash = 17;
  hash = hash * 31 + hashBytes(&a->value, sizeof a->value);
  return hash;
}

static int64_t Container_i64_unwrapOrDefault(Container_i64* self, int64_t def) {
  if (!(self->value).isOk) {
    return def;
  }
  return (*(int64_t*) (self->value).held);
}

static Pair_i64_i64* Pair_i64_i64_new(int64_t a, int64_t b) {
  Pair_i64_i64* made = gcAlloc(sizeof(Pair_i64_i64));
  made->a = a;
  made->b = b;
  return made;
}

static bool Pair_i64_i64_equal(Pair_i64_i64* a, Pair_i64_i64* b) {
  if (a == b || a == NULL || b == NULL) {
    return a == b;
  }

  bool same = true;
  same = same && a->a == b->a;
  same = same && a->b == b->b;
  return same;
}

static uint64_t Pair_i64_i64_hash(Pair_i64_i64* a) {
  uint64_t hash = 17;
  hash = hash * 31 + (uint64_t) a->a;
  hash = hash * 31 + (uint64_t) a->b;
  return hash;
}

static int64_t Pair_i64_i64_sumAsInt(Pair_i64_i64* self) {
  return (int64_t) (self->a) + (int64_t) (self->b);
}
