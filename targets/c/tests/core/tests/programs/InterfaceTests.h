#pragma once

#include <setjmp.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../examples/interfaces.c"
#include "Check.h"

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

#ifndef POLYIR_CLOSURE_H
#define POLYIR_CLOSURE_H

// A closure: its function and its environment, which the function takes first. The function is kept as any function pointer
// and called as its own type.
typedef void (*AnyFn)(void);

typedef struct Closure {
  AnyFn fn;
  void* env;
} Closure;
#endif

#ifndef POLYIR_LIST_H
#define POLYIR_LIST_H

// A list: its items' size, how many it holds, and its items. `LIST_AT(list, i, T)` is item `i`, to read or to write.
typedef struct List {
  size_t size;
  int64_t count;
  int64_t capacity;
  char* items;
} List;

static List* listNew(size_t size) {
  List* list = gcAlloc(sizeof(List));
  list->size = size;

  return list;
}

static void* listAt(List* list, int64_t at) {
  if (at < 0 || at >= list->count) {
    checkFailed(-1, "index out of range");
  }

  return list->items + at * list->size;
}

static void listPush(List* list, const void* item) {
  if (list->count == list->capacity) {
    list->capacity = list->capacity == 0 ? 8 : list->capacity * 2;
    char* grown = gcAlloc(list->capacity * list->size);
    memcpy(grown, list->items, list->count * list->size);
    list->items = grown;
  }

  memcpy(list->items + list->count * list->size, item, list->size);
  list->count++;
}

static List* listOf(size_t size, int64_t count, const void* items) {
  List* list = listNew(size);

  for (int64_t i = 0; i < count; i++) {
    listPush(list, (const char*) items + i * size);
  }

  return list;
}

// A list of `count` items, each `item`.
static List* listFilled(size_t size, int64_t count, const void* item) {
  List* list = listNew(size);

  for (int64_t i = 0; i < count; i++) {
    listPush(list, item);
  }

  return list;
}

static List* listCopy(List* from) {
  return listOf(from->size, from->count, from->items);
}

static List* listPushed(List* list, const void* item) {
  listPush(list, item);

  return list;
}

static List* listWith(List* from, const void* item) {
  return listPushed(listCopy(from), item);
}

static List* listConcat(List* a, List* b) {
  List* out = listCopy(a);

  for (int64_t i = 0; i < b->count; i++) {
    listPush(out, b->items + i * b->size);
  }

  return out;
}

static List* listTruncate(List* list, int64_t count) {
  list->count = count < 0 ? 0 : count < list->count ? count : list->count;

  return list;
}

static bool listHas(List* list, const void* item, SameFn same) {
  for (int64_t i = 0; i < list->count; i++) {
    if ((same != NULL ? same : sameBytes)(list->items + i * list->size, item, list->size)) {
      return true;
    }
  }

  return false;
}

// Whether two lists hold the same items, compared by `same` (by their bytes when it is NULL).
static bool listSame(List* a, List* b, SameFn same) {
  if (a == b || a->count != b->count) {
    return a == b;
  }

  for (int64_t i = 0; i < a->count; i++) {
    if (!(same != NULL ? same : sameBytes)(a->items + i * a->size, b->items + i * b->size, a->size)) {
      return false;
    }
  }

  return true;
}

#define LIST_AT(list, at, T) (*(T*) listAt((list), (at)))
#define LIST_OF(T, ...) listOf(sizeof(T), sizeof((T[]){__VA_ARGS__}) / sizeof(T), (T[]){__VA_ARGS__})
#endif

typedef struct Halver Halver;
typedef struct Cell_i64 Cell_i64;
typedef struct Cell_double Cell_double;

static const char Halver_kind[] = "Halver";

struct Halver {
  const char* kind;
  int64_t base;
};

static const char Cell_i64_kind[] = "Cell_i64";

struct Cell_i64 {
  const char* kind;
  int64_t value;
};

static const char Cell_double_kind[] = "Cell_double";

struct Cell_double {
  const char* kind;
  double value;
};

static double checkConversionAtACall(void);
static double checkConversionAtAReturn(void);
static double checkDarrayOfInterfaceValues(void);
static double checkInterfaceAsAStructField(void);
static double checkOneStructTwoInterfaces(void);
static double checkInterfaceMethodTakingAnInterface(void);
static double checkDispatchFollowsTheValue(void);
static double checkSameMethodNameOnTwoStructs(void);
static double checkConvertedValueIsACopy(void);

static float Scaler_half(void* self, float p0);
static int64_t Scaler_offset(void* self, int32_t p0);

static Halver* Halver_new(int64_t base);
static bool Halver_equal(Halver* a, Halver* b);
static uint64_t Halver_hash(Halver* a);
static float Halver_half(Halver* self, float x);
static int64_t Halver_offset(Halver* self, int32_t by);

static double checkArgumentWidthsFollowTheInterface(void);
static double checkInterfaceInAnEither(void);
static double checkInterfaceCapturedByAClosure(void);
static double checkInterfaceThroughAVar(void);
static int64_t Weighted_weight(void* self);
static int64_t totalWeight(void* a, void* b);
static double checkGenericStructConvertsToAnInterface(void);
static double checkGenericInterfaceValuesInADarray(void);

static Cell_i64* Cell_i64_new(int64_t value);
static bool Cell_i64_equal(Cell_i64* a, Cell_i64* b);
static uint64_t Cell_i64_hash(Cell_i64* a);
static int64_t Cell_i64_weight(Cell_i64* self);
static int64_t Cell_i64_get(Cell_i64* self);

static Cell_double* Cell_double_new(double value);
static bool Cell_double_equal(Cell_double* a, Cell_double* b);
static uint64_t Cell_double_hash(Cell_double* a);
static int64_t Cell_double_weight(Cell_double* self);
static double Cell_double_get(Cell_double* self);

typedef struct closure1_Env {
  void* s;
} closure1_Env;

static closure1_Env* closure1_capture(void* s) {
  closure1_Env* env = gcAlloc(sizeof(closure1_Env));
  env->s = s;
  return env;
}

static int64_t closure1(void* raw) {
  closure1_Env* env = raw;
  return Shape_area(env->s);
}

static double checkConversionAtACall(void) {
  return offInt(directCall(), 920);
}

static double checkConversionAtAReturn(void) {
  return offInt(chosen(), 36034);
}

static double checkDarrayOfInterfaceValues(void) {
  return offInt(sumAll(), 41);
}

static double checkInterfaceAsAStructField(void) {
  return offInt(viaField(), 307);
}

static double checkOneStructTwoInterfaces(void) {
  return offInt(twoInterfaces(), 1016);
}

static double checkInterfaceMethodTakingAnInterface(void) {
  return offInt(interfaceArgument(), 109);
}

static double checkDispatchFollowsTheValue(void) {
  void* a = Square_new(5);
  void* b = Rect_new(5, 2);
  return offInt(Shape_area(a), 25) + offInt(Shape_area(b), 10);
}

static double checkSameMethodNameOnTwoStructs(void) {
  Square* s = Square_new(3);
  Rect* r = Rect_new(3, 4);
  return offInt(Square_area(s) + Rect_area(r), 21);
}

static double checkConvertedValueIsACopy(void) {
  Square* s = Square_new(4);
  void* a = s;
  void* b = s;
  return offInt(Shape_area(a) + Shape_area(b), 32);
}

static float Scaler_half(void* self, float p0) {
  const char* kind = *(const char**) self;
  if (kind == Halver_kind) {
    return Halver_half(self, p0);
  }
  fail("no struct of this kind implements Scaler");
}

static int64_t Scaler_offset(void* self, int32_t p0) {
  const char* kind = *(const char**) self;
  if (kind == Halver_kind) {
    return Halver_offset(self, p0);
  }
  fail("no struct of this kind implements Scaler");
}

static Halver* Halver_new(int64_t base) {
  Halver* made = gcAlloc(sizeof(Halver));
  made->kind = Halver_kind;
  made->base = base;
  return made;
}

static bool Halver_equal(Halver* a, Halver* b) {
  if (a == b || a == NULL || b == NULL) {
    return a == b;
  }

  bool same = true;
  same = same && a->base == b->base;
  return same;
}

static uint64_t Halver_hash(Halver* a) {
  uint64_t hash = 17;
  hash = hash * 31 + (uint64_t) a->base;
  return hash;
}

static float Halver_half(Halver* self, float x) {
  return x / 2.0f;
}

static int64_t Halver_offset(Halver* self, int32_t by) {
  return self->base + (int64_t) by;
}

static double checkArgumentWidthsFollowTheInterface(void) {
  void* s = Halver_new(40);
  return off(Scaler_half(s, 9.0f), 4.5f) + offInt(Scaler_offset(s, 2), 42);
}

static double checkInterfaceInAnEither(void) {
  Either r = makeEither(true, (Rect*[]){Rect_new(4, 4)}, sizeof(Rect*));
  if (!r.isOk) {
    return 1.0;
  }
  void* s = (*(void**) r.held);
  return offInt(Shape_area(s), 16);
}

static double checkInterfaceCapturedByAClosure(void) {
  void* s = Square_new(7);
  Closure f = (Closure){(AnyFn) closure1, closure1_capture(s)};
  return offInt(((int64_t (*)(void*)) f.fn)(f.env), 49);
}

static double checkInterfaceThroughAVar(void) {
  void* s = Square_new(2);
  int64_t first = Shape_area(s);
  s = Rect_new(3, 5);
  return offInt(first * 100 + Shape_area(s), 415);
}

static int64_t Weighted_weight(void* self) {
  const char* kind = *(const char**) self;
  if (kind == Cell_i64_kind) {
    return Cell_i64_weight(self);
  }
  if (kind == Cell_double_kind) {
    return Cell_double_weight(self);
  }
  fail("no struct of this kind implements Weighted");
}

static int64_t totalWeight(void* a, void* b) {
  return Weighted_weight(a) + Weighted_weight(b);
}

static double checkGenericStructConvertsToAnInterface(void) {
  Cell_i64* a = Cell_i64_new(5);
  Cell_double* b = Cell_double_new(2.5);
  return offInt(totalWeight(a, b), 14) + offInt(Cell_i64_get(a), 5) + off((float) Cell_double_get(b), 2.5f);
}

static double checkGenericInterfaceValuesInADarray(void) {
  List* all = listNew(sizeof(void*));
  Cell_i64* a = Cell_i64_new(1);
  listPush(all, (void*[]){a});
  listPush(all, (void*[]){a});
  return offInt(Weighted_weight(LIST_AT(all, 0, void*)) + Weighted_weight(LIST_AT(all, 1, void*)), 14);
}

static Cell_i64* Cell_i64_new(int64_t value) {
  Cell_i64* made = gcAlloc(sizeof(Cell_i64));
  made->kind = Cell_i64_kind;
  made->value = value;
  return made;
}

static bool Cell_i64_equal(Cell_i64* a, Cell_i64* b) {
  if (a == b || a == NULL || b == NULL) {
    return a == b;
  }

  bool same = true;
  same = same && a->value == b->value;
  return same;
}

static uint64_t Cell_i64_hash(Cell_i64* a) {
  uint64_t hash = 17;
  hash = hash * 31 + (uint64_t) a->value;
  return hash;
}

static int64_t Cell_i64_weight(Cell_i64* self) {
  return 7;
}

static int64_t Cell_i64_get(Cell_i64* self) {
  return self->value;
}

static Cell_double* Cell_double_new(double value) {
  Cell_double* made = gcAlloc(sizeof(Cell_double));
  made->kind = Cell_double_kind;
  made->value = value;
  return made;
}

static bool Cell_double_equal(Cell_double* a, Cell_double* b) {
  if (a == b || a == NULL || b == NULL) {
    return a == b;
  }

  bool same = true;
  same = same && a->value == b->value;
  return same;
}

static uint64_t Cell_double_hash(Cell_double* a) {
  uint64_t hash = 17;
  hash = hash * 31 + hashBytes(&a->value, sizeof a->value);
  return hash;
}

static int64_t Cell_double_weight(Cell_double* self) {
  return 7;
}

static double Cell_double_get(Cell_double* self) {
  return self->value;
}
