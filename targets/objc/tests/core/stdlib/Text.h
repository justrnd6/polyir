#pragma once

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

typedef struct StringBuilder StringBuilder;

struct StringBuilder {
  List* pieces;
  List* indents;
};

static bool contains(const char* s, const char* part);
static bool startsWith(const char* s, const char* prefix);
static bool endsWith(const char* s, const char* suffix);
static List* split(const char* s, const char* sep);
static List* lines(const char* s);
static const char* join(List* parts, const char* sep);
static bool isSpaceChar(int8_t c);
static const char* trim(const char* s);
static const char* replace(const char* s, const char* old, const char* by);
static const char* repeat(const char* s, int64_t n);
static const char* indent(const char* s, int64_t n);
static const char* indentRest(const char* s, int64_t n);
static const char* fill(const char* template, Map* values);
static const char* filledName(const char* placeholder, Map* values);

static StringBuilder* StringBuilder_new(List* pieces, List* indents);
static bool StringBuilder_equal(StringBuilder* a, StringBuilder* b);
static uint64_t StringBuilder_hash(StringBuilder* a);
static StringBuilder* StringBuilder_add(StringBuilder* self, const char* s);
static StringBuilder* StringBuilder_line(StringBuilder* self, const char* s);
static StringBuilder* StringBuilder_blank(StringBuilder* self);
static StringBuilder* StringBuilder_lines(StringBuilder* self, List* all);
static StringBuilder* StringBuilder_indent(StringBuilder* self, int64_t n);
static StringBuilder* StringBuilder_dedent(StringBuilder* self);
static int64_t StringBuilder_indentNow(StringBuilder* self);
static int64_t StringBuilder_length(StringBuilder* self);
static bool StringBuilder_isEmpty(StringBuilder* self);
static const char* StringBuilder_text(StringBuilder* self);

static bool contains(const char* s, const char* part) {
  return textFind(s, part, 0) >= 0;
}

static bool startsWith(const char* s, const char* prefix) {
  if ((int64_t) strlen(prefix) > (int64_t) strlen(s)) {
    return false;
  }
  return textSame(textSlice(s, 0, (int64_t) strlen(prefix)), prefix);
}

static bool endsWith(const char* s, const char* suffix) {
  if ((int64_t) strlen(suffix) > (int64_t) strlen(s)) {
    return false;
  }
  return textSame(textSlice(s, (int64_t) strlen(s) - (int64_t) strlen(suffix), (int64_t) strlen(s)), suffix);
}

static List* split(const char* s, const char* sep) {
  List* parts = listNew(sizeof(const char*));
  if ((int64_t) strlen(sep) == 0) {
    listPush(parts, (const char*[]){s});
    return parts;
  }
  int64_t from = 0;
  for (int64_t at = textFind(s, sep, 0); at >= 0; at = textFind(s, sep, from)) {
    listPush(parts, (const char*[]){textSlice(s, from, at)});
    from = at + (int64_t) strlen(sep);
  }
  listPush(parts, (const char*[]){textSlice(s, from, (int64_t) strlen(s))});
  return parts;
}

static List* lines(const char* s) {
  return split(s, "\n");
}

static const char* join(List* parts, const char* sep) {
  const char* out = "";
  for (int64_t i = 0; i < parts->count; i = i + 1) {
    if (i > 0) {
      out = textJoin(out, sep);
    }
    out = textJoin(out, LIST_AT(parts, i, const char*));
  }
  return out;
}

static bool isSpaceChar(int8_t c) {
  return c == 32 || c == 9 || c == 10 || c == 13;
}

static const char* trim(const char* s) {
  int64_t from = 0;
  while (from < (int64_t) strlen(s) && isSpaceChar(textByte(s, from))) {
    from = from + 1;
  }
  int64_t to = (int64_t) strlen(s);
  while (to > from && isSpaceChar(textByte(s, to - 1))) {
    to = to - 1;
  }
  return textSlice(s, from, to);
}

static const char* replace(const char* s, const char* old, const char* by) {
  return join(split(s, old), by);
}

static const char* repeat(const char* s, int64_t n) {
  const char* out = "";
  for (int64_t i = 0; i < n; i = i + 1) {
    out = textJoin(out, s);
  }
  return out;
}

static const char* indent(const char* s, int64_t n) {
  List* all = lines(s);
  const char* pad = repeat(" ", n);
  for (int64_t i = 0; i < all->count; i = i + 1) {
    if ((int64_t) strlen(LIST_AT(all, i, const char*)) > 0) {
      LIST_AT(all, i, const char*) = textJoin(pad, LIST_AT(all, i, const char*));
    }
  }
  return join(all, "\n");
}

static const char* indentRest(const char* s, int64_t n) {
  int64_t first = textFind(s, "\n", 0);
  if (first < 0) {
    return s;
  }
  return textJoin(textSlice(s, 0, first + 1), indent(textSlice(s, first + 1, (int64_t) strlen(s)), n));
}

static const char* fill(const char* template, Map* values) {
  const char* out = "";
  int64_t from = 0;
  for (int64_t at = textFind(template, "${", 0); at >= 0; at = textFind(template, "${", from)) {
    int64_t close = textFind(template, "}", at);
    if (close < 0) {
      return textJoin(out, textSlice(template, from, (int64_t) strlen(template)));
    }
    out = textJoin(textJoin(out, textSlice(template, from, at)), filledName(textSlice(template, at, close + 1), values));
    from = close + 1;
  }
  return textJoin(out, textSlice(template, from, (int64_t) strlen(template)));
}

static const char* filledName(const char* placeholder, Map* values) {
  const char* name = trim(textSlice(placeholder, 2, (int64_t) strlen(placeholder) - 1));
  if (mapHas(values, (const char*[]){name})) {
    return MAP_GET(values, const char*, const char*, name, "");
  }
  return placeholder;
}

static StringBuilder* StringBuilder_new(List* pieces, List* indents) {
  StringBuilder* made = gcAlloc(sizeof(StringBuilder));
  made->pieces = pieces;
  made->indents = indents;
  return made;
}

static bool StringBuilder_equal(StringBuilder* a, StringBuilder* b) {
  if (a == b || a == NULL || b == NULL) {
    return a == b;
  }

  bool same = true;
  same = same && listSame(a->pieces, b->pieces, sameTextKey);
  same = same && listSame(a->indents, b->indents, NULL);
  return same;
}

static uint64_t StringBuilder_hash(StringBuilder* a) {
  uint64_t hash = 17;
  hash = hash * 31 + hashBytes(&a->pieces, sizeof a->pieces);
  hash = hash * 31 + hashBytes(&a->indents, sizeof a->indents);
  return hash;
}

static StringBuilder* StringBuilder_add(StringBuilder* self, const char* s) {
  listPush(self->pieces, (const char*[]){s});
  return self;
}

static StringBuilder* StringBuilder_line(StringBuilder* self, const char* s) {
  if ((int64_t) strlen(s) > 0) {
    listPush(self->pieces, (const char*[]){repeat(" ", StringBuilder_indentNow(self))});
  }
  listPush(self->pieces, (const char*[]){s});
  listPush(self->pieces, (const char*[]){"\n"});
  return self;
}

static StringBuilder* StringBuilder_blank(StringBuilder* self) {
  listPush(self->pieces, (const char*[]){"\n"});
  return self;
}

static StringBuilder* StringBuilder_lines(StringBuilder* self, List* all) {
  for (int64_t i = 0; i < all->count; i = i + 1) {
    StringBuilder_line(self, LIST_AT(all, i, const char*));
  }
  return self;
}

static StringBuilder* StringBuilder_indent(StringBuilder* self, int64_t n) {
  listPush(self->indents, (int64_t[]){StringBuilder_indentNow(self) + n});
  return self;
}

static StringBuilder* StringBuilder_dedent(StringBuilder* self) {
  if (self->indents->count > 0) {
    listTruncate(self->indents, self->indents->count - 1);
  }
  return self;
}

static int64_t StringBuilder_indentNow(StringBuilder* self) {
  if (self->indents->count == 0) {
    return 0;
  }
  return LIST_AT(self->indents, self->indents->count - 1, int64_t);
}

static int64_t StringBuilder_length(StringBuilder* self) {
  return (int64_t) strlen(StringBuilder_text(self));
}

static bool StringBuilder_isEmpty(StringBuilder* self) {
  return StringBuilder_length(self) == 0;
}

static const char* StringBuilder_text(StringBuilder* self) {
  return join(self->pieces, "");
}
