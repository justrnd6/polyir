#import <setjmp.h>
#import <stdbool.h>
#import <stdint.h>
#import <stdio.h>
#import <stdlib.h>
#import <string.h>

#import "../Metal.h"
#import "shared/matrix.h"

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

static int32_t circleHit(int32_t i, int32_t w, float radius);
static float __attribute__((vector_size(4 * sizeof(float)))) rotate(float __attribute__((vector_size(4 * sizeof(float)))) p, float c, float s);
static int32_t mandel(int32_t i, int32_t w, int32_t limit);
static float dotTerm(float a, float b);
static int64_t scaleAll(float* x, float* out, int64_t n, float k, int64_t smallest);
static float scale(float x, float k);
static int64_t scaleOnGpu(float* x, float* out, int64_t n, float k);
static int64_t copyFloats(float* from, float* to, int64_t n);
static int64_t main_(void);
static int64_t checkCircle(int64_t device, const char* source);
static int64_t checkRotate(int64_t device, const char* source);
static int64_t checkMandel(int64_t device, const char* source);
static int64_t checkDot(int64_t device, const char* source);
static int64_t checkGlue(void);
static int64_t checkTransform(int64_t device, const char* source);

static int32_t circleHit(int32_t i, int32_t w, float radius) {
  float dx = (float) (i % w) - (float) w * 0.5f;
  float dy = (float) (i / w) - (float) w * 0.5f;
  if (dx * dx + dy * dy <= radius * radius) {
    return 1;
  }
  return 0;
}

static float __attribute__((vector_size(4 * sizeof(float)))) rotate(float __attribute__((vector_size(4 * sizeof(float)))) p, float c, float s) {
  return (float __attribute__((vector_size(4 * sizeof(float))))){c * p[0] - s * p[1], s * p[0] + c * p[1], 0.0f, 0.0f};
}

static int32_t mandel(int32_t i, int32_t w, int32_t limit) {
  float cr = (float) (i % w) * 3.0f / (float) w - 2.0f;
  float ci = (float) (i / w) * 2.0f / (float) w - 1.0f;
  float zr = 0.0f;
  float zi = 0.0f;
  int32_t steps = 0;
  for (int64_t k = 0; k < (int64_t) limit; k = k + 1) {
    if (zr * zr + zi * zi <= 4.0f) {
      float next = zr * zr - zi * zi + cr;
      zi = (float) (2.0 * (double) zr * (double) zi + (double) ci);
      zr = next;
      steps = steps + 1;
    }
  }
  return steps;
}

static float dotTerm(float a, float b) {
  return a * b;
}

static int64_t scaleAll(float* x, float* out, int64_t n, float k, int64_t smallest) {
  if (n >= smallest) {
    if (scaleOnGpu(x, out, n, k) == 1) {
      return 1;
    }
  }
  for (int64_t i = 0; i < n; i = i + 1) {
    out[i] = scale(x[i], k);
  }
  return 0;
}

static float scale(float x, float k) {
  return x * k;
}

static int64_t scaleOnGpu(float* x, float* out, int64_t n, float k) {
  int64_t device = metalDevice();
  if (device == 0) {
    return 0;
  }
  int64_t pipeline = metalPipeline(device, metalReadFile("applications.metal"), "scale");
  if (pipeline == 0) {
    return 0;
  }
  List* factor = listFilled(sizeof(float), 1, (float[]){0.0f});
  LIST_AT(factor, 0, float) = k;
  int64_t bytes = n * 4;
  int64_t result = metalEmptyBuffer(device, bytes);
  int64_t ok = metalDispatch(device, pipeline, n, metalBuffer(device, (int64_t) (intptr_t) x, bytes), metalBuffer(device, (int64_t) factor, 4), result, 0);
  if (ok == 0) {
    return 0;
  }
  copyFloats((int8_t*) (intptr_t) metalContents(result), out, n);
  return 1;
}

static int64_t copyFloats(float* from, float* to, int64_t n) {
  for (int64_t i = 0; i < n; i = i + 1) {
    to[i] = from[i];
  }
  return 0;
}

static int64_t main_(void) {
  int64_t device = metalDevice();
  if (device == 0) {
    return 10;
  }
  const char* source = metalReadFile("applications.metal");
  if (checkCircle(device, source) != 0) {
    return 21;
  }
  if (checkRotate(device, source) != 0) {
    return 22;
  }
  if (checkMandel(device, source) != 0) {
    return 23;
  }
  if (checkDot(device, source) != 0) {
    return 24;
  }
  if (checkTransform(device, source) != 0) {
    return 26;
  }
  if (checkGlue() != 0) {
    return 25;
  }
  return 0;
}

static int64_t checkCircle(int64_t device, const char* source) {
  int64_t pipeline = metalPipeline(device, source, "circleHit");
  if (pipeline == 0) {
    return 1;
  }
  List* w = listFilled(sizeof(int32_t), 1, (int32_t[]){0});
  LIST_AT(w, 0, int32_t) = 16;
  List* r = listFilled(sizeof(float), 1, (float[]){0.0f});
  LIST_AT(r, 0, float) = 6.0f;
  int64_t out = metalEmptyBuffer(device, 1024);
  metalDispatch(device, pipeline, 256, metalBuffer(device, (int64_t) w, 4), metalBuffer(device, (int64_t) r, 4), out, 0);
  int32_t* got = (int8_t*) (intptr_t) metalContents(out);
  int64_t inside = 0;
  for (int64_t i = 0; i < 256; i = i + 1) {
    if (got[i] != circleHit((int32_t) i, 16, 6.0f)) {
      return 1;
    }
    inside = inside + (int64_t) got[i];
  }
  if (inside < 100 || inside > 130) {
    return 1;
  }
  return 0;
}

static int64_t checkRotate(int64_t device, const char* source) {
  int64_t pipeline = metalPipeline(device, source, "rotate");
  if (pipeline == 0) {
    return 1;
  }
  List* pts = listFilled(sizeof(float), 8, (float[]){0.0f});
  LIST_AT(pts, 0, float) = 1.0f;
  LIST_AT(pts, 1, float) = 0.0f;
  LIST_AT(pts, 2, float) = 0.0f;
  LIST_AT(pts, 3, float) = 0.0f;
  LIST_AT(pts, 4, float) = 2.0f;
  LIST_AT(pts, 5, float) = 3.0f;
  LIST_AT(pts, 6, float) = 0.0f;
  LIST_AT(pts, 7, float) = 0.0f;
  List* c = listFilled(sizeof(float), 1, (float[]){0.0f});
  LIST_AT(c, 0, float) = 0.0f;
  List* s = listFilled(sizeof(float), 1, (float[]){0.0f});
  LIST_AT(s, 0, float) = 1.0f;
  int64_t out = metalEmptyBuffer(device, 32);
  metalDispatch(device, pipeline, 2, metalBuffer(device, (int64_t) pts, 32), metalBuffer(device, (int64_t) c, 4), metalBuffer(device, (int64_t) s, 4), out);
  float* got = (int8_t*) (intptr_t) metalContents(out);
  if (got[0] != 0.0f || got[1] != 1.0f) {
    return 1;
  }
  if (got[4] != (float) (0.0 - 3.0) || got[5] != 2.0f) {
    return 1;
  }
  return 0;
}

static int64_t checkMandel(int64_t device, const char* source) {
  int64_t pipeline = metalPipeline(device, source, "mandel");
  if (pipeline == 0) {
    return 1;
  }
  List* w = listFilled(sizeof(int32_t), 1, (int32_t[]){0});
  LIST_AT(w, 0, int32_t) = 24;
  List* limit = listFilled(sizeof(int32_t), 1, (int32_t[]){0});
  LIST_AT(limit, 0, int32_t) = 50;
  int64_t out = metalEmptyBuffer(device, 2304);
  metalDispatch(device, pipeline, 576, metalBuffer(device, (int64_t) w, 4), metalBuffer(device, (int64_t) limit, 4), out, 0);
  int32_t* got = (int8_t*) (intptr_t) metalContents(out);
  int64_t differ = 0;
  for (int64_t i = 0; i < 576; i = i + 1) {
    if (got[i] != mandel((int32_t) i, 24, 50)) {
      differ = differ + 1;
    }
  }
  if (differ > 5) {
    return 1;
  }
  return 0;
}

static int64_t checkDot(int64_t device, const char* source) {
  int64_t pipeline = metalPipeline(device, source, "dotTerm");
  if (pipeline == 0) {
    return 1;
  }
  List* a = listFilled(sizeof(float), 4, (float[]){0.0f});
  List* b = listFilled(sizeof(float), 4, (float[]){0.0f});
  for (int64_t i = 0; i < 4; i = i + 1) {
    LIST_AT(a, i, float) = (float) (i + 1);
    LIST_AT(b, i, float) = (float) (i + 2);
  }
  int64_t out = metalEmptyBuffer(device, 16);
  metalDispatch(device, pipeline, 4, metalBuffer(device, (int64_t) a, 16), metalBuffer(device, (int64_t) b, 16), out, 0);
  float* got = (int8_t*) (intptr_t) metalContents(out);
  float total = 0.0f;
  for (int64_t i = 0; i < 4; i = i + 1) {
    total = total + got[i];
  }
  if (total != 40.0f) {
    return 1;
  }
  return 0;
}

static int64_t checkGlue(void) {
  List* x = listFilled(sizeof(float), 8, (float[]){0.0f});
  for (int64_t i = 0; i < 8; i = i + 1) {
    LIST_AT(x, i, float) = (float) i;
  }
  List* big = listFilled(sizeof(float), 8, (float[]){0.0f});
  List* small = listFilled(sizeof(float), 8, (float[]){0.0f});
  float* xs = (int8_t*) (intptr_t) (int64_t) x;
  int64_t onGpu = scaleAll(xs, (int8_t*) (intptr_t) (int64_t) big, 8, 2.5f, 4);
  int64_t onCpu = scaleAll(xs, (int8_t*) (intptr_t) (int64_t) small, 8, 2.5f, 1000);
  if (onGpu != 1 || onCpu != 0) {
    return 1;
  }
  for (int64_t i = 0; i < 8; i = i + 1) {
    if (LIST_AT(big, i, float) != LIST_AT(small, i, float) || LIST_AT(big, i, float) != (float) i * 2.5f) {
      return 1;
    }
  }
  return 0;
}

static int64_t checkTransform(int64_t device, const char* source) {
  int64_t pipeline = metalPipeline(device, source, "transform");
  if (pipeline == 0) {
    return 1;
  }
  List* turn = listFilled(sizeof(float), 16, (float[]){0.0f});
  List* move = listFilled(sizeof(float), 16, (float[]){0.0f});
  identity((int8_t*) (intptr_t) (int64_t) turn);
  identity((int8_t*) (intptr_t) (int64_t) move);
  LIST_AT(turn, 0, float) = 0.0f;
  LIST_AT(turn, 1, float) = (float) (0.0 - 1.0);
  LIST_AT(turn, 4, float) = 1.0f;
  LIST_AT(turn, 5, float) = 0.0f;
  LIST_AT(move, 3, float) = 1.0f;
  LIST_AT(move, 7, float) = 2.0f;
  LIST_AT(move, 11, float) = 3.0f;
  List* turnThenMove = listFilled(sizeof(float), 16, (float[]){0.0f});
  multiply4((int8_t*) (intptr_t) (int64_t) move, (int8_t*) (intptr_t) (int64_t) turn, (int8_t*) (intptr_t) (int64_t) turnThenMove);
  List* pts = listFilled(sizeof(float), 12, (float[]){0.0f});
  LIST_AT(pts, 0, float) = 1.0f;
  LIST_AT(pts, 1, float) = 0.0f;
  LIST_AT(pts, 2, float) = 0.0f;
  LIST_AT(pts, 3, float) = 1.0f;
  LIST_AT(pts, 4, float) = 0.0f;
  LIST_AT(pts, 5, float) = 2.0f;
  LIST_AT(pts, 6, float) = 0.0f;
  LIST_AT(pts, 7, float) = 1.0f;
  LIST_AT(pts, 8, float) = 1.0f;
  LIST_AT(pts, 9, float) = 1.0f;
  LIST_AT(pts, 10, float) = 1.0f;
  LIST_AT(pts, 11, float) = 1.0f;
  int64_t out = metalEmptyBuffer(device, 48);
  metalDispatch(device, pipeline, 3, metalBuffer(device, (int64_t) pts, 48), metalBuffer(device, (int64_t) turnThenMove, 64), out, 0);
  float* got = (int8_t*) (intptr_t) metalContents(out);
  if (got[0] != 1.0f || got[1] != 3.0f || got[2] != 3.0f) {
    return 1;
  }
  if (got[4] != (float) (0.0 - 1.0) || got[5] != 2.0f || got[6] != 3.0f) {
    return 1;
  }
  if (got[8] != 0.0f || got[9] != 3.0f || got[10] != 4.0f) {
    return 1;
  }
  List* moveThenTurn = listFilled(sizeof(float), 16, (float[]){0.0f});
  multiply4((int8_t*) (intptr_t) (int64_t) turn, (int8_t*) (intptr_t) (int64_t) move, (int8_t*) (intptr_t) (int64_t) moveThenTurn);
  int64_t out2 = metalEmptyBuffer(device, 48);
  metalDispatch(device, pipeline, 3, metalBuffer(device, (int64_t) pts, 48), metalBuffer(device, (int64_t) moveThenTurn, 64), out2, 0);
  float* other = (int8_t*) (intptr_t) metalContents(out2);
  if (other[0] != (float) (0.0 - 2.0) || other[1] != 2.0f || other[2] != 3.0f) {
    return 1;
  }
  return 0;
}

int main(void) {
  return (int) main_();
}
