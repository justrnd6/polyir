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

typedef struct Geometry_Point Geometry_Point;

typedef struct Geometry_Shapes_Square Geometry_Shapes_Square;
typedef struct Geometry_Shapes_Rect Geometry_Shapes_Rect;

struct Geometry_Point {
  int64_t x;
  int64_t y;
};

static const char Geometry_Shapes_Square_kind[] = "Geometry_Shapes_Square";

struct Geometry_Shapes_Square {
  const char* kind;
  int64_t side;
};

static const char Geometry_Shapes_Rect_kind[] = "Geometry_Shapes_Rect";

struct Geometry_Shapes_Rect {
  const char* kind;
  int64_t w;
  int64_t h;
};

static Geometry_Point* Geometry_Point_new(int64_t x, int64_t y);
static bool Geometry_Point_equal(Geometry_Point* a, Geometry_Point* b);
static uint64_t Geometry_Point_hash(Geometry_Point* a);
static int64_t Geometry_Point_manhattan(Geometry_Point* self);
static int64_t Geometry_Point_absolute(Geometry_Point* self, int64_t n);

static int64_t Geometry_clamp(int64_t n, int64_t low, int64_t high);
static int64_t Geometry_distance(Geometry_Point* a, Geometry_Point* b);

static int64_t Geometry_Shapes_Shape_area(void* self);

static Geometry_Shapes_Square* Geometry_Shapes_Square_new(int64_t side);
static bool Geometry_Shapes_Square_equal(Geometry_Shapes_Square* a, Geometry_Shapes_Square* b);
static uint64_t Geometry_Shapes_Square_hash(Geometry_Shapes_Square* a);
static int64_t Geometry_Shapes_Square_area(Geometry_Shapes_Square* self);

static Geometry_Shapes_Rect* Geometry_Shapes_Rect_new(int64_t w, int64_t h);
static bool Geometry_Shapes_Rect_equal(Geometry_Shapes_Rect* a, Geometry_Shapes_Rect* b);
static uint64_t Geometry_Shapes_Rect_hash(Geometry_Shapes_Rect* a);
static int64_t Geometry_Shapes_Rect_area(Geometry_Shapes_Rect* self);

static int64_t Geometry_Shapes_total(List* shapes);

static void main_(void);
static int64_t larger_i64(int64_t a, int64_t b, bool aFirst);
static const char* larger_string(const char* a, const char* b, bool aFirst);
static const char* firstOf_string(List* xs);

static Geometry_Point* Geometry_Point_new(int64_t x, int64_t y) {
  Geometry_Point* made = gcAlloc(sizeof(Geometry_Point));
  made->x = x;
  made->y = y;
  return made;
}

static bool Geometry_Point_equal(Geometry_Point* a, Geometry_Point* b) {
  if (a == b || a == NULL || b == NULL) {
    return a == b;
  }

  bool same = true;
  same = same && a->x == b->x;
  same = same && a->y == b->y;
  return same;
}

static uint64_t Geometry_Point_hash(Geometry_Point* a) {
  uint64_t hash = 17;
  hash = hash * 31 + (uint64_t) a->x;
  hash = hash * 31 + (uint64_t) a->y;
  return hash;
}

static int64_t Geometry_Point_manhattan(Geometry_Point* self) {
  return Geometry_Point_absolute(self, self->x) + Geometry_Point_absolute(self, self->y);
}

static int64_t Geometry_Point_absolute(Geometry_Point* self, int64_t n) {
  return n < 0 ? 0 - n : n;
}

static int64_t Geometry_clamp(int64_t n, int64_t low, int64_t high) {
  return n < low ? low : n > high ? high : n;
}

static int64_t Geometry_distance(Geometry_Point* a, Geometry_Point* b) {
  Geometry_Point* step = Geometry_Point_new(a->x - b->x, a->y - b->y);
  return Geometry_clamp(Geometry_Point_manhattan(step), 0, 100);
}

static int64_t Geometry_Shapes_Shape_area(void* self) {
  const char* kind = *(const char**) self;
  if (kind == Geometry_Shapes_Square_kind) {
    return Geometry_Shapes_Square_area(self);
  }
  if (kind == Geometry_Shapes_Rect_kind) {
    return Geometry_Shapes_Rect_area(self);
  }
  fail("no struct of this kind implements Geometry_Shapes_Shape");
}

static Geometry_Shapes_Square* Geometry_Shapes_Square_new(int64_t side) {
  Geometry_Shapes_Square* made = gcAlloc(sizeof(Geometry_Shapes_Square));
  made->kind = Geometry_Shapes_Square_kind;
  made->side = side;
  return made;
}

static bool Geometry_Shapes_Square_equal(Geometry_Shapes_Square* a, Geometry_Shapes_Square* b) {
  if (a == b || a == NULL || b == NULL) {
    return a == b;
  }

  bool same = true;
  same = same && a->side == b->side;
  return same;
}

static uint64_t Geometry_Shapes_Square_hash(Geometry_Shapes_Square* a) {
  uint64_t hash = 17;
  hash = hash * 31 + (uint64_t) a->side;
  return hash;
}

static int64_t Geometry_Shapes_Square_area(Geometry_Shapes_Square* self) {
  return self->side * self->side;
}

static Geometry_Shapes_Rect* Geometry_Shapes_Rect_new(int64_t w, int64_t h) {
  Geometry_Shapes_Rect* made = gcAlloc(sizeof(Geometry_Shapes_Rect));
  made->kind = Geometry_Shapes_Rect_kind;
  made->w = w;
  made->h = h;
  return made;
}

static bool Geometry_Shapes_Rect_equal(Geometry_Shapes_Rect* a, Geometry_Shapes_Rect* b) {
  if (a == b || a == NULL || b == NULL) {
    return a == b;
  }

  bool same = true;
  same = same && a->w == b->w;
  same = same && a->h == b->h;
  return same;
}

static uint64_t Geometry_Shapes_Rect_hash(Geometry_Shapes_Rect* a) {
  uint64_t hash = 17;
  hash = hash * 31 + (uint64_t) a->w;
  hash = hash * 31 + (uint64_t) a->h;
  return hash;
}

static int64_t Geometry_Shapes_Rect_area(Geometry_Shapes_Rect* self) {
  return self->w * self->h;
}

static int64_t Geometry_Shapes_total(List* shapes) {
  int64_t sum = 0;
  for (int64_t s_at = 0; s_at < shapes->count; s_at++) {
    void* s = LIST_AT(shapes, s_at, void*);
    sum = sum + Geometry_Shapes_Shape_area(s);
  }
  return sum;
}

static void main_(void) {
  Geometry_Point* a = Geometry_Point_new(1, 2);
  Geometry_Point* b = Geometry_Point_new(4, -2);
  List* shapes = LIST_OF(void*, Geometry_Shapes_Square_new(3), Geometry_Shapes_Rect_new(2, 5));
  const char* size = Geometry_Shapes_total(shapes) > 10 ? "big" : "small";
  puts((textJoin(textJoin(textJoin(textJoin(textJoin(textJoin(textJoin(textJoin(textOfInt(Geometry_distance(a, b)), " "), (size)), " "), textOfInt(larger_i64(3, 7, false))), " "), (larger_string("x", "y", true))), " "), (firstOf_string(LIST_OF(const char*, "first", "second"))))));
}

static int64_t larger_i64(int64_t a, int64_t b, bool aFirst) {
  return aFirst ? a : b;
}

static const char* larger_string(const char* a, const char* b, bool aFirst) {
  return aFirst ? a : b;
}

static const char* firstOf_string(List* xs) {
  return LIST_AT(xs, 0, const char*);
}

int main(void) {
  main_();
  return 0;
}
