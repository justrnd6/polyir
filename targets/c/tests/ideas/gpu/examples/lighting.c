#include <setjmp.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../Metal.h"
#include "shared/lighting.h"
#include "shared/picture.h"
#include "shared/scene.h"

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

static int64_t setLook(float* o, float ambient, float diffuse, float specular, float shininess);
static int64_t main_(void);
static int64_t draw(int64_t device, int64_t pipeline, float* cam, float* o, const char* name);

static int64_t setLook(float* o, float ambient, float diffuse, float specular, float shininess) {
  for (int64_t k = 0; k < 16; k = k + 1) {
    o[k] = 0.0f;
  }
  o[0] = ambient;
  o[1] = diffuse;
  o[2] = specular;
  o[3] = shininess;
  o[4] = 1.0f;
  o[5] = 1.0f;
  o[6] = 1.0f;
  o[7] = 1.0f;
  o[8] = 1.0f;
  return 0;
}

static int64_t main_(void) {
  int64_t device = metalDevice();
  if (device == 0) {
    return 10;
  }
  int64_t pipeline = metalPipeline(device, metalReadFile("lighting.metal"), "lit");
  if (pipeline == 0) {
    return 11;
  }
  float* cam = (int8_t*) (intptr_t) calloc(28, 4);
  buildCamera(cam, 240, 135);
  float* o = (int8_t*) (intptr_t) calloc(16, 4);
  setLook(o, 0.18f, 0.0f, 0.0f, 40.0f);
  if (draw(device, pipeline, cam, o, "light-1-ambient.ppm") != 0) {
    return 13;
  }
  setLook(o, 0.0f, 1.0f, 0.0f, 40.0f);
  if (draw(device, pipeline, cam, o, "light-2-diffuse.ppm") != 0) {
    return 13;
  }
  setLook(o, 0.0f, 0.0f, 1.0f, 40.0f);
  if (draw(device, pipeline, cam, o, "light-3-specular.ppm") != 0) {
    return 13;
  }
  setLook(o, 0.18f, 1.0f, 0.0f, 40.0f);
  if (draw(device, pipeline, cam, o, "light-4-ambient-diffuse.ppm") != 0) {
    return 13;
  }
  setLook(o, 0.18f, 0.9f, 0.7f, 40.0f);
  if (draw(device, pipeline, cam, o, "light-5-full.ppm") != 0) {
    return 13;
  }
  o[7] = 0.0f;
  if (draw(device, pipeline, cam, o, "light-6-no-gamma.ppm") != 0) {
    return 13;
  }
  setLook(o, 0.18f, 0.9f, 0.7f, 40.0f);
  o[4] = 1.0f;
  o[5] = 0.55f;
  o[6] = 0.25f;
  if (draw(device, pipeline, cam, o, "light-7-warm.ppm") != 0) {
    return 13;
  }
  setLook(o, 0.1f, 0.9f, 0.7f, 40.0f);
  o[9] = 1.0f;
  o[10] = (float) (0.0 - 1.5);
  o[11] = 0.9f;
  o[12] = (float) (0.0 - 1.2);
  o[13] = 2.4f;
  if (draw(device, pipeline, cam, o, "light-8-lamp.ppm") != 0) {
    return 13;
  }
  setLook(o, 0.18f, 0.9f, 0.8f, 8.0f);
  if (draw(device, pipeline, cam, o, "light-9-shiny-low.ppm") != 0) {
    return 13;
  }
  setLook(o, 0.18f, 0.9f, 0.8f, 200.0f);
  if (draw(device, pipeline, cam, o, "light-10-shiny-high.ppm") != 0) {
    return 13;
  }
  setLook(o, 0.18f, 0.9f, 0.7f, 40.0f);
  o[8] = 0.0f;
  if (draw(device, pipeline, cam, o, "light-11-no-shadow.ppm") != 0) {
    return 13;
  }
  setLook(o, 0.18f, 0.9f, 0.7f, 40.0f);
  o[14] = 1.0f;
  if (draw(device, pipeline, cam, o, "normal-1-colours.ppm") != 0) {
    return 13;
  }
  o[14] = 2.0f;
  o[15] = 0.8f;
  if (draw(device, pipeline, cam, o, "normal-2-mirror.ppm") != 0) {
    return 13;
  }
  o[14] = 3.0f;
  o[15] = 0.0f;
  if (draw(device, pipeline, cam, o, "normal-3-bumps.ppm") != 0) {
    return 13;
  }
  return 0;
}

static int64_t draw(int64_t device, int64_t pipeline, float* cam, float* o, const char* name) {
  int64_t w = 240;
  int64_t h = 135;
  int64_t n = w * h;
  int64_t out = metalEmptyBuffer(device, n * 16);
  if (metalDispatch(device, pipeline, n, metalBuffer(device, (int64_t) (intptr_t) cam, 112), metalBuffer(device, (int64_t) (intptr_t) o, 64), out, 0) == 0) {
    return 1;
  }
  float* gpu = (int8_t*) (intptr_t) metalContents(out);
  saveColorPicture(name, gpu, w, h);
  int64_t differ = 0;
  for (int64_t i = 0; i < n; i = i + 1) {
    float __attribute__((vector_size(4 * sizeof(float)))) cpu = lit((int32_t) i, cam, o);
    for (int64_t c = 0; c < 3; c = c + 1) {
      float gap = gpu[i * 4 + c] - cpu[c];
      if (gap < 0.0f) {
        gap = (float) (0.0 - (double) gap);
      }
      if (gap > 0.02f) {
        differ = differ + 1;
      }
    }
  }
  if (differ > 80) {
    return 1;
  }
  return 0;
}

int main(void) {
  return (int) main_();
}
