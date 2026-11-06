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

typedef struct Point Point;
typedef struct Named Named;

struct Point {
  int64_t x;
  int64_t y;
};

struct Named {
  const char* name;
  Point* at;
};

static Point* Point_new(int64_t x, int64_t y);
static bool Point_equal(Point* a, Point* b);
static uint64_t Point_hash(Point* a);

static Named* Named_new(const char* name, Point* at);
static bool Named_equal(Named* a, Named* b);
static uint64_t Named_hash(Named* a);

static double checkAnArrayGrowsByMakingANewOne(void);
static double checkThawAndFreezeCopy(void);
static double checkAMapGrowsByMakingANewOne(void);
static double checkWithReplacesAnExistingKey(void);
static double checkHasTellsPresentFromAbsent(void);
static double checkASetKeepsEachMemberOnce(void);
static double checkAMutsetChangesInPlace(void);
static double checkAMutmapChangesInPlace(void);
static double checkAStructCanBeAKey(void);
static double checkAStructWithAStringIsAKeyByContent(void);
static double checkAnImmutableArrayIsAKey(void);
static double checkMapsAreEqualWhateverTheOrderTheyWereBuiltIn(void);
static double checkSetsAreEqualWhateverTheOrderTheyWereBuiltIn(void);
static double checkMutableAndImmutableListsAreEqualByContent(void);
static double checkRepeatedWithInALoopIsNotQuadraticInMemory(void);

static uint64_t Point_hashKey(const void* key, size_t size) {
  (void) size;
  return Point_hash(*(Point* const*) key);
}

static bool Point_sameKey(const void* a, const void* b, size_t size) {
  (void) size;
  return Point_equal(*(Point* const*) a, *(Point* const*) b);
}

static uint64_t Named_hashKey(const void* key, size_t size) {
  (void) size;
  return Named_hash(*(Named* const*) key);
}

static bool Named_sameKey(const void* a, const void* b, size_t size) {
  (void) size;
  return Named_equal(*(Named* const*) a, *(Named* const*) b);
}

static Point* Point_new(int64_t x, int64_t y) {
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
  hash = hash * 31 + (uint64_t) a->x;
  hash = hash * 31 + (uint64_t) a->y;
  return hash;
}

static Named* Named_new(const char* name, Point* at) {
  Named* made = gcAlloc(sizeof(Named));
  made->name = name;
  made->at = at;
  return made;
}

static bool Named_equal(Named* a, Named* b) {
  if (a == b || a == NULL || b == NULL) {
    return a == b;
  }

  bool same = true;
  same = same && textSame(a->name, b->name);
  same = same && Point_equal(a->at, b->at);
  return same;
}

static uint64_t Named_hash(Named* a) {
  uint64_t hash = 17;
  hash = hash * 31 + (uint64_t) textHash(a->name);
  hash = hash * 31 + Point_hash(a->at);
  return hash;
}

static double checkAnArrayGrowsByMakingANewOne(void) {
  List* a0 = listNew(sizeof(int64_t));
  List* a1 = listWith(a0, (int64_t[]){10});
  List* a2 = listWith(a1, (int64_t[]){20});
  return offInt(a0->count, 0) + offInt(a1->count, 1) + offInt(a2->count, 2) + offInt(LIST_AT(a2, 0, int64_t), 10) + offInt(LIST_AT(a2, 1, int64_t), 20);
}

static double checkThawAndFreezeCopy(void) {
  List* a = LIST_OF(int64_t, 1, 2);
  List* m = listCopy(a);
  listPush(m, (int64_t[]){3});
  List* f = listCopy(m);
  listPush(m, (int64_t[]){4});
  return offInt(a->count, 2) + offInt(m->count, 4) + offInt(f->count, 3) + offInt(LIST_AT(f, 2, int64_t), 3);
}

static double checkAMapGrowsByMakingANewOne(void) {
  Map* m0 = mapNew(sizeof(int64_t), sizeof(int64_t), NULL, NULL);
  Map* m1 = mapWith(m0, (int64_t[]){1}, (int64_t[]){100});
  Map* m = m1;
  for (int64_t i = 2; i < 30; i = i + 1) {
    m = mapWith(m, (int64_t[]){i}, (int64_t[]){i * 100});
  }
  return offInt(m0->count, 0) + offInt(m1->count, 1) + offInt(m->count, 29) + offInt(MAP_GET(m, int64_t, int64_t, 1, 0), 100) + offInt(MAP_GET(m, int64_t, int64_t, 29, 0), 2900) + offInt(MAP_GET(m1, int64_t, int64_t, 2, 0), 0);
}

static double checkWithReplacesAnExistingKey(void) {
  Map* m = mapWith(mapWith(mapNew(sizeof(int64_t), sizeof(int64_t), NULL, NULL), (int64_t[]){1}, (int64_t[]){10}), (int64_t[]){1}, (int64_t[]){11});
  return offInt(m->count, 1) + offInt(MAP_GET(m, int64_t, int64_t, 1, 0), 11);
}

static double checkHasTellsPresentFromAbsent(void) {
  Map* m = mapWith(mapNew(sizeof(const char*), sizeof(int64_t), hashTextKey, sameTextKey), (const char*[]){"a"}, (int64_t[]){0});
  int64_t score = 0;
  if (mapHas(m, (const char*[]){"a"})) {
    score = score + 1;
  }
  if (mapHas(m, (const char*[]){"b"}) == false) {
    score = score + 2;
  }
  return offInt(score, 3);
}

static double checkASetKeepsEachMemberOnce(void) {
  Map* s = mapWith(mapWith(mapWith(mapNew(sizeof(int64_t), 0, NULL, NULL), (int64_t[]){7}, NULL), (int64_t[]){8}, NULL), (int64_t[]){7}, NULL);
  int64_t score = 0;
  if (mapHas(s, (int64_t[]){7})) {
    score = score + 1;
  }
  if (mapHas(s, (int64_t[]){9}) == false) {
    score = score + 2;
  }
  return offInt(s->count, 2) + offInt(score, 3);
}

static double checkAMutsetChangesInPlace(void) {
  Map* s = mapNew(sizeof(const char*), 0, hashTextKey, sameTextKey);
  mapPut(s, (const char*[]){"x"}, NULL);
  mapPut(s, (const char*[]){"y"}, NULL);
  mapPut(s, (const char*[]){"x"}, NULL);
  int64_t score = 0;
  if (mapHas(s, (const char*[]){"y"})) {
    score = score + 1;
  }
  if (mapHas(s, (const char*[]){"z"}) == false) {
    score = score + 2;
  }
  return offInt(s->count, 2) + offInt(score, 3);
}

static double checkAMutmapChangesInPlace(void) {
  Map* m = mapNew(sizeof(int64_t), sizeof(int64_t), NULL, NULL);
  mapPut(m, (int64_t[]){1}, (int64_t[]){10});
  mapPut(m, (int64_t[]){2}, (int64_t[]){20});
  mapPut(m, (int64_t[]){1}, (int64_t[]){11});
  return offInt(m->count, 2) + offInt(MAP_GET(m, int64_t, int64_t, 1, 0), 11) + offInt(MAP_GET(m, int64_t, int64_t, 2, 0), 20);
}

static double checkAStructCanBeAKey(void) {
  Map* seen = mapNew(sizeof(Point*), sizeof(int64_t), Point_hashKey, Point_sameKey);
  mapPut(seen, (Point*[]){Point_new(1, 2)}, (int64_t[]){10});
  mapPut(seen, (Point*[]){Point_new(3, 4)}, (int64_t[]){20});
  mapPut(seen, (Point*[]){Point_new(1, 2)}, (int64_t[]){11});
  int64_t score = 0;
  if (mapHas(seen, (Point*[]){Point_new(3, 4)})) {
    score = score + 1;
  }
  if (mapHas(seen, (Point*[]){Point_new(9, 9)}) == false) {
    score = score + 2;
  }
  return offInt(seen->count, 2) + offInt(MAP_GET(seen, Point*, int64_t, Point_new(1, 2), 0), 11) + offInt(score, 3);
}

static double checkAStructWithAStringIsAKeyByContent(void) {
  Map* names = mapWith(mapWith(mapWith(mapNew(sizeof(Named*), 0, Named_hashKey, Named_sameKey), (Named*[]){Named_new(textJoin("a", "b"), Point_new(1, 1))}, NULL), (Named*[]){Named_new("ab", Point_new(1, 1))}, NULL), (Named*[]){Named_new("c", Point_new(2, 2))}, NULL);
  int64_t score = 0;
  if (mapHas(names, (Named*[]){Named_new("c", Point_new(2, 2))})) {
    score = score + 1;
  }
  return offInt(names->count, 2) + offInt(score, 1);
}

static double checkAnImmutableArrayIsAKey(void) {
  Map* lists = mapWith(mapWith(mapNew(sizeof(List*), sizeof(const char*), NULL, NULL), (List*[]){LIST_OF(int64_t, 1, 2)}, (const char*[]){"one-two"}), (List*[]){LIST_OF(int64_t, 3)}, (const char*[]){"three"});
  int64_t score = 0;
  if (textSame(MAP_GET(lists, List*, const char*, LIST_OF(int64_t, 1, 2), ""), "one-two")) {
    score = score + 1;
  }
  if (textSame(MAP_GET(lists, List*, const char*, LIST_OF(int64_t, 3), ""), "three")) {
    score = score + 2;
  }
  return offInt(score, 3);
}

static double checkMapsAreEqualWhateverTheOrderTheyWereBuiltIn(void) {
  Map* a = mapWith(mapWith(mapNew(sizeof(int64_t), sizeof(const char*), NULL, NULL), (int64_t[]){1}, (const char*[]){"a"}), (int64_t[]){2}, (const char*[]){"b"});
  Map* b = mapWith(mapWith(mapNew(sizeof(int64_t), sizeof(const char*), NULL, NULL), (int64_t[]){2}, (const char*[]){"b"}), (int64_t[]){1}, (const char*[]){"a"});
  Map* c = mapWith(mapWith(mapNew(sizeof(int64_t), sizeof(const char*), NULL, NULL), (int64_t[]){2}, (const char*[]){"b"}), (int64_t[]){1}, (const char*[]){"z"});
  int64_t score = 0;
  if (a == b) {
    score = score + 1;
  }
  if (a != c) {
    score = score + 2;
  }
  return offInt(score, 3);
}

static double checkSetsAreEqualWhateverTheOrderTheyWereBuiltIn(void) {
  Map* a = mapWith(mapWith(mapNew(sizeof(const char*), 0, hashTextKey, sameTextKey), (const char*[]){"x"}, NULL), (const char*[]){"y"}, NULL);
  Map* b = mapWith(mapWith(mapNew(sizeof(const char*), 0, hashTextKey, sameTextKey), (const char*[]){"y"}, NULL), (const char*[]){"x"}, NULL);
  int64_t score = 0;
  if (a == b) {
    score = score + 1;
  }
  if (a != mapWith(mapNew(sizeof(const char*), 0, hashTextKey, sameTextKey), (const char*[]){"x"}, NULL)) {
    score = score + 2;
  }
  return offInt(score, 3);
}

static double checkMutableAndImmutableListsAreEqualByContent(void) {
  List* a = LIST_OF(int64_t, 1, 2);
  List* b = LIST_OF(int64_t, 1, 2);
  int64_t score = 0;
  if (listSame(a, b, NULL)) {
    score = score + 1;
  }
  if (a == b) {
    score = score + 100;
  }
  return offInt(score, 1);
}

static double checkRepeatedWithInALoopIsNotQuadraticInMemory(void) {
  int64_t total = 0;
  for (int64_t r = 0; r < 200; r = r + 1) {
    List* a = listNew(sizeof(int64_t));
    Map* m = mapNew(sizeof(int64_t), sizeof(int64_t), NULL, NULL);
    Map* s = mapNew(sizeof(int64_t), 0, NULL, NULL);
    for (int64_t i = 0; i < 20; i = i + 1) {
      a = listWith(a, (int64_t[]){i});
      m = mapWith(m, (int64_t[]){i}, (int64_t[]){i * 2});
      s = mapWith(s, (int64_t[]){i % 7}, NULL);
    }
    total = total + a->count + MAP_GET(m, int64_t, int64_t, 19, 0) + s->count;
  }
  return offInt(total, 200 * 65);
}
