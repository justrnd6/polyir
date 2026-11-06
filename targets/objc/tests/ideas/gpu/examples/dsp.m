#import <math.h>
#import <setjmp.h>
#import <stdbool.h>
#import <stdint.h>
#import <stdio.h>
#import <stdlib.h>
#import <string.h>

#import "../Metal.h"
#import "shared/picture.h"
#import "shared/plot.h"
#import "shared/spectrum.h"

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

static float dft2d(int32_t i, float* img, float* p);
static int64_t twoSines(float* x, int64_t n);
static int64_t tooFast(float* x, int64_t n);
static int64_t stripes(float* img, int64_t s, float across, float down);
static int64_t blurFilter(float* x, int64_t n);
static int64_t edgeFilter(float* x, int64_t n);
static int64_t main_(void);
static float* transform1D(int64_t device, const char* source, float* x, int64_t n);
static int64_t plotPair(const char* signalName, const char* spectrumName, float* x, int64_t n, float* spectrum, int64_t shown, float signalHigh, float spectrumHigh);
static int64_t scaleBy(float* values, int64_t n, float factor);
static int64_t spectra1D(int64_t device, const char* source);
static int64_t transform2D(int64_t device, const char* source, float* img, const char* name, const char* spectrumName);
static int64_t spectra2D(int64_t device, const char* source);

static float dft2d(int32_t i, float* img, float* p) {
  int32_t s = (int32_t) p[0];
  int32_t u = (i % s + s / 2) % s;
  int32_t v = (i / s + s / 2) % s;
  float re = 0.0f;
  float im = 0.0f;
  for (int64_t y = 0; y < (int64_t) s; y = y + 1) {
    for (int64_t x = 0; x < (int64_t) s; x = x + 1) {
      float turns = (float) ((u * (int32_t) x + v * (int32_t) y) % s) / (float) s;
      re = re + img[y * (int64_t) s + x] * cosf((float) (6.2831853 * (double) turns));
      im = im - img[y * (int64_t) s + x] * sinf((float) (6.2831853 * (double) turns));
    }
  }
  float strength = sqrtf(re * re + im * im) / (float) (s * s);
  return logf((float) (1.0 + (double) (p[1] * strength))) / logf((float) (1.0 + (double) p[1]));
}

static int64_t twoSines(float* x, int64_t n) {
  for (int64_t j = 0; j < n; j = j + 1) {
    float t = (float) j / (float) n;
    x[j] = sinf((float) (6.2831853 * 5.0 * (double) t)) + (float) (0.5 * (double) sinf((float) (6.2831853 * 20.0 * (double) t)));
  }
  return 0;
}

static int64_t tooFast(float* x, int64_t n) {
  for (int64_t j = 0; j < n; j = j + 1) {
    x[j] = sinf((float) (6.2831853 * 50.0 * (double) (float) j / (double) (float) n));
  }
  return 0;
}

static int64_t stripes(float* img, int64_t s, float across, float down) {
  for (int64_t y = 0; y < s; y = y + 1) {
    for (int64_t x = 0; x < s; x = x + 1) {
      float turns = (across * (float) x + down * (float) y) / (float) s;
      img[y * s + x] = (float) (0.5 + 0.5 * (double) sinf((float) (6.2831853 * (double) turns)));
    }
  }
  return 0;
}

static int64_t blurFilter(float* x, int64_t n) {
  for (int64_t j = 0; j < n; j = j + 1) {
    x[j] = 0.0f;
  }
  x[0] = 0.3333f;
  x[1] = 0.3333f;
  x[2] = 0.3333f;
  return 0;
}

static int64_t edgeFilter(float* x, int64_t n) {
  for (int64_t j = 0; j < n; j = j + 1) {
    x[j] = 0.0f;
  }
  x[0] = (float) (0.0 - 1.0);
  x[2] = 1.0f;
  return 0;
}

static int64_t main_(void) {
  int64_t device = metalDevice();
  if (device == 0) {
    return 10;
  }
  const char* source = metalReadFile("dsp.metal");
  if (spectra1D(device, source) != 0) {
    return 13;
  }
  return spectra2D(device, source);
}

static float* transform1D(int64_t device, const char* source, float* x, int64_t n) {
  int64_t pipeline = metalPipeline(device, source, "dft");
  if (pipeline == 0) {
    return (int8_t*) (intptr_t) 0;
  }
  float* p = (int8_t*) (intptr_t) calloc(2, 4);
  p[0] = (float) n;
  int64_t out = metalEmptyBuffer(device, n * 4);
  if (metalDispatch(device, pipeline, n, metalBuffer(device, (int64_t) (intptr_t) x, n * 4), metalBuffer(device, (int64_t) (intptr_t) p, 8), out, 0) == 0) {
    return (int8_t*) (intptr_t) 0;
  }
  float* gpu = (int8_t*) (intptr_t) metalContents(out);
  for (int64_t k = 0; k < n; k = k + 1) {
    float gap = gpu[k] - dft((int32_t) k, x, p);
    if (gap < 0.0f) {
      gap = (float) (0.0 - (double) gap);
    }
    if (gap > 0.002f) {
      return (int8_t*) (intptr_t) 0;
    }
  }
  return gpu;
}

static int64_t plotPair(const char* signalName, const char* spectrumName, float* x, int64_t n, float* spectrum, int64_t shown, float signalHigh, float spectrumHigh) {
  float* pic = (int8_t*) (intptr_t) calloc(256 * 96, 4);
  drawStems(pic, 256, 96, x, n, (float) (0.0 - (double) signalHigh), signalHigh);
  savePicture(signalName, pic, 256, 96);
  drawStems(pic, 256, 96, spectrum, shown, 0.0f, spectrumHigh);
  savePicture(spectrumName, pic, 256, 96);
  return 0;
}

static int64_t scaleBy(float* values, int64_t n, float factor) {
  for (int64_t k = 0; k < n; k = k + 1) {
    values[k] = values[k] * factor;
  }
  return 0;
}

static int64_t spectra1D(int64_t device, const char* source) {
  float* x = (int8_t*) (intptr_t) calloc(128, 4);
  twoSines(x, 128);
  float* s1 = transform1D(device, source, x, 128);
  if ((int64_t) (intptr_t) s1 == 0) {
    return 1;
  }
  plotPair("dsp-1-signal.pgm", "dsp-1-spectrum.pgm", x, 128, s1, 64, 1.6f, 1.2f);
  float* y = (int8_t*) (intptr_t) calloc(64, 4);
  tooFast(y, 64);
  float* s2 = transform1D(device, source, y, 64);
  if ((int64_t) (intptr_t) s2 == 0) {
    return 1;
  }
  plotPair("dsp-2-alias-signal.pgm", "dsp-2-alias-spectrum.pgm", y, 64, s2, 33, 1.2f, 1.2f);
  float* img = (int8_t*) (intptr_t) calloc(4096, 4);
  fillScene(img);
  float* row = (int8_t*) (intptr_t) calloc(64, 4);
  for (int64_t c = 0; c < 64; c = c + 1) {
    row[c] = img[44 * 64 + c] - 0.5f;
  }
  float* s3 = transform1D(device, source, row, 64);
  if ((int64_t) (intptr_t) s3 == 0) {
    return 1;
  }
  plotPair("dsp-3-row-signal.pgm", "dsp-3-row-spectrum.pgm", row, 64, s3, 33, 0.6f, 0.35f);
  float* f = (int8_t*) (intptr_t) calloc(64, 4);
  blurFilter(f, 64);
  float* blurResponse = transform1D(device, source, f, 64);
  if ((int64_t) (intptr_t) blurResponse == 0) {
    return 1;
  }
  scaleBy(blurResponse, 33, 32.0f);
  plotPair("dsp-4-blur-kernel.pgm", "dsp-4-blur-response.pgm", f, 64, blurResponse, 33, 1.0f, 2.2f);
  edgeFilter(f, 64);
  float* edgeResponse = transform1D(device, source, f, 64);
  if ((int64_t) (intptr_t) edgeResponse == 0) {
    return 1;
  }
  scaleBy(edgeResponse, 33, 32.0f);
  plotPair("dsp-4-edge-kernel.pgm", "dsp-4-edge-response.pgm", f, 64, edgeResponse, 33, 1.0f, 2.2f);
  return 0;
}

static int64_t transform2D(int64_t device, const char* source, float* img, const char* name, const char* spectrumName) {
  int64_t pipeline = metalPipeline(device, source, "dft2d");
  if (pipeline == 0) {
    return 1;
  }
  float* p = (int8_t*) (intptr_t) calloc(2, 4);
  p[0] = 64.0f;
  p[1] = 400.0f;
  int64_t out = metalEmptyBuffer(device, 4096 * 4);
  if (metalDispatch(device, pipeline, 4096, metalBuffer(device, (int64_t) (intptr_t) img, 4096 * 4), metalBuffer(device, (int64_t) (intptr_t) p, 8), out, 0) == 0) {
    return 1;
  }
  float* gpu = (int8_t*) (intptr_t) metalContents(out);
  savePicture(name, img, 64, 64);
  savePicture(spectrumName, gpu, 64, 64);
  for (int64_t i = 0; i < 4096; i = i + 1) {
    float gap = gpu[i] - dft2d((int32_t) i, img, p);
    if (gap < 0.0f) {
      gap = (float) (0.0 - (double) gap);
    }
    if (gap > 0.01f) {
      return 1;
    }
  }
  return 0;
}

static int64_t spectra2D(int64_t device, const char* source) {
  float* img = (int8_t*) (intptr_t) calloc(4096, 4);
  stripes(img, 64, 6.0f, 0.0f);
  if (transform2D(device, source, img, "dsp-5-stripes.pgm", "dsp-5-stripes-spectrum.pgm") != 0) {
    return 13;
  }
  stripes(img, 64, 4.0f, 3.0f);
  if (transform2D(device, source, img, "dsp-6-diagonal.pgm", "dsp-6-diagonal-spectrum.pgm") != 0) {
    return 13;
  }
  fillScene(img);
  return transform2D(device, source, img, "dsp-7-scene.pgm", "dsp-7-scene-spectrum.pgm") * 13;
}

int main(void) {
  return (int) main_();
}
