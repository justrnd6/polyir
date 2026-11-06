#pragma once

#import <setjmp.h>
#import <stdbool.h>
#import <stdint.h>
#import <stdio.h>
#import <stdlib.h>
#import <string.h>

#import "../../../core/tests/programs/Check.h"
#import "../Sound.h"
#import "../Wave.h"

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

static Sound* ramp(void);
static double checkNewStereoShape(void);
static double checkSecondsIgnoresChannels(void);
static double checkToneFillsBothChannels(void);
static double checkChannelExtraction(void);
static double checkToMonoAverages(void);
static double checkStereoFromTwoMonos(void);
static double checkToStereoCopies(void);
static Sound* panned(float position);
static double checkPanHardLeft(void);
static double checkPanCentre(void);
static double checkPanHardRight(void);
static double checkPanKeepsPower(void);
static double checkMixMonoIntoStereo(void);
static double checkMixStereoIntoStereo(void);
static double checkPeakOverChannels(void);
static double checkNormalizeStereo(void);
static double checkFilterKeepsChannelsApart(void);
static double checkStereoLowPassEqualsMono(void);
static double checkEchoStaysInChannel(void);
static double checkConvolveStereoWithMonoResponse(void);
static double checkWavRoundTrip(void);

static Sound* ramp(void) {
  Sound* s = soundNewFrames(4, 1000, 2);
  float* d = s->samples;
  for (int64_t i = 0; i < 4; i = i + 1) {
    d[i * 2] = (float) (i + 1);
    d[i * 2 + 1] = (float) (i + 1) * 10.0f;
  }
  return s;
}

static double checkNewStereoShape(void) {
  Sound* s = soundNewChannels(1.0f, 8000, 2);
  return offInt(s->channels, 2) + offInt(s->count, 8000) + offInt(s->rate, 8000);
}

static double checkSecondsIgnoresChannels(void) {
  return off(Sound_seconds(soundNewChannels(2.0f, 8000, 2)), 2.0f);
}

static double checkToneFillsBothChannels(void) {
  Sound* s = soundNewChannels(0.1f, 1000, 2);
  Sound_tone(s, 0, 125.0f, 0.5f, 0.0f, 0.1f);
  int64_t different = 0;
  for (int64_t i = 0; i < 100; i = i + 1) {
    if ((s->samples)[i * 2] != (s->samples)[i * 2 + 1]) {
      different = different + 1;
    }
  }
  return offInt(different, 0) + off(Sound_peak(s), 0.5f);
}

static double checkChannelExtraction(void) {
  Sound* left = Sound_channel(ramp(), 0);
  Sound* right = Sound_channel(ramp(), 1);
  return offInt(left->channels, 1) + off((left->samples)[2], 3.0f) + off((right->samples)[2], 30.0f) + offInt(right->count, 4);
}

static double checkToMonoAverages(void) {
  Sound* m = Sound_toMono(ramp());
  return offInt(m->channels, 1) + off((m->samples)[0], 5.5f) + off((m->samples)[3], 22.0f);
}

static double checkStereoFromTwoMonos(void) {
  Sound* l = soundNew(0.003f, 1000);
  Sound* r = soundNew(0.002f, 1000);
  (l->samples)[0] = 1.0f;
  (l->samples)[2] = 3.0f;
  (r->samples)[1] = 7.0f;
  Sound* s = soundStereo(l, r);
  return offInt(s->count, 3) + offInt(s->channels, 2) + off((s->samples)[0], 1.0f) + off((s->samples)[1], 0.0f) + off((s->samples)[3], 7.0f) + off((s->samples)[4], 3.0f) + off((s->samples)[5], 0.0f);
}

static double checkToStereoCopies(void) {
  Sound* m = soundNew(0.002f, 1000);
  (m->samples)[0] = 0.5f;
  (m->samples)[1] = 0.25f;
  Sound* s = Sound_toStereo(m);
  return offInt(s->channels, 2) + off((s->samples)[0], 0.5f) + off((s->samples)[1], 0.5f) + off((s->samples)[2], 0.25f) + off((s->samples)[3], 0.25f);
}

static Sound* panned(float position) {
  Sound* m = soundNew(0.001f, 1000);
  (m->samples)[0] = 1.0f;
  return Sound_pan(m, position);
}

static double checkPanHardLeft(void) {
  Sound* s = panned((float) (0.0 - 1.0));
  return off((s->samples)[0], 1.0f) + off((s->samples)[1], 0.0f);
}

static double checkPanCentre(void) {
  Sound* s = panned(0.0f);
  return off((s->samples)[0], 0.70710678f) + off((s->samples)[1], 0.70710678f);
}

static double checkPanHardRight(void) {
  Sound* s = panned(1.0f);
  return off((s->samples)[0], 0.0f) + off((s->samples)[1], 1.0f);
}

static double checkPanKeepsPower(void) {
  float worst = 0.0f;
  for (int64_t k = 0; k < 9; k = k + 1) {
    Sound* s = panned((float) k / 4.0f - 1.0f);
    float power = (s->samples)[0] * (s->samples)[0] + (s->samples)[1] * (s->samples)[1];
    float gap = power - 1.0f;
    if (gap < 0.0f) {
      gap = (float) (0.0 - (double) gap);
    }
    if (gap > worst) {
      worst = gap;
    }
  }
  return (double) worst;
}

static double checkMixMonoIntoStereo(void) {
  Sound* bed = soundNewChannels(0.004f, 1000, 2);
  Sound* m = soundNew(0.002f, 1000);
  (m->samples)[0] = 2.0f;
  Sound_add(bed, m, 0.001f, 1.0f);
  return off((bed->samples)[2], 2.0f) + off((bed->samples)[3], 2.0f) + off((bed->samples)[0], 0.0f);
}

static double checkMixStereoIntoStereo(void) {
  Sound* bed = soundNewChannels(0.004f, 1000, 2);
  Sound_add(bed, ramp(), 0.0f, 0.5f);
  return off((bed->samples)[2], 1.0f) + off((bed->samples)[3], 10.0f) + off((bed->samples)[6], 2.0f) + off((bed->samples)[7], 20.0f);
}

static double checkPeakOverChannels(void) {
  return off(Sound_peak(ramp()), 40.0f);
}

static double checkNormalizeStereo(void) {
  Sound* s = ramp();
  Sound_normalize(s, 1.0f);
  return off((s->samples)[7], 1.0f) + off((s->samples)[6], 0.1f);
}

static double checkFilterKeepsChannelsApart(void) {
  Sound* s = soundNewChannels(0.1f, 8000, 2);
  for (int64_t i = 0; i < 800; i = i + 1) {
    (s->samples)[i * 2] = sine((float) (440.0 * (double) (float) i / 8000.0));
  }
  Sound_lowPass(s, 500.0f);
  Sound_highPass(s, 100.0f);
  Sound_bell(s, 1000.0f, 1.0f, 6.0f);
  float rightPeak = 0.0f;
  for (int64_t i = 0; i < 800; i = i + 1) {
    float level = (s->samples)[i * 2 + 1];
    if (level < 0.0f) {
      level = (float) (0.0 - (double) level);
    }
    if (level > rightPeak) {
      rightPeak = level;
    }
  }
  if (Sound_peak(Sound_channel(s, 0)) < 0.1f) {
    return 1.0;
  }
  return off(rightPeak, 0.0f);
}

static double checkStereoLowPassEqualsMono(void) {
  Sound* mono = soundNew(0.1f, 8000);
  Sound* stereo = soundNewChannels(0.1f, 8000, 2);
  for (int64_t i = 0; i < 800; i = i + 1) {
    float v = sine((float) (1000.0 * (double) (float) i / 8000.0));
    (mono->samples)[i] = v;
    (stereo->samples)[i * 2] = v;
    (stereo->samples)[i * 2 + 1] = (float) (0.5 * (double) v);
  }
  Sound_lowPass(mono, 500.0f);
  Sound_lowPass(stereo, 500.0f);
  float worst = 0.0f;
  for (int64_t i = 0; i < 800; i = i + 1) {
    float gapLeft = (stereo->samples)[i * 2] - (mono->samples)[i];
    float gapRight = (stereo->samples)[i * 2 + 1] - (float) (0.5 * (double) (mono->samples)[i]);
    if (gapLeft < 0.0f) {
      gapLeft = (float) (0.0 - (double) gapLeft);
    }
    if (gapRight < 0.0f) {
      gapRight = (float) (0.0 - (double) gapRight);
    }
    if (gapLeft > worst) {
      worst = gapLeft;
    }
    if (gapRight > worst) {
      worst = gapRight;
    }
  }
  return (double) worst;
}

static double checkEchoStaysInChannel(void) {
  Sound* s = soundNewChannels(0.5f, 1000, 2);
  (s->samples)[0] = 1.0f;
  Sound_echo(s, 0.1f, 0.5f);
  return off((s->samples)[200], 0.5f) + off((s->samples)[201], 0.0f) + off((s->samples)[400], 0.25f);
}

static double checkConvolveStereoWithMonoResponse(void) {
  Sound* response = soundNew(0.002f, 1000);
  (response->samples)[0] = 1.0f;
  (response->samples)[1] = 1.0f;
  Sound* out = Sound_convolve(ramp(), response);
  return offInt(out->count, 5) + offInt(out->channels, 2) + off((out->samples)[2], 3.0f) + off((out->samples)[3], 30.0f) + off((out->samples)[8], 4.0f) + off((out->samples)[9], 40.0f);
}

static double checkWavRoundTrip(void) {
  Sound* s = soundNewChannels(0.05f, 8000, 2);
  Sound_tone(s, 0, 440.0f, 0.5f, 0.0f, 0.05f);
  (s->samples)[1] = 0.25f;
  Sound_saveWav(s, "tests-stereo-roundtrip.wav");
  Sound* back = soundLoadWav("tests-stereo-roundtrip.wav");
  remove_("tests-stereo-roundtrip.wav");
  float worst = 0.0f;
  for (int64_t i = 0; i < back->count * back->channels; i = i + 1) {
    float gap = (back->samples)[i] - (s->samples)[i];
    if (gap < 0.0f) {
      gap = (float) (0.0 - (double) gap);
    }
    if (gap > worst) {
      worst = gap;
    }
  }
  return offInt(back->count, s->count) + offInt(back->channels, 2) + offInt(back->rate, 8000) + (double) worst;
}
