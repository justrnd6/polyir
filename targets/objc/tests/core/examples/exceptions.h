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

static int64_t riskyDouble(int64_t x);
static int64_t safeDouble(int64_t x);
static int64_t nestedTryCheck(int64_t x);

static int64_t riskyDouble(int64_t x) {
  if (x < 0) {
    throwCode(99);
  }
  return x * 2;
}

static int64_t safeDouble(int64_t x) {
  int64_t volatile result = 0;
  {
    Handler tried = {.outer = handlers};
    handlers = &tried;

    if (setjmp(tried.jump) == 0) {
      result = riskyDouble(x);
      handlers = tried.outer;
    } else {
      handlers = tried.outer;
      const int64_t code = tried.code;
      result = code;
    }
  }
  return result;
}

static int64_t nestedTryCheck(int64_t x) {
  int64_t volatile result = 0;
  {
    Handler tried = {.outer = handlers};
    handlers = &tried;

    if (setjmp(tried.jump) == 0) {
      {
        Handler tried = {.outer = handlers};
        handlers = &tried;

        if (setjmp(tried.jump) == 0) {
          result = riskyDouble(x);
          handlers = tried.outer;
        } else {
          handlers = tried.outer;
          const int64_t innerCode = tried.code;
          result = innerCode + 1;
          throwCode(result);
        }
      }
      handlers = tried.outer;
    } else {
      handlers = tried.outer;
      const int64_t outerCode = tried.code;
      result = outerCode * 10;
    }
  }
  return result;
}
