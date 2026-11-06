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

typedef struct Bag Bag;
typedef struct Pt Pt;
typedef struct Pocket Pocket;
typedef struct Shelf Shelf;
typedef struct Cabinet Cabinet;
typedef struct Crate Crate;
typedef struct GrowKey GrowKey;
typedef struct OptPoint OptPoint;

struct Bag {
  List* items;
  int64_t total;
};

struct Pt {
  int64_t x;
  int64_t y;
};

struct Pocket {
  List* items;
};

struct Shelf {
  List* books;
  const char* tags;
};

struct Cabinet {
  List* drawer;
};

struct Crate {
  List* items;
};

struct GrowKey {
  int64_t a;
  int64_t b;
};

struct OptPoint {
  int64_t x;
  int64_t y;
};

static Bag* Bag_new(List* items, int64_t total);
static bool Bag_equal(Bag* a, Bag* b);
static uint64_t Bag_hash(Bag* a);

static Pt* Pt_new(int64_t x, int64_t y);
static bool Pt_equal(Pt* a, Pt* b);
static uint64_t Pt_hash(Pt* a);

static int64_t pushThree(List* xs);
static int64_t pushMany(List* xs, int64_t count);
static double checkAPushInsideAFunctionIsSeenByTheCaller(void);
static double checkManyPushesInsideAFunctionLeaveTheCallerValid(void);
static double checkTwoNamesForOneDarrayShareIt(void);
static double checkAnElementWrittenInAFunctionIsSeenByTheCaller(void);
static int64_t overwriteFirst(List* xs, int64_t value);
static List* makeFilled(int64_t count);
static double checkAReturnedDarrayIsTheOneTheFunctionBuilt(void);
static double checkAStructFieldIsTheSameDarray(void);
static double checkAMethodStylePushOnAFieldWorks(void);
static double checkConcatenationMakesANewDarray(void);
static double checkAClosureSharesTheDarrayItCaptures(void);
static double checkPushGivesBackTheSameDarray(void);
static double checkReassigningAVarSwitchesDarrays(void);
static double checkAListOfListsSharesItsInnerLists(void);
static double checkADarrayOfStructsKeepsTheStructs(void);
static double checkAThousandPushesKeepEveryElement(void);
static double checkAListInsideAnEitherIsTheSameList(void);
static int64_t fillSparse(Sparse* sp);
static double checkASparseWrittenInAFunctionIsSeenByTheCaller(void);
static double checkLastWriteWinsForASparse(void);
static double checkOneDimensionalSparseSharesToo(void);
static int64_t fillMap(Map* hm);
static double checkAHashmapWrittenInAFunctionIsSeenByTheCaller(void);
static double checkTwoNamesForOneHashmapShareIt(void);
static double checkAStringKeyedHashmapIsShared(void);
static int64_t addWord(Map* hm, const char* word, int64_t value);
static double checkAHashmapOfListsKeepsTheLists(void);
static int64_t fillGrid(Grid* g);
static double checkAGridFilledInAFunctionIsSeenByTheCaller(void);
static double checkTwoNamesForOneGridShareIt(void);
static double checkAGridStartsZeroed(void);
static double checkDarrayCopyIsIndependent(void);
static double checkGridAndTensorCopyAreIndependent(void);
static double checkSparseCopyIsIndependent(void);
static double checkHashmapCopyIsIndependent(void);
static double checkCopyOfAStringKeyedHashmap(void);
static List* makeBag(int64_t n);
static int64_t sumGrid(Grid* g);
static int64_t holdsOnto(List* store, List* d);
static double checkAReturnedContainerSurvives(void);
static double checkAContainerLentToACalleeIsStillUsableAfterwards(void);
static double checkAContainerGivenToACalleeThatKeepsItSurvives(void);
static double checkAContainerReturnedFromInsideAnIfSurvives(void);
static int64_t earlyExit(int64_t n);
static double checkAReturnInsideAnIfStillReleasesAndReturnsTheRightValue(void);
static const char* greeting(const char* name);
static int64_t shout(const char* s);
static Closure counterFrom(int64_t start);
static int64_t applyTwice(Closure f);
static double checkAReturnedStringSurvives(void);
static double checkAStringLentToACalleeIsUsableAfterwards(void);
static double checkAStringUsedAsAHashmapKeyIsKept(void);
static double checkAClosureThatIsReturnedKeepsItsBox(void);
static double checkAClosureGivenToACalleeKeepsWorking(void);
static double checkAClosureOnlyCalledLocallyInALoop(void);
static double checkTwoClosuresShareOneCapturedName(void);

static Pocket* Pocket_new(List* items);
static bool Pocket_equal(Pocket* a, Pocket* b);
static uint64_t Pocket_hash(Pocket* a);

static List* madeHere(void);
static List* madeThroughAnother(void);
static List* madeInPlace(void);
static List* sameAsArgument(List* d);
static List* elementOf(List* xs);
static List* fieldOf(Pocket* p);
static List* keptAndReturned(List* store);
static List* sometimesTheArgument(int64_t flag, List* d);
static double checkAFreshResultIsUsableAndReleased(void);
static double checkAnAliasReturnedByACallIsNotReleasedUnderItsOwner(void);
static double checkAnObjectKeptByTheCalleeSurvivesItsCaller(void);
static List* handedOn(void);
static double checkAResultHandedOnAgainIsStillFresh(void);
static double checkADiscardedFreshResultIsHarmless(void);
static Pocket* madeStruct(int64_t n);
static const char* makeLabel(const char* a);
static double checkAStructAndAStringHandedOverByACall(void);

static Shelf* Shelf_new(List* books, const char* tags);
static bool Shelf_equal(Shelf* a, Shelf* b);
static uint64_t Shelf_hash(Shelf* a);

static Cabinet* Cabinet_new(List* drawer);
static bool Cabinet_equal(Cabinet* a, Cabinet* b);
static uint64_t Cabinet_hash(Cabinet* a);

static Crate* Crate_new(List* items);
static bool Crate_equal(Crate* a, Crate* b);
static uint64_t Crate_hash(Crate* a);

static Shelf* stockedShelf(int64_t n);
static List* openDrawer(Cabinet* c);
static double checkAnOwnedFieldIsUsableThenReleasedWithItsStruct(void);
static double checkAReplacedFieldValueIsReleasedAndTheNewOneWorks(void);
static double checkAFieldThatIsReadOutIsNeverReleasedWithItsStruct(void);
static double checkACopiedStructSharesNothingItMustNotFree(void);
static double checkAListOfListsReleasesItsElements(void);
static List* firstOf(List* xs);
static double checkAnElementReadOutOfAListIsNotReleasedWithIt(void);
static double checkAMapPastItsStartingCapacityKeepsEveryEntry(void);
static double checkAGrownMapIsStillTheSameMapForItsAliases(void);
static double checkStringKeysSurviveGrowing(void);

static GrowKey* GrowKey_new(int64_t a, int64_t b);
static bool GrowKey_equal(GrowKey* a, GrowKey* b);
static uint64_t GrowKey_hash(GrowKey* a);

static double checkStructKeysSurviveGrowing(void);
static double checkASetPastItsStartingCapacityKeepsEveryMember(void);
static int64_t readAt(List* xs, int64_t i);
static int64_t caughtCode(List* xs, int64_t i);
static double checkAnIndexPastTheEndOfAListThrowsMinusOne(void);
static double checkANegativeIndexThrowsMinusOne(void);
static double checkAnIndexInsideTheListStillReads(void);
static double checkAnEmptyListCannotBeRead(void);
static int64_t divideBy(int64_t a, int64_t b);
static double checkDividingByZeroThrowsMinusFour(void);
static double checkAFixedArrayAndAGridAreCheckedPerDimension(void);

static OptPoint* OptPoint_new(int64_t x, int64_t y);
static bool OptPoint_equal(OptPoint* a, OptPoint* b);
static uint64_t OptPoint_hash(OptPoint* a);

static int64_t* optionalFind(List* xs, int64_t want);
static OptPoint** optionalPoint(int64_t ok);
static const char** optionalName(int64_t ok);
static int64_t unwrapCode(int64_t* o);
static double checkAnOptionalHoldsAValueOrNone(void);
static double checkAnOptionalOfAStructAndOfAStringWork(void);
static double checkUnwrappingNoneThrowsMinusThree(void);

typedef struct closure1_Env {
  List* xs;
} closure1_Env;

static closure1_Env* closure1_capture(List* xs) {
  closure1_Env* env = gcAlloc(sizeof(closure1_Env));
  env->xs = xs;
  return env;
}

static int64_t closure1(void* raw, int64_t v) {
  closure1_Env* env = raw;
  listPush(env->xs, (int64_t[]){v});
  return env->xs->count;
}

typedef struct closure2_Env {
  int64_t* n;
} closure2_Env;

static closure2_Env* closure2_capture(int64_t* n) {
  closure2_Env* env = gcAlloc(sizeof(closure2_Env));
  env->n = n;
  return env;
}

static int64_t closure2(void* raw) {
  closure2_Env* env = raw;
  (*env->n) = (*env->n) + 1;
  return (*env->n);
}

typedef struct closure3_Env {
  int64_t* n;
} closure3_Env;

static closure3_Env* closure3_capture(int64_t* n) {
  closure3_Env* env = gcAlloc(sizeof(closure3_Env));
  env->n = n;
  return env;
}

static int64_t closure3(void* raw) {
  closure3_Env* env = raw;
  (*env->n) = (*env->n) + 5;
  return (*env->n);
}

typedef struct closure4_Env {
  int64_t* count;
} closure4_Env;

static closure4_Env* closure4_capture(int64_t* count) {
  closure4_Env* env = gcAlloc(sizeof(closure4_Env));
  env->count = count;
  return env;
}

static int64_t closure4(void* raw) {
  closure4_Env* env = raw;
  (*env->count) = (*env->count) + 1;
  return (*env->count);
}

typedef struct closure6_Env {
  int64_t* n;
} closure6_Env;

static closure6_Env* closure6_capture(int64_t* n) {
  closure6_Env* env = gcAlloc(sizeof(closure6_Env));
  env->n = n;
  return env;
}

static int64_t closure6(void* raw) {
  closure6_Env* env = raw;
  (*env->n) = (*env->n) + 1;
  return (*env->n);
}

typedef struct closure7_Env {
  int64_t* n;
} closure7_Env;

static closure7_Env* closure7_capture(int64_t* n) {
  closure7_Env* env = gcAlloc(sizeof(closure7_Env));
  env->n = n;
  return env;
}

static int64_t closure7(void* raw) {
  closure7_Env* env = raw;
  return (*env->n) * 10;
}

static uint64_t GrowKey_hashKey(const void* key, size_t size) {
  (void) size;
  return GrowKey_hash(*(GrowKey* const*) key);
}

static bool GrowKey_sameKey(const void* a, const void* b, size_t size) {
  (void) size;
  return GrowKey_equal(*(GrowKey* const*) a, *(GrowKey* const*) b);
}

static Bag* Bag_new(List* items, int64_t total) {
  Bag* made = gcAlloc(sizeof(Bag));
  made->items = items;
  made->total = total;
  return made;
}

static bool Bag_equal(Bag* a, Bag* b) {
  if (a == b || a == NULL || b == NULL) {
    return a == b;
  }

  bool same = true;
  same = same && listSame(a->items, b->items, NULL);
  same = same && a->total == b->total;
  return same;
}

static uint64_t Bag_hash(Bag* a) {
  uint64_t hash = 17;
  hash = hash * 31 + hashBytes(&a->items, sizeof a->items);
  hash = hash * 31 + (uint64_t) a->total;
  return hash;
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

static int64_t pushThree(List* xs) {
  listPush(xs, (int64_t[]){3});
  return xs->count;
}

static int64_t pushMany(List* xs, int64_t count) {
  for (int64_t i = 0; i < count; i = i + 1) {
    listPush(xs, (int64_t[]){i * 2});
  }
  return xs->count;
}

static double checkAPushInsideAFunctionIsSeenByTheCaller(void) {
  List* xs = listNew(sizeof(int64_t));
  listPush(xs, (int64_t[]){1});
  pushThree(xs);
  return offInt(xs->count, 2) + offInt(LIST_AT(xs, 1, int64_t), 3);
}

static double checkManyPushesInsideAFunctionLeaveTheCallerValid(void) {
  List* xs = listNew(sizeof(int64_t));
  listPush(xs, (int64_t[]){7});
  pushMany(xs, 100);
  return offInt(xs->count, 101) + offInt(LIST_AT(xs, 0, int64_t), 7) + offInt(LIST_AT(xs, 1, int64_t), 0) + offInt(LIST_AT(xs, 100, int64_t), 198);
}

static double checkTwoNamesForOneDarrayShareIt(void) {
  List* a = listNew(sizeof(int64_t));
  List* b = a;
  listPush(a, (int64_t[]){5});
  listPush(b, (int64_t[]){6});
  return offInt(a->count, 2) + offInt(b->count, 2) + offInt(LIST_AT(a, 1, int64_t), 6) + offInt(LIST_AT(b, 0, int64_t), 5);
}

static double checkAnElementWrittenInAFunctionIsSeenByTheCaller(void) {
  List* xs = listNew(sizeof(int64_t));
  listPush(xs, (int64_t[]){1});
  listPush(xs, (int64_t[]){2});
  overwriteFirst(xs, 40);
  return offInt(LIST_AT(xs, 0, int64_t), 40) + offInt(LIST_AT(xs, 1, int64_t), 2);
}

static int64_t overwriteFirst(List* xs, int64_t value) {
  LIST_AT(xs, 0, int64_t) = value;
  return 0;
}

static List* makeFilled(int64_t count) {
  List* xs = listNew(sizeof(int64_t));
  for (int64_t i = 0; i < count; i = i + 1) {
    listPush(xs, (int64_t[]){i + 1});
  }
  return xs;
}

static double checkAReturnedDarrayIsTheOneTheFunctionBuilt(void) {
  List* xs = makeFilled(4);
  listPush(xs, (int64_t[]){50});
  return offInt(xs->count, 5) + offInt(LIST_AT(xs, 3, int64_t), 4) + offInt(LIST_AT(xs, 4, int64_t), 50);
}

static double checkAStructFieldIsTheSameDarray(void) {
  List* xs = listNew(sizeof(int64_t));
  Bag* bag = Bag_new(xs, 0);
  listPush(xs, (int64_t[]){8});
  listPush(bag->items, (int64_t[]){9});
  return offInt(bag->items->count, 2) + offInt(xs->count, 2) + offInt(LIST_AT(bag->items, 1, int64_t), 9);
}

static double checkAMethodStylePushOnAFieldWorks(void) {
  Bag* bag = Bag_new(listNew(sizeof(int64_t)), 0);
  listPush(bag->items, (int64_t[]){4});
  listPush(bag->items, (int64_t[]){5});
  return offInt(bag->items->count, 2) + offInt(LIST_AT(bag->items, 1, int64_t), 5);
}

static double checkConcatenationMakesANewDarray(void) {
  List* a = listNew(sizeof(int64_t));
  List* b = listNew(sizeof(int64_t));
  listPush(a, (int64_t[]){1});
  listPush(b, (int64_t[]){2});
  List* c = listConcat(a, b);
  listPush(a, (int64_t[]){99});
  return offInt(c->count, 2) + offInt(LIST_AT(c, 0, int64_t), 1) + offInt(LIST_AT(c, 1, int64_t), 2) + offInt(a->count, 2);
}

static double checkAClosureSharesTheDarrayItCaptures(void) {
  List* xs = listNew(sizeof(int64_t));
  Closure add = (Closure){(AnyFn) closure1, closure1_capture(xs)};
  ((int64_t (*)(void*, int64_t)) add.fn)(add.env, 10);
  ((int64_t (*)(void*, int64_t)) add.fn)(add.env, 20);
  return offInt(xs->count, 2) + offInt(LIST_AT(xs, 1, int64_t), 20);
}

static double checkPushGivesBackTheSameDarray(void) {
  List* xs = listNew(sizeof(int64_t));
  xs = listPushed(xs, (int64_t[]){5});
  xs = listPushed(xs, (int64_t[]){6});
  return offInt(xs->count, 2) + offInt(LIST_AT(xs, 1, int64_t), 6);
}

static double checkReassigningAVarSwitchesDarrays(void) {
  List* xs = listNew(sizeof(int64_t));
  List* other = listNew(sizeof(int64_t));
  listPush(other, (int64_t[]){1});
  listPush(other, (int64_t[]){2});
  listPush(other, (int64_t[]){3});
  xs = other;
  listPush(xs, (int64_t[]){4});
  return offInt(other->count, 4) + offInt(xs->count, 4);
}

static double checkAListOfListsSharesItsInnerLists(void) {
  List* outer = listNew(sizeof(List*));
  List* a = listNew(sizeof(int64_t));
  listPush(a, (int64_t[]){1});
  listPush(a, (int64_t[]){2});
  listPush(outer, (List*[]){a});
  List* b = listNew(sizeof(int64_t));
  listPush(b, (int64_t[]){10});
  listPush(outer, (List*[]){b});
  listPush(a, (int64_t[]){3});
  List* first = LIST_AT(outer, 0, List*);
  List* second = LIST_AT(outer, 1, List*);
  return offInt(outer->count, 2) + offInt(first->count, 3) + offInt(LIST_AT(first, 2, int64_t), 3) + offInt(LIST_AT(second, 0, int64_t), 10);
}

static double checkADarrayOfStructsKeepsTheStructs(void) {
  List* ps = listNew(sizeof(Pt*));
  listPush(ps, (Pt*[]){Pt_new(1, 2)});
  listPush(ps, (Pt*[]){Pt_new(3, 4)});
  Pt* p = LIST_AT(ps, 1, Pt*);
  return offInt(ps->count, 2) + offInt(p->x, 3) + offInt(p->y, 4);
}

static double checkAThousandPushesKeepEveryElement(void) {
  List* xs = listNew(sizeof(int64_t));
  for (int64_t i = 0; i < 1000; i = i + 1) {
    listPush(xs, (int64_t[]){i});
  }
  int64_t total = 0;
  for (int64_t i = 0; i < xs->count; i = i + 1) {
    total = total + LIST_AT(xs, i, int64_t);
  }
  return offInt(xs->count, 1000) + offInt(total, 499500);
}

static double checkAListInsideAnEitherIsTheSameList(void) {
  List* xs = listNew(sizeof(int64_t));
  listPush(xs, (int64_t[]){4});
  Either r = makeEither(true, (List*[]){xs}, sizeof(List*));
  listPush(xs, (int64_t[]){5});
  List* back = (*(List**) r.held);
  return offInt(back->count, 2) + offInt(LIST_AT(back, 1, int64_t), 5);
}

static int64_t fillSparse(Sparse* sp) {
  sparseSet(sp, 1, 1, (int64_t[]){5});
  sparseSet(sp, 2, 3, (int64_t[]){7});
  return 0;
}

static double checkASparseWrittenInAFunctionIsSeenByTheCaller(void) {
  Sparse* sp = sparseNew(sizeof(int64_t), (int64_t[]){0});
  fillSparse(sp);
  return offInt((*(int64_t*) sparseGet(sp, 1, 1)), 5) + offInt((*(int64_t*) sparseGet(sp, 2, 3)), 7) + offInt((*(int64_t*) sparseGet(sp, 9, 9)), 0) + offInt(sp->values->count, 2);
}

static double checkLastWriteWinsForASparse(void) {
  Sparse* sp = sparseNew(sizeof(int64_t), (int64_t[]){0});
  sparseSet(sp, 3, 3, (int64_t[]){1});
  sparseSet(sp, 3, 3, (int64_t[]){2});
  sparseSet(sp, 3, 3, (int64_t[]){3});
  return offInt((*(int64_t*) sparseGet(sp, 3, 3)), 3);
}

static double checkOneDimensionalSparseSharesToo(void) {
  Sparse* sp = sparseNew(sizeof(int64_t), (int64_t[]){0});
  Sparse* alias = sp;
  sparseSet(alias, 0, 0, (int64_t[]){100});
  sparseSet(sp, 7, 0, (int64_t[]){200});
  return offInt((*(int64_t*) sparseGet(sp, 0, 0)), 100) + offInt((*(int64_t*) sparseGet(alias, 7, 0)), 200);
}

static int64_t fillMap(Map* hm) {
  mapPut(hm, (int64_t[]){1}, (int64_t[]){10});
  mapPut(hm, (int64_t[]){2}, (int64_t[]){20});
  mapPut(hm, (int64_t[]){1}, (int64_t[]){11});
  return 0;
}

static double checkAHashmapWrittenInAFunctionIsSeenByTheCaller(void) {
  Map* hm = mapNew(sizeof(int64_t), sizeof(int64_t), NULL, NULL);
  fillMap(hm);
  return offInt(MAP_GET(hm, int64_t, int64_t, 1, 0), 11) + offInt(MAP_GET(hm, int64_t, int64_t, 2, 0), 20) + offInt(MAP_GET(hm, int64_t, int64_t, 3, 0), 0) + offInt(hm->count, 2);
}

static double checkTwoNamesForOneHashmapShareIt(void) {
  Map* a = mapNew(sizeof(int64_t), sizeof(int64_t), NULL, NULL);
  Map* b = a;
  mapPut(a, (int64_t[]){5}, (int64_t[]){50});
  mapPut(b, (int64_t[]){6}, (int64_t[]){60});
  return offInt(a->count, 2) + offInt(MAP_GET(b, int64_t, int64_t, 5, 0), 50) + offInt(MAP_GET(a, int64_t, int64_t, 6, 0), 60);
}

static double checkAStringKeyedHashmapIsShared(void) {
  Map* hm = mapNew(sizeof(const char*), sizeof(int64_t), hashTextKey, sameTextKey);
  addWord(hm, "one", 1);
  addWord(hm, "two", 2);
  return offInt(MAP_GET(hm, const char*, int64_t, "one", 0), 1) + offInt(MAP_GET(hm, const char*, int64_t, "two", 0), 2) + offInt(hm->count, 2);
}

static int64_t addWord(Map* hm, const char* word, int64_t value) {
  mapPut(hm, (const char*[]){word}, (int64_t[]){value});
  return 0;
}

static double checkAHashmapOfListsKeepsTheLists(void) {
  Map* hm = mapNew(sizeof(int64_t), sizeof(List*), NULL, NULL);
  List* l = listNew(sizeof(int64_t));
  mapPut(hm, (int64_t[]){7}, (List*[]){l});
  listPush(l, (int64_t[]){5});
  listPush(l, (int64_t[]){6});
  List* got = MAP_GET(hm, int64_t, List*, 7, listNew(sizeof(int64_t)));
  return offInt(got->count, 2) + offInt(LIST_AT(got, 1, int64_t), 6);
}

static int64_t fillGrid(Grid* g) {
  for (int64_t r = 0; r < 2; r = r + 1) {
    for (int64_t c = 0; c < 3; c = c + 1) {
      GRID_AT(g, int64_t, r, c) = r * 10 + c;
    }
  }
  return 0;
}

static double checkAGridFilledInAFunctionIsSeenByTheCaller(void) {
  Grid* g = GRID_NEW(int64_t, 0, 2, 3);
  fillGrid(g);
  return offInt(GRID_AT(g, int64_t, 1, 2), 12) + offInt(GRID_AT(g, int64_t, 0, 1), 1);
}

static double checkTwoNamesForOneGridShareIt(void) {
  Grid* a = GRID_NEW(int64_t, 0, 2, 3);
  Grid* b = a;
  GRID_AT(a, int64_t, 1, 1) = 9;
  return offInt(GRID_AT(b, int64_t, 1, 1), 9);
}

static double checkAGridStartsZeroed(void) {
  Grid* t = GRID_NEW(int64_t, 0, 2, 2, 2);
  GRID_AT(t, int64_t, 1, 1, 1) = 5;
  return offInt(GRID_AT(t, int64_t, 0, 1, 0), 0) + offInt(GRID_AT(t, int64_t, 1, 1, 1), 5);
}

static double checkDarrayCopyIsIndependent(void) {
  List* a = listNew(sizeof(int64_t));
  listPush(a, (int64_t[]){1});
  listPush(a, (int64_t[]){2});
  List* b = listCopy(a);
  listPush(b, (int64_t[]){3});
  listPush(a, (int64_t[]){9});
  return offInt(a->count, 3) + offInt(b->count, 3) + offInt(LIST_AT(b, 2, int64_t), 3) + offInt(LIST_AT(a, 2, int64_t), 9) + offInt(LIST_AT(b, 0, int64_t), 1);
}

static double checkGridAndTensorCopyAreIndependent(void) {
  Grid* g = GRID_NEW(int64_t, 0, 2, 2);
  GRID_AT(g, int64_t, 1, 1) = 5;
  Grid* h = gridCopy(g);
  GRID_AT(g, int64_t, 1, 1) = 6;
  Grid* t = GRID_NEW(int64_t, 0, 2, 2, 2);
  GRID_AT(t, int64_t, 1, 0, 1) = 7;
  Grid* u = gridCopy(t);
  GRID_AT(t, int64_t, 1, 0, 1) = 8;
  return offInt(GRID_AT(h, int64_t, 1, 1), 5) + offInt(GRID_AT(g, int64_t, 1, 1), 6) + offInt(GRID_AT(u, int64_t, 1, 0, 1), 7);
}

static double checkSparseCopyIsIndependent(void) {
  Sparse* s = sparseNew(sizeof(int64_t), (int64_t[]){0});
  sparseSet(s, 1, 2, (int64_t[]){7});
  Sparse* c = sparseCopy(s);
  sparseSet(s, 3, 4, (int64_t[]){8});
  sparseSet(c, 5, 6, (int64_t[]){9});
  return offInt(s->values->count, 2) + offInt(c->values->count, 2) + offInt((*(int64_t*) sparseGet(c, 1, 2)), 7);
}

static double checkHashmapCopyIsIndependent(void) {
  Map* m = mapNew(sizeof(int64_t), sizeof(int64_t), NULL, NULL);
  mapPut(m, (int64_t[]){1}, (int64_t[]){10});
  Map* c = mapCopy(m);
  mapPut(m, (int64_t[]){2}, (int64_t[]){20});
  mapPut(c, (int64_t[]){3}, (int64_t[]){30});
  return offInt(m->count, 2) + offInt(c->count, 2) + offInt(MAP_GET(c, int64_t, int64_t, 1, 0), 10) + offInt(MAP_GET(c, int64_t, int64_t, 3, 0), 30);
}

static double checkCopyOfAStringKeyedHashmap(void) {
  Map* m = mapNew(sizeof(const char*), sizeof(int64_t), hashTextKey, sameTextKey);
  mapPut(m, (const char*[]){"a"}, (int64_t[]){1});
  Map* c = mapCopy(m);
  mapPut(m, (const char*[]){"b"}, (int64_t[]){2});
  return offInt(MAP_GET(c, const char*, int64_t, "a", 0), 1) + offInt(c->count, 1);
}

static List* makeBag(int64_t n) {
  List* d = listNew(sizeof(int64_t));
  listPush(d, (int64_t[]){n});
  listPush(d, (int64_t[]){n + 1});
  return d;
}

static int64_t sumGrid(Grid* g) {
  return GRID_AT(g, int64_t, 0, 0) + GRID_AT(g, int64_t, 1, 1);
}

static int64_t holdsOnto(List* store, List* d) {
  listPush(store, (List*[]){d});
  return 0;
}

static double checkAReturnedContainerSurvives(void) {
  List* a = makeBag(10);
  List* b = makeBag(20);
  return offInt(LIST_AT(a, 0, int64_t) + LIST_AT(a, 1, int64_t) + LIST_AT(b, 0, int64_t) + LIST_AT(b, 1, int64_t), 62);
}

static double checkAContainerLentToACalleeIsStillUsableAfterwards(void) {
  int64_t total = 0;
  for (int64_t i = 0; i < 50; i = i + 1) {
    Grid* g = GRID_NEW(int64_t, 0, 2, 2);
    GRID_AT(g, int64_t, 0, 0) = i;
    GRID_AT(g, int64_t, 1, 1) = 1;
    total = total + sumGrid(g) + GRID_AT(g, int64_t, 0, 0);
  }
  return offInt(total, 2 * 1225 + 50 + 0);
}

static double checkAContainerGivenToACalleeThatKeepsItSurvives(void) {
  List* store = listNew(sizeof(List*));
  for (int64_t i = 0; i < 3; i = i + 1) {
    List* d = listNew(sizeof(int64_t));
    listPush(d, (int64_t[]){i * 10});
    holdsOnto(store, d);
  }
  return offInt(LIST_AT(LIST_AT(store, 0, List*), 0, int64_t) + LIST_AT(LIST_AT(store, 1, List*), 0, int64_t) + LIST_AT(LIST_AT(store, 2, List*), 0, int64_t), 30);
}

static double checkAContainerReturnedFromInsideAnIfSurvives(void) {
  List* keep = listNew(sizeof(int64_t));
  listPush(keep, (int64_t[]){4});
  List* other = listNew(sizeof(int64_t));
  listPush(other, (int64_t[]){5});
  int64_t total = 0;
  for (int64_t i = 0; i < 20; i = i + 1) {
    List* scratch = listNew(sizeof(int64_t));
    listPush(scratch, (int64_t[]){i});
    if (i == 7) {
      total = total + LIST_AT(scratch, 0, int64_t);
    }
  }
  return offInt(total + LIST_AT(keep, 0, int64_t) + LIST_AT(other, 0, int64_t), 16);
}

static int64_t earlyExit(int64_t n) {
  List* scratch = listNew(sizeof(int64_t));
  listPush(scratch, (int64_t[]){n});
  if (n > 5) {
    return LIST_AT(scratch, 0, int64_t) * 2;
  }
  listPush(scratch, (int64_t[]){1});
  return scratch->count + LIST_AT(scratch, 0, int64_t);
}

static double checkAReturnInsideAnIfStillReleasesAndReturnsTheRightValue(void) {
  return offInt(earlyExit(9) * 100 + earlyExit(2), 18 * 100 + 4);
}

static const char* greeting(const char* name) {
  const char* hello = "hello, ";
  const char* full = textJoin(textJoin(hello, name), "!");
  return full;
}

static int64_t shout(const char* s) {
  if (textSame(s, "hi!")) {
    return 1;
  }
  return 0;
}

static Closure counterFrom(int64_t start) {
  int64_t* n = copyBytes((int64_t[]){start}, sizeof(int64_t));
  Closure next = (Closure){(AnyFn) closure2, closure2_capture(n)};
  return next;
}

static int64_t applyTwice(Closure f) {
  ((int64_t (*)(void*)) f.fn)(f.env);
  return ((int64_t (*)(void*)) f.fn)(f.env);
}

static double checkAReturnedStringSurvives(void) {
  const char* a = greeting("ann");
  const char* b = greeting("bo");
  if (!textSame(a, "hello, ann!")) {
    return 1.0;
  }
  if (!textSame(b, "hello, bo!")) {
    return 1.0;
  }
  return 0.0;
}

static double checkAStringLentToACalleeIsUsableAfterwards(void) {
  int64_t hits = 0;
  for (int64_t i = 0; i < 20; i = i + 1) {
    const char* s = textJoin(textJoin("h", "i"), "!");
    hits = hits + shout(s);
    if (textSame(s, "hi!")) {
      hits = hits + 1;
    }
  }
  return offInt(hits, 40);
}

static double checkAStringUsedAsAHashmapKeyIsKept(void) {
  Map* m = mapNew(sizeof(const char*), sizeof(int64_t), hashTextKey, sameTextKey);
  for (int64_t i = 0; i < 3; i = i + 1) {
    const char* key = textJoin("k", "ey");
    mapPut(m, (const char*[]){key}, (int64_t[]){i});
  }
  return offInt(MAP_GET(m, const char*, int64_t, "key", 0), 2) + offInt(m->count, 1);
}

static double checkAClosureThatIsReturnedKeepsItsBox(void) {
  Closure c = counterFrom(10);
  ((int64_t (*)(void*)) c.fn)(c.env);
  return offInt(((int64_t (*)(void*)) c.fn)(c.env), 12);
}

static double checkAClosureGivenToACalleeKeepsWorking(void) {
  int64_t* n = copyBytes((int64_t[]){0}, sizeof(int64_t));
  Closure tick = (Closure){(AnyFn) closure3, closure3_capture(n)};
  return offInt(applyTwice(tick), 10) + offInt((*n), 10);
}

static double checkAClosureOnlyCalledLocallyInALoop(void) {
  int64_t total = 0;
  for (int64_t i = 0; i < 50; i = i + 1) {
    int64_t* count = copyBytes((int64_t[]){i}, sizeof(int64_t));
    Closure bump = (Closure){(AnyFn) closure4, closure4_capture(count)};
    ((int64_t (*)(void*)) bump.fn)(bump.env);
    total = total + ((int64_t (*)(void*)) bump.fn)(bump.env) - i;
  }
  return offInt(total, 100);
}

static double checkTwoClosuresShareOneCapturedName(void) {
  int64_t* n = copyBytes((int64_t[]){0}, sizeof(int64_t));
  Closure inc = (Closure){(AnyFn) closure6, closure6_capture(n)};
  Closure read = (Closure){(AnyFn) closure7, closure7_capture(n)};
  ((int64_t (*)(void*)) inc.fn)(inc.env);
  ((int64_t (*)(void*)) inc.fn)(inc.env);
  return offInt(((int64_t (*)(void*)) read.fn)(read.env), 20);
}

static Pocket* Pocket_new(List* items) {
  Pocket* made = gcAlloc(sizeof(Pocket));
  made->items = items;
  return made;
}

static bool Pocket_equal(Pocket* a, Pocket* b) {
  if (a == b || a == NULL || b == NULL) {
    return a == b;
  }

  bool same = true;
  same = same && listSame(a->items, b->items, NULL);
  return same;
}

static uint64_t Pocket_hash(Pocket* a) {
  uint64_t hash = 17;
  hash = hash * 31 + hashBytes(&a->items, sizeof a->items);
  return hash;
}

static List* madeHere(void) {
  List* d = listNew(sizeof(int64_t));
  listPush(d, (int64_t[]){1});
  return d;
}

static List* madeThroughAnother(void) {
  List* d = madeHere();
  listPush(d, (int64_t[]){2});
  return d;
}

static List* madeInPlace(void) {
  return listNew(sizeof(int64_t));
}

static List* sameAsArgument(List* d) {
  return d;
}

static List* elementOf(List* xs) {
  return LIST_AT(xs, 0, List*);
}

static List* fieldOf(Pocket* p) {
  return p->items;
}

static List* keptAndReturned(List* store) {
  List* d = listNew(sizeof(int64_t));
  listPush(store, (List*[]){d});
  return d;
}

static List* sometimesTheArgument(int64_t flag, List* d) {
  if (flag == 0) {
    return d;
  }
  return listNew(sizeof(int64_t));
}

static double checkAFreshResultIsUsableAndReleased(void) {
  int64_t total = 0;
  for (int64_t i = 0; i < 100; i = i + 1) {
    List* a = madeHere();
    List* b = madeThroughAnother();
    List* c = madeInPlace();
    listPush(c, (int64_t[]){i});
    total = total + a->count + b->count + c->count;
  }
  return offInt(total, 100 * 4);
}

static double checkAnAliasReturnedByACallIsNotReleasedUnderItsOwner(void) {
  List* src = listNew(sizeof(int64_t));
  listPush(src, (int64_t[]){7});
  List* same_ = sameAsArgument(src);
  listPush(same_, (int64_t[]){8});
  List* outer = listNew(sizeof(List*));
  listPush(outer, (List*[]){src});
  List* inner = elementOf(outer);
  Pocket* pocket = Pocket_new(src);
  List* field = fieldOf(pocket);
  List* maybe = sometimesTheArgument(0, src);
  return offInt(src->count, 2) + offInt(inner->count, 2) + offInt(field->count, 2) + offInt(maybe->count, 2);
}

static double checkAnObjectKeptByTheCalleeSurvivesItsCaller(void) {
  List* store = listNew(sizeof(List*));
  int64_t total = 0;
  for (int64_t i = 0; i < 5; i = i + 1) {
    List* d = keptAndReturned(store);
    listPush(d, (int64_t[]){i});
  }
  for (int64_t j = 0; j < 5; j = j + 1) {
    total = total + LIST_AT(LIST_AT(store, j, List*), 0, int64_t);
  }
  return offInt(total, 10);
}

static List* handedOn(void) {
  List* a = madeHere();
  return a;
}

static double checkAResultHandedOnAgainIsStillFresh(void) {
  List* a = handedOn();
  return offInt(LIST_AT(a, 0, int64_t), 1);
}

static double checkADiscardedFreshResultIsHarmless(void) {
  madeHere();
  madeThroughAnother();
  return 0.0;
}

static Pocket* madeStruct(int64_t n) {
  Pocket* p = Pocket_new(listNew(sizeof(int64_t)));
  listPush(p->items, (int64_t[]){n});
  return p;
}

static const char* makeLabel(const char* a) {
  const char* s = textJoin(textJoin(a, "-"), a);
  return s;
}

static double checkAStructAndAStringHandedOverByACall(void) {
  int64_t total = 0;
  for (int64_t i = 0; i < 50; i = i + 1) {
    Pocket* p = madeStruct(i);
    const char* s = makeLabel("x");
    if (textSame(s, "x-x")) {
      total = total + LIST_AT(p->items, 0, int64_t) + 1;
    }
  }
  return offInt(total, 1225 + 50);
}

static Shelf* Shelf_new(List* books, const char* tags) {
  Shelf* made = gcAlloc(sizeof(Shelf));
  made->books = books;
  made->tags = tags;
  return made;
}

static bool Shelf_equal(Shelf* a, Shelf* b) {
  if (a == b || a == NULL || b == NULL) {
    return a == b;
  }

  bool same = true;
  same = same && listSame(a->books, b->books, NULL);
  same = same && textSame(a->tags, b->tags);
  return same;
}

static uint64_t Shelf_hash(Shelf* a) {
  uint64_t hash = 17;
  hash = hash * 31 + hashBytes(&a->books, sizeof a->books);
  hash = hash * 31 + (uint64_t) textHash(a->tags);
  return hash;
}

static Cabinet* Cabinet_new(List* drawer) {
  Cabinet* made = gcAlloc(sizeof(Cabinet));
  made->drawer = drawer;
  return made;
}

static bool Cabinet_equal(Cabinet* a, Cabinet* b) {
  if (a == b || a == NULL || b == NULL) {
    return a == b;
  }

  bool same = true;
  same = same && listSame(a->drawer, b->drawer, NULL);
  return same;
}

static uint64_t Cabinet_hash(Cabinet* a) {
  uint64_t hash = 17;
  hash = hash * 31 + hashBytes(&a->drawer, sizeof a->drawer);
  return hash;
}

static Crate* Crate_new(List* items) {
  Crate* made = gcAlloc(sizeof(Crate));
  made->items = items;
  return made;
}

static bool Crate_equal(Crate* a, Crate* b) {
  if (a == b || a == NULL || b == NULL) {
    return a == b;
  }

  bool same = true;
  same = same && listSame(a->items, b->items, NULL);
  return same;
}

static uint64_t Crate_hash(Crate* a) {
  uint64_t hash = 17;
  hash = hash * 31 + hashBytes(&a->items, sizeof a->items);
  return hash;
}

static Shelf* stockedShelf(int64_t n) {
  Shelf* s = Shelf_new(listNew(sizeof(int64_t)), textJoin("a", "b"));
  listPush(s->books, (int64_t[]){n});
  listPush(s->books, (int64_t[]){n + 1});
  return s;
}

static List* openDrawer(Cabinet* c) {
  return c->drawer;
}

static double checkAnOwnedFieldIsUsableThenReleasedWithItsStruct(void) {
  int64_t total = 0;
  for (int64_t i = 0; i < 100; i = i + 1) {
    Shelf* s = stockedShelf(i);
    total = total + s->books->count + LIST_AT(s->books, 0, int64_t) - i;
    if (textSame(s->tags, "ab")) {
      total = total + 1;
    }
  }
  return offInt(total, 100 * 3);
}

static double checkAReplacedFieldValueIsReleasedAndTheNewOneWorks(void) {
  Shelf* s = stockedShelf(5);
  s->books = listNew(sizeof(int64_t));
  listPush(s->books, (int64_t[]){40});
  s->tags = textJoin("x", "y");
  if (!textSame(s->tags, "xy")) {
    return 1.0;
  }
  return offInt(s->books->count, 1) + offInt(LIST_AT(s->books, 0, int64_t), 40);
}

static double checkAFieldThatIsReadOutIsNeverReleasedWithItsStruct(void) {
  Cabinet* c = Cabinet_new(listNew(sizeof(int64_t)));
  listPush(c->drawer, (int64_t[]){9});
  List* drawer = openDrawer(c);
  listPush(drawer, (int64_t[]){10});
  return offInt(c->drawer->count, 2) + offInt(LIST_AT(drawer, 1, int64_t), 10);
}

static double checkACopiedStructSharesNothingItMustNotFree(void) {
  Crate* a = Crate_new(listNew(sizeof(int64_t)));
  listPush(a->items, (int64_t[]){3});
  Crate* b = copyBytes(a, sizeof *a);
  listPush(b->items, (int64_t[]){4});
  return offInt(a->items->count, 2) + offInt(b->items->count, 2);
}

static double checkAListOfListsReleasesItsElements(void) {
  int64_t total = 0;
  for (int64_t i = 0; i < 100; i = i + 1) {
    List* outer = listNew(sizeof(List*));
    for (int64_t j = 0; j < 3; j = j + 1) {
      listPush(outer, (List*[]){listNew(sizeof(int64_t))});
      listPush(LIST_AT(outer, j, List*), (int64_t[]){i + j});
    }
    total = total + outer->count + LIST_AT(outer, 2, List*)->count + LIST_AT(LIST_AT(outer, 1, List*), 0, int64_t) - i;
  }
  return offInt(total, 100 * (3 + 1 + 1));
}

static List* firstOf(List* xs) {
  List* first = LIST_AT(xs, 0, List*);
  return first;
}

static double checkAnElementReadOutOfAListIsNotReleasedWithIt(void) {
  List* store = listNew(sizeof(List*));
  listPush(store, (List*[]){listNew(sizeof(int64_t))});
  listPush(LIST_AT(store, 0, List*), (int64_t[]){6});
  List* got = firstOf(store);
  listPush(got, (int64_t[]){7});
  return offInt(LIST_AT(store, 0, List*)->count, 2) + offInt(LIST_AT(got, 1, int64_t), 7);
}

static double checkAMapPastItsStartingCapacityKeepsEveryEntry(void) {
  Map* m = mapNew(sizeof(int64_t), sizeof(int64_t), NULL, NULL);
  for (int64_t i = 0; i < 1000; i = i + 1) {
    mapPut(m, (int64_t[]){i}, (int64_t[]){i * 3});
  }
  int64_t wrong = 0;
  for (int64_t i = 0; i < 1000; i = i + 1) {
    if (MAP_GET(m, int64_t, int64_t, i, 0) != i * 3) {
      wrong = wrong + 1;
    }
  }
  return offInt(wrong, 0) + offInt(m->count, 1000);
}

static double checkAGrownMapIsStillTheSameMapForItsAliases(void) {
  Map* m = mapNew(sizeof(int64_t), sizeof(int64_t), NULL, NULL);
  Map* alias = m;
  for (int64_t i = 0; i < 20; i = i + 1) {
    mapPut(alias, (int64_t[]){i}, (int64_t[]){i});
  }
  return offInt(m->count, 20) + offInt(MAP_GET(m, int64_t, int64_t, 19, 0), 19);
}

static double checkStringKeysSurviveGrowing(void) {
  Map* m = mapNew(sizeof(const char*), sizeof(int64_t), hashTextKey, sameTextKey);
  mapPut(m, (const char*[]){"a"}, (int64_t[]){1});
  mapPut(m, (const char*[]){"b"}, (int64_t[]){2});
  mapPut(m, (const char*[]){"c"}, (int64_t[]){3});
  mapPut(m, (const char*[]){"d"}, (int64_t[]){4});
  mapPut(m, (const char*[]){"a"}, (int64_t[]){10});
  return offInt(MAP_GET(m, const char*, int64_t, "a", 0), 10) + offInt(MAP_GET(m, const char*, int64_t, "b", 0), 2) + offInt(MAP_GET(m, const char*, int64_t, "c", 0), 3) + offInt(MAP_GET(m, const char*, int64_t, "d", 0), 4) + offInt(m->count, 4);
}

static GrowKey* GrowKey_new(int64_t a, int64_t b) {
  GrowKey* made = gcAlloc(sizeof(GrowKey));
  made->a = a;
  made->b = b;
  return made;
}

static bool GrowKey_equal(GrowKey* a, GrowKey* b) {
  if (a == b || a == NULL || b == NULL) {
    return a == b;
  }

  bool same = true;
  same = same && a->a == b->a;
  same = same && a->b == b->b;
  return same;
}

static uint64_t GrowKey_hash(GrowKey* a) {
  uint64_t hash = 17;
  hash = hash * 31 + (uint64_t) a->a;
  hash = hash * 31 + (uint64_t) a->b;
  return hash;
}

static double checkStructKeysSurviveGrowing(void) {
  Map* m = mapNew(sizeof(GrowKey*), sizeof(int64_t), GrowKey_hashKey, GrowKey_sameKey);
  for (int64_t i = 0; i < 50; i = i + 1) {
    mapPut(m, (GrowKey*[]){GrowKey_new(i, i + 1)}, (int64_t[]){i});
  }
  int64_t wrong = 0;
  for (int64_t i = 0; i < 50; i = i + 1) {
    if (MAP_GET(m, GrowKey*, int64_t, GrowKey_new(i, i + 1), 0) != i) {
      wrong = wrong + 1;
    }
  }
  return offInt(wrong, 0) + offInt(m->count, 50);
}

static double checkASetPastItsStartingCapacityKeepsEveryMember(void) {
  Map* seen = mapNew(sizeof(int64_t), 0, NULL, NULL);
  for (int64_t i = 0; i < 100; i = i + 1) {
    mapPut(seen, (int64_t[]){i % 40}, NULL);
  }
  int64_t absent = 0;
  if (mapHas(seen, (int64_t[]){39}) == false) {
    absent = absent + 1;
  }
  if (mapHas(seen, (int64_t[]){40})) {
    absent = absent + 1;
  }
  return offInt(seen->count, 40) + offInt(absent, 0);
}

static int64_t readAt(List* xs, int64_t i) {
  return LIST_AT(xs, i, int64_t);
}

static int64_t caughtCode(List* xs, int64_t i) {
  int64_t volatile code = 0;
  {
    Handler tried = {.outer = handlers};
    handlers = &tried;

    if (setjmp(tried.jump) == 0) {
      code = readAt(xs, i);
      handlers = tried.outer;
    } else {
      handlers = tried.outer;
      const int64_t c = tried.code;
      code = c;
    }
  }
  return code;
}

static double checkAnIndexPastTheEndOfAListThrowsMinusOne(void) {
  List* xs = listNew(sizeof(int64_t));
  listPush(xs, (int64_t[]){5});
  return offInt(caughtCode(xs, 1), 0 - 1) + offInt(caughtCode(xs, 100000), 0 - 1);
}

static double checkANegativeIndexThrowsMinusOne(void) {
  List* xs = listNew(sizeof(int64_t));
  listPush(xs, (int64_t[]){5});
  return offInt(caughtCode(xs, 0 - 1), 0 - 1);
}

static double checkAnIndexInsideTheListStillReads(void) {
  List* xs = listNew(sizeof(int64_t));
  listPush(xs, (int64_t[]){5});
  listPush(xs, (int64_t[]){6});
  return offInt(caughtCode(xs, 0), 5) + offInt(caughtCode(xs, 1), 6);
}

static double checkAnEmptyListCannotBeRead(void) {
  List* xs = listNew(sizeof(int64_t));
  return offInt(caughtCode(xs, 0), 0 - 1);
}

static int64_t divideBy(int64_t a, int64_t b) {
  int64_t volatile out = 0;
  {
    Handler tried = {.outer = handlers};
    handlers = &tried;

    if (setjmp(tried.jump) == 0) {
      out = a / b;
      handlers = tried.outer;
    } else {
      handlers = tried.outer;
      const int64_t c = tried.code;
      out = c;
    }
  }
  return out;
}

static double checkDividingByZeroThrowsMinusFour(void) {
  return offInt(divideBy(10, 0), 0 - 4) + offInt(divideBy(10, 2), 5);
}

static double checkAFixedArrayAndAGridAreCheckedPerDimension(void) {
  int64_t volatile code = 0;
  Grid* volatile g = GRID_NEW(int64_t, 0, 2, 2);
  {
    Handler tried = {.outer = handlers};
    handlers = &tried;

    if (setjmp(tried.jump) == 0) {
      GRID_AT(g, int64_t, 0, 5) = 1;
      handlers = tried.outer;
    } else {
      handlers = tried.outer;
      const int64_t c = tried.code;
      code = c;
    }
  }
  return offInt(code, 0 - 1);
}

static OptPoint* OptPoint_new(int64_t x, int64_t y) {
  OptPoint* made = gcAlloc(sizeof(OptPoint));
  made->x = x;
  made->y = y;
  return made;
}

static bool OptPoint_equal(OptPoint* a, OptPoint* b) {
  if (a == b || a == NULL || b == NULL) {
    return a == b;
  }

  bool same = true;
  same = same && a->x == b->x;
  same = same && a->y == b->y;
  return same;
}

static uint64_t OptPoint_hash(OptPoint* a) {
  uint64_t hash = 17;
  hash = hash * 31 + (uint64_t) a->x;
  hash = hash * 31 + (uint64_t) a->y;
  return hash;
}

static int64_t* optionalFind(List* xs, int64_t want) {
  for (int64_t i = 0; i < xs->count; i = i + 1) {
    if (LIST_AT(xs, i, int64_t) == want) {
      return copyBytes((int64_t[]){i}, sizeof(int64_t));
    }
  }
  return NULL;
}

static OptPoint** optionalPoint(int64_t ok) {
  if (ok == 1) {
    return copyBytes((OptPoint*[]){OptPoint_new(7, 8)}, sizeof(OptPoint*));
  }
  return NULL;
}

static const char** optionalName(int64_t ok) {
  if (ok == 1) {
    return copyBytes((const char*[]){"ann"}, sizeof(const char*));
  }
  return NULL;
}

static int64_t unwrapCode(int64_t* o) {
  int64_t volatile code = 0;
  {
    Handler tried = {.outer = handlers};
    handlers = &tried;

    if (setjmp(tried.jump) == 0) {
      code = (*(int64_t*) present(o));
      handlers = tried.outer;
    } else {
      handlers = tried.outer;
      const int64_t c = tried.code;
      code = c;
    }
  }
  return code;
}

static double checkAnOptionalHoldsAValueOrNone(void) {
  List* xs = listNew(sizeof(int64_t));
  listPush(xs, (int64_t[]){10});
  listPush(xs, (int64_t[]){20});
  int64_t seen = 0;
  if (optionalFind(xs, 20) != NULL) {
    seen = seen + 1;
  }
  if (optionalFind(xs, 99) == NULL) {
    seen = seen + 10;
  }
  return offInt(seen, 11) + offInt((*(int64_t*) present(optionalFind(xs, 20))), 1);
}

static double checkAnOptionalOfAStructAndOfAStringWork(void) {
  OptPoint** p = optionalPoint(1);
  int64_t seen = 0;
  if (p != NULL) {
    seen = seen + (*(OptPoint**) present(p))->x + (*(OptPoint**) present(p))->y;
  }
  if (optionalPoint(0) == NULL) {
    seen = seen + 100;
  }
  if (optionalName(1) != NULL) {
    seen = seen + 1000;
  }
  return offInt(seen, 1115);
}

static double checkUnwrappingNoneThrowsMinusThree(void) {
  return offInt(unwrapCode(NULL), 0 - 3) + offInt(unwrapCode(copyBytes((int64_t[]){42}, sizeof(int64_t))), 42);
}
