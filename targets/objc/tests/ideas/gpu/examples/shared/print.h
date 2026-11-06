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

int32_t gettimeofday(int64_t timeValue, int64_t zone);
int64_t write(int32_t fd, const char* text, int64_t count);

static int64_t wallMs(void);
static int64_t say(const char* text);
static int64_t newline(void);
static int64_t sayNumber(int64_t value);
static int64_t sayRight(int64_t value, int64_t width);
static int64_t sayDecimal(float value);

static int64_t wallMs(void) {
  int64_t* tv = (int8_t*) (intptr_t) calloc(2, 8);
  gettimeofday((int64_t) (intptr_t) tv, 0);
  int32_t* micro = (int8_t*) (intptr_t) (int64_t) (intptr_t) tv;
  return tv[0] * 1000 + (int64_t) micro[2] / 1000;
}

static int64_t say(const char* text) {
  return write(1, text, strlen(text));
}

static int64_t newline(void) {
  int8_t* one = (int8_t*) (intptr_t) calloc(2, 1);
  one[0] = 10;
  const char* text = (int8_t*) (intptr_t) (int64_t) (intptr_t) one;
  return write(1, text, 1);
}

static int64_t sayNumber(int64_t value) {
  int8_t* digits = (int8_t*) (intptr_t) calloc(24, 1);
  int64_t n = value;
  int64_t length = 0;
  if (n == 0) {
    digits[0] = 48;
    length = 1;
  }
  int64_t scratch = n;
  for (int64_t k = 0; k < 20; k = k + 1) {
    if (scratch > 0) {
      length = length + 1;
      scratch = scratch / 10;
    }
  }
  for (int64_t k = 0; k < 20; k = k + 1) {
    if (n > 0) {
      digits[length - 1 - k] = (int8_t) (48 + n % 10);
      n = n / 10;
    }
  }
  const char* text = (int8_t*) (intptr_t) (int64_t) (intptr_t) digits;
  return write(1, text, length);
}

static int64_t sayRight(int64_t value, int64_t width) {
  int64_t digits = 1;
  int64_t rest = value / 10;
  for (int64_t k = 0; k < 20; k = k + 1) {
    if (rest > 0) {
      digits = digits + 1;
      rest = rest / 10;
    }
  }
  int8_t* spaces = (int8_t*) (intptr_t) calloc(width + 1, 1);
  int64_t count = width - digits;
  if (count < 0) {
    count = 0;
  }
  for (int64_t k = 0; k < count; k = k + 1) {
    spaces[k] = 32;
  }
  const char* text = (int8_t*) (intptr_t) (int64_t) (intptr_t) spaces;
  write(1, text, count);
  return sayNumber(value);
}

static int64_t sayDecimal(float value) {
  float v = value;
  if (v < 0.0f) {
    say("-");
    v = (float) (0.0 - (double) v);
  }
  int64_t thousandths = (int64_t) (v * 1000.0f + 0.5f);
  sayNumber(thousandths / 1000);
  say(".");
  int64_t fraction = thousandths % 1000;
  if (fraction < 100) {
    say("0");
  }
  if (fraction < 10) {
    say("0");
  }
  return sayNumber(fraction);
}
