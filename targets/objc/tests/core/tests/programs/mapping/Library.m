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

typedef struct Account Account;
typedef struct Vec2 Vec2;
typedef struct Box_double Box_double;

static int64_t visits = 0;

struct Account {
  int64_t balance;
  const char* owner;
};

struct Vec2 {
  double x;
  double y;
};

struct Box_double {
  double value;
};

static Account* Account_new(int64_t balance, const char* owner);
static bool Account_equal(Account* a, Account* b);
static uint64_t Account_hash(Account* a);
static int64_t Account_deposit(Account* self, int64_t amount);
static int64_t Account_depositTwice(Account* self, int64_t amount);

static Vec2* Vec2_new(double x, double y);
static bool Vec2_equal(Vec2* a, Vec2* b);
static uint64_t Vec2_hash(Vec2* a);
static double Vec2_length2(Vec2* self);
static Vec2* Vec2_scaled(Vec2* self, double k);

static int64_t fib(int64_t n);
static int64_t classify(int64_t n);
static int64_t bump(void);
static const char* greeting(const char* name);
static int32_t narrow(int32_t a, int32_t b);
static int64_t apply(Closure f, int64_t x);
static int64_t main_(void);

static Box_double* Box_double_new(double value);
static bool Box_double_equal(Box_double* a, Box_double* b);
static uint64_t Box_double_hash(Box_double* a);

typedef struct closure1_Env {
  int64_t factor;
} closure1_Env;

static closure1_Env* closure1_capture(int64_t factor) {
  closure1_Env* env = gcAlloc(sizeof(closure1_Env));
  env->factor = factor;
  return env;
}

static int64_t closure1(void* raw, int64_t x) {
  closure1_Env* env = raw;
  return x * env->factor;
}

static Account* Account_new(int64_t balance, const char* owner) {
  Account* made = gcAlloc(sizeof(Account));
  made->balance = balance;
  made->owner = owner;
  return made;
}

static bool Account_equal(Account* a, Account* b) {
  if (a == b || a == NULL || b == NULL) {
    return a == b;
  }

  bool same = true;
  same = same && a->balance == b->balance;
  same = same && textSame(a->owner, b->owner);
  return same;
}

static uint64_t Account_hash(Account* a) {
  uint64_t hash = 17;
  hash = hash * 31 + (uint64_t) a->balance;
  hash = hash * 31 + (uint64_t) textHash(a->owner);
  return hash;
}

static int64_t Account_deposit(Account* self, int64_t amount) {
  self->balance = self->balance + amount;
  return self->balance;
}

static int64_t Account_depositTwice(Account* self, int64_t amount) {
  Account_deposit(self, amount);
  return Account_deposit(self, amount);
}

static Vec2* Vec2_new(double x, double y) {
  Vec2* made = gcAlloc(sizeof(Vec2));
  made->x = x;
  made->y = y;
  return made;
}

static bool Vec2_equal(Vec2* a, Vec2* b) {
  if (a == b || a == NULL || b == NULL) {
    return a == b;
  }

  bool same = true;
  same = same && a->x == b->x;
  same = same && a->y == b->y;
  return same;
}

static uint64_t Vec2_hash(Vec2* a) {
  uint64_t hash = 17;
  hash = hash * 31 + hashBytes(&a->x, sizeof a->x);
  hash = hash * 31 + hashBytes(&a->y, sizeof a->y);
  return hash;
}

static double Vec2_length2(Vec2* self) {
  return self->x * self->x + self->y * self->y;
}

static Vec2* Vec2_scaled(Vec2* self, double k) {
  return Vec2_new(self->x * k, self->y * k);
}

static int64_t fib(int64_t n) {
  return n < 2 ? n : fib(n - 1) + fib(n - 2);
}

static int64_t classify(int64_t n) {
  return n < 0 ? 1 : n == 0 ? 2 : n < 10 ? 3 : 4;
}

static int64_t bump(void) {
  visits = visits + 1;
  return visits;
}

static const char* greeting(const char* name) {
  return textJoin("hello, ", name);
}

static int32_t narrow(int32_t a, int32_t b) {
  return a * b + 7;
}

static int64_t apply(Closure f, int64_t x) {
  return ((int64_t (*)(void*, int64_t)) f.fn)(f.env, x) + ((int64_t (*)(void*, int64_t)) f.fn)(f.env, x + 1);
}

static int64_t main_(void) {
  int64_t score = 0;
  score = score + fib(10);
  Account* acct = Account_new(100, "ann");
  score = score + Account_depositTwice(acct, 5);
  Vec2* v = Vec2_new(3.0, 4.0);
  Vec2* w = Vec2_scaled(v, 2.0);
  score = score + (int64_t) Vec2_length2(w);
  if (textSame(greeting("bo"), "hello, bo")) {
    score = score + 1;
  }
  score = score + (int64_t) narrow(6, 7);
  Map* seen = mapNew(sizeof(int64_t), 0, NULL, NULL);
  for (int64_t i = 0; i < 20; i = i + 1) {
    mapPut(seen, (int64_t[]){i % 7}, NULL);
  }
  score = score + seen->count;
  List* nums = listNew(sizeof(int64_t));
  {
    int64_t i = 0;
    int64_t j = 10;
    while (i < 5) {
      listPush(nums, (int64_t[]){i * j});
      i = i + 1;
      j = j - 1;
    }
  }
  LIST_AT(nums, 0, int64_t) = 100;
  score = score + LIST_AT(nums, 0, int64_t) + LIST_AT(nums, 4, int64_t) + nums->count;
  Map* ages = mapNew(sizeof(const char*), sizeof(int64_t), hashTextKey, sameTextKey);
  mapPut(ages, (const char*[]){"ann"}, (int64_t[]){31});
  if (mapHas(ages, (const char*[]){"ann"})) {
    score = score + MAP_GET(ages, const char*, int64_t, "ann", 0);
  }
  if (mapHas(ages, (const char*[]){"zed"}) == false) {
    score = score + 1;
  }
  int64_t factor = 3;
  score = score + apply((Closure){(AnyFn) closure1, closure1_capture(factor)}, 4);
  Box_double* b = Box_double_new(2.5);
  score = score + (int64_t) (b->value * 4.0);
  score = score + classify(0 - 5) + classify(0) + classify(7) + classify(70);
  score = score + bump() + bump();
  return score;
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

int main(void) {
  return (int) main_();
}
