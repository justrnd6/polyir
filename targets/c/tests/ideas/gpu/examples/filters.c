#include <setjmp.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../audio/Sound.h"
#include "../../audio/Wave.h"
#include "../Metal.h"
#include "shared/picture.h"
#include "shared/plot.h"

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

static float* spectrumOf(int64_t device, const char* source, float* signal_, int64_t n);
static Sound* twoTones(void);
static Sound* click(void);
static int64_t near(float a, float b, float tolerance);
static int64_t plotSpectrum(const char* name, float* spectrum, int64_t bins, float high);
static int64_t plotWave(const char* name, float* wave, int64_t n, float high);
static int64_t main_(void);
static int64_t spectra(int64_t device, const char* source);
static int64_t responses(int64_t device, const char* source);
static int64_t echoAndReverb(void);

static float* spectrumOf(int64_t device, const char* source, float* signal_, int64_t n) {
  int64_t pipeline = metalPipeline(device, source, "dft");
  if (pipeline == 0) {
    return (int8_t*) (intptr_t) 0;
  }
  float* size = (int8_t*) (intptr_t) calloc(2, 4);
  size[0] = (float) n;
  int64_t out = metalEmptyBuffer(device, n * 4);
  if (metalDispatch(device, pipeline, n, metalBuffer(device, (int64_t) (intptr_t) signal_, n * 4), metalBuffer(device, (int64_t) (intptr_t) size, 8), out, 0) == 0) {
    return (int8_t*) (intptr_t) 0;
  }
  return (int8_t*) (intptr_t) metalContents(out);
}

static Sound* twoTones(void) {
  Sound* s = soundNew(0.25f, 8000);
  float* data = s->samples;
  for (int64_t i = 0; i < 2000; i = i + 1) {
    float t = (float) i / 8000.0f;
    data[i] = sine((float) (200.0 * (double) t)) + sine((float) (1000.0 * (double) t));
  }
  return s;
}

static Sound* click(void) {
  Sound* s = soundNew(0.25f, 8000);
  float* data = s->samples;
  data[0] = 1.0f;
  return s;
}

static int64_t near(float a, float b, float tolerance) {
  float gap = a - b;
  if (gap < 0.0f) {
    gap = (float) (0.0 - (double) gap);
  }
  if (gap <= tolerance) {
    return 1;
  }
  return 0;
}

static int64_t plotSpectrum(const char* name, float* spectrum, int64_t bins, float high) {
  float* pic = (int8_t*) (intptr_t) calloc(320 * 96, 4);
  drawStems(pic, 320, 96, spectrum, bins, 0.0f, high);
  return savePicture(name, pic, 320, 96);
}

static int64_t plotWave(const char* name, float* wave, int64_t n, float high) {
  float* pic = (int8_t*) (intptr_t) calloc(320 * 96, 4);
  drawWave(pic, 320, 96, wave, n, (float) (0.0 - (double) high), high);
  return savePicture(name, pic, 320, 96);
}

static int64_t main_(void) {
  int64_t device = metalDevice();
  if (device == 0) {
    return 10;
  }
  const char* source = metalReadFile("filters.metal");
  if (spectra(device, source) != 0) {
    return 13;
  }
  if (responses(device, source) != 0) {
    return 13;
  }
  echoAndReverb();
  return 0;
}

static int64_t spectra(int64_t device, const char* source) {
  Sound* original = twoTones();
  Sound* low = twoTones();
  Sound_lowPass(low, 500.0f);
  Sound* high = twoTones();
  Sound_highPass(high, 500.0f);
  float* before = spectrumOf(device, source, original->samples, 2000);
  float* afterLow = spectrumOf(device, source, low->samples, 2000);
  float* afterHigh = spectrumOf(device, source, high->samples, 2000);
  if ((int64_t) (intptr_t) before == 0 || (int64_t) (intptr_t) afterLow == 0 || (int64_t) (intptr_t) afterHigh == 0) {
    return 1;
  }
  if (near(before[50], 1.0f, 0.02f) == 0 || near(before[250], 1.0f, 0.02f) == 0) {
    return 1;
  }
  if (near(afterLow[50], 0.903f, 0.04f) == 0 || near(afterLow[250], 0.399f, 0.04f) == 0) {
    return 1;
  }
  if (near(afterHigh[50], 0.359f, 0.04f) == 0 || near(afterHigh[250], 0.776f, 0.04f) == 0) {
    return 1;
  }
  plotSpectrum("filters-1-original.pgm", before, 300, 1.3f);
  plotSpectrum("filters-2-lowpass.pgm", afterLow, 300, 1.3f);
  plotSpectrum("filters-3-highpass.pgm", afterHigh, 300, 1.3f);
  return 0;
}

static int64_t responses(int64_t device, const char* source) {
  Sound* low = click();
  Sound_lowPass(low, 500.0f);
  Sound* high = click();
  Sound_highPass(high, 500.0f);
  float* lowSpectrum = spectrumOf(device, source, low->samples, 2000);
  float* highSpectrum = spectrumOf(device, source, high->samples, 2000);
  if ((int64_t) (intptr_t) lowSpectrum == 0 || (int64_t) (intptr_t) highSpectrum == 0) {
    return 1;
  }
  for (int64_t k = 0; k < 1000; k = k + 1) {
    lowSpectrum[k] = lowSpectrum[k] * 1000.0f;
    highSpectrum[k] = highSpectrum[k] * 1000.0f;
  }
  if (near(lowSpectrum[0], 1.0f, 0.02f) == 0 || near(highSpectrum[0], 0.0f, 0.02f) == 0) {
    return 1;
  }
  if (near(lowSpectrum[125], 0.649f, 0.03f) == 0) {
    return 1;
  }
  plotSpectrum("filters-4-lowpass-response.pgm", lowSpectrum, 500, 1.1f);
  plotSpectrum("filters-5-highpass-response.pgm", highSpectrum, 500, 1.1f);
  return 0;
}

static int64_t echoAndReverb(void) {
  Sound* beep = soundNew(1.0f, 8000);
  Sound_tone(beep, 0, 400.0f, 0.8f, 0.02f, 0.06f);
  plotWave("filters-6-beep.pgm", beep->samples, 8000, 1.0f);
  Sound* echoed = soundNew(1.0f, 8000);
  Sound_tone(echoed, 0, 400.0f, 0.8f, 0.02f, 0.06f);
  Sound_echo(echoed, 0.25f, 0.5f);
  plotWave("filters-7-echo.pgm", echoed->samples, 8000, 1.0f);
  Sound* hall = soundNew(1.0f, 8000);
  Sound_tone(hall, 0, 400.0f, 0.8f, 0.02f, 0.06f);
  Sound_reverb(hall, 0.8f);
  plotWave("filters-8-reverb.pgm", hall->samples, 8000, 1.0f);
  return 0;
}

int main(void) {
  return (int) main_();
}
