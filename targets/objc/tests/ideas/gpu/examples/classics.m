#import <setjmp.h>
#import <stdbool.h>
#import <stdint.h>
#import <stdio.h>
#import <stdlib.h>
#import <string.h>

#import "../Metal.h"
#import "shared/classics.h"

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

static int64_t main_(void) {
  int64_t device = metalDevice();
  if (device == 0) {
    return 10;
  }
  const char* source = metalReadFile("classics.metal");
  if (textSame(source, "")) {
    return 11;
  }
  return runAll(device, source);
}

int main(void) {
  return (int) main_();
}
