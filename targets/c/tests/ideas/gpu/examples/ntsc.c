#include <math.h>
#include <setjmp.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../Metal.h"
#include "shared/picture.h"

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

static float phase(int32_t x, int32_t y);
static float encode(int32_t i, float* src, int32_t w);
static int32_t nearColumn(int32_t x, int32_t k, int32_t w);
static float __attribute__((vector_size(4 * sizeof(float)))) decode(int32_t i, float* comp, int32_t w, float* opts);
static float* testPicture(int64_t w, int64_t h);
static int64_t near(float a, float b, float tolerance);
static int64_t main_(void);
static float* runEncode(int64_t device, int64_t pipeline, float* img, int64_t w, int64_t h);
static int64_t checkEncode(float* composite, float* img, int64_t w, int64_t h);
static float* runDecode(int64_t device, int64_t pipeline, float* composite, int64_t w, int64_t h, float tint, float color);
static int64_t decodeAll(int64_t device, int64_t pipeline, float* img, float* composite, int64_t w, int64_t h);
static int64_t barsSurvive(float* decoded, float* img, int64_t w);

static float phase(int32_t x, int32_t y) {
  return (float) (1.5707963 * (double) (float) x + 3.1415927 * (double) (float) (y % 2));
}

static float encode(int32_t i, float* src, int32_t w) {
  float r = src[i * 4];
  float g = src[i * 4 + 1];
  float b = src[i * 4 + 2];
  float y = (float) (0.299 * (double) r + 0.587 * (double) g + 0.114 * (double) b);
  float ci = (float) (0.596 * (double) r - 0.274 * (double) g - 0.322 * (double) b);
  float cq = (float) (0.211 * (double) r - 0.523 * (double) g + 0.312 * (double) b);
  float p = phase(i % w, i / w);
  return y + ci * cosf(p) + cq * sinf(p);
}

static int32_t nearColumn(int32_t x, int32_t k, int32_t w) {
  int32_t c = x + k - 2;
  if (c < 0) {
    c = 0;
  }
  if (c > w - 1) {
    c = w - 1;
  }
  return c;
}

static float __attribute__((vector_size(4 * sizeof(float)))) decode(int32_t i, float* comp, int32_t w, float* opts) {
  int32_t x = i % w;
  int32_t y = i / w;
  float luma = 0.0f;
  float ci = 0.0f;
  float cq = 0.0f;
  for (int64_t k = 0; k < 4; k = k + 1) {
    int32_t c = nearColumn(x, (int32_t) k, w);
    float sample = comp[y * w + c];
    float p = phase(c, y);
    luma = luma + sample * 0.25f;
    ci = ci + sample * cosf(p) * 0.5f;
    cq = cq + sample * sinf(p) * 0.5f;
  }
  float turn = opts[0];
  float ti = (ci * cosf(turn) - cq * sinf(turn)) * opts[1];
  float tq = (ci * sinf(turn) + cq * cosf(turn)) * opts[1];
  float r = luma + (float) (0.956 * (double) ti) + (float) (0.621 * (double) tq);
  float g = luma - (float) (0.272 * (double) ti) - (float) (0.647 * (double) tq);
  float b = luma - (float) (1.106 * (double) ti) + (float) (1.703 * (double) tq);
  return (float __attribute__((vector_size(4 * sizeof(float))))){r, g, b, 1.0f};
}

static float* testPicture(int64_t w, int64_t h) {
  float* img = (int8_t*) (intptr_t) calloc(w * h * 4, 4);
  for (int64_t y = 0; y < h; y = y + 1) {
    for (int64_t x = 0; x < w; x = x + 1) {
      int64_t bar = x * 8 / w;
      float r = 0.0f;
      float g = 0.0f;
      float b = 0.0f;
      if (bar == 0 || bar == 1 || bar == 4 || bar == 5) {
        r = 0.9f;
      }
      if (bar == 0 || bar == 1 || bar == 2 || bar == 3) {
        g = 0.9f;
      }
      if (bar == 0 || bar == 2 || bar == 4 || bar == 6) {
        b = 0.9f;
      }
      if (y >= h * 2 / 3) {
        r = 0.0f;
        g = 0.0f;
        b = 0.0f;
        if (x % 3 == 0 && x > w / 4 && x < w * 3 / 4) {
          r = 0.9f;
          g = 0.9f;
          b = 0.9f;
        }
      }
      int64_t at = (y * w + x) * 4;
      img[at] = r;
      img[at + 1] = g;
      img[at + 2] = b;
      img[at + 3] = 1.0f;
    }
  }
  return img;
}

static int64_t near(float a, float b, float tolerance) {
  float gap = a - b;
  if (gap < 0.0f) {
    gap = (float) (0.0 - (double) gap);
  }
  if (gap <= tolerance) {
    return 1;
  }
  return 0;
}

static int64_t main_(void) {
  int64_t device = metalDevice();
  if (device == 0) {
    return 10;
  }
  const char* source = metalReadFile("ntsc.metal");
  int64_t encoder = metalPipeline(device, source, "encode");
  int64_t decoder = metalPipeline(device, source, "decode");
  if (encoder == 0 || decoder == 0) {
    return 11;
  }
  int64_t w = 160;
  int64_t h = 120;
  float* img = testPicture(w, h);
  float* composite = runEncode(device, encoder, img, w, h);
  if ((int64_t) (intptr_t) composite == 0) {
    return 12;
  }
  int64_t code = checkEncode(composite, img, w, h);
  if (code != 0) {
    return code;
  }
  return decodeAll(device, decoder, img, composite, w, h);
}

static float* runEncode(int64_t device, int64_t pipeline, float* img, int64_t w, int64_t h) {
  List* width = listFilled(sizeof(int32_t), 1, (int32_t[]){0});
  LIST_AT(width, 0, int32_t) = (int32_t) w;
  int64_t out = metalEmptyBuffer(device, w * h * 4);
  if (metalDispatch(device, pipeline, w * h, metalBuffer(device, (int64_t) (intptr_t) img, w * h * 16), metalBuffer(device, (int64_t) width, 4), out, 0) == 0) {
    return (int8_t*) (intptr_t) 0;
  }
  return (int8_t*) (intptr_t) metalContents(out);
}

static int64_t checkEncode(float* composite, float* img, int64_t w, int64_t h) {
  for (int64_t i = 0; i < w * h; i = i + 1) {
    if (near(composite[i], encode((int32_t) i, img, (int32_t) w), 0.001f) == 0) {
      return 13;
    }
  }
  return 0;
}

static float* runDecode(int64_t device, int64_t pipeline, float* composite, int64_t w, int64_t h, float tint, float color) {
  List* width = listFilled(sizeof(int32_t), 1, (int32_t[]){0});
  LIST_AT(width, 0, int32_t) = (int32_t) w;
  List* opts = listFilled(sizeof(float), 2, (float[]){0.0f});
  LIST_AT(opts, 0, float) = tint;
  LIST_AT(opts, 1, float) = color;
  int64_t* buffers = (int8_t*) (intptr_t) calloc(4, 8);
  buffers[0] = metalBuffer(device, (int64_t) (intptr_t) composite, w * h * 4);
  buffers[1] = metalBuffer(device, (int64_t) width, 4);
  buffers[2] = metalBuffer(device, (int64_t) opts, 8);
  buffers[3] = metalEmptyBuffer(device, w * h * 16);
  if (metalDispatchBuffers(device, pipeline, w * h, buffers, 4) == 0) {
    return (int8_t*) (intptr_t) 0;
  }
  return (int8_t*) (intptr_t) metalContents(buffers[3]);
}

static int64_t decodeAll(int64_t device, int64_t pipeline, float* img, float* composite, int64_t w, int64_t h) {
  float* plain = runDecode(device, pipeline, composite, w, h, 0.0f, 1.0f);
  float* turned = runDecode(device, pipeline, composite, w, h, 0.6981317f, 1.0f);
  float* grey = runDecode(device, pipeline, composite, w, h, 0.0f, 0.0f);
  if ((int64_t) (intptr_t) plain == 0 || (int64_t) (intptr_t) turned == 0 || (int64_t) (intptr_t) grey == 0) {
    return 12;
  }
  List* opts = listFilled(sizeof(float), 2, (float[]){0.0f});
  LIST_AT(opts, 0, float) = 0.0f;
  LIST_AT(opts, 1, float) = 1.0f;
  for (int64_t i = 0; i < w * h; i = i + 3) {
    float __attribute__((vector_size(4 * sizeof(float)))) cpu = decode((int32_t) i, composite, (int32_t) w, (int8_t*) (intptr_t) (int64_t) opts);
    for (int64_t c = 0; c < 3; c = c + 1) {
      if (near(plain[i * 4 + c], cpu[c], 0.01f) == 0) {
        return 13;
      }
    }
  }
  if (barsSurvive(plain, img, w) == 0) {
    return 14;
  }
  saveColorPicture("ntsc-1-source.ppm", img, w, h);
  savePicture("ntsc-2-composite.pgm", composite, w, h);
  saveColorPicture("ntsc-3-decoded.ppm", plain, w, h);
  saveColorPicture("ntsc-4-tint.ppm", turned, w, h);
  saveColorPicture("ntsc-5-color-off.ppm", grey, w, h);
  return 0;
}

static int64_t barsSurvive(float* decoded, float* img, int64_t w) {
  for (int64_t bar = 0; bar < 8; bar = bar + 1) {
    int64_t at = (20 * w + bar * w / 8 + w / 16) * 4;
    for (int64_t c = 0; c < 3; c = c + 1) {
      if (near(decoded[at + c], img[at + c], 0.02f) == 0) {
        return 0;
      }
    }
  }
  return 1;
}

int main(void) {
  return (int) main_();
}
