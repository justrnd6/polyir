#pragma once

#import <setjmp.h>
#import <stdbool.h>
#import <stdint.h>
#import <stdio.h>
#import <stdlib.h>
#import <string.h>

#import "Check.h"

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

#ifndef POLYIR_MAP_H
#define POLYIR_MAP_H

// A map from keys to values, or a set (a map whose values have no size), by open addressing. Keys compare and hash by their
// bytes, unless the map is given its key type's own functions.
typedef struct Map {
  size_t keySize;
  size_t valueSize;
  HashFn hash;
  SameFn same;
  int64_t count;
  int64_t capacity;
  char* used;
  char* keys;
  char* values;
} Map;

static Map* mapNew(size_t keySize, size_t valueSize, HashFn hash, SameFn same) {
  Map* map = gcAlloc(sizeof(Map));
  map->keySize = keySize;
  map->valueSize = valueSize;
  map->hash = hash != NULL ? hash : hashBytes;
  map->same = same != NULL ? same : sameBytes;

  return map;
}

// The slot of `key`, or of the empty slot where it would go.
static int64_t mapSlot(Map* map, const void* key) {
  int64_t at = (int64_t) (map->hash(key, map->keySize) % (uint64_t) map->capacity);

  while (map->used[at] && !map->same(map->keys + at * map->keySize, key, map->keySize)) {
    at = (at + 1) % map->capacity;
  }

  return at;
}

static void mapPut(Map* map, const void* key, const void* value);

static void mapGrow(Map* map) {
  Map old = *map;
  map->capacity = old.capacity == 0 ? 16 : old.capacity * 2;
  map->count = 0;
  map->used = gcAlloc(map->capacity);
  map->keys = gcAlloc(map->capacity * map->keySize);
  map->values = gcAlloc(map->capacity * (map->valueSize > 0 ? map->valueSize : 1));

  for (int64_t i = 0; i < old.capacity; i++) {
    if (old.used[i]) {
      mapPut(map, old.keys + i * old.keySize, old.values + i * old.valueSize);
    }
  }
}

static void mapPut(Map* map, const void* key, const void* value) {
  if ((map->count + 1) * 2 > map->capacity) {
    mapGrow(map);
  }

  const int64_t at = mapSlot(map, key);

  if (!map->used[at]) {
    map->used[at] = 1;
    map->count++;
    memcpy(map->keys + at * map->keySize, key, map->keySize);
  }

  if (map->valueSize > 0 && value != NULL) {
    memcpy(map->values + at * map->valueSize, value, map->valueSize);
  }
}

static bool mapHas(Map* map, const void* key) {
  return map->capacity > 0 && map->used[mapSlot(map, key)];
}

// The value of `key`, or `zero` when the map does not hold it.
static void* mapGetOr(Map* map, const void* key, void* zero) {
  return mapHas(map, key) ? map->values + mapSlot(map, key) * map->valueSize : zero;
}

// Takes a key out (the other entries are put back, so none is lost) and says whether it was there.
static bool mapRemove(Map* map, const void* key) {
  if (!mapHas(map, key)) {
    return false;
  }

  Map old = *map;
  map->capacity = 0;
  map->count = 0;

  for (int64_t i = 0; i < old.capacity; i++) {
    if (old.used[i] && !old.same(old.keys + i * old.keySize, key, old.keySize)) {
      mapPut(map, old.keys + i * old.keySize, old.values + i * old.valueSize);
    }
  }

  return true;
}

static List* mapKeys(Map* map) {
  List* keys = listNew(map->keySize);

  for (int64_t i = 0; i < map->capacity; i++) {
    if (map->used[i]) {
      listPush(keys, map->keys + i * map->keySize);
    }
  }

  return keys;
}

static Map* mapCopy(Map* from) {
  Map* map = mapNew(from->keySize, from->valueSize, from->hash, from->same);

  for (int64_t i = 0; i < from->capacity; i++) {
    if (from->used[i]) {
      mapPut(map, from->keys + i * from->keySize, from->values + i * from->valueSize);
    }
  }

  return map;
}

static Map* mapWith(Map* from, const void* key, const void* value) {
  Map* map = mapCopy(from);
  mapPut(map, key, value);

  return map;
}

#define MAP_GET(map, K, V, key, zero) (*(V*) mapGetOr((map), (K[]){key}, (V[]){zero}))
#endif

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

#ifndef POLYIR_TEXT_H
#define POLYIR_TEXT_H

// Text is a `const char*`, made and joined on the heap. A string's positions count its bytes. A byte outside the string is 0,
// a slice keeps what lies inside the string, and a search starts inside it.
static const char* textJoin(const char* a, const char* b) {
  const size_t first = strlen(a);
  char* out = gcAlloc(first + strlen(b) + 1);
  memcpy(out, a, first);
  strcpy(out + first, b);

  return out;
}

static bool textSame(const char* a, const char* b) {
  return strcmp(a, b) == 0;
}

static const char* textOfInt(int64_t value) {
  char* out = gcAlloc(24);
  snprintf(out, 24, "%lld", (long long) value);

  return out;
}

static const char* textOfUint(uint64_t value) {
  char* out = gcAlloc(24);
  snprintf(out, 24, "%llu", (unsigned long long) value);

  return out;
}

static const char* textOfDouble(double value) {
  char* out = gcAlloc(400);
  snprintf(out, 400, "%f", value);

  return out;
}

static const char* textOfBool(bool flag) {
  return flag ? "true" : "false";
}

static const char* textOfChar(int8_t c) {
  char* out = gcAlloc(2);
  out[0] = (char) c;

  return out;
}

static int8_t textByte(const char* text, int64_t at) {
  return at >= 0 && at < (int64_t) strlen(text) ? (int8_t) text[at] : 0;
}

static const char* textSlice(const char* text, int64_t from, int64_t to) {
  const int64_t size = strlen(text);
  const int64_t start = from < 0 ? 0 : from > size ? size : from;
  const int64_t end = to < start ? start : to > size ? size : to;
  char* out = gcAlloc(end - start + 1);
  memcpy(out, text + start, end - start);

  return out;
}

static int64_t textFind(const char* text, const char* part, int64_t from) {
  const int64_t size = strlen(text);
  const char* found = strstr(text + (from < 0 ? 0 : from > size ? size : from), part);

  return found == NULL ? -1 : found - text;
}

// A string's hash: djb2 over its bytes, wrapping at 64 bits, as the native code computes it.
static int64_t textHash(const char* text) {
  uint64_t hash = 5381;

  for (const char* c = text; *c != 0; c++) {
    hash = hash * 33 + (unsigned char) *c;
  }

  return (int64_t) hash;
}
#endif

typedef struct Counter Counter;
typedef struct Pt Pt;
typedef struct Holder Holder;
typedef struct Gauge Gauge;
typedef struct Dot Dot;
typedef struct Span Span;
typedef struct Meter Meter;
typedef struct Plate Plate;
typedef struct Cell_i64 Cell_i64;
typedef struct Wrapper_i64 Wrapper_i64;

struct Counter {
  int64_t n;
  int64_t id_;
};

struct Pt {
  int64_t x;
  int64_t y;
};

struct Holder {
  List* items;
  Counter* inner;
};

static const char Gauge_kind[] = "Gauge";

struct Gauge {
  const char* kind;
  int64_t level;
};

struct Dot {
  int64_t x;
  int64_t y;
};

struct Span {
  int64_t lo;
  int64_t hi;
};

struct Meter {
  int64_t reading;
  int64_t id_;
};

static const char Plate_kind[] = "Plate";

struct Plate {
  const char* kind;
  int64_t code;
};

struct Cell_i64 {
  int64_t v;
};

struct Wrapper_i64 {
  int64_t value;
};

static Counter* Counter_new(int64_t n, int64_t id_);
static bool Counter_equal(Counter* a, Counter* b);
static uint64_t Counter_hash(Counter* a);
static int64_t Counter_bump(Counter* self);

static Pt* Pt_new(int64_t x, int64_t y);
static bool Pt_equal(Pt* a, Pt* b);
static uint64_t Pt_hash(Pt* a);

static Holder* Holder_new(List* items, Counter* inner);
static bool Holder_equal(Holder* a, Holder* b);
static uint64_t Holder_hash(Holder* a);

static int64_t Reading_read(void* self);

static Gauge* Gauge_new(int64_t level);
static bool Gauge_equal(Gauge* a, Gauge* b);
static uint64_t Gauge_hash(Gauge* a);
static int64_t Gauge_read(Gauge* self);

static int64_t addTen(Counter* c);
static int64_t addTenOnACopy(Counter* c);
static double checkTwoNamesForOneObjectShareIt(void);
static double checkACalleeChangesTheCallersObject(void);
static double checkAValParameterIsTheCalleesOwnCopy(void);
static double checkValInFrontOfAnExpressionCopies(void);
static double checkCopyMethodAndValAgree(void);
static double checkAMethodChangesThis(void);
static double checkAnImmutableStructIsAValue(void);
static double checkAFieldHoldingAnotherReferenceStruct(void);
static double checkAFieldCanBeReplacedWithAnotherObject(void);
static double checkAGenericStructWithAVarField(void);
static double checkAnInterfaceValueSeesLaterChanges(void);
static double checkAStructInADarrayIsTheSameObject(void);
static double checkAStructInAClosureIsShared(void);
static Counter* makeCounter(int64_t start);
static int64_t lend(Counter* c);
static int64_t keepIn(List* store, Counter* c);
static double checkAReturnedObjectSurvives(void);
static double checkAnObjectLentToACalleeIsUsableAfterwards(void);
static double checkAnObjectAnotherCalleeKeepsSurvives(void);
static double checkAValParameterCopyIsReleasedAndTheOriginalIsNot(void);
static int64_t earlyReturnWithObjects(int64_t n);
static double checkAReturnInsideAnIfReleasesTheObjectAndKeepsTheValue(void);
static double checkLetVarAndValOnReferenceStructs(void);
static double checkValOnAContainerIsAnIndependentCopy(void);

static Dot* Dot_new(int64_t x, int64_t y);
static bool Dot_equal(Dot* a, Dot* b);
static uint64_t Dot_hash(Dot* a);

static Span* Span_new(int64_t lo, int64_t hi);
static bool Span_equal(Span* a, Span* b);
static uint64_t Span_hash(Span* a);
static int64_t Span_width(Span* self);

static Meter* Meter_new(int64_t reading, int64_t id_);
static bool Meter_equal(Meter* a, Meter* b);
static uint64_t Meter_hash(Meter* a);
static int64_t Meter_bump(Meter* self);

static Plate* Plate_new(int64_t code);
static bool Plate_equal(Plate* a, Plate* b);
static uint64_t Plate_hash(Plate* a);
static int64_t Plate_read(Plate* self);

static double checkShorthandStructsWork(void);
static double checkAShorthandStructWithAVarFieldIsAReference(void);
static double checkAHashmapOfObjectsIsUsableThenReleased(void);
static double checkASparseOfStringsIsUsableThenReleased(void);

static Cell_i64* Cell_i64_new(int64_t v);
static bool Cell_i64_equal(Cell_i64* a, Cell_i64* b);
static uint64_t Cell_i64_hash(Cell_i64* a);

static Wrapper_i64* Wrapper_i64_new(int64_t value);
static bool Wrapper_i64_equal(Wrapper_i64* a, Wrapper_i64* b);
static uint64_t Wrapper_i64_hash(Wrapper_i64* a);

typedef struct closure1_Env {
  Counter* a;
} closure1_Env;

static closure1_Env* closure1_capture(Counter* a) {
  closure1_Env* env = gcAlloc(sizeof(closure1_Env));
  env->a = a;
  return env;
}

static int64_t closure1(void* raw) {
  closure1_Env* env = raw;
  env->a->n = env->a->n + 1;
  return env->a->n;
}

static Counter* Counter_new(int64_t n, int64_t id_) {
  Counter* made = gcAlloc(sizeof(Counter));
  made->n = n;
  made->id_ = id_;
  return made;
}

static bool Counter_equal(Counter* a, Counter* b) {
  if (a == b || a == NULL || b == NULL) {
    return a == b;
  }

  bool same = true;
  same = same && a->n == b->n;
  same = same && a->id_ == b->id_;
  return same;
}

static uint64_t Counter_hash(Counter* a) {
  uint64_t hash = 17;
  hash = hash * 31 + (uint64_t) a->n;
  hash = hash * 31 + (uint64_t) a->id_;
  return hash;
}

static int64_t Counter_bump(Counter* self) {
  self->n = self->n + 1;
  return self->n;
}

static Pt* Pt_new(int64_t x, int64_t y) {
  Pt* made = gcAlloc(sizeof(Pt));
  made->x = x;
  made->y = y;
  return made;
}

static bool Pt_equal(Pt* a, Pt* b) {
  if (a == b || a == NULL || b == NULL) {
    return a == b;
  }

  bool same = true;
  same = same && a->x == b->x;
  same = same && a->y == b->y;
  return same;
}

static uint64_t Pt_hash(Pt* a) {
  uint64_t hash = 17;
  hash = hash * 31 + (uint64_t) a->x;
  hash = hash * 31 + (uint64_t) a->y;
  return hash;
}

static Holder* Holder_new(List* items, Counter* inner) {
  Holder* made = gcAlloc(sizeof(Holder));
  made->items = items;
  made->inner = inner;
  return made;
}

static bool Holder_equal(Holder* a, Holder* b) {
  if (a == b || a == NULL || b == NULL) {
    return a == b;
  }

  bool same = true;
  same = same && listSame(a->items, b->items, NULL);
  same = same && Counter_equal(a->inner, b->inner);
  return same;
}

static uint64_t Holder_hash(Holder* a) {
  uint64_t hash = 17;
  hash = hash * 31 + hashBytes(&a->items, sizeof a->items);
  hash = hash * 31 + Counter_hash(a->inner);
  return hash;
}

static int64_t Reading_read(void* self) {
  const char* kind = *(const char**) self;
  if (kind == Gauge_kind) {
    return Gauge_read(self);
  }
  if (kind == Plate_kind) {
    return Plate_read(self);
  }
  fail("no struct of this kind implements Reading");
}

static Gauge* Gauge_new(int64_t level) {
  Gauge* made = gcAlloc(sizeof(Gauge));
  made->kind = Gauge_kind;
  made->level = level;
  return made;
}

static bool Gauge_equal(Gauge* a, Gauge* b) {
  if (a == b || a == NULL || b == NULL) {
    return a == b;
  }

  bool same = true;
  same = same && a->level == b->level;
  return same;
}

static uint64_t Gauge_hash(Gauge* a) {
  uint64_t hash = 17;
  hash = hash * 31 + (uint64_t) a->level;
  return hash;
}

static int64_t Gauge_read(Gauge* self) {
  return self->level;
}

static int64_t addTen(Counter* c) {
  c->n = c->n + 10;
  return 0;
}

static int64_t addTenOnACopy(Counter* c) {
  Counter* c2 = copyBytes(c, sizeof *c);
  c2->n = c2->n + 10;
  return c2->n;
}

static double checkTwoNamesForOneObjectShareIt(void) {
  Counter* a = Counter_new(1, 7);
  Counter* b = a;
  b->n = 5;
  return offInt(a->n, 5) + offInt(a->id_, 7);
}

static double checkACalleeChangesTheCallersObject(void) {
  Counter* a = Counter_new(1, 0);
  addTen(a);
  return offInt(a->n, 11);
}

static double checkAValParameterIsTheCalleesOwnCopy(void) {
  Counter* a = Counter_new(1, 0);
  int64_t seen = addTenOnACopy(a);
  return offInt(seen, 11) + offInt(a->n, 1);
}

static double checkValInFrontOfAnExpressionCopies(void) {
  Counter* a = Counter_new(1, 3);
  Counter* b = copyBytes(a, sizeof *a);
  b->n = 50;
  return offInt(a->n, 1) + offInt(b->n, 50) + offInt(b->id_, 3);
}

static double checkCopyMethodAndValAgree(void) {
  Counter* a = Counter_new(4, 0);
  Counter* b = copyBytes(a, sizeof *a);
  a->n = 9;
  return offInt(b->n, 4);
}

static double checkAMethodChangesThis(void) {
  Counter* a = Counter_new(0, 0);
  Counter_bump(a);
  Counter_bump(a);
  return offInt(a->n, 2);
}

static double checkAnImmutableStructIsAValue(void) {
  Pt* p = Pt_new(1, 2);
  Pt* q = copyBytes(p, sizeof *p);
  Pt* r = copyBytes(p, sizeof *p);
  return offInt(q->x + r->y, 3);
}

static double checkAFieldHoldingAnotherReferenceStruct(void) {
  Holder* h = Holder_new(listNew(sizeof(int64_t)), Counter_new(1, 0));
  Holder* alias = h;
  alias->inner->n = 8;
  listPush(alias->items, (int64_t[]){5});
  return offInt(h->inner->n, 8) + offInt(h->items->count, 1);
}

static double checkAFieldCanBeReplacedWithAnotherObject(void) {
  Holder* h = Holder_new(listNew(sizeof(int64_t)), Counter_new(1, 0));
  Counter* keep = h->inner;
  h->inner = Counter_new(2, 0);
  return offInt(keep->n, 1) + offInt(h->inner->n, 2);
}

static double checkAGenericStructWithAVarField(void) {
  Cell_i64* c = Cell_i64_new(1);
  Cell_i64* d = c;
  d->v = 9;
  return offInt(c->v, 9);
}

static double checkAnInterfaceValueSeesLaterChanges(void) {
  Gauge* g = Gauge_new(1);
  void* r = g;
  g->level = 42;
  return offInt(Reading_read(r), 42);
}

static double checkAStructInADarrayIsTheSameObject(void) {
  List* all = listNew(sizeof(Counter*));
  Counter* a = Counter_new(1, 0);
  listPush(all, (Counter*[]){a});
  a->n = 6;
  return offInt(LIST_AT(all, 0, Counter*)->n, 6);
}

static double checkAStructInAClosureIsShared(void) {
  Counter* a = Counter_new(1, 0);
  Closure f = (Closure){(AnyFn) closure1, closure1_capture(a)};
  ((int64_t (*)(void*)) f.fn)(f.env);
  return offInt(a->n, 2);
}

static Counter* makeCounter(int64_t start) {
  Counter* c = Counter_new(start, 1);
  c->n = c->n + 1;
  return c;
}

static int64_t lend(Counter* c) {
  return c->n * 2;
}

static int64_t keepIn(List* store, Counter* c) {
  listPush(store, (Counter*[]){c});
  return 0;
}

static double checkAReturnedObjectSurvives(void) {
  Counter* a = makeCounter(5);
  Counter* b = makeCounter(10);
  return offInt(a->n * 100 + b->n, 611);
}

static double checkAnObjectLentToACalleeIsUsableAfterwards(void) {
  int64_t total = 0;
  for (int64_t i = 0; i < 100; i = i + 1) {
    Counter* c = Counter_new(i, 0);
    total = total + lend(c) + c->n;
    c->n = 0;
  }
  return offInt(total, 3 * 4950);
}

static double checkAnObjectAnotherCalleeKeepsSurvives(void) {
  List* store = listNew(sizeof(Counter*));
  for (int64_t i = 0; i < 4; i = i + 1) {
    Counter* c = Counter_new(i * 10, 0);
    keepIn(store, c);
  }
  return offInt(LIST_AT(store, 0, Counter*)->n + LIST_AT(store, 1, Counter*)->n + LIST_AT(store, 2, Counter*)->n + LIST_AT(store, 3, Counter*)->n, 60);
}

static double checkAValParameterCopyIsReleasedAndTheOriginalIsNot(void) {
  Counter* a = Counter_new(1, 0);
  int64_t total = 0;
  for (int64_t i = 0; i < 50; i = i + 1) {
    total = total + addTenOnACopy(a);
  }
  return offInt(total, 50 * 11) + offInt(a->n, 1);
}

static int64_t earlyReturnWithObjects(int64_t n) {
  Counter* c = Counter_new(n, 0);
  if (n > 5) {
    return c->n * 2;
  }
  Counter_bump(c);
  return c->n;
}

static double checkAReturnInsideAnIfReleasesTheObjectAndKeepsTheValue(void) {
  return offInt(earlyReturnWithObjects(9) * 100 + earlyReturnWithObjects(2), 1800 + 3);
}

static double checkLetVarAndValOnReferenceStructs(void) {
  Counter* a = Counter_new(1, 0);
  Counter* alias = a;
  Counter* copy = copyBytes(a, sizeof *a);
  Counter* moving = a;
  a->n = 10;
  moving = Counter_new(100, 0);
  moving->n = moving->n + 1;
  return offInt(alias->n, 10) + offInt(copy->n, 1) + offInt(moving->n, 101) + offInt(a->n, 10);
}

static double checkValOnAContainerIsAnIndependentCopy(void) {
  List* d = listNew(sizeof(int64_t));
  listPush(d, (int64_t[]){4});
  List* e = listCopy(d);
  listPush(d, (int64_t[]){5});
  return offInt(e->count, 1) + offInt(d->count, 2);
}

static Dot* Dot_new(int64_t x, int64_t y) {
  Dot* made = gcAlloc(sizeof(Dot));
  made->x = x;
  made->y = y;
  return made;
}

static bool Dot_equal(Dot* a, Dot* b) {
  if (a == b || a == NULL || b == NULL) {
    return a == b;
  }

  bool same = true;
  same = same && a->x == b->x;
  same = same && a->y == b->y;
  return same;
}

static uint64_t Dot_hash(Dot* a) {
  uint64_t hash = 17;
  hash = hash * 31 + (uint64_t) a->x;
  hash = hash * 31 + (uint64_t) a->y;
  return hash;
}

static Span* Span_new(int64_t lo, int64_t hi) {
  Span* made = gcAlloc(sizeof(Span));
  made->lo = lo;
  made->hi = hi;
  return made;
}

static bool Span_equal(Span* a, Span* b) {
  if (a == b || a == NULL || b == NULL) {
    return a == b;
  }

  bool same = true;
  same = same && a->lo == b->lo;
  same = same && a->hi == b->hi;
  return same;
}

static uint64_t Span_hash(Span* a) {
  uint64_t hash = 17;
  hash = hash * 31 + (uint64_t) a->lo;
  hash = hash * 31 + (uint64_t) a->hi;
  return hash;
}

static int64_t Span_width(Span* self) {
  return self->hi - self->lo;
}

static Meter* Meter_new(int64_t reading, int64_t id_) {
  Meter* made = gcAlloc(sizeof(Meter));
  made->reading = reading;
  made->id_ = id_;
  return made;
}

static bool Meter_equal(Meter* a, Meter* b) {
  if (a == b || a == NULL || b == NULL) {
    return a == b;
  }

  bool same = true;
  same = same && a->reading == b->reading;
  same = same && a->id_ == b->id_;
  return same;
}

static uint64_t Meter_hash(Meter* a) {
  uint64_t hash = 17;
  hash = hash * 31 + (uint64_t) a->reading;
  hash = hash * 31 + (uint64_t) a->id_;
  return hash;
}

static int64_t Meter_bump(Meter* self) {
  self->reading = self->reading + 1;
  return self->reading;
}

static Plate* Plate_new(int64_t code) {
  Plate* made = gcAlloc(sizeof(Plate));
  made->kind = Plate_kind;
  made->code = code;
  return made;
}

static bool Plate_equal(Plate* a, Plate* b) {
  if (a == b || a == NULL || b == NULL) {
    return a == b;
  }

  bool same = true;
  same = same && a->code == b->code;
  return same;
}

static uint64_t Plate_hash(Plate* a) {
  uint64_t hash = 17;
  hash = hash * 31 + (uint64_t) a->code;
  return hash;
}

static int64_t Plate_read(Plate* self) {
  return self->code;
}

static double checkShorthandStructsWork(void) {
  Dot* d = Dot_new(3, 4);
  Span* s = Span_new(2, 9);
  Wrapper_i64* w = Wrapper_i64_new(5);
  void* r = Plate_new(7);
  return offInt(d->x + d->y, 7) + offInt(Span_width(s), 7) + offInt(w->value, 5) + offInt(Reading_read(r), 7);
}

static double checkAShorthandStructWithAVarFieldIsAReference(void) {
  Meter* m = Meter_new(1, 2);
  Meter* alias = m;
  Meter_bump(alias);
  return offInt(m->reading, 2) + offInt(m->id_, 2);
}

static double checkAHashmapOfObjectsIsUsableThenReleased(void) {
  int64_t total = 0;
  for (int64_t i = 0; i < 100; i = i + 1) {
    Map* m = mapNew(sizeof(int64_t), sizeof(Counter*), NULL, NULL);
    mapPut(m, (int64_t[]){1}, (Counter*[]){Counter_new(i, 1)});
    mapPut(m, (int64_t[]){2}, (Counter*[]){Counter_new(5, 2)});
    mapPut(m, (int64_t[]){2}, (Counter*[]){Counter_new(6, 2)});
    Counter_bump(MAP_GET(m, int64_t, Counter*, 1, NULL));
    total = total + MAP_GET(m, int64_t, Counter*, 1, NULL)->n - i + MAP_GET(m, int64_t, Counter*, 2, NULL)->n + m->count;
  }
  return offInt(total, 100 * (1 + 6 + 2));
}

static double checkASparseOfStringsIsUsableThenReleased(void) {
  int64_t total = 0;
  for (int64_t i = 0; i < 100; i = i + 1) {
    Sparse* s = sparseNew(sizeof(const char*), (const char*[]){""});
    sparseSet(s, 0, 0, (const char*[]){textJoin("a", "b")});
    sparseSet(s, 2, 3, (const char*[]){textJoin("c", "d")});
    if (textSame((*(const char**) sparseGet(s, 2, 3)), "cd")) {
      total = total + s->values->count;
    }
  }
  return offInt(total, 200);
}

static Cell_i64* Cell_i64_new(int64_t v) {
  Cell_i64* made = gcAlloc(sizeof(Cell_i64));
  made->v = v;
  return made;
}

static bool Cell_i64_equal(Cell_i64* a, Cell_i64* b) {
  if (a == b || a == NULL || b == NULL) {
    return a == b;
  }

  bool same = true;
  same = same && a->v == b->v;
  return same;
}

static uint64_t Cell_i64_hash(Cell_i64* a) {
  uint64_t hash = 17;
  hash = hash * 31 + (uint64_t) a->v;
  return hash;
}

static Wrapper_i64* Wrapper_i64_new(int64_t value) {
  Wrapper_i64* made = gcAlloc(sizeof(Wrapper_i64));
  made->value = value;
  return made;
}

static bool Wrapper_i64_equal(Wrapper_i64* a, Wrapper_i64* b) {
  if (a == b || a == NULL || b == NULL) {
    return a == b;
  }

  bool same = true;
  same = same && a->value == b->value;
  return same;
}

static uint64_t Wrapper_i64_hash(Wrapper_i64* a) {
  uint64_t hash = 17;
  hash = hash * 31 + (uint64_t) a->value;
  return hash;
}
