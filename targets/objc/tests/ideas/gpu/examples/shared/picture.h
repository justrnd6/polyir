#pragma once

#import <setjmp.h>
#import <stdbool.h>
#import <stdint.h>
#import <stdio.h>
#import <stdlib.h>
#import <string.h>

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

static int64_t putNumber(int8_t* bytes, int64_t at, int64_t n);
static int64_t savePicture(const char* path, float* brightness, int64_t w, int64_t h);
static int64_t fillScene(float* img);
static int64_t saveColorPicture(const char* path, float* pixels, int64_t w, int64_t h);

static int64_t putNumber(int8_t* bytes, int64_t at, int64_t n) {
  int64_t pos = at;
  if (n >= 100) {
    bytes[pos] = (int8_t) (48 + n / 100);
    pos = pos + 1;
  }
  if (n >= 10) {
    bytes[pos] = (int8_t) (48 + n / 10 % 10);
    pos = pos + 1;
  }
  bytes[pos] = (int8_t) (48 + n % 10);
  return pos + 1;
}

static int64_t savePicture(const char* path, float* brightness, int64_t w, int64_t h) {
  int8_t* bytes = (int8_t*) (intptr_t) calloc(w * h + 32, 1);
  bytes[0] = 80;
  bytes[1] = 53;
  bytes[2] = 10;
  int64_t at = putNumber(bytes, 3, w);
  bytes[at] = 32;
  at = putNumber(bytes, at + 1, h);
  bytes[at] = 10;
  bytes[at + 1] = 50;
  bytes[at + 2] = 53;
  bytes[at + 3] = 53;
  bytes[at + 4] = 10;
  at = at + 5;
  for (int64_t p = 0; p < w * h; p = p + 1) {
    float level = brightness[p];
    if (level < 0.0f) {
      level = 0.0f;
    }
    if (level > 1.0f) {
      level = 1.0f;
    }
    bytes[at + p] = (int8_t) (int32_t) (level * 255.0f);
  }
  int64_t file = fopen(path, "wb");
  if (file == 0) {
    return 0;
  }
  fwrite((int64_t) (intptr_t) bytes, 1, at + w * h, file);
  fclose(file);
  return 1;
}

static int64_t fillScene(float* img) {
  for (int64_t y = 0; y < 64; y = y + 1) {
    for (int64_t x = 0; x < 64; x = x + 1) {
      float level = (float) (0.3 + 0.3 * (double) (float) x / 64.0);
      float dx = (float) x - 22.0f;
      float dy = (float) y - 24.0f;
      if (dx * dx + dy * dy < 225.0f) {
        level = 0.95f;
      }
      if (x > 38 && x < 58 && y > 36 && y < 56) {
        level = 0.1f;
      }
      img[y * 64 + x] = level;
    }
  }
  return 0;
}

static int64_t saveColorPicture(const char* path, float* pixels, int64_t w, int64_t h) {
  int8_t* bytes = (int8_t*) (intptr_t) calloc(w * h * 3 + 32, 1);
  bytes[0] = 80;
  bytes[1] = 54;
  bytes[2] = 10;
  int64_t at = putNumber(bytes, 3, w);
  bytes[at] = 32;
  at = putNumber(bytes, at + 1, h);
  bytes[at] = 10;
  bytes[at + 1] = 50;
  bytes[at + 2] = 53;
  bytes[at + 3] = 53;
  bytes[at + 4] = 10;
  at = at + 5;
  for (int64_t p = 0; p < w * h; p = p + 1) {
    for (int64_t c = 0; c < 3; c = c + 1) {
      float level = pixels[p * 4 + c];
      if (level < 0.0f) {
        level = 0.0f;
      }
      if (level > 1.0f) {
        level = 1.0f;
      }
      bytes[at + p * 3 + c] = (int8_t) (int32_t) (level * 255.0f);
    }
  }
  int64_t file = fopen(path, "wb");
  if (file == 0) {
    return 0;
  }
  fwrite((int64_t) (intptr_t) bytes, 1, at + w * h * 3, file);
  fclose(file);
  return 1;
}
