#include <setjmp.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

typedef struct Square Square;
typedef struct Rect Rect;
typedef struct Summer Summer;
typedef struct Holder Holder;

static const char Square_kind[] = "Square";

struct Square {
  const char* kind;
  int64_t side;
};

static const char Rect_kind[] = "Rect";

struct Rect {
  const char* kind;
  int64_t w;
  int64_t h;
};

static const char Summer_kind[] = "Summer";

struct Summer {
  const char* kind;
  int64_t base;
};

struct Holder {
  void* shape;
  int64_t tag;
};

static int64_t Shape_area(void* self);
static int64_t Shape_scaled(void* self, int64_t p0);

static int64_t Named_id(void* self);
static int64_t Combiner_combine(void* self, void* p0);

static Square* Square_new(int64_t side);
static bool Square_equal(Square* a, Square* b);
static uint64_t Square_hash(Square* a);
static int64_t Square_area(Square* self);
static int64_t Square_scaled(Square* self, int64_t k);
static int64_t Square_id(Square* self);

static Rect* Rect_new(int64_t w, int64_t h);
static bool Rect_equal(Rect* a, Rect* b);
static uint64_t Rect_hash(Rect* a);
static int64_t Rect_area(Rect* self);
static int64_t Rect_scaled(Rect* self, int64_t k);

static Summer* Summer_new(int64_t base);
static bool Summer_equal(Summer* a, Summer* b);
static uint64_t Summer_hash(Summer* a);
static int64_t Summer_combine(Summer* self, void* other);

static Holder* Holder_new(void* shape, int64_t tag);
static bool Holder_equal(Holder* a, Holder* b);
static uint64_t Holder_hash(Holder* a);

static int64_t totalArea(void* s);
static int64_t directCall(void);
static void* pick(int64_t which);
static int64_t chosen(void);
static int64_t sumAll(void);
static int64_t viaField(void);
static int64_t twoInterfaces(void);
static int64_t interfaceArgument(void);
static int64_t main_(void);

static int64_t Shape_area(void* self) {
  const char* kind = *(const char**) self;
  if (kind == Square_kind) {
    return Square_area(self);
  }
  if (kind == Rect_kind) {
    return Rect_area(self);
  }
  fail("no struct of this kind implements Shape");
}

static int64_t Shape_scaled(void* self, int64_t p0) {
  const char* kind = *(const char**) self;
  if (kind == Square_kind) {
    return Square_scaled(self, p0);
  }
  if (kind == Rect_kind) {
    return Rect_scaled(self, p0);
  }
  fail("no struct of this kind implements Shape");
}

static int64_t Named_id(void* self) {
  const char* kind = *(const char**) self;
  if (kind == Square_kind) {
    return Square_id(self);
  }
  fail("no struct of this kind implements Named");
}

static int64_t Combiner_combine(void* self, void* p0) {
  const char* kind = *(const char**) self;
  if (kind == Summer_kind) {
    return Summer_combine(self, p0);
  }
  fail("no struct of this kind implements Combiner");
}

static Square* Square_new(int64_t side) {
  Square* made = gcAlloc(sizeof(Square));
  made->kind = Square_kind;
  made->side = side;
  return made;
}

static bool Square_equal(Square* a, Square* b) {
  if (a == b || a == NULL || b == NULL) {
    return a == b;
  }

  bool same = true;
  same = same && a->side == b->side;
  return same;
}

static uint64_t Square_hash(Square* a) {
  uint64_t hash = 17;
  hash = hash * 31 + (uint64_t) a->side;
  return hash;
}

static int64_t Square_area(Square* self) {
  return self->side * self->side;
}

static int64_t Square_scaled(Square* self, int64_t k) {
  return self->side * k;
}

static int64_t Square_id(Square* self) {
  return 1;
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

static int64_t Rect_scaled(Rect* self, int64_t k) {
  return self->w * k;
}

static Summer* Summer_new(int64_t base) {
  Summer* made = gcAlloc(sizeof(Summer));
  made->kind = Summer_kind;
  made->base = base;
  return made;
}

static bool Summer_equal(Summer* a, Summer* b) {
  if (a == b || a == NULL || b == NULL) {
    return a == b;
  }

  bool same = true;
  same = same && a->base == b->base;
  return same;
}

static uint64_t Summer_hash(Summer* a) {
  uint64_t hash = 17;
  hash = hash * 31 + (uint64_t) a->base;
  return hash;
}

static int64_t Summer_combine(Summer* self, void* other) {
  return self->base + Shape_area(other);
}

static Holder* Holder_new(void* shape, int64_t tag) {
  Holder* made = gcAlloc(sizeof(Holder));
  made->shape = shape;
  made->tag = tag;
  return made;
}

static bool Holder_equal(Holder* a, Holder* b) {
  if (a == b || a == NULL || b == NULL) {
    return a == b;
  }

  bool same = true;
  same = same && a->shape == b->shape;
  same = same && a->tag == b->tag;
  return same;
}

static uint64_t Holder_hash(Holder* a) {
  uint64_t hash = 17;
  hash = hash * 31 + hashBytes(&a->shape, sizeof a->shape);
  hash = hash * 31 + (uint64_t) a->tag;
  return hash;
}

static int64_t totalArea(void* s) {
  return Shape_area(s);
}

static int64_t directCall(void) {
  return totalArea(Square_new(3)) * 100 + totalArea(Rect_new(4, 5));
}

static void* pick(int64_t which) {
  if (which == 0) {
    return Square_new(6);
  }
  return Rect_new(2, 7);
}

static int64_t chosen(void) {
  void* a = pick(0);
  void* b = pick(1);
  return Shape_area(a) * 1000 + Shape_area(b) + Shape_scaled(b, 10);
}

static int64_t sumAll(void) {
  List* shapes = listNew(sizeof(void*));
  listPush(shapes, (void*[]){Square_new(2)});
  listPush(shapes, (void*[]){Rect_new(3, 4)});
  listPush(shapes, (void*[]){Square_new(5)});
  int64_t total = 0;
  for (int64_t i = 0; i < shapes->count; i = i + 1) {
    total = total + Shape_area(LIST_AT(shapes, i, void*));
  }
  return total;
}

static int64_t viaField(void) {
  Holder* h = Holder_new(Rect_new(5, 6), 7);
  return Shape_area(h->shape) * 10 + h->tag;
}

static int64_t twoInterfaces(void) {
  Square* sq = Square_new(4);
  void* n = sq;
  void* s = sq;
  return Named_id(n) * 1000 + Shape_area(s);
}

static int64_t interfaceArgument(void) {
  void* c = Summer_new(100);
  return Combiner_combine(c, Rect_new(3, 3));
}

static int64_t main_(void) {
  return directCall() + chosen() + sumAll() + viaField() + twoInterfaces() + interfaceArgument();
}

int main(void) {
  return (int) main_();
}
