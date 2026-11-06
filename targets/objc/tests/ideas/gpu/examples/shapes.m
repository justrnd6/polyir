#import <setjmp.h>
#import <stdbool.h>
#import <stdint.h>
#import <stdio.h>
#import <stdlib.h>
#import <string.h>

#import "../Metal.h"
#import "shared/picture.h"
#import "shared/shapes.h"

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
static int64_t checkManyBuffers(int64_t device, const char* source);
static int64_t* makeInputs(int64_t count, int64_t n);
static int64_t blendMatches(int64_t* inputs, float* got, int64_t n);
static float blendAt(int64_t* inputs, int64_t i);
static int64_t checkGrid(int64_t device, const char* source);
static int64_t plasmaMatches(float* pic, int64_t width, int64_t height);
static int64_t checkVignette(int64_t device, const char* source, int64_t image, int64_t width, int64_t height);
static int64_t vignetteMatches(float* source, float* pic, int64_t width, int64_t height);
static int64_t checkAtomics(int64_t device, const char* source);
static int64_t compareTallies(int64_t* buffers, int64_t n);

static int64_t main_(void) {
  int64_t device = metalDevice();
  if (device == 0) {
    return 10;
  }
  const char* source = metalReadFile("shapes.metal");
  int64_t code = checkManyBuffers(device, source);
  if (code == 0) {
    code = checkGrid(device, source);
  }
  if (code == 0) {
    code = checkAtomics(device, source);
  }
  return code;
}

static int64_t checkManyBuffers(int64_t device, const char* source) {
  int64_t pipeline = metalPipeline(device, source, "blend7");
  if (pipeline == 0) {
    return 12;
  }
  int64_t n = 100000;
  int64_t* inputs = makeInputs(7, n);
  int64_t* buffers = (int8_t*) (intptr_t) calloc(9, 8);
  for (int64_t k = 0; k < 7; k = k + 1) {
    buffers[k] = metalBuffer(device, inputs[k], n * 4);
  }
  List* scale = listFilled(sizeof(float), 1, (float[]){0.0f});
  LIST_AT(scale, 0, float) = 1.5f;
  buffers[7] = metalBuffer(device, (int64_t) scale, 4);
  buffers[8] = metalEmptyBuffer(device, n * 4);
  if (metalDispatchBuffers(device, pipeline, n, buffers, 9) == 0) {
    return 12;
  }
  return blendMatches(inputs, (int8_t*) (intptr_t) metalContents(buffers[8]), n);
}

static int64_t* makeInputs(int64_t count, int64_t n) {
  int64_t* inputs = (int8_t*) (intptr_t) calloc(count, 8);
  for (int64_t k = 0; k < count; k = k + 1) {
    float* data = (int8_t*) (intptr_t) calloc(n, 4);
    for (int64_t i = 0; i < n; i = i + 1) {
      data[i] = (float) (i % (k + 3)) * 0.25f;
    }
    inputs[k] = (int64_t) (intptr_t) data;
  }
  return inputs;
}

static int64_t blendMatches(int64_t* inputs, float* got, int64_t n) {
  for (int64_t i = 0; i < n; i = i + 1) {
    if (nearly(got[i], blendAt(inputs, i), 0.0001f) == 0) {
      return 13;
    }
  }
  return 0;
}

static float blendAt(int64_t* inputs, int64_t i) {
  float* a = (int8_t*) (intptr_t) inputs[0];
  float* b = (int8_t*) (intptr_t) inputs[1];
  float* c = (int8_t*) (intptr_t) inputs[2];
  float* d = (int8_t*) (intptr_t) inputs[3];
  float* e = (int8_t*) (intptr_t) inputs[4];
  float* f = (int8_t*) (intptr_t) inputs[5];
  float* g = (int8_t*) (intptr_t) inputs[6];
  return blend7(a[i], b[i], c[i], d[i], e[i], f[i], g[i], 1.5f);
}

static int64_t checkGrid(int64_t device, const char* source) {
  int64_t width = 320;
  int64_t height = 200;
  int64_t pipeline = metalPipeline(device, source, "plasma");
  if (pipeline == 0) {
    return 12;
  }
  List* size = listFilled(sizeof(int32_t), 2, (int32_t[]){0});
  LIST_AT(size, 0, int32_t) = (int32_t) width;
  LIST_AT(size, 1, int32_t) = (int32_t) height;
  List* phase = listFilled(sizeof(float), 1, (float[]){0.0f});
  LIST_AT(phase, 0, float) = 0.7f;
  int64_t image = metalEmptyBuffer(device, width * height * 4);
  int64_t* args = (int8_t*) (intptr_t) calloc(4, 8);
  args[0] = metalBuffer(device, (int64_t) size, 4);
  args[1] = metalBuffer(device, (int64_t) size + 4, 4);
  args[2] = metalBuffer(device, (int64_t) phase, 4);
  args[3] = image;
  if (metalDispatch2D(device, pipeline, width, height, args, 4) == 0) {
    return 12;
  }
  if (plasmaMatches((int8_t*) (intptr_t) metalContents(image), width, height) == 0) {
    return 14;
  }
  savePicture("shapes-plasma.pgm", (int8_t*) (intptr_t) metalContents(image), width, height);
  return checkVignette(device, source, image, width, height);
}

static int64_t plasmaMatches(float* pic, int64_t width, int64_t height) {
  for (int64_t y = 0; y < height; y = y + 1) {
    for (int64_t x = 0; x < width; x = x + 1) {
      float want = plasma((int32_t) x, (int32_t) y, (int32_t) width, (int32_t) height, 0.7f);
      if (nearly(pic[y * width + x], want, 0.001f) == 0) {
        return 0;
      }
    }
  }
  return 1;
}

static int64_t checkVignette(int64_t device, const char* source, int64_t image, int64_t width, int64_t height) {
  int64_t pipeline = metalPipeline(device, source, "vignette");
  if (pipeline == 0) {
    return 12;
  }
  List* size = listFilled(sizeof(int32_t), 2, (int32_t[]){0});
  LIST_AT(size, 0, int32_t) = (int32_t) width;
  LIST_AT(size, 1, int32_t) = (int32_t) height;
  int64_t out = metalEmptyBuffer(device, width * height * 4);
  int64_t* args = (int8_t*) (intptr_t) calloc(4, 8);
  args[0] = image;
  args[1] = metalBuffer(device, (int64_t) size, 4);
  args[2] = metalBuffer(device, (int64_t) size + 4, 4);
  args[3] = out;
  if (metalDispatch2D(device, pipeline, width, height, args, 4) == 0) {
    return 12;
  }
  if (vignetteMatches((int8_t*) (intptr_t) metalContents(image), (int8_t*) (intptr_t) metalContents(out), width, height) == 0) {
    return 14;
  }
  savePicture("shapes-vignette.pgm", (int8_t*) (intptr_t) metalContents(out), width, height);
  return 0;
}

static int64_t vignetteMatches(float* source, float* pic, int64_t width, int64_t height) {
  for (int64_t y = 0; y < height; y = y + 1) {
    for (int64_t x = 0; x < width; x = x + 1) {
      float want = vignette(source[y * width + x], (int32_t) x, (int32_t) y, (int32_t) width, (int32_t) height);
      if (nearly(pic[y * width + x], want, 0.0005f) == 0) {
        return 0;
      }
    }
  }
  return 1;
}

static int64_t checkAtomics(int64_t device, const char* source) {
  int64_t pipeline = metalPipeline(device, source, "tally");
  if (pipeline == 0) {
    return 12;
  }
  int64_t n = 1000000;
  List* counts = listFilled(sizeof(int32_t), 4, (int32_t[]){0});
  List* sums = listFilled(sizeof(float), 4, (float[]){0.0f});
  List* lows = listFilled(sizeof(int32_t), 1, (int32_t[]){0});
  List* highs = listFilled(sizeof(int32_t), 1, (int32_t[]){0});
  for (int64_t b = 0; b < 4; b = b + 1) {
    LIST_AT(counts, b, int32_t) = 0;
    LIST_AT(sums, b, float) = 0.0f;
  }
  LIST_AT(lows, 0, int32_t) = 2147483647;
  LIST_AT(highs, 0, int32_t) = (int32_t) (0 - 1);
  int64_t* buffers = (int8_t*) (intptr_t) calloc(5, 8);
  buffers[0] = metalBuffer(device, (int64_t) counts, 16);
  buffers[1] = metalBuffer(device, (int64_t) sums, 16);
  buffers[2] = metalBuffer(device, (int64_t) lows, 4);
  buffers[3] = metalBuffer(device, (int64_t) highs, 4);
  buffers[4] = metalEmptyBuffer(device, n * 4);
  if (metalDispatchBuffers(device, pipeline, n, buffers, 5) == 0) {
    return 12;
  }
  return compareTallies(buffers, n);
}

static int64_t compareTallies(int64_t* buffers, int64_t n) {
  int32_t* counts = (int8_t*) (intptr_t) metalContents(buffers[0]);
  float* sums = (int8_t*) (intptr_t) metalContents(buffers[1]);
  int32_t* lows = (int8_t*) (intptr_t) metalContents(buffers[2]);
  int32_t* highs = (int8_t*) (intptr_t) metalContents(buffers[3]);
  int32_t low = 2147483647;
  int32_t high = (int32_t) (0 - 1);
  for (int64_t i = 0; i < n; i = i + 1) {
    int32_t value = reported((int32_t) i);
    if (value < low) {
      low = value;
    }
    if (value > high) {
      high = value;
    }
  }
  for (int64_t b = 0; b < 4; b = b + 1) {
    if ((int64_t) counts[b] != n / 4) {
      return 15;
    }
    if (sums[b] != (float) (n / 4) * 0.5f) {
      return 15;
    }
  }
  if (lows[0] != low || highs[0] != high) {
    return 15;
  }
  return 0;
}

int main(void) {
  return (int) main_();
}
