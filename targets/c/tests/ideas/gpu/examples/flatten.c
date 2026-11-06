#include <setjmp.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../Metal.h"
#include "shared/matrix.h"
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

static float cornerCoordinate(int64_t k, int64_t bit);
static int64_t modelMatrix(float* m);
static int64_t viewMatrix(float* m);
static int64_t projectionMatrix(float* m);
static int64_t main_(void);
static int64_t checkCorners(float* corners, float* clip, float* picture);
static int64_t differs(float a, float b);
static float screenX(float* clip, int64_t k);
static float screenY(float* clip, int64_t k);
static int64_t drawCube(float* picture, float* clip);
static int64_t drawLine(float* picture, float x0, float y0, float x1, float y1, float tone);
static int64_t drawDot(float* picture, float x, float y);

static float cornerCoordinate(int64_t k, int64_t bit) {
  return (float) (1.0 - 2.0 * (double) (float) (k / bit % 2));
}

static int64_t modelMatrix(float* m) {
  identity(m);
  m[11] = (float) (0.0 - 5.0);
  return 0;
}

static int64_t viewMatrix(float* m) {
  identity(m);
  return 0;
}

static int64_t projectionMatrix(float* m) {
  for (int64_t k = 0; k < 16; k = k + 1) {
    m[k] = 0.0f;
  }
  m[0] = 1.0f;
  m[5] = 1.0f;
  m[10] = (float) (0.0 - 11.0 / 9.0);
  m[11] = (float) (0.0 - 20.0 / 9.0);
  m[14] = (float) (0.0 - 1.0);
  return 0;
}

static int64_t main_(void) {
  int64_t device = metalDevice();
  if (device == 0) {
    return 10;
  }
  int64_t pipeline = metalPipeline(device, metalReadFile("flatten.metal"), "transform");
  if (pipeline == 0) {
    return 11;
  }
  float* model = (int8_t*) (intptr_t) calloc(16, 4);
  float* view = (int8_t*) (intptr_t) calloc(16, 4);
  float* projection = (int8_t*) (intptr_t) calloc(16, 4);
  float* viewModel = (int8_t*) (intptr_t) calloc(16, 4);
  float* all = (int8_t*) (intptr_t) calloc(16, 4);
  modelMatrix(model);
  viewMatrix(view);
  projectionMatrix(projection);
  multiply4(view, model, viewModel);
  multiply4(projection, viewModel, all);
  float* corners = (int8_t*) (intptr_t) calloc(32, 4);
  for (int64_t k = 0; k < 8; k = k + 1) {
    corners[k * 4] = cornerCoordinate(k, 1);
    corners[k * 4 + 1] = cornerCoordinate(k, 2);
    corners[k * 4 + 2] = cornerCoordinate(k, 4);
    corners[k * 4 + 3] = 1.0f;
  }
  int64_t out = metalEmptyBuffer(device, 128);
  if (metalDispatch(device, pipeline, 8, metalBuffer(device, (int64_t) (intptr_t) corners, 128), metalBuffer(device, (int64_t) (intptr_t) all, 64), out, 0) == 0) {
    return 12;
  }
  float* clip = (int8_t*) (intptr_t) metalContents(out);
  float* picture = (int8_t*) (intptr_t) calloc(200 * 200, 4);
  for (int64_t p = 0; p < 40000; p = p + 1) {
    picture[p] = 0.08f;
  }
  if (checkCorners(corners, clip, picture) != 0) {
    return 13;
  }
  drawCube(picture, clip);
  savePicture("flatten.pgm", picture, 200, 200);
  return 0;
}

static int64_t checkCorners(float* corners, float* clip, float* picture) {
  for (int64_t k = 0; k < 8; k = k + 1) {
    float w = clip[k * 4 + 3];
    float distance = (float) (5.0 - (double) corners[k * 4 + 2]);
    if (differs(w, distance) == 1) {
      return 1;
    }
    float ndcX = clip[k * 4] / w;
    float ndcY = clip[k * 4 + 1] / w;
    float ndcZ = clip[k * 4 + 2] / w;
    if (differs(ndcX, corners[k * 4] / distance) == 1) {
      return 1;
    }
    if (differs(ndcY, corners[k * 4 + 1] / distance) == 1) {
      return 1;
    }
    if (differs(ndcZ, (float) ((11.0 * (double) distance - 20.0) / (9.0 * (double) distance))) == 1) {
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
  if (gap > 0.0005f) {
    return 1;
  }
  return 0;
}

static float screenX(float* clip, int64_t k) {
  return (clip[k * 4] / clip[k * 4 + 3] + 1.0f) * 0.5f * 200.0f;
}

static float screenY(float* clip, int64_t k) {
  return (float) ((1.0 - (double) (clip[k * 4 + 1] / clip[k * 4 + 3])) * 0.5 * 200.0);
}

static int64_t drawCube(float* picture, float* clip) {
  for (int64_t a = 0; a < 8; a = a + 1) {
    for (int64_t bit = 1; bit < 8; bit = bit * 2) {
      if (a / bit % 2 == 0) {
        int64_t b = a + bit;
        drawLine(picture, screenX(clip, a), screenY(clip, a), screenX(clip, b), screenY(clip, b), (float) (0.55 + 0.4 * (double) (float) (a / 4)));
      }
    }
  }
  for (int64_t k = 0; k < 8; k = k + 1) {
    drawDot(picture, screenX(clip, k), screenY(clip, k));
  }
  return 0;
}

static int64_t drawLine(float* picture, float x0, float y0, float x1, float y1, float tone) {
  for (int64_t s = 0; s <= 400; s = s + 1) {
    float t = (float) s / 400.0f;
    int64_t x = (int64_t) (x0 + (x1 - x0) * t);
    int64_t y = (int64_t) (y0 + (y1 - y0) * t);
    if (x >= 0 && x < 200 && y >= 0 && y < 200) {
      picture[y * 200 + x] = tone;
    }
  }
  return 0;
}

static int64_t drawDot(float* picture, float x, float y) {
  for (int64_t dy = 0 - 2; dy <= 2; dy = dy + 1) {
    for (int64_t dx = 0 - 2; dx <= 2; dx = dx + 1) {
      int64_t px = (int64_t) x + dx;
      int64_t py = (int64_t) y + dy;
      if (px >= 0 && px < 200 && py >= 0 && py < 200) {
        picture[py * 200 + px] = 1.0f;
      }
    }
  }
  return 0;
}

int main(void) {
  return (int) main_();
}
