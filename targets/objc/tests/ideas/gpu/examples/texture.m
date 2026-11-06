#import <math.h>
#import <setjmp.h>
#import <stdbool.h>
#import <stdint.h>
#import <stdio.h>
#import <stdlib.h>
#import <string.h>

#import "../Metal.h"
#import "shared/picture.h"
#import "shared/scene.h"
#import "shared/texture.h"

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

static float resample(int32_t i, float* tex, float* p);
static float blur(int32_t i, float* tex, float* p);
static float edges(int32_t i, float* tex, float* p);
static int64_t fillSmiley(float* tex);
static int64_t main_(void);
static float* run(int64_t device, const char* source, const char* name, float* first, int64_t firstBytes, float* second, int64_t secondBytes, int64_t n);
static int64_t resampleDemos(int64_t device, const char* source);
static int64_t resampleOne(int64_t device, const char* source, float* tex, const char* name, float filter, float wrapMode, float from, float to);
static int64_t differs(float a, float b);
static int64_t floorDemos(int64_t device, const char* source);
static int64_t floorOne(int64_t device, const char* source, float* pyramid, const char* name, float look);
static int64_t imageDemos(int64_t device, const char* source);
static int64_t filterOne(int64_t device, const char* source, const char* kernelName, const char* name, float* img, float* p);

static float resample(int32_t i, float* tex, float* p) {
  int32_t w = (int32_t) p[1];
  float u = p[4] + (p[5] - p[4]) * ((float) (i % w) + 0.5f) / p[1];
  float v = p[4] + (p[5] - p[4]) * ((float) (i / w) + 0.5f) / p[1];
  if (p[2] < 0.5f) {
    return nearest(tex, 0, (int32_t) p[0], u, v, (int32_t) p[3]);
  }
  return bilinear(tex, 0, (int32_t) p[0], u, v, (int32_t) p[3]);
}

static float blur(int32_t i, float* tex, float* p) {
  int32_t size = (int32_t) p[0];
  int32_t x = i % size;
  int32_t y = i / size;
  float sum = 0.0f;
  for (int64_t dy = 0 - 1; dy <= 1; dy = dy + 1) {
    for (int64_t dx = 0 - 1; dx <= 1; dx = dx + 1) {
      sum = sum + texel(tex, 0, size, x + (int32_t) dx, y + (int32_t) dy, 0);
    }
  }
  return sum / 9.0f;
}

static float edges(int32_t i, float* tex, float* p) {
  int32_t size = (int32_t) p[0];
  int32_t x = i % size;
  int32_t y = i / size;
  float acrossRight = texel(tex, 0, size, x + 1, y - 1, 0) + (float) (2.0 * (double) texel(tex, 0, size, x + 1, y, 0)) + texel(tex, 0, size, x + 1, y + 1, 0);
  float acrossLeft = texel(tex, 0, size, x - 1, y - 1, 0) + (float) (2.0 * (double) texel(tex, 0, size, x - 1, y, 0)) + texel(tex, 0, size, x - 1, y + 1, 0);
  float downBelow = texel(tex, 0, size, x - 1, y + 1, 0) + (float) (2.0 * (double) texel(tex, 0, size, x, y + 1, 0)) + texel(tex, 0, size, x + 1, y + 1, 0);
  float downAbove = texel(tex, 0, size, x - 1, y - 1, 0) + (float) (2.0 * (double) texel(tex, 0, size, x, y - 1, 0)) + texel(tex, 0, size, x + 1, y - 1, 0);
  float gx = acrossRight - acrossLeft;
  float gy = downBelow - downAbove;
  return sqrtf(gx * gx + gy * gy) * 0.25f;
}

static int64_t fillSmiley(float* tex) {
  List* rows = listFilled(sizeof(int32_t), 8, (int32_t[]){0});
  LIST_AT(rows, 0, int32_t) = 60;
  LIST_AT(rows, 1, int32_t) = 66;
  LIST_AT(rows, 2, int32_t) = 165;
  LIST_AT(rows, 3, int32_t) = 129;
  LIST_AT(rows, 4, int32_t) = 165;
  LIST_AT(rows, 5, int32_t) = 153;
  LIST_AT(rows, 6, int32_t) = 66;
  LIST_AT(rows, 7, int32_t) = 60;
  for (int64_t y = 0; y < 8; y = y + 1) {
    for (int64_t x = 0; x < 8; x = x + 1) {
      int32_t bit = LIST_AT(rows, y, int32_t) / (int32_t) (1 << 7 - x) % 2;
      tex[y * 8 + x] = (float) (0.9 - 0.8 * (double) (float) bit);
    }
  }
  return 0;
}

static int64_t main_(void) {
  int64_t device = metalDevice();
  if (device == 0) {
    return 10;
  }
  const char* source = metalReadFile("texture.metal");
  if (resampleDemos(device, source) != 0) {
    return 13;
  }
  if (floorDemos(device, source) != 0) {
    return 13;
  }
  return imageDemos(device, source);
}

static float* run(int64_t device, const char* source, const char* name, float* first, int64_t firstBytes, float* second, int64_t secondBytes, int64_t n) {
  int64_t pipeline = metalPipeline(device, source, name);
  int64_t out = metalEmptyBuffer(device, n * 4);
  if (pipeline == 0) {
    return (int8_t*) (intptr_t) 0;
  }
  if (metalDispatch(device, pipeline, n, metalBuffer(device, (int64_t) (intptr_t) first, firstBytes), metalBuffer(device, (int64_t) (intptr_t) second, secondBytes), out, 0) == 0) {
    return (int8_t*) (intptr_t) 0;
  }
  return (int8_t*) (intptr_t) metalContents(out);
}

static int64_t resampleDemos(int64_t device, const char* source) {
  float* tex = (int8_t*) (intptr_t) calloc(64, 4);
  fillSmiley(tex);
  if (resampleOne(device, source, tex, "tex-nearest.pgm", 0.0f, 0.0f, 0.0f, 1.0f) != 0) {
    return 1;
  }
  if (resampleOne(device, source, tex, "tex-bilinear.pgm", 1.0f, 0.0f, 0.0f, 1.0f) != 0) {
    return 1;
  }
  if (resampleOne(device, source, tex, "tex-clamp.pgm", 0.0f, 0.0f, (float) (0.0 - 0.5), 1.5f) != 0) {
    return 1;
  }
  if (resampleOne(device, source, tex, "tex-repeat.pgm", 0.0f, 1.0f, (float) (0.0 - 0.5), 1.5f) != 0) {
    return 1;
  }
  return resampleOne(device, source, tex, "tex-mirror.pgm", 0.0f, 2.0f, (float) (0.0 - 0.5), 1.5f);
}

static int64_t resampleOne(int64_t device, const char* source, float* tex, const char* name, float filter, float wrapMode, float from, float to) {
  float* p = (int8_t*) (intptr_t) calloc(8, 4);
  p[0] = 8.0f;
  p[1] = 128.0f;
  p[2] = filter;
  p[3] = wrapMode;
  p[4] = from;
  p[5] = to;
  float* gpu = run(device, source, "resample", tex, 256, p, 32, 16384);
  if ((int64_t) (intptr_t) gpu == 0) {
    return 1;
  }
  savePicture(name, gpu, 128, 128);
  for (int64_t i = 0; i < 16384; i = i + 1) {
    if (differs(gpu[i], resample((int32_t) i, tex, p)) == 1) {
      return 1;
    }
  }
  return 0;
}

static int64_t differs(float a, float b) {
  float gap = a - b;
  if (gap < 0.0f) {
    gap = (float) (0.0 - (double) gap);
  }
  if (gap > 0.002f) {
    return 1;
  }
  return 0;
}

static int64_t floorDemos(int64_t device, const char* source) {
  float* pyramid = (int8_t*) (intptr_t) calloc(1400, 4);
  buildPyramid(pyramid, 32);
  if (floorOne(device, source, pyramid, "floor-nearest.pgm", 1.0f) != 0) {
    return 1;
  }
  if (floorOne(device, source, pyramid, "floor-bilinear.pgm", 2.0f) != 0) {
    return 1;
  }
  return floorOne(device, source, pyramid, "floor-mip.pgm", 3.0f);
}

static int64_t floorOne(int64_t device, const char* source, float* pyramid, const char* name, float look) {
  float* cam = (int8_t*) (intptr_t) calloc(28, 4);
  buildCamera(cam, 320, 180);
  cam[24] = look;
  float* gpu = run(device, source, "trace", cam, 112, pyramid, 5460, 57600);
  if ((int64_t) (intptr_t) gpu == 0) {
    return 1;
  }
  savePicture(name, gpu, 320, 180);
  int64_t different = 0;
  for (int64_t i = 0; i < 57600; i = i + 1) {
    float gap = gpu[i] - trace((int32_t) i, cam, pyramid);
    if (gap < 0.0f) {
      gap = (float) (0.0 - (double) gap);
    }
    if (gap > 0.01f) {
      different = different + 1;
    }
  }
  if (different > 60) {
    return 1;
  }
  return 0;
}

static int64_t imageDemos(int64_t device, const char* source) {
  float* img = (int8_t*) (intptr_t) calloc(4096, 4);
  fillScene(img);
  savePicture("img-source.pgm", img, 64, 64);
  float* p = (int8_t*) (intptr_t) calloc(2, 4);
  p[0] = 64.0f;
  if (filterOne(device, source, "blur", "img-blur.pgm", img, p) != 0) {
    return 13;
  }
  return filterOne(device, source, "edges", "img-edges.pgm", img, p) * 13;
}

static int64_t filterOne(int64_t device, const char* source, const char* kernelName, const char* name, float* img, float* p) {
  float* gpu = run(device, source, kernelName, img, 16384, p, 8, 4096);
  if ((int64_t) (intptr_t) gpu == 0) {
    return 1;
  }
  savePicture(name, gpu, 64, 64);
  for (int64_t i = 0; i < 4096; i = i + 1) {
    float expected = blur((int32_t) i, img, p);
    if (textSame(kernelName, "edges")) {
      expected = edges((int32_t) i, img, p);
    }
    if (differs(gpu[i], expected) == 1) {
      return 1;
    }
  }
  return 0;
}

int main(void) {
  return (int) main_();
}
