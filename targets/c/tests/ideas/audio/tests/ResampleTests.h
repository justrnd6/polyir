#pragma once

#include <math.h>
#include <setjmp.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../../core/tests/programs/Check.h"
#include "../Resample.h"
#include "../Sound.h"

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

static Sound* sineSound(float hz, float seconds, int64_t rate);
static double checkCountGoingUp(void);
static double checkCountGoingDown(void);
static double checkCountRounds(void);
static double checkSoundKeepsItsLength(void);
static double checkUpsamplingFollowsTheWave(void);
static double checkDownsamplingRemovesAliases(void);
static double checkDownsamplingKeepsLowTones(void);
static double checkRoundTrip(void);
static double checkConstantStaysConstant(void);
static double checkStereoResampling(void);
static double checkSameRateIsACopy(void);

static Sound* sineSound(float hz, float seconds, int64_t rate) {
  Sound* s = soundNew(seconds, rate);
  for (int64_t i = 0; i < s->count; i = i + 1) {
    (s->samples)[i] = (float) sin(6.283185307179586 * (double) hz * (double) i / (double) rate);
  }
  return s;
}

static double checkCountGoingUp(void) {
  return offInt(resampledCount(44100, 44100, 48000), 48000);
}

static double checkCountGoingDown(void) {
  return offInt(resampledCount(1000, 8000, 4000), 500);
}

static double checkCountRounds(void) {
  return offInt(resampledCount(10, 3, 4), 13);
}

static double checkSoundKeepsItsLength(void) {
  Sound* s = Sound_resample(sineSound(440.0f, 0.5f, 8000), 12000);
  return offInt(s->rate, 12000) + offInt(s->count, 6000) + offInt(s->channels, 1) + off(Sound_seconds(s), 0.5f);
}

static double checkUpsamplingFollowsTheWave(void) {
  Sound* s = Sound_resample(sineSound(440.0f, 1.0f, 8000), 12000);
  double worst = 0.0;
  for (int64_t j = 1000; j < 11000; j = j + 7) {
    double expected = sin(6.283185307179586 * 440.0 * (double) j / 12000.0);
    double gap = (double) (s->samples)[j] - expected;
    if (gap < 0.0) {
      gap = 0.0 - gap;
    }
    if (gap > worst) {
      worst = gap;
    }
  }
  return worst;
}

static double checkDownsamplingRemovesAliases(void) {
  Sound* s = Sound_resample(sineSound(3000.0f, 1.0f, 8000), 4000);
  return (double) amplitudeAt(s->samples, 500, 2000, 4000, 1000.0f);
}

static double checkDownsamplingKeepsLowTones(void) {
  Sound* s = Sound_resample(sineSound(1000.0f, 1.0f, 8000), 4000);
  return off(amplitudeAt(s->samples, 500, 2000, 4000, 1000.0f), 1.0f);
}

static double checkRoundTrip(void) {
  Sound* original = sineSound(300.0f, 1.0f, 8000);
  Sound* back = Sound_resample(Sound_resample(original, 12000), 8000);
  double worst = 0.0;
  for (int64_t j = 1000; j < 7000; j = j + 11) {
    double gap = (double) ((back->samples)[j] - (original->samples)[j]);
    if (gap < 0.0) {
      gap = 0.0 - gap;
    }
    if (gap > worst) {
      worst = gap;
    }
  }
  return worst;
}

static double checkConstantStaysConstant(void) {
  Sound* s = soundNew(0.5f, 8000);
  for (int64_t i = 0; i < s->count; i = i + 1) {
    (s->samples)[i] = 1.0f;
  }
  Sound* up = Sound_resample(s, 22050);
  Sound* down = Sound_resample(s, 3000);
  return off((up->samples)[up->count / 2], 1.0f) + off((down->samples)[down->count / 2], 1.0f);
}

static double checkStereoResampling(void) {
  Sound* left = sineSound(440.0f, 0.5f, 8000);
  Sound* right = sineSound(880.0f, 0.5f, 8000);
  Sound* out = Sound_resample(soundStereo(left, right), 16000);
  return offInt(out->channels, 2) + offInt(out->count, 8000) + off(amplitudeAt(Sound_channel(out, 0)->samples, 2000, 4000, 16000, 440.0f), 1.0f) + off(amplitudeAt(Sound_channel(out, 1)->samples, 2000, 4000, 16000, 880.0f), 1.0f);
}

static double checkSameRateIsACopy(void) {
  Sound* s = sineSound(440.0f, 0.1f, 8000);
  Sound* copy = Sound_resample(s, 8000);
  double worst = 0.0;
  for (int64_t i = 100; i < 700; i = i + 1) {
    double gap = (double) ((copy->samples)[i] - (s->samples)[i]);
    if (gap < 0.0) {
      gap = 0.0 - gap;
    }
    if (gap > worst) {
      worst = gap;
    }
  }
  return worst + offInt(copy->count, s->count);
}
