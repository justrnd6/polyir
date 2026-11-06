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

typedef struct Pt Pt;
typedef struct Counter Counter;
typedef struct Sq Sq;
typedef struct Rect Rect;
typedef struct Box_i64 Box_i64;

struct Pt {
  int64_t x;
  int64_t y;
};

struct Counter {
  int64_t n;
  const char* name;
};

static const char Sq_kind[] = "Sq";

struct Sq {
  const char* kind;
  int64_t side;
};

static const char Rect_kind[] = "Rect";

struct Rect {
  const char* kind;
  int64_t w;
  int64_t h;
};

struct Box_i64 {
  int64_t value;
};

static Pt* Pt_new(int64_t x, int64_t y);
static bool Pt_equal(Pt* a, Pt* b);
static uint64_t Pt_hash(Pt* a);

static Counter* Counter_new(int64_t n, const char* name);
static bool Counter_equal(Counter* a, Counter* b);
static uint64_t Counter_hash(Counter* a);

static int64_t Shape_area(void* self);

static Sq* Sq_new(int64_t side);
static bool Sq_equal(Sq* a, Sq* b);
static uint64_t Sq_hash(Sq* a);
static int64_t Sq_area(Sq* self);

static Rect* Rect_new(int64_t w, int64_t h);
static bool Rect_equal(Rect* a, Rect* b);
static uint64_t Rect_hash(Rect* a);
static int64_t Rect_area(Rect* self);

static int64_t totalArea(List* shapes);
static int64_t risky(int64_t x);
static int64_t guarded(int64_t x);
static int64_t main_(void);

static Box_i64* Box_i64_new(int64_t value);
static bool Box_i64_equal(Box_i64* a, Box_i64* b);
static uint64_t Box_i64_hash(Box_i64* a);

typedef struct closure1_Env {
  int64_t* calls;
} closure1_Env;

static closure1_Env* closure1_capture(int64_t* calls) {
  closure1_Env* env = gcAlloc(sizeof(closure1_Env));
  env->calls = calls;
  return env;
}

static int64_t closure1(void* raw) {
  closure1_Env* env = raw;
  (*env->calls) = (*env->calls) + 1;
  return (*env->calls);
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

static Counter* Counter_new(int64_t n, const char* name) {
  Counter* made = gcAlloc(sizeof(Counter));
  made->n = n;
  made->name = name;
  return made;
}

static bool Counter_equal(Counter* a, Counter* b) {
  if (a == b || a == NULL || b == NULL) {
    return a == b;
  }

  bool same = true;
  same = same && a->n == b->n;
  same = same && textSame(a->name, b->name);
  return same;
}

static uint64_t Counter_hash(Counter* a) {
  uint64_t hash = 17;
  hash = hash * 31 + (uint64_t) a->n;
  hash = hash * 31 + (uint64_t) textHash(a->name);
  return hash;
}

static int64_t Shape_area(void* self) {
  const char* kind = *(const char**) self;
  if (kind == Sq_kind) {
    return Sq_area(self);
  }
  if (kind == Rect_kind) {
    return Rect_area(self);
  }
  fail("no struct of this kind implements Shape");
}

static Sq* Sq_new(int64_t side) {
  Sq* made = gcAlloc(sizeof(Sq));
  made->kind = Sq_kind;
  made->side = side;
  return made;
}

static bool Sq_equal(Sq* a, Sq* b) {
  if (a == b || a == NULL || b == NULL) {
    return a == b;
  }

  bool same = true;
  same = same && a->side == b->side;
  return same;
}

static uint64_t Sq_hash(Sq* a) {
  uint64_t hash = 17;
  hash = hash * 31 + (uint64_t) a->side;
  return hash;
}

static int64_t Sq_area(Sq* self) {
  return self->side * self->side;
}

static Rect* Rect_new(int64_t w, int64_t h) {
  Rect* made = gcAlloc(sizeof(Rect));
  made->kind = Rect_kind;
  made->w = w;
  made->h = h;
  return made;
}

static bool Rect_equal(Rect* a, Rect* b) {
  if (a == b || a == NULL || b == NULL) {
    return a == b;
  }

  bool same = true;
  same = same && a->w == b->w;
  same = same && a->h == b->h;
  return same;
}

static uint64_t Rect_hash(Rect* a) {
  uint64_t hash = 17;
  hash = hash * 31 + (uint64_t) a->w;
  hash = hash * 31 + (uint64_t) a->h;
  return hash;
}

static int64_t Rect_area(Rect* self) {
  return self->w * self->h;
}

static int64_t totalArea(List* shapes) {
  int64_t sum = 0;
  for (int64_t i = 0; i < shapes->count; i = i + 1) {
    sum = sum + Shape_area(LIST_AT(shapes, i, void*));
  }
  return sum;
}

static int64_t risky(int64_t x) {
  if (x < 0) {
    throwCode(7);
  }
  return x;
}

static int64_t guarded(int64_t x) {
  int64_t volatile out = 0;
  {
    Handler tried = {.outer = handlers};
    handlers = &tried;

    if (setjmp(tried.jump) == 0) {
      out = risky(x);
      handlers = tried.outer;
    } else {
      handlers = tried.outer;
      const int64_t code = tried.code;
      out = code * 10;
    }
  }
  return out;
}

static int64_t main_(void) {
  int64_t score = 0;
  Pt* p = Pt_new(3, 4);
  Pt* q = Pt_new(3, 4);
  if (Pt_equal(p, q)) {
    score = score + 1;
  }
  Counter* c = Counter_new(5, "hits");
  Counter* d = c;
  d->n = d->n + 1;
  if (c == d) {
    score = score + c->n;
  }
  List* shapes = listNew(sizeof(void*));
  listPush(shapes, (void*[]){Sq_new(3)});
  listPush(shapes, (void*[]){Rect_new(2, 5)});
  score = score + totalArea(shapes);
  int64_t* calls = copyBytes((int64_t[]){0}, sizeof(int64_t));
  Closure tick = (Closure){(AnyFn) closure1, closure1_capture(calls)};
  int64_t first = ((int64_t (*)(void*)) tick.fn)(tick.env);
  int64_t second = ((int64_t (*)(void*)) tick.fn)(tick.env);
  score = score + first + second + (*calls);
  Map* stock = mapNew(sizeof(const char*), sizeof(int64_t), hashTextKey, sameTextKey);
  mapPut(stock, (const char*[]){"apple"}, (int64_t[]){3});
  mapPut(stock, (const char*[]){"pear"}, (int64_t[]){4});
  score = score + MAP_GET(stock, const char*, int64_t, "apple", 0) * MAP_GET(stock, const char*, int64_t, "pear", 0);
  Box_i64* b = Box_i64_new(7);
  score = score + b->value;
  score = score + guarded(5) + guarded(0 - 1);
  return score;
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

int main(void) {
  return (int) main_();
}
