#include <setjmp.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../Vulkan.h"
#include "shared/kernels.h"

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

static int64_t main_(void);
static int64_t pushCount(int64_t count);
static int64_t runScale(int64_t gpu);
static int64_t runNorm(int64_t gpu);
static int64_t runSumTo(int64_t gpu);
static int64_t runLane(int64_t gpu);
static int64_t runSmooth(int64_t gpu);
static int64_t runLeftover(int64_t gpu);

static int64_t main_(void) {
  int64_t gpu = vulkanStart();
  if (gpu == 0) {
    return 10;
  }
  if (runScale(gpu) != 0) {
    return 21;
  }
  if (runNorm(gpu) != 0) {
    return 22;
  }
  if (runSumTo(gpu) != 0) {
    return 23;
  }
  if (runLane(gpu) != 0) {
    return 24;
  }
  if (runSmooth(gpu) != 0) {
    return 25;
  }
  if (runLeftover(gpu) != 0) {
    return 26;
  }
  return 0;
}

static int64_t pushCount(int64_t count) {
  int64_t push = words(2);
  int32_t* slots = (int8_t*) (intptr_t) push;
  slots[0] = (int32_t) count;
  return push;
}

static int64_t runScale(int64_t gpu) {
  int64_t pipeline = vulkanPipeline(gpu, "scale.spv", 2, 8);
  if (pipeline == 0) {
    return 1;
  }
  List* xs = listFilled(sizeof(float), 4, (float[]){0.0f});
  LIST_AT(xs, 0, float) = 1.0f;
  LIST_AT(xs, 1, float) = 2.5f;
  LIST_AT(xs, 2, float) = 0.0f;
  LIST_AT(xs, 3, float) = 8.0f;
  int64_t push = pushCount(4);
  float* uniforms = (int8_t*) (intptr_t) push;
  uniforms[1] = 3.0f;
  int64_t out = vulkanEmptyBuffer(gpu, 16);
  vulkanDispatch(gpu, pipeline, 4, push, vulkanBuffer(gpu, (int64_t) xs, 16), out, 0, 0);
  float* got = (int8_t*) (intptr_t) vulkanContents(out);
  for (int64_t i = 0; i < 4; i = i + 1) {
    if (close(got[i], scale(LIST_AT(xs, i, float), 3.0f)) == 0) {
      return 1;
    }
  }
  return 0;
}

static int64_t runNorm(int64_t gpu) {
  int64_t pipeline = vulkanPipeline(gpu, "norm.spv", 3, 4);
  if (pipeline == 0) {
    return 1;
  }
  List* a = listFilled(sizeof(float), 3, (float[]){0.0f});
  LIST_AT(a, 0, float) = 1.0f;
  LIST_AT(a, 1, float) = 4.0f;
  LIST_AT(a, 2, float) = 0.5f;
  List* b = listFilled(sizeof(float), 3, (float[]){0.0f});
  LIST_AT(b, 0, float) = 2.0f;
  LIST_AT(b, 1, float) = 0.0f;
  LIST_AT(b, 2, float) = 7.0f;
  int64_t out = vulkanEmptyBuffer(gpu, 12);
  vulkanDispatch(gpu, pipeline, 3, pushCount(3), vulkanBuffer(gpu, (int64_t) a, 12), vulkanBuffer(gpu, (int64_t) b, 12), out, 0);
  float* got = (int8_t*) (intptr_t) vulkanContents(out);
  for (int64_t i = 0; i < 3; i = i + 1) {
    if (close(got[i], norm(LIST_AT(a, i, float), LIST_AT(b, i, float))) == 0) {
      return 1;
    }
  }
  return 0;
}

static int64_t runSumTo(int64_t gpu) {
  int64_t pipeline = vulkanPipeline(gpu, "sumTo.spv", 2, 4);
  if (pipeline == 0) {
    return 1;
  }
  List* n = listFilled(sizeof(int32_t), 4, (int32_t[]){0});
  LIST_AT(n, 0, int32_t) = 0;
  LIST_AT(n, 1, int32_t) = 5;
  LIST_AT(n, 2, int32_t) = 10;
  LIST_AT(n, 3, int32_t) = 101;
  int64_t out = vulkanEmptyBuffer(gpu, 16);
  vulkanDispatch(gpu, pipeline, 4, pushCount(4), vulkanBuffer(gpu, (int64_t) n, 16), out, 0, 0);
  int32_t* got = (int8_t*) (intptr_t) vulkanContents(out);
  for (int64_t i = 0; i < 4; i = i + 1) {
    if (got[i] != sumTo(LIST_AT(n, i, int32_t))) {
      return 1;
    }
  }
  return 0;
}

static int64_t runLane(int64_t gpu) {
  int64_t pipeline = vulkanPipeline(gpu, "lane.spv", 2, 4);
  if (pipeline == 0) {
    return 1;
  }
  List* v = listFilled(sizeof(float), 8, (float[]){0.0f});
  for (int64_t i = 0; i < 8; i = i + 1) {
    LIST_AT(v, i, float) = (float) (i * 3 + 1);
  }
  int64_t out = vulkanEmptyBuffer(gpu, 8);
  vulkanDispatch(gpu, pipeline, 2, pushCount(2), vulkanBuffer(gpu, (int64_t) v, 32), out, 0, 0);
  float* got = (int8_t*) (intptr_t) vulkanContents(out);
  if (close(got[0], lane((float __attribute__((vector_size(4 * sizeof(float))))){LIST_AT(v, 0, float), LIST_AT(v, 1, float), LIST_AT(v, 2, float), LIST_AT(v, 3, float)})) == 0) {
    return 1;
  }
  if (close(got[1], lane((float __attribute__((vector_size(4 * sizeof(float))))){LIST_AT(v, 4, float), LIST_AT(v, 5, float), LIST_AT(v, 6, float), LIST_AT(v, 7, float)})) == 0) {
    return 1;
  }
  return 0;
}

static int64_t runSmooth(int64_t gpu) {
  int64_t pipeline = vulkanPipeline(gpu, "smooth.spv", 2, 8);
  if (pipeline == 0) {
    return 1;
  }
  List* xs = listFilled(sizeof(float), 6, (float[]){0.0f});
  LIST_AT(xs, 0, float) = 3.0f;
  LIST_AT(xs, 1, float) = 9.0f;
  LIST_AT(xs, 2, float) = 0.0f;
  LIST_AT(xs, 3, float) = 6.0f;
  LIST_AT(xs, 4, float) = 12.0f;
  LIST_AT(xs, 5, float) = 3.0f;
  int64_t push = pushCount(6);
  int32_t* sizes = (int8_t*) (intptr_t) push;
  sizes[1] = 6;
  int64_t out = vulkanEmptyBuffer(gpu, 24);
  vulkanDispatch(gpu, pipeline, 6, push, vulkanBuffer(gpu, (int64_t) xs, 24), out, 0, 0);
  float* got = (int8_t*) (intptr_t) vulkanContents(out);
  float* cpu = (int8_t*) (intptr_t) (int64_t) xs;
  for (int64_t i = 0; i < 6; i = i + 1) {
    if (close(got[i], smooth((int32_t) i, cpu, 6)) == 0) {
      return 1;
    }
  }
  return 0;
}

static int64_t runLeftover(int64_t gpu) {
  int64_t pipeline = vulkanPipeline(gpu, "leftover.spv", 3, 4);
  if (pipeline == 0) {
    return 1;
  }
  List* a = listFilled(sizeof(float), 7, (float[]){0.0f});
  LIST_AT(a, 0, float) = 2867.0f;
  LIST_AT(a, 1, float) = 5969.0f;
  LIST_AT(a, 2, float) = 9409.0f;
  LIST_AT(a, 3, float) = 7.5f;
  LIST_AT(a, 4, float) = (float) (0.0 - 7.5);
  LIST_AT(a, 5, float) = 100.0f;
  LIST_AT(a, 6, float) = 0.0f;
  List* b = listFilled(sizeof(float), 7, (float[]){0.0f});
  LIST_AT(b, 0, float) = 47.0f;
  LIST_AT(b, 1, float) = 47.0f;
  LIST_AT(b, 2, float) = 97.0f;
  LIST_AT(b, 3, float) = 2.0f;
  LIST_AT(b, 4, float) = 2.0f;
  LIST_AT(b, 5, float) = 7.0f;
  LIST_AT(b, 6, float) = 5.0f;
  int64_t out = vulkanEmptyBuffer(gpu, 28);
  vulkanDispatch(gpu, pipeline, 7, pushCount(7), vulkanBuffer(gpu, (int64_t) a, 28), vulkanBuffer(gpu, (int64_t) b, 28), out, 0);
  float* got = (int8_t*) (intptr_t) vulkanContents(out);
  for (int64_t i = 0; i < 7; i = i + 1) {
    if (close(got[i], leftover(LIST_AT(a, i, float), LIST_AT(b, i, float))) == 0) {
      return 1;
    }
  }
  return 0;
}

int main(void) {
  return (int) main_();
}
