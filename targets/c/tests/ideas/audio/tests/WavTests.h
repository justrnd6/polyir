#pragma once

#include <setjmp.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../../core/tests/programs/Check.h"
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

int64_t remove_(const char* path);

static Sound* fixture(const char* name);
static double checkEightBit(void);
static double checkSixteenBitStereo(void);
static double checkTwentyFourBit(void);
static double checkThirtyTwoBit(void);
static double checkFloat32Stereo(void);
static double checkFloat64(void);
static double checkExtensibleHeader(void);
static double checkTruncatedData(void);
static double checkNotAWavFile(void);
static double checkMissingFile(void);
static double checkSaveThenLoadMono(void);

static Sound* fixture(const char* name) {
  return soundLoadWav(name);
}

static double checkEightBit(void) {
  Sound* s = fixture("fixtures/pcm8-mono.wav");
  return offInt(s->count, 5) + offInt(s->rate, 8000) + offInt(s->channels, 1) + off((s->samples)[0], (float) (0.0 - 1.0)) + off((s->samples)[1], (float) (0.0 - 0.5)) + off((s->samples)[2], 0.0f) + off((s->samples)[3], 0.5f) + off((s->samples)[4], 0.9921875f);
}

static double checkSixteenBitStereo(void) {
  Sound* s = fixture("fixtures/pcm16-stereo.wav");
  double header = offInt(s->count, 4) + offInt(s->rate, 44100) + offInt(s->channels, 2);
  float* d = s->samples;
  return header + off(d[0], 0.0f) + off(d[1], 0.0f) + off(d[2], 0.5f) + off(d[3], (float) (0.0 - 0.5)) + off(d[4], 0.99996948f) + off(d[5], (float) (0.0 - 1.0)) + off(d[6], (float) (0.0 - 0.25)) + off(d[7], 0.25f);
}

static double checkTwentyFourBit(void) {
  Sound* s = fixture("fixtures/pcm24-mono.wav");
  return offInt(s->count, 4) + offInt(s->rate, 22050) + off((s->samples)[0], 0.5f) + off((s->samples)[1], (float) (0.0 - 0.5)) + off((s->samples)[2], 0.99999988f) + off((s->samples)[3], (float) (0.0 - 1.0));
}

static double checkThirtyTwoBit(void) {
  Sound* s = fixture("fixtures/pcm32-mono.wav");
  return offInt(s->count, 4) + offInt(s->rate, 48000) + off((s->samples)[0], 0.5f) + off((s->samples)[1], (float) (0.0 - 0.5)) + off((s->samples)[2], 1.0f) + off((s->samples)[3], (float) (0.0 - 1.0));
}

static double checkFloat32Stereo(void) {
  Sound* s = fixture("fixtures/float32-stereo.wav");
  float* d = s->samples;
  return offInt(s->count, 3) + offInt(s->channels, 2) + offInt(s->rate, 32000) + off(d[0], 0.25f) + off(d[1], (float) (0.0 - 0.75)) + off(d[2], 1.0f) + off(d[3], (float) (0.0 - 1.0)) + off(d[4], 0.125f) + off(d[5], 0.0f);
}

static double checkFloat64(void) {
  Sound* s = fixture("fixtures/float64-mono.wav");
  return offInt(s->count, 3) + offInt(s->rate, 16000) + off((s->samples)[0], 0.5f) + off((s->samples)[1], (float) (0.0 - 0.5)) + off((s->samples)[2], 0.125f);
}

static double checkExtensibleHeader(void) {
  Sound* s = fixture("fixtures/extensible16.wav");
  float* d = s->samples;
  return offInt(s->count, 2) + offInt(s->channels, 2) + offInt(s->rate, 44100) + off(d[0], 0.030517578f) + off(d[1], (float) (0.0 - 0.030517578)) + off(d[2], 0.061035156f) + off(d[3], (float) (0.0 - 0.061035156));
}

static double checkTruncatedData(void) {
  Sound* s = fixture("fixtures/truncated.wav");
  return offInt(s->count, 4) + off((s->samples)[3], 0.012207031f);
}

static double checkNotAWavFile(void) {
  Sound* s = fixture("fixtures/not-audio.wav");
  return offInt(s->count, 0);
}

static double checkMissingFile(void) {
  Sound* s = fixture("fixtures/there-is-no-such-file.wav");
  return offInt(s->count, 0);
}

static double checkSaveThenLoadMono(void) {
  Sound* s = soundNew(0.05f, 8000);
  Sound_tone(s, 0, 440.0f, 0.5f, 0.0f, 0.05f);
  Sound_saveWav(s, "tests-mono-roundtrip.wav");
  Sound* back = soundLoadWav("tests-mono-roundtrip.wav");
  remove_("tests-mono-roundtrip.wav");
  float worst = 0.0f;
  for (int64_t i = 0; i < s->count; i = i + 1) {
    float gap = (back->samples)[i] - (s->samples)[i];
    if (gap < 0.0f) {
      gap = (float) (0.0 - (double) gap);
    }
    if (gap > worst) {
      worst = gap;
    }
  }
  return offInt(back->count, 400) + offInt(back->rate, 8000) + offInt(back->channels, 1) + (double) worst;
}
