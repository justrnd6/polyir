#pragma once

#import <math.h>
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

typedef struct Biquad Biquad;

struct Biquad {
  double b0;
  double b1;
  double b2;
  double a1;
  double a2;
};

static Biquad* Biquad_new(double b0, double b1, double b2, double a1, double a2);
static bool Biquad_equal(Biquad* a, Biquad* b);
static uint64_t Biquad_hash(Biquad* a);
static int64_t Biquad_run(Biquad* self, float* data, int64_t count);
static float Biquad_gain(Biquad* self, float hz, int64_t rate);

static Biquad* biquadFrom(double b0, double b1, double b2, double a0, double a1, double a2);
static double biquadAngle(int64_t rate, float hz);
static Biquad* biquadLowPass(int64_t rate, float hz, float q);
static Biquad* biquadHighPass(int64_t rate, float hz, float q);
static Biquad* biquadBandPass(int64_t rate, float hz, float q);
static Biquad* biquadNotch(int64_t rate, float hz, float q);
static Biquad* biquadBell(int64_t rate, float hz, float q, float gainDb);
static Biquad* biquadLowShelf(int64_t rate, float hz, float gainDb);
static Biquad* biquadHighShelf(int64_t rate, float hz, float gainDb);

static Biquad* Biquad_new(double b0, double b1, double b2, double a1, double a2) {
  Biquad* made = gcAlloc(sizeof(Biquad));
  made->b0 = b0;
  made->b1 = b1;
  made->b2 = b2;
  made->a1 = a1;
  made->a2 = a2;
  return made;
}

static bool Biquad_equal(Biquad* a, Biquad* b) {
  if (a == b || a == NULL || b == NULL) {
    return a == b;
  }

  bool same = true;
  same = same && a->b0 == b->b0;
  same = same && a->b1 == b->b1;
  same = same && a->b2 == b->b2;
  same = same && a->a1 == b->a1;
  same = same && a->a2 == b->a2;
  return same;
}

static uint64_t Biquad_hash(Biquad* a) {
  uint64_t hash = 17;
  hash = hash * 31 + hashBytes(&a->b0, sizeof a->b0);
  hash = hash * 31 + hashBytes(&a->b1, sizeof a->b1);
  hash = hash * 31 + hashBytes(&a->b2, sizeof a->b2);
  hash = hash * 31 + hashBytes(&a->a1, sizeof a->a1);
  hash = hash * 31 + hashBytes(&a->a2, sizeof a->a2);
  return hash;
}

static int64_t Biquad_run(Biquad* self, float* data, int64_t count) {
  double x1 = 0.0;
  double x2 = 0.0;
  double y1 = 0.0;
  double y2 = 0.0;
  for (int64_t i = 0; i < count; i = i + 1) {
    double x = (double) data[i];
    double y = self->b0 * x + self->b1 * x1 + self->b2 * x2 - self->a1 * y1 - self->a2 * y2;
    x2 = x1;
    x1 = x;
    y2 = y1;
    y1 = y;
    data[i] = (float) y;
  }
  return 0;
}

static float Biquad_gain(Biquad* self, float hz, int64_t rate) {
  double w = 6.283185307179586 * (double) hz / (double) rate;
  double c1 = cos(w);
  double s1 = sin(w);
  double c2 = cos(2.0 * w);
  double s2 = sin(2.0 * w);
  double topReal = self->b0 + self->b1 * c1 + self->b2 * c2;
  double topImag = 0.0 - (self->b1 * s1 + self->b2 * s2);
  double bottomReal = 1.0 + self->a1 * c1 + self->a2 * c2;
  double bottomImag = 0.0 - (self->a1 * s1 + self->a2 * s2);
  double strength = (topReal * topReal + topImag * topImag) / (bottomReal * bottomReal + bottomImag * bottomImag);
  return (float) sqrt(strength);
}

static Biquad* biquadFrom(double b0, double b1, double b2, double a0, double a1, double a2) {
  return Biquad_new(b0 / a0, b1 / a0, b2 / a0, a1 / a0, a2 / a0);
}

static double biquadAngle(int64_t rate, float hz) {
  return 6.283185307179586 * (double) hz / (double) rate;
}

static Biquad* biquadLowPass(int64_t rate, float hz, float q) {
  double w = biquadAngle(rate, hz);
  double c = cos(w);
  double alpha = sin(w) / (2.0 * (double) q);
  return biquadFrom((1.0 - c) / 2.0, 1.0 - c, (1.0 - c) / 2.0, 1.0 + alpha, 0.0 - 2.0 * c, 1.0 - alpha);
}

static Biquad* biquadHighPass(int64_t rate, float hz, float q) {
  double w = biquadAngle(rate, hz);
  double c = cos(w);
  double alpha = sin(w) / (2.0 * (double) q);
  return biquadFrom((1.0 + c) / 2.0, 0.0 - (1.0 + c), (1.0 + c) / 2.0, 1.0 + alpha, 0.0 - 2.0 * c, 1.0 - alpha);
}

static Biquad* biquadBandPass(int64_t rate, float hz, float q) {
  double w = biquadAngle(rate, hz);
  double c = cos(w);
  double alpha = sin(w) / (2.0 * (double) q);
  return biquadFrom(alpha, 0.0, 0.0 - alpha, 1.0 + alpha, 0.0 - 2.0 * c, 1.0 - alpha);
}

static Biquad* biquadNotch(int64_t rate, float hz, float q) {
  double w = biquadAngle(rate, hz);
  double c = cos(w);
  double alpha = sin(w) / (2.0 * (double) q);
  return biquadFrom(1.0, 0.0 - 2.0 * c, 1.0, 1.0 + alpha, 0.0 - 2.0 * c, 1.0 - alpha);
}

static Biquad* biquadBell(int64_t rate, float hz, float q, float gainDb) {
  double w = biquadAngle(rate, hz);
  double c = cos(w);
  double alpha = sin(w) / (2.0 * (double) q);
  double a = pow(10.0, (double) gainDb / 40.0);
  return biquadFrom(1.0 + alpha * a, 0.0 - 2.0 * c, 1.0 - alpha * a, 1.0 + alpha / a, 0.0 - 2.0 * c, 1.0 - alpha / a);
}

static Biquad* biquadLowShelf(int64_t rate, float hz, float gainDb) {
  double w = biquadAngle(rate, hz);
  double c = cos(w);
  double a = pow(10.0, (double) gainDb / 40.0);
  double beta = sqrt(2.0 * a) * sin(w);
  return biquadFrom(a * (a + 1.0 - (a - 1.0) * c + beta), 2.0 * a * (a - 1.0 - (a + 1.0) * c), a * (a + 1.0 - (a - 1.0) * c - beta), a + 1.0 + (a - 1.0) * c + beta, 0.0 - 2.0 * (a - 1.0 + (a + 1.0) * c), a + 1.0 + (a - 1.0) * c - beta);
}

static Biquad* biquadHighShelf(int64_t rate, float hz, float gainDb) {
  double w = biquadAngle(rate, hz);
  double c = cos(w);
  double a = pow(10.0, (double) gainDb / 40.0);
  double beta = sqrt(2.0 * a) * sin(w);
  return biquadFrom(a * (a + 1.0 + (a - 1.0) * c + beta), 0.0 - 2.0 * a * (a - 1.0 + (a + 1.0) * c), a * (a + 1.0 + (a - 1.0) * c - beta), a + 1.0 - (a - 1.0) * c + beta, 2.0 * (a - 1.0 - (a + 1.0) * c), a + 1.0 - (a - 1.0) * c - beta);
}
