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

static float rasterize(int32_t i, float* tris, float* view);
static int64_t putTriangle(float* mesh, int64_t t, float* a, float* b, float* c);
static int64_t point(float* p, float x, float y, float z);
static int64_t cubeMesh(float* mesh);
static int64_t faceTriangle(float* mesh, int64_t t, float* c, int32_t i0, int32_t i1, int32_t i2);
static int64_t sphereMesh(float* mesh, int64_t rings, int64_t slices);
static int64_t spherePoint(float* p, int64_t r, int64_t s, int64_t rings, int64_t slices);
static int64_t polygonFan(float* mesh, int64_t n);
static int64_t placeMesh(float* mesh, int64_t count, float* screen, float ax, float ay, float distance, int64_t flat, float size);
static int64_t flatten(float* screen, int64_t t);
static int64_t setView(float* view, int64_t count, float outline, float colouring, float size);
static int64_t main_(void);
static int64_t draw(int64_t device, int64_t pipeline, float* screen, float* view, const char* name, int64_t count);

static float rasterize(int32_t i, float* tris, float* view) {
  int32_t w = (int32_t) view[3];
  float px = (float) (i % w) + 0.5f;
  float py = (float) (i / w) + 0.5f;
  int32_t count = (int32_t) view[0];
  float nearest = 0.0f;
  float tone = 0.06f;
  for (int64_t t = 0; t < (int64_t) count; t = t + 1) {
    int32_t b = (int32_t) (t * 18);
    float x0 = tris[b];
    float y0 = tris[b + 1];
    float x1 = tris[b + 6];
    float y1 = tris[b + 7];
    float x2 = tris[b + 12];
    float y2 = tris[b + 13];
    float area = (x1 - x0) * (y2 - y0) - (x2 - x0) * (y1 - y0);
    if (area < 0.0f) {
      float w0 = ((x2 - x1) * (py - y1) - (y2 - y1) * (px - x1)) / area;
      float w1 = ((x0 - x2) * (py - y2) - (y0 - y2) * (px - x2)) / area;
      float w2 = ((x1 - x0) * (py - y0) - (y1 - y0) * (px - x0)) / area;
      if (w0 >= 0.0f && w1 >= 0.0f && w2 >= 0.0f) {
        float depth = w0 * tris[b + 2] + w1 * tris[b + 8] + w2 * tris[b + 14];
        if (depth > nearest) {
          nearest = depth;
          if (view[2] > 0.5f) {
            tone = w0 * 0.95f + w1 * 0.5f + w2 * 0.1f;
          } else {
            float nx = w0 * tris[b + 3] + w1 * tris[b + 9] + w2 * tris[b + 15];
            float ny = w0 * tris[b + 4] + w1 * tris[b + 10] + w2 * tris[b + 16];
            float nz = w0 * tris[b + 5] + w1 * tris[b + 11] + w2 * tris[b + 17];
            float lit = (nx * view[5] + ny * view[6] + nz * view[7]) / sqrtf(nx * nx + ny * ny + nz * nz);
            if (lit < 0.0f) {
              lit = 0.0f;
            }
            tone = (float) (0.12 + 0.88 * (double) lit);
          }
          if (view[1] > 0.5f) {
            float d0 = (float) (0.0 - (double) (w0 * area / sqrtf((x2 - x1) * (x2 - x1) + (y2 - y1) * (y2 - y1))));
            float d1 = (float) (0.0 - (double) (w1 * area / sqrtf((x0 - x2) * (x0 - x2) + (y0 - y2) * (y0 - y2))));
            float d2 = (float) (0.0 - (double) (w2 * area / sqrtf((x1 - x0) * (x1 - x0) + (y1 - y0) * (y1 - y0))));
            if (d0 < 1.0f || d1 < 1.0f || d2 < 1.0f) {
              tone = 0.0f;
            }
          }
        }
      }
    }
  }
  return tone;
}

static int64_t putTriangle(float* mesh, int64_t t, float* a, float* b, float* c) {
  for (int64_t k = 0; k < 3; k = k + 1) {
    mesh[t * 18 + k] = a[k];
    mesh[t * 18 + 3 + k] = b[k];
    mesh[t * 18 + 6 + k] = c[k];
    mesh[t * 18 + 9 + k] = a[k];
    mesh[t * 18 + 12 + k] = b[k];
    mesh[t * 18 + 15 + k] = c[k];
  }
  return t + 1;
}

static int64_t point(float* p, float x, float y, float z) {
  p[0] = x;
  p[1] = y;
  p[2] = z;
  return 0;
}

static int64_t cubeMesh(float* mesh) {
  List* c = listFilled(sizeof(float), 24, (float[]){0.0f});
  for (int64_t k = 0; k < 8; k = k + 1) {
    LIST_AT(c, k * 3, float) = (float) (1.0 - 2.0 * (double) (float) (k % 2));
    LIST_AT(c, k * 3 + 1, float) = (float) (1.0 - 2.0 * (double) (float) (k / 2 % 2));
    LIST_AT(c, k * 3 + 2, float) = (float) (1.0 - 2.0 * (double) (float) (k / 4));
  }
  List* faces = listFilled(sizeof(int32_t), 24, (int32_t[]){0});
  LIST_AT(faces, 0, int32_t) = 0;
  LIST_AT(faces, 1, int32_t) = 1;
  LIST_AT(faces, 2, int32_t) = 3;
  LIST_AT(faces, 3, int32_t) = 2;
  LIST_AT(faces, 4, int32_t) = 4;
  LIST_AT(faces, 5, int32_t) = 5;
  LIST_AT(faces, 6, int32_t) = 7;
  LIST_AT(faces, 7, int32_t) = 6;
  LIST_AT(faces, 8, int32_t) = 0;
  LIST_AT(faces, 9, int32_t) = 1;
  LIST_AT(faces, 10, int32_t) = 5;
  LIST_AT(faces, 11, int32_t) = 4;
  LIST_AT(faces, 12, int32_t) = 2;
  LIST_AT(faces, 13, int32_t) = 3;
  LIST_AT(faces, 14, int32_t) = 7;
  LIST_AT(faces, 15, int32_t) = 6;
  LIST_AT(faces, 16, int32_t) = 0;
  LIST_AT(faces, 17, int32_t) = 2;
  LIST_AT(faces, 18, int32_t) = 6;
  LIST_AT(faces, 19, int32_t) = 4;
  LIST_AT(faces, 20, int32_t) = 1;
  LIST_AT(faces, 21, int32_t) = 3;
  LIST_AT(faces, 22, int32_t) = 7;
  LIST_AT(faces, 23, int32_t) = 5;
  int64_t t = 0;
  for (int64_t f = 0; f < 6; f = f + 1) {
    t = faceTriangle(mesh, t, c, LIST_AT(faces, f * 4, int32_t), LIST_AT(faces, f * 4 + 1, int32_t), LIST_AT(faces, f * 4 + 2, int32_t));
    t = faceTriangle(mesh, t, c, LIST_AT(faces, f * 4, int32_t), LIST_AT(faces, f * 4 + 2, int32_t), LIST_AT(faces, f * 4 + 3, int32_t));
  }
  return t;
}

static int64_t faceTriangle(float* mesh, int64_t t, float* c, int32_t i0, int32_t i1, int32_t i2) {
  List* a = listFilled(sizeof(float), 3, (float[]){0.0f});
  List* b = listFilled(sizeof(float), 3, (float[]){0.0f});
  List* d = listFilled(sizeof(float), 3, (float[]){0.0f});
  point((int8_t*) (intptr_t) (int64_t) a, c[i0 * 3], c[i0 * 3 + 1], c[i0 * 3 + 2]);
  point((int8_t*) (intptr_t) (int64_t) b, c[i1 * 3], c[i1 * 3 + 1], c[i1 * 3 + 2]);
  point((int8_t*) (intptr_t) (int64_t) d, c[i2 * 3], c[i2 * 3 + 1], c[i2 * 3 + 2]);
  float nx = (LIST_AT(b, 1, float) - LIST_AT(a, 1, float)) * (LIST_AT(d, 2, float) - LIST_AT(a, 2, float)) - (LIST_AT(b, 2, float) - LIST_AT(a, 2, float)) * (LIST_AT(d, 1, float) - LIST_AT(a, 1, float));
  float ny = (LIST_AT(b, 2, float) - LIST_AT(a, 2, float)) * (LIST_AT(d, 0, float) - LIST_AT(a, 0, float)) - (LIST_AT(b, 0, float) - LIST_AT(a, 0, float)) * (LIST_AT(d, 2, float) - LIST_AT(a, 2, float));
  float nz = (LIST_AT(b, 0, float) - LIST_AT(a, 0, float)) * (LIST_AT(d, 1, float) - LIST_AT(a, 1, float)) - (LIST_AT(b, 1, float) - LIST_AT(a, 1, float)) * (LIST_AT(d, 0, float) - LIST_AT(a, 0, float));
  int64_t after = 0;
  if (nx * LIST_AT(a, 0, float) + ny * LIST_AT(a, 1, float) + nz * LIST_AT(a, 2, float) > 0.0f) {
    after = putTriangle(mesh, t, (int8_t*) (intptr_t) (int64_t) a, (int8_t*) (intptr_t) (int64_t) b, (int8_t*) (intptr_t) (int64_t) d);
  } else {
    after = putTriangle(mesh, t, (int8_t*) (intptr_t) (int64_t) a, (int8_t*) (intptr_t) (int64_t) d, (int8_t*) (intptr_t) (int64_t) b);
  }
  return after;
}

static int64_t sphereMesh(float* mesh, int64_t rings, int64_t slices) {
  int64_t t = 0;
  List* a = listFilled(sizeof(float), 3, (float[]){0.0f});
  List* b = listFilled(sizeof(float), 3, (float[]){0.0f});
  List* c = listFilled(sizeof(float), 3, (float[]){0.0f});
  List* d = listFilled(sizeof(float), 3, (float[]){0.0f});
  for (int64_t r = 0; r < rings; r = r + 1) {
    for (int64_t s = 0; s < slices; s = s + 1) {
      spherePoint((int8_t*) (intptr_t) (int64_t) a, r, s, rings, slices);
      spherePoint((int8_t*) (intptr_t) (int64_t) b, r, s + 1, rings, slices);
      spherePoint((int8_t*) (intptr_t) (int64_t) c, r + 1, s + 1, rings, slices);
      spherePoint((int8_t*) (intptr_t) (int64_t) d, r + 1, s, rings, slices);
      t = putTriangle(mesh, t, (int8_t*) (intptr_t) (int64_t) a, (int8_t*) (intptr_t) (int64_t) c, (int8_t*) (intptr_t) (int64_t) b);
      t = putTriangle(mesh, t, (int8_t*) (intptr_t) (int64_t) a, (int8_t*) (intptr_t) (int64_t) d, (int8_t*) (intptr_t) (int64_t) c);
    }
  }
  return t;
}

static int64_t spherePoint(float* p, int64_t r, int64_t s, int64_t rings, int64_t slices) {
  float latitude = (float) (3.14159265 * (double) ((float) r / (float) rings - 0.5f));
  float longitude = (float) (6.2831853 * (double) (float) s / (double) (float) slices);
  point(p, cosf(latitude) * cosf(longitude), sinf(latitude), cosf(latitude) * sinf(longitude));
  return 0;
}

static int64_t polygonFan(float* mesh, int64_t n) {
  List* corners = listFilled(sizeof(float), 24, (float[]){0.0f});
  for (int64_t k = 0; k < n; k = k + 1) {
    float angle = (float) (6.2831853 * (double) (float) k / (double) (float) n + 0.3);
    LIST_AT(corners, k * 3, float) = cosf(angle);
    LIST_AT(corners, k * 3 + 1, float) = sinf(angle);
    LIST_AT(corners, k * 3 + 2, float) = 0.0f;
  }
  int64_t t = 0;
  List* toward = listFilled(sizeof(float), 3, (float[]){0.0f});
  for (int64_t k = 1; k < n - 1; k = k + 1) {
    List* a = listFilled(sizeof(float), 3, (float[]){0.0f});
    List* b = listFilled(sizeof(float), 3, (float[]){0.0f});
    List* c = listFilled(sizeof(float), 3, (float[]){0.0f});
    point((int8_t*) (intptr_t) (int64_t) a, LIST_AT(corners, 0, float), LIST_AT(corners, 1, float), LIST_AT(corners, 2, float));
    point((int8_t*) (intptr_t) (int64_t) b, LIST_AT(corners, k * 3, float), LIST_AT(corners, k * 3 + 1, float), LIST_AT(corners, k * 3 + 2, float));
    point((int8_t*) (intptr_t) (int64_t) c, LIST_AT(corners, k * 3 + 3, float), LIST_AT(corners, k * 3 + 4, float), LIST_AT(corners, k * 3 + 5, float));
    t = putTriangle(mesh, t, (int8_t*) (intptr_t) (int64_t) a, (int8_t*) (intptr_t) (int64_t) b, (int8_t*) (intptr_t) (int64_t) c);
  }
  for (int64_t j = 0; j < t; j = j + 1) {
    for (int64_t v = 0; v < 3; v = v + 1) {
      mesh[j * 18 + 9 + v * 3] = 0.0f;
      mesh[j * 18 + 10 + v * 3] = 0.0f;
      mesh[j * 18 + 11 + v * 3] = 1.0f;
    }
  }
  return t;
}

static int64_t placeMesh(float* mesh, int64_t count, float* screen, float ax, float ay, float distance, int64_t flat, float size) {
  float cy = cosf(ay);
  float sy = sinf(ay);
  float cx = cosf(ax);
  float sx = sinf(ax);
  float focal = size * 1.0f;
  for (int64_t t = 0; t < count; t = t + 1) {
    for (int64_t v = 0; v < 3; v = v + 1) {
      int64_t o = t * 18 + v * 3;
      float x = mesh[o];
      float y = mesh[o + 1];
      float z = mesh[o + 2];
      float x1 = cy * x + sy * z;
      float z1 = (float) (0.0 - (double) (sy * x) + (double) (cy * z));
      float y2 = cx * y - sx * z1;
      float z2 = sx * y + cx * z1;
      float depth = distance - z2;
      int64_t s = t * 18 + v * 6;
      screen[s] = size * 0.5f + focal * x1 / depth;
      screen[s + 1] = size * 0.5f - focal * y2 / depth;
      screen[s + 2] = (float) (1.0 / (double) depth);
      float nx = mesh[o + 9];
      float ny = mesh[o + 10];
      float nz = mesh[o + 11];
      float nx1 = cy * nx + sy * nz;
      float nz1 = (float) (0.0 - (double) (sy * nx) + (double) (cy * nz));
      screen[s + 3] = nx1;
      screen[s + 4] = cx * ny - sx * nz1;
      screen[s + 5] = sx * ny + cx * nz1;
    }
    if (flat == 1) {
      flatten(screen, t);
    }
  }
  return 0;
}

static int64_t flatten(float* screen, int64_t t) {
  float fx = 0.0f;
  float fy = 0.0f;
  float fz = 0.0f;
  for (int64_t v = 0; v < 3; v = v + 1) {
    fx = fx + screen[t * 18 + v * 6 + 3];
    fy = fy + screen[t * 18 + v * 6 + 4];
    fz = fz + screen[t * 18 + v * 6 + 5];
  }
  for (int64_t v = 0; v < 3; v = v + 1) {
    screen[t * 18 + v * 6 + 3] = fx;
    screen[t * 18 + v * 6 + 4] = fy;
    screen[t * 18 + v * 6 + 5] = fz;
  }
  return 0;
}

static int64_t setView(float* view, int64_t count, float outline, float colouring, float size) {
  view[0] = (float) count;
  view[1] = outline;
  view[2] = colouring;
  view[3] = size;
  view[4] = size;
  float length = sqrtf((float) (0.4 * 0.4 + 0.7 * 0.7 + 0.6 * 0.6));
  view[5] = (float) (0.4 / (double) length);
  view[6] = (float) (0.7 / (double) length);
  view[7] = (float) (0.6 / (double) length);
  return 0;
}

static int64_t main_(void) {
  int64_t device = metalDevice();
  if (device == 0) {
    return 10;
  }
  int64_t pipeline = metalPipeline(device, metalReadFile("triangles.metal"), "rasterize");
  if (pipeline == 0) {
    return 11;
  }
  float* mesh = (int8_t*) (intptr_t) calloc(2000 * 18, 4);
  float* screen = (int8_t*) (intptr_t) calloc(2000 * 18, 4);
  float* view = (int8_t*) (intptr_t) calloc(8, 4);
  int64_t one = polygonFan(mesh, 3);
  placeMesh(mesh, one, screen, 0.0f, 0.0f, 3.0f, 1, 192.0f);
  setView(view, one, 0.0f, 1.0f, 192.0f);
  if (draw(device, pipeline, screen, view, "tri-1-barycentric.pgm", one) != 0) {
    return 13;
  }
  int64_t fan = polygonFan(mesh, 6);
  placeMesh(mesh, fan, screen, 0.0f, 0.0f, 3.0f, 1, 192.0f);
  setView(view, fan, 1.0f, 0.0f, 192.0f);
  if (draw(device, pipeline, screen, view, "tri-2-fan.pgm", fan) != 0) {
    return 13;
  }
  int64_t cube = cubeMesh(mesh);
  placeMesh(mesh, cube, screen, 0.5f, 0.6f, 5.0f, 1, 192.0f);
  setView(view, cube, 0.0f, 0.0f, 192.0f);
  if (draw(device, pipeline, screen, view, "tri-3-cube.pgm", cube) != 0) {
    return 13;
  }
  setView(view, cube, 1.0f, 0.0f, 192.0f);
  if (draw(device, pipeline, screen, view, "tri-4-cube-wire.pgm", cube) != 0) {
    return 13;
  }
  int64_t coarse = sphereMesh(mesh, 6, 8);
  placeMesh(mesh, coarse, screen, 0.35f, 0.4f, 3.4f, 1, 192.0f);
  setView(view, coarse, 0.0f, 0.0f, 192.0f);
  if (draw(device, pipeline, screen, view, "tri-5-sphere-flat.pgm", coarse) != 0) {
    return 13;
  }
  placeMesh(mesh, coarse, screen, 0.35f, 0.4f, 3.4f, 0, 192.0f);
  if (draw(device, pipeline, screen, view, "tri-6-sphere-smooth.pgm", coarse) != 0) {
    return 13;
  }
  setView(view, coarse, 1.0f, 0.0f, 192.0f);
  if (draw(device, pipeline, screen, view, "tri-7-sphere-wire.pgm", coarse) != 0) {
    return 13;
  }
  int64_t fine = sphereMesh(mesh, 16, 24);
  placeMesh(mesh, fine, screen, 0.35f, 0.4f, 3.4f, 0, 192.0f);
  setView(view, fine, 0.0f, 0.0f, 192.0f);
  if (draw(device, pipeline, screen, view, "tri-8-sphere-fine.pgm", fine) != 0) {
    return 13;
  }
  setView(view, fine, 1.0f, 0.0f, 192.0f);
  return draw(device, pipeline, screen, view, "tri-9-sphere-fine-wire.pgm", fine) * 13;
}

static int64_t draw(int64_t device, int64_t pipeline, float* screen, float* view, const char* name, int64_t count) {
  int64_t n = 192 * 192;
  int64_t out = metalEmptyBuffer(device, n * 4);
  int64_t ok = metalDispatch(device, pipeline, n, metalBuffer(device, (int64_t) (intptr_t) screen, count * 72), metalBuffer(device, (int64_t) (intptr_t) view, 32), out, 0);
  if (ok == 0) {
    return 1;
  }
  float* gpu = (int8_t*) (intptr_t) metalContents(out);
  savePicture(name, gpu, 192, 192);
  int64_t differ = 0;
  for (int64_t i = 0; i < n; i = i + 1) {
    float gap = gpu[i] - rasterize((int32_t) i, screen, view);
    if (gap < 0.0f) {
      gap = (float) (0.0 - (double) gap);
    }
    if (gap > 0.01f) {
      differ = differ + 1;
    }
  }
  if (differ > 40) {
    return 1;
  }
  return 0;
}

int main(void) {
  return (int) main_();
}
