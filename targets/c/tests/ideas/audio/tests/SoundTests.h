#pragma once

#include <setjmp.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../../core/tests/programs/Check.h"
#include "../Sound.h"
#include "../Wave.h"

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

static Sound* quietTone(void);
static double checkNewSoundIsMono(void);
static double checkNewSoundLength(void);
static double checkSeconds(void);
static double checkTonePeak(void);
static double checkSquareTonePeak(void);
static double checkScale(void);
static double checkMixAdds(void);
static double checkNormalize(void);
static double checkDelayedMixKeepsLength(void);
static double checkDelayedMixIsSilentBefore(void);
static double checkDelayedMixStartsLater(void);
static double checkToneFadesIn(void);
static double checkNoiseAmplitude(void);
static float filterGain(int64_t kind_, float hz, float cutoff);
static double checkLowPassCutsHighTone(void);
static double checkLowPassKeepsLowTone(void);
static double checkHighPassKeepsHighTone(void);
static double checkHighPassCutsLowTone(void);
static double checkEchoFirstRepeat(void);
static double checkEchoSecondRepeat(void);
static Sound* convolved(void);
static double checkConvolutionLength(void);
static double checkConvolutionValues(void);
static Sound* reverbOfImpulse(void);
static double checkReverbStart(void);
static double checkReverbFirstEcho(void);

static Sound* quietTone(void) {
  Sound* s = soundNew(0.1f, 1000);
  Sound_tone(s, 0, 125.0f, 0.5f, 0.0f, 0.1f);
  return s;
}

static double checkNewSoundIsMono(void) {
  return offInt(soundNew(1.0f, 8000)->channels, 1);
}

static double checkNewSoundLength(void) {
  return offInt(soundNew(2.5f, 8000)->count, 20000);
}

static double checkSeconds(void) {
  return off(Sound_seconds(soundNew(2.5f, 8000)), 2.5f);
}

static double checkTonePeak(void) {
  return off(Sound_peak(quietTone()), 0.5f);
}

static double checkSquareTonePeak(void) {
  Sound* s = soundNew(0.1f, 1000);
  Sound_tone(s, 1, 125.0f, 0.4f, 0.0f, 0.1f);
  return off(Sound_peak(s), 0.4f);
}

static double checkScale(void) {
  Sound* s = quietTone();
  Sound_scale(s, 0.5f);
  return off(Sound_peak(s), 0.25f);
}

static double checkMixAdds(void) {
  Sound* a = quietTone();
  Sound_add(a, quietTone(), 0.0f, 1.0f);
  return off(Sound_peak(a), 1.0f);
}

static double checkNormalize(void) {
  Sound* s = soundNew(0.1f, 1000);
  Sound_tone(s, 0, 125.0f, 0.3f, 0.0f, 0.1f);
  Sound_normalize(s, 0.9f);
  return off(Sound_peak(s), 0.9f);
}

static double checkDelayedMixKeepsLength(void) {
  Sound* a = soundNew(1.0f, 1000);
  Sound_add(a, quietTone(), 0.5f, 1.0f);
  return offInt(a->count, 1000);
}

static double checkDelayedMixIsSilentBefore(void) {
  Sound* a = soundNew(1.0f, 1000);
  Sound_add(a, quietTone(), 0.5f, 1.0f);
  return off((a->samples)[499], 0.0f);
}

static double checkDelayedMixStartsLater(void) {
  Sound* a = soundNew(1.0f, 1000);
  Sound_add(a, quietTone(), 0.5f, 1.0f);
  return off((a->samples)[525], 0.35355339f);
}

static double checkToneFadesIn(void) {
  Sound* s = soundNew(0.5f, 8000);
  Sound_tone(s, 0, 440.0f, 1.0f, 0.0f, 0.5f);
  return off((s->samples)[0], 0.0f);
}

static double checkNoiseAmplitude(void) {
  Sound* s = soundNew(0.1f, 8000);
  Sound_noise(s, 0.25f, 7);
  float loudest = Sound_peak(s);
  if (loudest > 0.25f) {
    return 1.0;
  }
  if (loudest < 0.2f) {
    return 1.0;
  }
  return 0.0;
}

static float filterGain(int64_t kind_, float hz, float cutoff) {
  Sound* s = soundNew(1.0f, 8000);
  float* data = s->samples;
  for (int64_t i = 0; i < 8000; i = i + 1) {
    data[i] = sine(hz * (float) i / 8000.0f);
  }
  if (kind_ == 0) {
    Sound_lowPass(s, cutoff);
  } else {
    Sound_highPass(s, cutoff);
  }
  float loudest = 0.0f;
  for (int64_t i = 6000; i < 8000; i = i + 1) {
    float level = data[i];
    if (level < 0.0f) {
      level = (float) (0.0 - (double) level);
    }
    if (level > loudest) {
      loudest = level;
    }
  }
  return loudest;
}

static double checkLowPassCutsHighTone(void) {
  return off(filterGain(0, 1000.0f, 500.0f), 0.3987f);
}

static double checkLowPassKeepsLowTone(void) {
  return off(filterGain(0, 200.0f, 500.0f), 0.9031f);
}

static double checkHighPassKeepsHighTone(void) {
  return off(filterGain(1, 1000.0f, 500.0f), 0.7225f);
}

static double checkHighPassCutsLowTone(void) {
  return off(filterGain(1, 200.0f, 500.0f), 0.3613f);
}

static double checkEchoFirstRepeat(void) {
  Sound* s = soundNew(0.5f, 1000);
  (s->samples)[0] = 1.0f;
  Sound_echo(s, 0.1f, 0.5f);
  return off((s->samples)[100], 0.5f);
}

static double checkEchoSecondRepeat(void) {
  Sound* s = soundNew(0.5f, 1000);
  (s->samples)[0] = 1.0f;
  Sound_echo(s, 0.1f, 0.5f);
  return off((s->samples)[200], 0.25f);
}

static Sound* convolved(void) {
  Sound* a = soundNew(0.002f, 1000);
  Sound* r = soundNew(0.003f, 1000);
  (a->samples)[0] = 1.0f;
  (a->samples)[1] = 2.0f;
  (r->samples)[0] = 1.0f;
  (r->samples)[2] = 3.0f;
  return Sound_convolve(a, r);
}

static double checkConvolutionLength(void) {
  return offInt(convolved()->count, 4);
}

static double checkConvolutionValues(void) {
  float* y = convolved()->samples;
  return off(y[0], 1.0f) + off(y[1], 2.0f) + off(y[2], 3.0f) + off(y[3], 6.0f);
}

static Sound* reverbOfImpulse(void) {
  Sound* s = soundNew(1.0f, 10000);
  (s->samples)[0] = 1.0f;
  Sound_reverb(s, 1.0f);
  return s;
}

static double checkReverbStart(void) {
  return off((reverbOfImpulse()->samples)[0], 0.20525f);
}

static double checkReverbFirstEcho(void) {
  return off((reverbOfImpulse()->samples)[297], 0.03924f);
}
