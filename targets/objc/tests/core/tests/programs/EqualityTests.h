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

#ifndef POLYIR_VARIANT_H
#define POLYIR_VARIANT_H

// A variant: the number of its case and the value it holds, kept on the heap with its size.
typedef struct Variant {
  int64_t tag;
  void* value;
  size_t size;
} Variant;

static Variant makeVariant(int64_t tag, const void* value, size_t size) {
  return (Variant){tag, size > 0 ? copyBytes(value, size) : NULL, size};
}

// Two variants are the same when they are of one case and hold the same bytes.
static bool variantSame(Variant a, Variant b) {
  return a.tag == b.tag && a.size == b.size && (a.size == 0 || memcmp(a.value, b.value, a.size) == 0);
}
#endif

typedef struct Point Point;
typedef struct Line Line;
typedef struct Named Named;
typedef struct Counter Counter;
typedef struct Digit Digit;
typedef struct Wrapped Wrapped;

struct Point {
  int64_t x;
  int64_t y;
};

struct Line {
  Point* a;
  Point* b;
};

struct Named {
  const char* name;
  List* tags;
};

struct Counter {
  int64_t n;
  int64_t id_;
};

struct Digit {
  int64_t v;
};

struct Wrapped {
  Either inner;
};

static Point* Point_new(int64_t x, int64_t y);
static bool Point_equal(Point* a, Point* b);
static uint64_t Point_hash(Point* a);

static Line* Line_new(Point* a, Point* b);
static bool Line_equal(Line* a, Line* b);
static uint64_t Line_hash(Line* a);

static Named* Named_new(const char* name, List* tags);
static bool Named_equal(Named* a, Named* b);
static uint64_t Named_hash(Named* a);

static Counter* Counter_new(int64_t n, int64_t id_);
static bool Counter_equal(Counter* a, Counter* b);
static uint64_t Counter_hash(Counter* a);

static Digit* Digit_new(int64_t v);
static bool Digit_equal(Digit* a, Digit* b);
static uint64_t Digit_hash(Digit* a);
static bool Digit_equals(Digit* self, Digit* other);

static Wrapped* Wrapped_new(Either inner);
static bool Wrapped_equal(Wrapped* a, Wrapped* b);
static uint64_t Wrapped_hash(Wrapped* a);

static double checkStructsAreEqualFieldByField(void);
static double checkNestedStructsAreEqualRecursively(void);
static double checkAStringAndAListInsideAStructAreComparedByContent(void);
static double checkStringsCompareByCharacters(void);
static double checkAStructWithVarFieldsIsComparedByWhatItHoldsNow(void);
static double checkIdentityIsTheSameObject(void);
static double checkAnImmutableStructComparedByIdentityIsAnObject(void);
static double checkAValStructIsADifferentObjectThatIsEqual(void);
static double checkAnEqualsMethodDecidesWhatEqualMeans(void);
static double checkListsAreEqualElementByElement(void);
static double checkListsOfStructsAreEqualDeeply(void);
static double checkEithersAreEqualWhenTheSameCaseWithAnEqualPayload(void);
static double checkVariantsAreEqualWhenTheSameCaseWithAnEqualPayload(void);
static double checkAStructHoldingAnEitherIsComparedThroughIt(void);
static double checkNanIsNotEqualToItself(void);

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

static Line* Line_new(Point* a, Point* b) {
  Line* made = gcAlloc(sizeof(Line));
  made->a = a;
  made->b = b;
  return made;
}

static bool Line_equal(Line* a, Line* b) {
  if (a == b || a == NULL || b == NULL) {
    return a == b;
  }

  bool same = true;
  same = same && Point_equal(a->a, b->a);
  same = same && Point_equal(a->b, b->b);
  return same;
}

static uint64_t Line_hash(Line* a) {
  uint64_t hash = 17;
  hash = hash * 31 + Point_hash(a->a);
  hash = hash * 31 + Point_hash(a->b);
  return hash;
}

static Named* Named_new(const char* name, List* tags) {
  Named* made = gcAlloc(sizeof(Named));
  made->name = name;
  made->tags = tags;
  return made;
}

static bool Named_equal(Named* a, Named* b) {
  if (a == b || a == NULL || b == NULL) {
    return a == b;
  }

  bool same = true;
  same = same && textSame(a->name, b->name);
  same = same && listSame(a->tags, b->tags, NULL);
  return same;
}

static uint64_t Named_hash(Named* a) {
  uint64_t hash = 17;
  hash = hash * 31 + (uint64_t) textHash(a->name);
  hash = hash * 31 + hashBytes(&a->tags, sizeof a->tags);
  return hash;
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

static Digit* Digit_new(int64_t v) {
  Digit* made = gcAlloc(sizeof(Digit));
  made->v = v;
  return made;
}

static bool Digit_equal(Digit* a, Digit* b) {
  if (a == b || a == NULL || b == NULL) {
    return a == b;
  }

  bool same = true;
  same = same && a->v == b->v;
  return same;
}

static uint64_t Digit_hash(Digit* a) {
  uint64_t hash = 17;
  hash = hash * 31 + (uint64_t) a->v;
  return hash;
}

static bool Digit_equals(Digit* self, Digit* other) {
  return self->v % 10 == other->v % 10;
}

static Wrapped* Wrapped_new(Either inner) {
  Wrapped* made = gcAlloc(sizeof(Wrapped));
  made->inner = inner;
  return made;
}

static bool Wrapped_equal(Wrapped* a, Wrapped* b) {
  if (a == b || a == NULL || b == NULL) {
    return a == b;
  }

  bool same = true;
  same = same && memcmp(&a->inner, &b->inner, sizeof a->inner) == 0;
  return same;
}

static uint64_t Wrapped_hash(Wrapped* a) {
  uint64_t hash = 17;
  hash = hash * 31 + hashBytes(&a->inner, sizeof a->inner);
  return hash;
}

static double checkStructsAreEqualFieldByField(void) {
  Point* p = Point_new(1, 2);
  Point* q = Point_new(1, 2);
  Point* r = Point_new(1, 3);
  int64_t score = 0;
  if (Point_equal(p, q)) {
    score = score + 1;
  }
  if (!Point_equal(p, r)) {
    score = score + 2;
  }
  if (Point_equal(p, r)) {
    score = score + 100;
  }
  return offInt(score, 3);
}

static double checkNestedStructsAreEqualRecursively(void) {
  Point* p = Point_new(1, 2);
  Point* z = Point_new(3, 4);
  Line* a = Line_new(p, z);
  Line* b = Line_new(Point_new(1, 2), Point_new(3, 4));
  Line* c = Line_new(z, p);
  int64_t score = 0;
  if (Line_equal(a, b)) {
    score = score + 1;
  }
  if (!Line_equal(a, c)) {
    score = score + 2;
  }
  return offInt(score, 3);
}

static double checkAStringAndAListInsideAStructAreComparedByContent(void) {
  List* t1 = listNew(sizeof(int64_t));
  listPush(t1, (int64_t[]){1});
  List* t2 = listNew(sizeof(int64_t));
  listPush(t2, (int64_t[]){1});
  Named* a = Named_new(textJoin("a", "b"), t1);
  Named* b = Named_new("ab", t2);
  int64_t score = 0;
  if (Named_equal(a, b)) {
    score = score + 1;
  }
  listPush(t2, (int64_t[]){2});
  if (!Named_equal(a, b)) {
    score = score + 2;
  }
  return offInt(score, 3);
}

static double checkStringsCompareByCharacters(void) {
  const char* x = "match";
  const char* y = textJoin("ma", "tch");
  int64_t score = 0;
  if (textSame(x, y)) {
    score = score + 1;
  }
  if (!textSame(x, "other")) {
    score = score + 2;
  }
  if (x == y) {
    score = score + 100;
  }
  return offInt(score, 3);
}

static double checkAStructWithVarFieldsIsComparedByWhatItHoldsNow(void) {
  Counter* a = Counter_new(1, 2);
  Counter* b = Counter_new(1, 2);
  int64_t score = 0;
  if (Counter_equal(a, b)) {
    score = score + 1;
  }
  b->n = 5;
  if (!Counter_equal(a, b)) {
    score = score + 2;
  }
  return offInt(score, 3);
}

static double checkIdentityIsTheSameObject(void) {
  Counter* a = Counter_new(1, 2);
  Counter* b = Counter_new(1, 2);
  Counter* alias = a;
  int64_t score = 0;
  if (a == alias) {
    score = score + 1;
  }
  if (a != b) {
    score = score + 2;
  }
  if (a == b) {
    score = score + 100;
  }
  return offInt(score, 3);
}

static double checkAnImmutableStructComparedByIdentityIsAnObject(void) {
  Point* p = Point_new(1, 2);
  Point* q = Point_new(1, 2);
  Point* same_ = p;
  int64_t score = 0;
  if (p == same_) {
    score = score + 1;
  }
  if (p != q) {
    score = score + 2;
  }
  if (Point_equal(p, q)) {
    score = score + 4;
  }
  return offInt(score, 7);
}

static double checkAValStructIsADifferentObjectThatIsEqual(void) {
  Counter* a = Counter_new(1, 2);
  Counter* b = copyBytes(a, sizeof *a);
  int64_t score = 0;
  if (Counter_equal(a, b)) {
    score = score + 1;
  }
  if (a != b) {
    score = score + 2;
  }
  return offInt(score, 3);
}

static double checkAnEqualsMethodDecidesWhatEqualMeans(void) {
  Digit* a = Digit_new(13);
  Digit* b = Digit_new(23);
  Digit* c = Digit_new(24);
  int64_t score = 0;
  if (Digit_equal(a, b)) {
    score = score + 1;
  }
  if (!Digit_equal(a, c)) {
    score = score + 2;
  }
  return offInt(score, 3);
}

static double checkListsAreEqualElementByElement(void) {
  List* a = listNew(sizeof(int64_t));
  List* b = listNew(sizeof(int64_t));
  listPush(a, (int64_t[]){1});
  listPush(a, (int64_t[]){2});
  listPush(b, (int64_t[]){1});
  listPush(b, (int64_t[]){2});
  int64_t score = 0;
  if (listSame(a, b, NULL)) {
    score = score + 1;
  }
  listPush(b, (int64_t[]){3});
  if (!listSame(a, b, NULL)) {
    score = score + 2;
  }
  if (a == b) {
    score = score + 100;
  }
  return offInt(score, 3);
}

static double checkListsOfStructsAreEqualDeeply(void) {
  List* a = listNew(sizeof(Point*));
  List* b = listNew(sizeof(Point*));
  listPush(a, (Point*[]){Point_new(1, 2)});
  listPush(b, (Point*[]){Point_new(1, 2)});
  int64_t score = 0;
  if (listSame(a, b, Point_sameKey)) {
    score = score + 1;
  }
  listPush(a, (Point*[]){Point_new(5, 6)});
  listPush(b, (Point*[]){Point_new(5, 7)});
  if (!listSame(a, b, Point_sameKey)) {
    score = score + 2;
  }
  return offInt(score, 3);
}

static double checkEithersAreEqualWhenTheSameCaseWithAnEqualPayload(void) {
  Either a = makeEither(true, (int64_t[]){5}, sizeof(int64_t));
  Either b = makeEither(true, (int64_t[]){5}, sizeof(int64_t));
  Either c = makeEither(false, (int64_t[]){5}, sizeof(int64_t));
  Either d = makeEither(true, (int64_t[]){6}, sizeof(int64_t));
  int64_t score = 0;
  if (eitherSame(a, b)) {
    score = score + 1;
  }
  if (!eitherSame(a, c)) {
    score = score + 2;
  }
  if (!eitherSame(a, d)) {
    score = score + 4;
  }
  return offInt(score, 7);
}

static double checkVariantsAreEqualWhenTheSameCaseWithAnEqualPayload(void) {
  Variant a = makeVariant(1, (double[]){2.5}, sizeof(double));
  Variant b = makeVariant(1, (double[]){2.5}, sizeof(double));
  Variant c = makeVariant(0, (int64_t[]){2}, sizeof(int64_t));
  int64_t score = 0;
  if (variantSame(a, b)) {
    score = score + 1;
  }
  if (!variantSame(a, c)) {
    score = score + 2;
  }
  return offInt(score, 3);
}

static double checkAStructHoldingAnEitherIsComparedThroughIt(void) {
  Wrapped* a = Wrapped_new(makeEither(true, (int64_t[]){1}, sizeof(int64_t)));
  Wrapped* b = Wrapped_new(makeEither(true, (int64_t[]){1}, sizeof(int64_t)));
  Wrapped* c = Wrapped_new(makeEither(false, (const char*[]){"no"}, sizeof(const char*)));
  int64_t score = 0;
  if (Wrapped_equal(a, b)) {
    score = score + 1;
  }
  if (!Wrapped_equal(a, c)) {
    score = score + 2;
  }
  return offInt(score, 3);
}

static double checkNanIsNotEqualToItself(void) {
  double nan = 0.0 / 0.0;
  int64_t score = 0;
  if (nan == nan) {
    score = score + 100;
  }
  if (nan != nan) {
    score = score + 1;
  }
  return offInt(score, 1);
}
