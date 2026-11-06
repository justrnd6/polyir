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

typedef struct Pixel Pixel;

struct Pixel {
  uint8_t r;
  uint8_t g;
  uint8_t b;
};

static Pixel* Pixel_new(uint8_t r, uint8_t g, uint8_t b);
static bool Pixel_equal(Pixel* a, Pixel* b);
static uint64_t Pixel_hash(Pixel* a);

static int64_t comparison(void);
static int64_t division(void);
static int64_t shifting(void);
static int64_t widening(void);
static int64_t wrapping(void);
static int64_t floats(void);
static int64_t largest(void);
static uint32_t fnv(int64_t n);
static int64_t hashing(void);
static int64_t fields(void);
static int64_t keys(void);
static int64_t main_(void);

static Pixel* Pixel_new(uint8_t r, uint8_t g, uint8_t b) {
  Pixel* made = gcAlloc(sizeof(Pixel));
  made->r = r;
  made->g = g;
  made->b = b;
  return made;
}

static bool Pixel_equal(Pixel* a, Pixel* b) {
  if (a == b || a == NULL || b == NULL) {
    return a == b;
  }

  bool same = true;
  same = same && a->r == b->r;
  same = same && a->g == b->g;
  same = same && a->b == b->b;
  return same;
}

static uint64_t Pixel_hash(Pixel* a) {
  uint64_t hash = 17;
  hash = hash * 31 + (uint64_t) a->r;
  hash = hash * 31 + (uint64_t) a->g;
  hash = hash * 31 + (uint64_t) a->b;
  return hash;
}

static int64_t comparison(void) {
  uint32_t big = 4000000000u;
  if (big > 3000000000u && big >= big && big < 5u == false) {
    return 1;
  }
  return 0;
}

static int64_t division(void) {
  uint32_t big = 4000000000u;
  uint32_t half = big / 2u;
  uint32_t rest = big % 7u;
  if ((int64_t) half == 2000000000 && (int64_t) rest == 3) {
    return 1;
  }
  return 0;
}

static int64_t shifting(void) {
  uint32_t big = 4000000000u;
  uint64_t top = (uint64_t) (0 - 1);
  if ((int64_t) (big >> 28u) == 14 && (int64_t) (top >> 63u) == 1) {
    return 1;
  }
  return 0;
}

static int64_t widening(void) {
  uint32_t big = 4000000000u;
  uint8_t small = 200;
  if ((int64_t) big == 4000000000 && (int64_t) small == 200) {
    return 1;
  }
  return 0;
}

static int64_t wrapping(void) {
  uint8_t a = 200;
  uint8_t b = 100;
  uint8_t sum = (uint8_t) (a + b);
  uint8_t under = (uint8_t) (b - a);
  uint16_t product = 300;
  uint16_t wide = (uint16_t) (product * 300);
  if ((int64_t) sum == 44 && (int64_t) under == 156 && (int64_t) wide == 24464) {
    return 1;
  }
  return 0;
}

static int64_t floats(void) {
  uint32_t big = 4000000000u;
  if ((int64_t) (double) big == 4000000000) {
    return 1;
  }
  return 0;
}

static int64_t largest(void) {
  uint64_t top = (uint64_t) (0 - 1);
  if (top > 5u && top / 2u == 9223372036854775807u) {
    return 1;
  }
  return 0;
}

static uint32_t fnv(int64_t n) {
  uint32_t h = 2166136261u;
  for (int64_t i = 0; i < n; i = i + 1) {
    h = (h ^ (uint32_t) i) * 16777619u;
  }
  return h;
}

static int64_t hashing(void) {
  if ((int64_t) fnv(3) == 581859880) {
    return 1;
  }
  return 0;
}

static int64_t fields(void) {
  Pixel* a = Pixel_new(250, 128, 7);
  Pixel* b = Pixel_new(250, 128, 7);
  if (Pixel_equal(a, b) && (int64_t) (a->g) == 128 && a->r > a->b) {
    return 1;
  }
  return 0;
}

static int64_t keys(void) {
  Map* m = mapNew(sizeof(uint32_t), sizeof(int64_t), NULL, NULL);
  uint32_t k = 4000000000u;
  mapPut(m, (uint32_t[]){k}, (int64_t[]){7});
  mapPut(m, (uint32_t[]){5u}, (int64_t[]){9});
  if (MAP_GET(m, uint32_t, int64_t, k, 0) == 7 && MAP_GET(m, uint32_t, int64_t, 5u, 0) == 9) {
    return 1;
  }
  return 0;
}

static int64_t main_(void) {
  return comparison() + division() + shifting() + widening() + wrapping() + floats() + largest() + hashing() + fields() + keys();
}

int main(void) {
  return (int) main_();
}
