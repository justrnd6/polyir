#import <math.h>
#import <setjmp.h>
#import <stdbool.h>
#import <stdint.h>
#import <stdio.h>
#import <stdlib.h>
#import <string.h>

#import "../Metal.h"
#import "shared/picture.h"

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

static float castView(int32_t i, float* view, float* cells);
static int64_t buildMap(float* cells);
static int64_t setView(float* view, float x, float y, float angle, float shift);
static int64_t main_(void);
static int64_t render(int64_t device, int64_t pipeline, float* view, float* cells, const char* name);

static float castView(int32_t i, float* view, float* cells) {
  int32_t w = (int32_t) view[4];
  int32_t mapW = (int32_t) view[6];
  float row = (float) (i / w) - view[8];
  float across = (float) (2.0 * (double) (float) (i % w) / (double) view[4] - 1.0);
  float dirX = cosf(view[2]);
  float dirY = sinf(view[2]);
  float rayX = dirX - dirY * view[3] * across;
  float rayY = dirY + dirX * view[3] * across;
  int32_t squareX = (int32_t) view[0];
  int32_t squareY = (int32_t) view[1];
  float deltaX = 1000000.0f;
  float deltaY = 1000000.0f;
  if (fabsf(rayX) > 0.000001f) {
    deltaX = fabsf((float) (1.0 / (double) rayX));
  }
  if (fabsf(rayY) > 0.000001f) {
    deltaY = fabsf((float) (1.0 / (double) rayY));
  }
  int32_t stepX = 1;
  float nextX = ((float) squareX + 1.0f - view[0]) * deltaX;
  if (rayX < 0.0f) {
    stepX = (int32_t) (0 - 1);
    nextX = (view[0] - (float) squareX) * deltaX;
  }
  int32_t stepY = 1;
  float nextY = ((float) squareY + 1.0f - view[1]) * deltaY;
  if (rayY < 0.0f) {
    stepY = (int32_t) (0 - 1);
    nextY = (view[1] - (float) squareY) * deltaY;
  }
  int32_t hit = 0;
  int32_t sideways = 0;
  for (int64_t n = 0; n < 48; n = n + 1) {
    if (hit == 0) {
      if (nextX < nextY) {
        nextX = nextX + deltaX;
        squareX = squareX + stepX;
        sideways = 0;
      } else {
        nextY = nextY + deltaY;
        squareY = squareY + stepY;
        sideways = 1;
      }
      if (cells[squareY * mapW + squareX] > 0.5f) {
        hit = 1;
      }
    }
  }
  float distance = nextX - deltaX;
  if (sideways == 1) {
    distance = nextY - deltaY;
  }
  float wallHeight = view[5] / distance;
  float top = view[5] * 0.5f - wallHeight * 0.5f;
  if (row >= top && row <= top + wallHeight) {
    float shade = (float) (1.0 / (1.0 + (double) (distance * 0.35f)));
    if (sideways == 1) {
      shade = shade * 0.6f;
    }
    return shade;
  }
  if (row < top) {
    return 0.05f;
  }
  return (float) (0.12 + 0.3 * (double) (row - view[5] * 0.5f) / (double) (view[5] * 0.5f));
}

static int64_t buildMap(float* cells) {
  for (int64_t y = 0; y < 12; y = y + 1) {
    for (int64_t x = 0; x < 12; x = x + 1) {
      float cell = x == 0 || y == 0 || x == 11 || y == 11 ? 1.0f : 0.0f;
      if (x == 6 && y == 6) {
        cell = 1.0f;
      }
      if (y == 8 && x >= 2 && x <= 4) {
        cell = 1.0f;
      }
      cells[y * 12 + x] = cell;
    }
  }
  return 0;
}

static int64_t setView(float* view, float x, float y, float angle, float shift) {
  view[0] = x;
  view[1] = y;
  view[2] = angle;
  view[3] = 0.9f;
  view[4] = 320.0f;
  view[5] = 200.0f;
  view[6] = 12.0f;
  view[7] = 12.0f;
  view[8] = shift;
  return 0;
}

static int64_t main_(void) {
  int64_t device = metalDevice();
  if (device == 0) {
    return 10;
  }
  int64_t pipeline = metalPipeline(device, metalReadFile("raycaster.metal"), "castView");
  if (pipeline == 0) {
    return 11;
  }
  float* cells = (int8_t*) (intptr_t) calloc(144, 4);
  buildMap(cells);
  float* view = (int8_t*) (intptr_t) calloc(9, 4);
  setView(view, 2.5f, 2.5f, 0.7f, 0.0f);
  if (render(device, pipeline, view, cells, "doom-1.pgm") != 0) {
    return 13;
  }
  setView(view, 2.5f, 2.5f, 2.2f, 0.0f);
  if (render(device, pipeline, view, cells, "doom-2.pgm") != 0) {
    return 13;
  }
  setView(view, 2.5f, 2.5f, 0.7f, 40.0f);
  return render(device, pipeline, view, cells, "doom-3.pgm");
}

static int64_t render(int64_t device, int64_t pipeline, float* view, float* cells, const char* name) {
  int64_t n = 320 * 200;
  int64_t out = metalEmptyBuffer(device, n * 4);
  int64_t ok = metalDispatch(device, pipeline, n, metalBuffer(device, (int64_t) (intptr_t) view, 36), metalBuffer(device, (int64_t) (intptr_t) cells, 576), out, 0);
  if (ok == 0) {
    return 1;
  }
  float* gpu = (int8_t*) (intptr_t) metalContents(out);
  savePicture(name, gpu, 320, 200);
  int64_t differ = 0;
  for (int64_t i = 0; i < n; i = i + 1) {
    float gap = gpu[i] - castView((int32_t) i, view, cells);
    if (gap < 0.0f) {
      gap = (float) (0.0 - (double) gap);
    }
    if (gap > 0.01f) {
      differ = differ + 1;
    }
  }
  if (differ > 60) {
    return 1;
  }
  return 0;
}

int main(void) {
  return (int) main_();
}
