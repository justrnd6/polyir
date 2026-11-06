#include <setjmp.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../Metal.h"
#include "shared/handshake.h"
#include "shared/print.h"

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

static int64_t main_(void);
static int64_t launches(int64_t device, int64_t pipeline, int64_t* buffers, float* input, int64_t n);

static int64_t main_(void) {
  int64_t n = 1048576;
  float* input = makeInput(n);
  say("Metal: where the time goes");
  newline();
  int64_t mark = wallMs();
  int64_t device = metalDevice();
  if (device == 0) {
    return 10;
  }
  reportRow("find the GPU                        ", wallMs() - mark, "ms", "once per program");
  const char* source = metalReadFile("handshake.metal");
  if (textSame(source, "")) {
    return 11;
  }
  mark = wallMs();
  int64_t pipeline = metalPipeline(device, source, "addOne");
  if (pipeline == 0) {
    return 11;
  }
  reportRow("compile the kernel                  ", wallMs() - mark, "ms", "once per kernel");
  mark = wallMs();
  int64_t* buffers = (int8_t*) (intptr_t) calloc(2, 8);
  buffers[0] = metalBuffer(device, (int64_t) (intptr_t) input, n * 4);
  buffers[1] = metalEmptyBuffer(device, n * 4);
  reportRow("make 2 buffers, fill one with 4 MiB ", wallMs() - mark, "ms", "once per buffer");
  return launches(device, pipeline, buffers, input, n);
}

static int64_t launches(int64_t device, int64_t pipeline, int64_t* buffers, float* input, int64_t n) {
  int64_t tiny = 1000;
  int64_t mark = wallMs();
  for (int64_t k = 0; k < tiny; k = k + 1) {
    if (metalDispatchBuffers(device, pipeline, 1, buffers, 2) == 0) {
      return 12;
    }
  }
  reportRow("launch for 1 element, and wait      ", microsEach(wallMs() - mark, tiny), "us", "every launch");
  int64_t big = 50;
  mark = wallMs();
  for (int64_t k = 0; k < big; k = k + 1) {
    if (metalDispatchBuffers(device, pipeline, n, buffers, 2) == 0) {
      return 12;
    }
  }
  reportRow("launch for 1,048,576 elements, wait ", microsEach(wallMs() - mark, big), "us", "every launch");
  mark = wallMs();
  float* output = (int8_t*) (intptr_t) metalContents(buffers[1]);
  float sum = 0.0f;
  for (int64_t i = 0; i < n; i = i + 1) {
    sum = sum + output[i];
  }
  reportRow("the CPU reads the 4 MiB of results  ", wallMs() - mark, "ms", "no copy: same memory");
  if (resultsAreRight(input, output, n) == 0) {
    return 13;
  }
  return 0;
}

int main(void) {
  return (int) main_();
}
