#import <math.h>
#import <setjmp.h>
#import <stdbool.h>
#import <stdint.h>
#import <stdio.h>
#import <stdlib.h>
#import <string.h>

#import "../Metal.h"
#import "shared/picture.h"
#import "shared/scatter.h"

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

#ifndef POLYIR_MEMORY_H
#define POLYIR_MEMORY_H

// The C library's functions polyir declares with numbers for their pointers or sizes (`extern func calloc(n: i64, size: i64)
// -> i64`), each called through a function of that shape. This file comes after the runtime, which calls the C library's own.
static int64_t polyCalloc(int64_t count, int64_t size) {
  return (int64_t) (intptr_t) calloc((size_t) count, (size_t) size);
}

static int64_t* polyMalloc(int64_t size) {
  return malloc((size_t) size);
}

static void polyFree(int64_t memory) {
  free((void*) (intptr_t) memory);
}

static int64_t polyFopen(const char* path, const char* mode) {
  return (int64_t) (intptr_t) fopen(path, mode);
}

static int64_t polyFclose(int64_t file) {
  return fclose((FILE*) (intptr_t) file);
}

static int64_t polyFread(int64_t data, int64_t size, int64_t count, int64_t file) {
  return (int64_t) fread((void*) (intptr_t) data, (size_t) size, (size_t) count, (FILE*) (intptr_t) file);
}

static int64_t polyFwrite(int64_t data, int64_t size, int64_t count, int64_t file) {
  return (int64_t) fwrite((const void*) (intptr_t) data, (size_t) size, (size_t) count, (FILE*) (intptr_t) file);
}

static int64_t polyFseek(int64_t file, int64_t offset, int64_t whence) {
  return fseek((FILE*) (intptr_t) file, (long) offset, (int) whence);
}

static int64_t polyFtell(int64_t file) {
  return ftell((FILE*) (intptr_t) file);
}

static int64_t polyGetenv(const char* name) {
  return (int64_t) (intptr_t) getenv(name);
}

static int64_t polyStrlen(const char* text) {
  return (int64_t) strlen(text);
}

static int64_t polyPutchar(int64_t c) {
  return putchar((int) c);
}

static int64_t polyPuts(const char* text) {
  return puts(text);
}

#define calloc polyCalloc
#define malloc polyMalloc
#define free polyFree
#define fopen polyFopen
#define fclose polyFclose
#define fread polyFread
#define fwrite polyFwrite
#define fseek polyFseek
#define ftell polyFtell
#define getenv polyGetenv
#define strlen polyStrlen
#define putchar polyPutchar
#define puts polyPuts
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
static int64_t runWriter(int64_t device, const char* source, const char* name, int64_t readOnly, int64_t readBytes, int32_t number, int64_t writeBytes, int64_t n);
static int64_t checkReverse(int64_t device, const char* source);
static int64_t checkHistogram(int64_t device, const char* source);
static int64_t checkRace(int64_t device, const char* source);
static int64_t checkSplat(int64_t device, const char* source);
static float* brightness(int32_t* counts, int64_t n, int32_t biggest);

static int64_t main_(void) {
  int64_t device = metalDevice();
  if (device == 0) {
    return 10;
  }
  const char* source = metalReadFile("scatter.metal");
  if (checkReverse(device, source) != 0) {
    return 13;
  }
  if (checkHistogram(device, source) != 0) {
    return 14;
  }
  if (checkSplat(device, source) != 0) {
    return 15;
  }
  if (checkRace(device, source) != 0) {
    return 16;
  }
  return 0;
}

static int64_t runWriter(int64_t device, const char* source, const char* name, int64_t readOnly, int64_t readBytes, int32_t number, int64_t writeBytes, int64_t n) {
  int64_t pipeline = metalPipeline(device, source, name);
  if (pipeline == 0) {
    return 0;
  }
  List* one = listFilled(sizeof(int32_t), 1, (int32_t[]){0});
  LIST_AT(one, 0, int32_t) = number;
  int64_t written = metalEmptyBuffer(device, writeBytes);
  int64_t result = metalEmptyBuffer(device, n * 4);
  if (metalDispatch(device, pipeline, n, metalBuffer(device, readOnly, readBytes), metalBuffer(device, (int64_t) one, 4), written, result) == 0) {
    return 0;
  }
  return metalContents(written);
}

static int64_t checkReverse(int64_t device, const char* source) {
  int64_t n = 1000;
  float* src = (int8_t*) (intptr_t) calloc(n, 4);
  for (int64_t i = 0; i < n; i = i + 1) {
    src[i] = (float) i * 0.5f;
  }
  float* gpu = (int8_t*) (intptr_t) runWriter(device, source, "reverseCopy", (int64_t) (intptr_t) src, n * 4, (int32_t) n, n * 4, n);
  for (int64_t i = 0; i < n; i = i + 1) {
    if (gpu[i] != src[n - 1 - i]) {
      return 1;
    }
  }
  return 0;
}

static int64_t checkHistogram(int64_t device, const char* source) {
  int64_t n = 1000000;
  int64_t bins = 16;
  int32_t* data = (int8_t*) (intptr_t) calloc(n, 4);
  for (int64_t i = 0; i < n; i = i + 1) {
    data[i] = (int32_t) ((i * 37 + 11) % 1000);
  }
  int32_t* cpu = (int8_t*) (intptr_t) calloc(bins, 4);
  for (int64_t i = 0; i < n; i = i + 1) {
    histogram((int32_t) i, data, (int32_t) bins, cpu);
  }
  int32_t* gpu = (int8_t*) (intptr_t) runWriter(device, source, "histogram", (int64_t) (intptr_t) data, n * 4, (int32_t) bins, bins * 4, n);
  for (int64_t b = 0; b < bins; b = b + 1) {
    if (gpu[b] != cpu[b]) {
      return 1;
    }
  }
  return 0;
}

static int64_t checkRace(int64_t device, const char* source) {
  int64_t n = 1000000;
  int32_t* data = (int8_t*) (intptr_t) calloc(n, 4);
  int32_t* racy = (int8_t*) (intptr_t) runWriter(device, source, "histogramRacy", (int64_t) (intptr_t) data, n * 4, 1, 4, n);
  int64_t total = (int64_t) racy[0];
  if (total > n) {
    return 1;
  }
  if (total == n) {
    return 1;
  }
  return 0;
}

static int64_t checkSplat(int64_t device, const char* source) {
  int64_t w = 256;
  int64_t n = 400000;
  float* p = (int8_t*) (intptr_t) calloc(2, 4);
  p[0] = (float) w;
  int64_t pipeline = metalPipeline(device, source, "splat");
  if (pipeline == 0) {
    return 1;
  }
  int64_t cells = metalEmptyBuffer(device, w * w * 4);
  int64_t result = metalEmptyBuffer(device, n * 4);
  if (metalDispatch(device, pipeline, n, metalBuffer(device, (int64_t) (intptr_t) p, 8), cells, result, 0) == 0) {
    return 1;
  }
  int32_t* gpu = (int8_t*) (intptr_t) metalContents(cells);
  int32_t* cpu = (int8_t*) (intptr_t) calloc(w * w, 4);
  for (int64_t i = 0; i < n; i = i + 1) {
    splat((int32_t) i, p, cpu);
  }
  int32_t biggest = 1;
  int64_t differ = 0;
  for (int64_t c = 0; c < w * w; c = c + 1) {
    if (gpu[c] != cpu[c]) {
      differ = differ + 1;
    }
    if (gpu[c] > biggest) {
      biggest = gpu[c];
    }
  }
  if (differ > 200) {
    return 1;
  }
  savePicture("splat.pgm", brightness(gpu, w * w, biggest), w, w);
  return 0;
}

static float* brightness(int32_t* counts, int64_t n, int32_t biggest) {
  float* out = (int8_t*) (intptr_t) calloc(n, 4);
  float scale = (float) (1.0 / (double) logf((float) (1.0 + (double) (float) biggest)));
  for (int64_t c = 0; c < n; c = c + 1) {
    out[c] = logf((float) (1.0 + (double) (float) counts[c])) * scale;
  }
  return out;
}

int main(void) {
  return (int) main_();
}
