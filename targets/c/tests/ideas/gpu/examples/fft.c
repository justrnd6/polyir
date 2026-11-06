#include <math.h>
#include <setjmp.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../audio/Fft.h"
#include "../../audio/Sound.h"
#include "../../audio/Wave.h"
#include "../FftGpu.h"
#include "../Metal.h"
#include "shared/picture.h"
#include "shared/print.h"
#include "shared/spectrum.h"

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

#ifndef POLYIR_LIST_H
#define POLYIR_LIST_H

// A list: its items' size, how many it holds, and its items. `LIST_AT(list, i, T)` is item `i`, to read or to write.
typedef struct List {
  size_t size;
  int64_t count;
  int64_t capacity;
  char* items;
} List;

static List* listNew(size_t size) {
  List* list = gcAlloc(sizeof(List));
  list->size = size;

  return list;
}

static void* listAt(List* list, int64_t at) {
  if (at < 0 || at >= list->count) {
    checkFailed(-1, "index out of range");
  }

  return list->items + at * list->size;
}

static void listPush(List* list, const void* item) {
  if (list->count == list->capacity) {
    list->capacity = list->capacity == 0 ? 8 : list->capacity * 2;
    char* grown = gcAlloc(list->capacity * list->size);
    memcpy(grown, list->items, list->count * list->size);
    list->items = grown;
  }

  memcpy(list->items + list->count * list->size, item, list->size);
  list->count++;
}

static List* listOf(size_t size, int64_t count, const void* items) {
  List* list = listNew(size);

  for (int64_t i = 0; i < count; i++) {
    listPush(list, (const char*) items + i * size);
  }

  return list;
}

// A list of `count` items, each `item`.
static List* listFilled(size_t size, int64_t count, const void* item) {
  List* list = listNew(size);

  for (int64_t i = 0; i < count; i++) {
    listPush(list, item);
  }

  return list;
}

static List* listCopy(List* from) {
  return listOf(from->size, from->count, from->items);
}

static List* listPushed(List* list, const void* item) {
  listPush(list, item);

  return list;
}

static List* listWith(List* from, const void* item) {
  return listPushed(listCopy(from), item);
}

static List* listConcat(List* a, List* b) {
  List* out = listCopy(a);

  for (int64_t i = 0; i < b->count; i++) {
    listPush(out, b->items + i * b->size);
  }

  return out;
}

static List* listTruncate(List* list, int64_t count) {
  list->count = count < 0 ? 0 : count < list->count ? count : list->count;

  return list;
}

static bool listHas(List* list, const void* item, SameFn same) {
  for (int64_t i = 0; i < list->count; i++) {
    if ((same != NULL ? same : sameBytes)(list->items + i * list->size, item, list->size)) {
      return true;
    }
  }

  return false;
}

// Whether two lists hold the same items, compared by `same` (by their bytes when it is NULL).
static bool listSame(List* a, List* b, SameFn same) {
  if (a == b || a->count != b->count) {
    return a == b;
  }

  for (int64_t i = 0; i < a->count; i++) {
    if (!(same != NULL ? same : sameBytes)(a->items + i * a->size, b->items + i * b->size, a->size)) {
      return false;
    }
  }

  return true;
}

#define LIST_AT(list, at, T) (*(T*) listAt((list), (at)))
#define LIST_OF(T, ...) listOf(sizeof(T), sizeof((T[]){__VA_ARGS__}) / sizeof(T), (T[]){__VA_ARGS__})
#endif

static float* magnitudes(int64_t device, const char* source, int64_t data, int64_t total);
static float* plainTransform(int64_t device, const char* source, float* signal_, int64_t n);
static float* asComplex(float* signal_, int64_t n);
static int64_t near(float a, float b, float tolerance);
static int64_t main_(void);
static int64_t correctness(int64_t device, const char* source, int64_t stage);
static float* fftOnCpu(float* signal_, int64_t n);
static float* fftBatchOnCpu(float* input, int64_t n, int64_t batch);
static int64_t speed(int64_t device, const char* source, int64_t stage);
static int64_t timeBatch(int64_t device, int64_t stage, int64_t n, int64_t batch);
static int64_t timeOne(int64_t device, const char* source, int64_t stage, int64_t n);
static Sound* makeMelodySignal(void);
static int64_t addNote(Sound* s, float frequency, float start, float length);
static int64_t spectrogram(int64_t device, const char* source, int64_t stage);
static int64_t checkSlices(int64_t done, Sound* signal_, int64_t total, int64_t hop);
static float cosOf(float x);
static float sqrtOf(float x);

static float* magnitudes(int64_t device, const char* source, int64_t data, int64_t total) {
  int64_t pipeline = metalPipeline(device, source, "fftMagnitude");
  if (pipeline == 0) {
    return (int8_t*) (intptr_t) 0;
  }
  List* size = listFilled(sizeof(int32_t), 1, (int32_t[]){0});
  LIST_AT(size, 0, int32_t) = (int32_t) total;
  int64_t out = metalEmptyBuffer(device, total * 4);
  if (metalDispatch(device, pipeline, total, data, metalBuffer(device, (int64_t) size, 4), out, 0) == 0) {
    return (int8_t*) (intptr_t) 0;
  }
  return (int8_t*) (intptr_t) metalContents(out);
}

static float* plainTransform(int64_t device, const char* source, float* signal_, int64_t n) {
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

static float* asComplex(float* signal_, int64_t n) {
  float* c = (int8_t*) (intptr_t) calloc(n * 2, 4);
  for (int64_t i = 0; i < n; i = i + 1) {
    c[2 * i] = signal_[i];
  }
  return c;
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

static int64_t main_(void) {
  int64_t device = metalDevice();
  if (device == 0) {
    return 10;
  }
  const char* source = metalReadFile("fft.metal");
  int64_t stage = metalPipeline(device, source, "fftStage");
  if (stage == 0) {
    return 11;
  }
  int64_t code = correctness(device, source, stage);
  if (code != 0) {
    return code;
  }
  speed(device, source, stage);
  return spectrogram(device, source, stage);
}

static int64_t correctness(int64_t device, const char* source, int64_t stage) {
  int64_t n = 2048;
  Sound* tones = soundNew(0.25f, 8192);
  float* data = tones->samples;
  for (int64_t i = 0; i < n; i = i + 1) {
    float t = (float) i / 8192.0f;
    data[i] = sine((float) (200.0 * (double) t)) + sine((float) (1000.0 * (double) t));
  }
  float* input = asComplex(data, n);
  int64_t a = metalBuffer(device, (int64_t) (intptr_t) input, n * 8);
  int64_t b = metalEmptyBuffer(device, n * 8);
  int64_t done = fftStagesGpu(device, stage, a, b, n, 1, 1);
  if (done == 0) {
    return 12;
  }
  float* fast = magnitudes(device, source, done, n);
  float* plain = plainTransform(device, source, data, n);
  if ((int64_t) (intptr_t) fast == 0 || (int64_t) (intptr_t) plain == 0) {
    return 12;
  }
  if (near(fast[50], 1.0f, 0.01f) == 0 || near(fast[250], 1.0f, 0.01f) == 0) {
    return 13;
  }
  for (int64_t k = 0; k < n; k = k + 1) {
    if (near(fast[k], plain[k], 0.002f) == 0) {
      return 13;
    }
  }
  int64_t c = metalEmptyBuffer(device, n * 8);
  int64_t back = fftStagesGpu(device, stage, done, c, n, 1, 0 - 1);
  if (back == 0) {
    return 12;
  }
  float* restored = (int8_t*) (intptr_t) metalContents(back);
  for (int64_t i = 0; i < n; i = i + 1) {
    if (near(restored[2 * i] / (float) n, data[i], 0.002f) == 0) {
      return 14;
    }
  }
  return 0;
}

static float* fftOnCpu(float* signal_, int64_t n) {
  return fftBatchOnCpu(asComplex(signal_, n), n, 1);
}

static float* fftBatchOnCpu(float* input, int64_t n, int64_t batch) {
  float* from = input;
  float* to = (int8_t*) (intptr_t) calloc(n * batch * 2, 4);
  List* settings = listFilled(sizeof(int32_t), 4, (int32_t[]){0});
  LIST_AT(settings, 0, int32_t) = (int32_t) n;
  LIST_AT(settings, 3, int32_t) = 1;
  int64_t size = n;
  int64_t stride = 1;
  for (int64_t pass = 0; pass < 30; pass = pass + 1) {
    if (size > 1) {
      LIST_AT(settings, 1, int32_t) = (int32_t) size;
      LIST_AT(settings, 2, int32_t) = (int32_t) stride;
      for (int64_t t = 0; t < batch * (n / 2); t = t + 1) {
        fftStage((int32_t) t, from, (int8_t*) (intptr_t) (int64_t) settings, to);
      }
      float* swap = from;
      from = to;
      to = swap;
      size = size / 2;
      stride = stride * 2;
    }
  }
  return from;
}

static int64_t speed(int64_t device, const char* source, int64_t stage) {
  say("N          plain transform (GPU)   FFT (one CPU core)   FFT (GPU)");
  newline();
  for (int64_t round = 0; round < 5; round = round + 1) {
    int64_t n = 1024;
    for (int64_t k = 0; k < round; k = k + 1) {
      n = n * 4;
    }
    timeOne(device, source, stage, n);
  }
  timeBatch(device, stage, 4096, 2048);
  return 0;
}

static int64_t timeBatch(int64_t device, int64_t stage, int64_t n, int64_t batch) {
  float* input = (int8_t*) (intptr_t) calloc(n * batch * 2, 4);
  for (int64_t i = 0; i < n * batch; i = i + 1) {
    input[2 * i] = sine((float) (11.0 * (double) (float) (i % n) / (double) (float) n)) + (float) (0.3 * (double) sine((float) (97.0 * (double) (float) (i % n) / (double) (float) n)));
  }
  say("batch of ");
  sayNumber(batch);
  say(" transforms of ");
  sayNumber(n);
  say(" numbers:  one CPU core ");
  int64_t cpuStart = wallMs();
  fftBatchOnCpu(input, n, batch);
  sayNumber(wallMs() - cpuStart);
  say(" ms,  GPU ");
  int64_t gpuStart = wallMs();
  int64_t a = metalBuffer(device, (int64_t) (intptr_t) input, n * batch * 8);
  int64_t b = metalEmptyBuffer(device, n * batch * 8);
  fftStagesGpu(device, stage, a, b, n, batch, 1);
  sayNumber(wallMs() - gpuStart);
  say(" ms");
  newline();
  return 0;
}

static int64_t timeOne(int64_t device, const char* source, int64_t stage, int64_t n) {
  float* signal_ = (int8_t*) (intptr_t) calloc(n, 4);
  for (int64_t i = 0; i < n; i = i + 1) {
    signal_[i] = sine((float) (50.0 * (double) (float) i / (double) (float) n)) + (float) (0.5 * (double) sine((float) (317.0 * (double) (float) i / (double) (float) n)));
  }
  sayRight(n, 7);
  if (n <= 16384) {
    int64_t start = wallMs();
    plainTransform(device, source, signal_, n);
    say("          ");
    sayRight(wallMs() - start, 12);
  } else {
    say("                         -");
  }
  int64_t cpuStart = wallMs();
  fftOnCpu(signal_, n);
  say("          ");
  sayRight(wallMs() - cpuStart, 8);
  float* input = asComplex(signal_, n);
  int64_t gpuStart = wallMs();
  int64_t a = metalBuffer(device, (int64_t) (intptr_t) input, n * 8);
  int64_t b = metalEmptyBuffer(device, n * 8);
  fftStagesGpu(device, stage, a, b, n, 1, 1);
  say("          ");
  sayRight(wallMs() - gpuStart, 6);
  newline();
  return 0;
}

static Sound* makeMelodySignal(void) {
  int64_t rate = 8000;
  Sound* s = soundNew(4.5f, rate);
  float* data = s->samples;
  for (int64_t i = 0; i < 8000; i = i + 1) {
    float t = (float) i / 8000.0f;
    data[i] = (float) (0.7 * (double) sine((float) (200.0 * (double) t + 800.0 * (double) t * (double) t)));
  }
  List* keys = listFilled(sizeof(float), 8, (float[]){0.0f});
  LIST_AT(keys, 0, float) = 60.0f;
  LIST_AT(keys, 1, float) = 62.0f;
  LIST_AT(keys, 2, float) = 64.0f;
  LIST_AT(keys, 3, float) = 65.0f;
  LIST_AT(keys, 4, float) = 67.0f;
  LIST_AT(keys, 5, float) = 69.0f;
  LIST_AT(keys, 6, float) = 71.0f;
  LIST_AT(keys, 7, float) = 72.0f;
  for (int64_t k = 0; k < 8; k = k + 1) {
    addNote(s, noteFrequency(LIST_AT(keys, k, float)), (float) (1.0 + (double) ((float) k * 0.3f)), 0.3f);
  }
  addNote(s, noteFrequency(60.0f), 3.4f, 1.1f);
  addNote(s, noteFrequency(64.0f), 3.4f, 1.1f);
  addNote(s, noteFrequency(67.0f), 3.4f, 1.1f);
  addNote(s, noteFrequency(72.0f), 3.4f, 1.1f);
  return s;
}

static int64_t addNote(Sound* s, float frequency, float start, float length) {
  Sound_tone(s, 0, frequency, 0.25f, start, length);
  Sound_tone(s, 0, (float) (2.0 * (double) frequency), 0.12f, start, length);
  Sound_tone(s, 0, (float) (3.0 * (double) frequency), 0.06f, start, length);
  return 0;
}

static int64_t spectrogram(int64_t device, const char* source, int64_t stage) {
  Sound* signal_ = makeMelodySignal();
  int64_t total = 512;
  int64_t hop = 128;
  int64_t frames = (signal_->count - total) / hop + 1;
  int64_t rows = 128;
  int64_t framing = metalPipeline(device, source, "fftFrames");
  int64_t pixels = metalPipeline(device, source, "spectrogramPixel");
  if (framing == 0 || pixels == 0) {
    return 12;
  }
  List* settings = listFilled(sizeof(int32_t), 3, (int32_t[]){0});
  LIST_AT(settings, 0, int32_t) = (int32_t) total;
  LIST_AT(settings, 1, int32_t) = (int32_t) hop;
  int64_t a = metalEmptyBuffer(device, frames * total * 8);
  int64_t b = metalEmptyBuffer(device, frames * total * 8);
  int64_t scratch = metalEmptyBuffer(device, frames * total * 4);
  if (metalDispatch(device, framing, frames * total, metalBuffer(device, (int64_t) (intptr_t) (signal_->samples), signal_->count * 4), metalBuffer(device, (int64_t) settings, 12), a, scratch) == 0) {
    return 12;
  }
  int64_t done = fftStagesGpu(device, stage, a, b, total, frames, 1);
  if (done == 0) {
    return 12;
  }
  List* picture = listFilled(sizeof(int32_t), 3, (int32_t[]){0});
  LIST_AT(picture, 0, int32_t) = (int32_t) total;
  LIST_AT(picture, 1, int32_t) = (int32_t) frames;
  LIST_AT(picture, 2, int32_t) = (int32_t) rows;
  int64_t out = metalEmptyBuffer(device, frames * rows * 4);
  if (metalDispatch(device, pixels, frames * rows, done, metalBuffer(device, (int64_t) picture, 12), out, 0) == 0) {
    return 12;
  }
  float* image = (int8_t*) (intptr_t) metalContents(out);
  savePicture("spectrogram.pgm", image, frames, rows);
  return checkSlices(done, signal_, total, hop);
}

static int64_t checkSlices(int64_t done, Sound* signal_, int64_t total, int64_t hop) {
  float* result = (int8_t*) (intptr_t) metalContents(done);
  float* slice = (int8_t*) (intptr_t) calloc(total, 4);
  float* size = (int8_t*) (intptr_t) calloc(2, 4);
  size[0] = (float) total;
  for (int64_t pick = 0; pick < 3; pick = pick + 1) {
    int64_t frame = 10 + 95 * pick;
    for (int64_t k = 0; k < total; k = k + 1) {
      float window = (float) (0.5 - 0.5 * (double) cosOf((float) (6.2831853 * (double) (float) k / (double) (float) (total - 1))));
      slice[k] = (signal_->samples)[frame * hop + k] * window;
    }
    for (int64_t bin = 0; bin < 256; bin = bin + 1) {
      float re = result[frame * total * 2 + 2 * bin];
      float im = result[frame * total * 2 + 2 * bin + 1];
      float fast = (float) (2.0 * (double) sqrtOf(re * re + im * im) / (double) (float) total);
      if (near(fast, dft((int32_t) bin, slice, size), 0.01f) == 0) {
        return 15;
      }
    }
  }
  return 0;
}

static float cosOf(float x) {
  return cosf(x);
}

static float sqrtOf(float x) {
  return sqrtf(x);
}

int main(void) {
  return (int) main_();
}
