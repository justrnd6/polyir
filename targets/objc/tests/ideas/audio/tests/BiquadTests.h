#pragma once

#import <math.h>
#import <setjmp.h>
#import <stdbool.h>
#import <stdint.h>
#import <stdio.h>
#import <stdlib.h>
#import <string.h>

#import "../../../core/tests/programs/Check.h"
#import "../Biquad.h"

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

static float measuredGain(Biquad* filter, float hz, int64_t rate);
static double checkLowPassGainAtCutoffIsQ(void);
static double checkLowPassResonanceGainIsQ(void);
static double checkLowPassPassesLowTones(void);
static double checkLowPassStopsHighTones(void);
static double checkHighPassGainAtCutoffIsQ(void);
static double checkHighPassStopsLowTones(void);
static double checkHighPassPassesHighTones(void);
static double checkBandPassPeaksAtOne(void);
static double checkBandPassRejectsOneOctaveAway(void);
static double checkNotchRemovesCentre(void);
static double checkNotchKeepsOthers(void);
static double checkBellBoostsCentre(void);
static double checkBellLeavesFarTonesAlone(void);
static double checkBellCutsCentre(void);
static double checkBellWithZeroGainIsFlat(void);
static double checkLowShelfBoostsBass(void);
static double checkLowShelfLeavesTrebleAlone(void);
static double checkLowShelfCornerIsHalfway(void);
static double checkHighShelfBoostsTreble(void);
static double checkHighShelfLeavesBassAlone(void);
static double checkHighShelfCornerIsHalfway(void);
static double checkRunMatchesGainLowPass(void);
static double checkRunMatchesGainHighPass(void);
static double checkRunMatchesGainBandPass(void);
static double checkRunMatchesGainNotch(void);
static double checkRunMatchesGainBell(void);
static double checkRunMatchesGainShelves(void);
static double checkLowCentreStaysAccurate(void);

static float measuredGain(Biquad* filter, float hz, int64_t rate) {
  float* data = (int8_t*) (intptr_t) calloc(rate, 4);
  for (int64_t i = 0; i < rate; i = i + 1) {
    data[i] = (float) sin(6.283185307179586 * (double) hz * (double) i / (double) rate);
  }
  Biquad_run(filter, data, rate);
  return amplitudeAt(data, rate / 2, rate / 2, rate, hz);
}

static double checkLowPassGainAtCutoffIsQ(void) {
  return off(Biquad_gain(biquadLowPass(44100, 1000.0f, 0.7071f), 1000.0f, 44100), 0.7071f);
}

static double checkLowPassResonanceGainIsQ(void) {
  return off(Biquad_gain(biquadLowPass(44100, 1000.0f, 2.0f), 1000.0f, 44100), 2.0f);
}

static double checkLowPassPassesLowTones(void) {
  return off(Biquad_gain(biquadLowPass(44100, 1000.0f, 0.7071f), 100.0f, 44100), 0.99995015f);
}

static double checkLowPassStopsHighTones(void) {
  return off(Biquad_gain(biquadLowPass(44100, 1000.0f, 0.7071f), 4000.0f, 44100), 0.05924098f);
}

static double checkHighPassGainAtCutoffIsQ(void) {
  return off(Biquad_gain(biquadHighPass(44100, 1000.0f, 0.7071f), 1000.0f, 44100), 0.7071f);
}

static double checkHighPassStopsLowTones(void) {
  return off(Biquad_gain(biquadHighPass(44100, 1000.0f, 0.7071f), 100.0f, 44100), 0.00996603f);
}

static double checkHighPassPassesHighTones(void) {
  return off(Biquad_gain(biquadHighPass(44100, 1000.0f, 0.7071f), 10000.0f, 44100), 0.99997657f);
}

static double checkBandPassPeaksAtOne(void) {
  return off(Biquad_gain(biquadBandPass(44100, 1000.0f, 4.0f), 1000.0f, 44100), 1.0f);
}

static double checkBandPassRejectsOneOctaveAway(void) {
  return off(Biquad_gain(biquadBandPass(44100, 1000.0f, 4.0f), 500.0f, 44100), 0.16406081f) + off(Biquad_gain(biquadBandPass(44100, 1000.0f, 4.0f), 2000.0f, 44100), 0.16304698f);
}

static double checkNotchRemovesCentre(void) {
  return off(Biquad_gain(biquadNotch(44100, 1000.0f, 4.0f), 1000.0f, 44100), 0.0f);
}

static double checkNotchKeepsOthers(void) {
  return off(Biquad_gain(biquadNotch(44100, 1000.0f, 4.0f), 100.0f, 44100), 0.9996824f) + off(Biquad_gain(biquadNotch(44100, 1000.0f, 4.0f), 5000.0f, 44100), 0.99876329f);
}

static double checkBellBoostsCentre(void) {
  return off(Biquad_gain(biquadBell(44100, 1000.0f, 2.0f, 6.0f), 1000.0f, 44100), 1.99526231f);
}

static double checkBellLeavesFarTonesAlone(void) {
  return off(Biquad_gain(biquadBell(44100, 1000.0f, 2.0f, 6.0f), 100.0f, 44100), 1.00189479f) + off(Biquad_gain(biquadBell(44100, 1000.0f, 2.0f, 6.0f), 10000.0f, 44100), 1.00129053f);
}

static double checkBellCutsCentre(void) {
  return off(Biquad_gain(biquadBell(44100, 1000.0f, 2.0f, (float) (0.0 - 6.0)), 1000.0f, 44100), 0.50118723f);
}

static double checkBellWithZeroGainIsFlat(void) {
  return off(Biquad_gain(biquadBell(44100, 1000.0f, 2.0f, 0.0f), 1000.0f, 44100), 1.0f);
}

static double checkLowShelfBoostsBass(void) {
  return off(Biquad_gain(biquadLowShelf(44100, 1000.0f, 6.0f), 1.0f, 44100), 1.99526231f);
}

static double checkLowShelfLeavesTrebleAlone(void) {
  return off(Biquad_gain(biquadLowShelf(44100, 1000.0f, 6.0f), 20000.0f, 44100), 1.0f);
}

static double checkLowShelfCornerIsHalfway(void) {
  return off(Biquad_gain(biquadLowShelf(44100, 1000.0f, 6.0f), 1000.0f, 44100), 1.41253754f);
}

static double checkHighShelfBoostsTreble(void) {
  return off(Biquad_gain(biquadHighShelf(44100, 1000.0f, 6.0f), 20000.0f, 44100), 1.9952623f);
}

static double checkHighShelfLeavesBassAlone(void) {
  return off(Biquad_gain(biquadHighShelf(44100, 1000.0f, 6.0f), 1.0f, 44100), 1.0f);
}

static double checkHighShelfCornerIsHalfway(void) {
  return off(Biquad_gain(biquadHighShelf(44100, 1000.0f, 6.0f), 1000.0f, 44100), 1.41253754f);
}

static double checkRunMatchesGainLowPass(void) {
  Biquad* f = biquadLowPass(44100, 1000.0f, 0.7071f);
  return off(measuredGain(f, 440.0f, 44100), Biquad_gain(f, 440.0f, 44100)) + off(measuredGain(f, 2000.0f, 44100), Biquad_gain(f, 2000.0f, 44100));
}

static double checkRunMatchesGainHighPass(void) {
  Biquad* f = biquadHighPass(44100, 1000.0f, 0.7071f);
  return off(measuredGain(f, 440.0f, 44100), Biquad_gain(f, 440.0f, 44100)) + off(measuredGain(f, 2000.0f, 44100), Biquad_gain(f, 2000.0f, 44100));
}

static double checkRunMatchesGainBandPass(void) {
  Biquad* f = biquadBandPass(44100, 882.0f, 4.0f);
  return off(measuredGain(f, 882.0f, 44100), Biquad_gain(f, 882.0f, 44100)) + off(measuredGain(f, 440.0f, 44100), Biquad_gain(f, 440.0f, 44100));
}

static double checkRunMatchesGainNotch(void) {
  Biquad* f = biquadNotch(44100, 882.0f, 4.0f);
  return off(measuredGain(f, 882.0f, 44100), Biquad_gain(f, 882.0f, 44100)) + off(measuredGain(f, 2000.0f, 44100), Biquad_gain(f, 2000.0f, 44100));
}

static double checkRunMatchesGainBell(void) {
  Biquad* f = biquadBell(44100, 882.0f, 2.0f, 6.0f);
  return off(measuredGain(f, 882.0f, 44100), Biquad_gain(f, 882.0f, 44100)) + off(measuredGain(f, 4410.0f, 44100), Biquad_gain(f, 4410.0f, 44100));
}

static double checkRunMatchesGainShelves(void) {
  Biquad* low = biquadLowShelf(44100, 2000.0f, (float) (0.0 - 6.0));
  Biquad* high = biquadHighShelf(44100, 2000.0f, 6.0f);
  return off(measuredGain(low, 440.0f, 44100), Biquad_gain(low, 440.0f, 44100)) + off(measuredGain(high, 4410.0f, 44100), Biquad_gain(high, 4410.0f, 44100));
}

static double checkLowCentreStaysAccurate(void) {
  Biquad* f = biquadLowPass(48000, 40.0f, 0.7071f);
  return off(Biquad_gain(f, 40.0f, 48000), 0.7071f) + off(measuredGain(f, 120.0f, 48000), Biquad_gain(f, 120.0f, 48000));
}
