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
typedef struct Cell2 Cell2;

struct Point {
  int64_t x;
  int64_t y;
};

struct Named {
  const char* name;
  const char** tag;
};

struct Cell2 {
  int64_t v;
  const char* note;
};

static Point* Point_new(int64_t x, int64_t y);
static bool Point_equal(Point* a, Point* b);
static uint64_t Point_hash(Point* a);

static Named* Named_new(const char* name, const char** tag);
static bool Named_equal(Named* a, Named* b);
static uint64_t Named_hash(Named* a);

static Cell2* Cell2_new(int64_t v, const char* note);
static bool Cell2_equal(Cell2* a, Cell2* b);
static uint64_t Cell2_hash(Cell2* a);
static int64_t Cell2_bump(Cell2* self);

static int64_t double_(int64_t x);
static int64_t inc(int64_t x);
static int64_t sets(void);
static int64_t maps(void);
static int64_t strs(void);
static int64_t opts(void);
static int64_t bufs(void);
static int64_t swaps(void);
static int64_t choose(int64_t k);
static int64_t loops(int64_t n);
static int64_t unsigned_(void);
static int64_t floats(void);
static int64_t main_(void);

static uint64_t Point_hashKey(const void* key, size_t size) {
  (void) size;
  return Point_hash(*(Point* const*) key);
}

static bool Point_sameKey(const void* a, const void* b, size_t size) {
  (void) size;
  return Point_equal(*(Point* const*) a, *(Point* const*) b);
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

static Named* Named_new(const char* name, const char** tag) {
  Named* made = gcAlloc(sizeof(Named));
  made->name = name;
  made->tag = tag;
  return made;
}

static bool Named_equal(Named* a, Named* b) {
  if (a == b || a == NULL || b == NULL) {
    return a == b;
  }

  bool same = true;
  same = same && textSame(a->name, b->name);
  same = same && memcmp(&a->tag, &b->tag, sizeof a->tag) == 0;
  return same;
}

static uint64_t Named_hash(Named* a) {
  uint64_t hash = 17;
  hash = hash * 31 + (uint64_t) textHash(a->name);
  hash = hash * 31 + hashBytes(&a->tag, sizeof a->tag);
  return hash;
}

static Cell2* Cell2_new(int64_t v, const char* note) {
  Cell2* made = gcAlloc(sizeof(Cell2));
  made->v = v;
  made->note = note;
  return made;
}

static bool Cell2_equal(Cell2* a, Cell2* b) {
  if (a == b || a == NULL || b == NULL) {
    return a == b;
  }

  bool same = true;
  same = same && a->v == b->v;
  same = same && textSame(a->note, b->note);
  return same;
}

static uint64_t Cell2_hash(Cell2* a) {
  uint64_t hash = 17;
  hash = hash * 31 + (uint64_t) a->v;
  hash = hash * 31 + (uint64_t) textHash(a->note);
  return hash;
}

static int64_t Cell2_bump(Cell2* self) {
  self->v = self->v + 1;
  return self->v;
}

static int64_t double_(int64_t x) {
  return x * 2;
}

static int64_t inc(int64_t x) {
  return x + 1;
}

static int64_t sets(void) {
  Map* seen = mapNew(sizeof(Point*), 0, Point_hashKey, Point_sameKey);
  mapPut(seen, (Point*[]){Point_new(1, 2)}, NULL);
  mapPut(seen, (Point*[]){Point_new(3, 4)}, NULL);
  mapPut(seen, (Point*[]){Point_new(1, 2)}, NULL);
  int64_t score = seen->count;
  if (mapHas(seen, (Point*[]){Point_new(3, 4)})) {
    score = score + 10;
  }
  Map* words = mapNew(sizeof(const char*), 0, hashTextKey, sameTextKey);
  mapPut(words, (const char*[]){"a"}, NULL);
  mapPut(words, (const char*[]){"b"}, NULL);
  mapPut(words, (const char*[]){"a"}, NULL);
  score = score + words->count * 100;
  return score;
}

static int64_t maps(void) {
  Map* m = mapNew(sizeof(const char*), sizeof(int64_t), hashTextKey, sameTextKey);
  mapPut(m, (const char*[]){"x"}, (int64_t[]){5});
  mapPut(m, (const char*[]){"y"}, (int64_t[]){6});
  mapPut(m, (const char*[]){"x"}, (int64_t[]){MAP_GET(m, const char*, int64_t, "x", 0) + 1});
  Map* byPoint = mapNew(sizeof(Point*), sizeof(const char*), Point_hashKey, Point_sameKey);
  mapPut(byPoint, (Point*[]){Point_new(1, 1)}, (const char*[]){"p"});
  int64_t score = MAP_GET(m, const char*, int64_t, "x", 0) * 10 + MAP_GET(m, const char*, int64_t, "y", 0) + m->count;
  if (mapHas(byPoint, (Point*[]){Point_new(1, 1)})) {
    score = score + 100;
  }
  if (MAP_GET(m, const char*, int64_t, "none", 0) == 0) {
    score = score + 1000;
  }
  return score;
}

static int64_t strs(void) {
  const char* a = "ab";
  const char* b = textJoin(textJoin(a, "cd"), a);
  int64_t score = (int64_t) strlen(b);
  if (textSame(b, "abcdab")) {
    score = score + 10;
  }
  if (!textSame(b, a)) {
    score = score + 100;
  }
  const char* q = "say hi {x}";
  score = score + (int64_t) strlen(q);
  return score;
}

static int64_t opts(void) {
  Named* n = Named_new("a", copyBytes((const char*[]){"t"}, sizeof(const char*)));
  Named* m = Named_new("a", copyBytes((const char*[]){"t"}, sizeof(const char*)));
  Named* o = Named_new("a", NULL);
  int64_t score = 0;
  if (Named_equal(n, m)) {
    score = score + 1;
  }
  if (Named_equal(n, o)) {
    score = score + 10;
  }
  if (o->tag == NULL) {
    score = score + 100;
  }
  return score + (int64_t) strlen((*(const char**) present(n->tag)));
}

static int64_t bufs(void) {
  List* b = listFilled(sizeof(int64_t), 4, (int64_t[]){0});
  LIST_AT(b, 0, int64_t) = 7;
  LIST_AT(b, 3, int64_t) = 9;
  int64_t s = 0;
  for (int64_t i = 0; i < 4; i = i + 1) {
    s = s + LIST_AT(b, i, int64_t);
  }
  return s;
}

static int64_t swaps(void) {
  Cell2* c = Cell2_new(1, "n");
  Cell2* d = c;
  Cell2_bump(d);
  c->note = textJoin(c->note, "!");
  if (textSame(d->note, "n!")) {
    return c->v * 10 + (int64_t) strlen(d->note);
  }
  return 0;
}

static int64_t choose(int64_t k) {
  int64_t label = 0;
  for (;;) {
    switch (label) {
      case 0: {
        if (k == 0) {
          label = 1;
          continue;
        } else if (k == 1) {
          label = 2;
          continue;
        } else {
          label = 3;
          continue;
        }
      }
      case 1: {
        return 100;
      }
      case 2: {
        return 200;
      }
      case 3: {
        return 300;
      }
    }
  }
}

static int64_t loops(int64_t n) {
  int64_t s = 0;
  {
    int64_t i = 0;
    int64_t j = n;
    while (i < j) {
      s = s + i * j;
      i = i + 1;
      j = j - 1;
    }
  }
  int64_t k = 0;
  for (int64_t t = 0; t < 4; t = t + 1) {
    k = k + 1;
  }
  return s + k;
}

static int64_t unsigned_(void) {
  uint8_t a = 250;
  uint8_t b = (uint8_t) (a + 10);
  uint32_t c = 4000000000u;
  uint32_t d = c + c;
  int32_t e = 2147483647;
  int32_t f = e + 1;
  return (int64_t) b + (int64_t) d + (int64_t) f % 1000;
}

static int64_t floats(void) {
  double x = 7.5;
  double y = 2.25;
  double z = x / 2.0 + y * 2.0;
  if (z > 8.0 && x != y) {
    return (int64_t) z;
  }
  return 0;
}

static int64_t main_(void) {
  int64_t score = 0;
  score = score + sets() + maps() + strs() + opts() + bufs() + swaps();
  score = score + choose(0) + choose(1) + choose(5) + loops(6) + unsigned_() + floats();
  return score % 251;
}

int main(void) {
  return (int) main_();
}
