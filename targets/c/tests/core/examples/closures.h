#pragma once

#include <setjmp.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef POLYIR_CLOSURE_H
#define POLYIR_CLOSURE_H

// A closure: its function and its environment, which the function takes first. The function is kept as any function pointer
// and called as its own type.
typedef void (*AnyFn)(void);

typedef struct Closure {
  AnyFn fn;
  void* env;
} Closure;
#endif

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

static int64_t counterTwice(void);
static Closure makeMultiplier(int64_t factor);
static int64_t tripleOf(int64_t x);
static int64_t sharedCounter(void);

typedef struct closure1_Env {
  int64_t* count;
} closure1_Env;

static closure1_Env* closure1_capture(int64_t* count) {
  closure1_Env* env = gcAlloc(sizeof(closure1_Env));
  env->count = count;
  return env;
}

static int64_t closure1(void* raw) {
  closure1_Env* env = raw;
  (*env->count) = (*env->count) + 1;
  return (*env->count);
}

typedef struct closure2_Env {
  int64_t factor;
} closure2_Env;

static closure2_Env* closure2_capture(int64_t factor) {
  closure2_Env* env = gcAlloc(sizeof(closure2_Env));
  env->factor = factor;
  return env;
}

static int64_t closure2(void* raw, int64_t x) {
  closure2_Env* env = raw;
  return x * env->factor;
}

typedef struct closure3_Env {
  int64_t* shared;
} closure3_Env;

static closure3_Env* closure3_capture(int64_t* shared) {
  closure3_Env* env = gcAlloc(sizeof(closure3_Env));
  env->shared = shared;
  return env;
}

static int64_t closure3(void* raw) {
  closure3_Env* env = raw;
  (*env->shared) = (*env->shared) + 1;
  return (*env->shared);
}

typedef struct closure4_Env {
  int64_t* shared;
} closure4_Env;

static closure4_Env* closure4_capture(int64_t* shared) {
  closure4_Env* env = gcAlloc(sizeof(closure4_Env));
  env->shared = shared;
  return env;
}

static int64_t closure4(void* raw) {
  closure4_Env* env = raw;
  (*env->shared) = (*env->shared) + 10;
  return (*env->shared);
}

static int64_t counterTwice(void) {
  int64_t* count = copyBytes((int64_t[]){0}, sizeof(int64_t));
  Closure inc = (Closure){(AnyFn) closure1, closure1_capture(count)};
  int64_t a = ((int64_t (*)(void*)) inc.fn)(inc.env);
  int64_t b = ((int64_t (*)(void*)) inc.fn)(inc.env);
  return a * 100 + b;
}

static Closure makeMultiplier(int64_t factor) {
  Closure mul = (Closure){(AnyFn) closure2, closure2_capture(factor)};
  return mul;
}

static int64_t tripleOf(int64_t x) {
  Closure triple = makeMultiplier(3);
  return ((int64_t (*)(void*, int64_t)) triple.fn)(triple.env, x);
}

static int64_t sharedCounter(void) {
  int64_t* shared = copyBytes((int64_t[]){0}, sizeof(int64_t));
  Closure bumpBy1 = (Closure){(AnyFn) closure3, closure3_capture(shared)};
  Closure bumpBy10 = (Closure){(AnyFn) closure4, closure4_capture(shared)};
  int64_t a = ((int64_t (*)(void*)) bumpBy1.fn)(bumpBy1.env);
  int64_t b = ((int64_t (*)(void*)) bumpBy10.fn)(bumpBy10.env);
  int64_t c = ((int64_t (*)(void*)) bumpBy1.fn)(bumpBy1.env);
  return a * 10000 + b * 100 + c;
}
