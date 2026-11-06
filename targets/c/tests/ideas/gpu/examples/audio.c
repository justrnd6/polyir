#include <setjmp.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../audio-output/Audio.h"
#include "../../audio/Sound.h"
#include "../../audio/Wave.h"
#include "../Metal.h"
#include "shared/audio.h"
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

static float* synthesize(int64_t device, const char* source, const char* name, float* settings, int64_t settingBytes, int64_t count);
static float* spectrumOf(int64_t device, const char* source, float* signal_, int64_t n);
static int64_t setSynth(float* p, float rate, int64_t partials, float total, float attack, float release, float volume);
static int64_t setPartial(float* p, int64_t which, float frequency, float amplitude);
static int64_t near(float a, float b, float tolerance);
static int64_t plotPair(const char* nameA, const char* nameB, float* signal_, int64_t shown, float* spectrum, int64_t bins, float signalHigh, float spectrumHigh, int64_t continuous);
static int64_t main_(void);
static int64_t analysis(int64_t device, const char* source);
static int64_t squareAndSums(int64_t device, const char* source);
static int64_t sound(int64_t device, const char* source);
static float* playableMelody(int64_t device, const char* source, float* notes, float* settings, int64_t count);
static int64_t writeMelody(float* notes);

static float* synthesize(int64_t device, const char* source, const char* name, float* settings, int64_t settingBytes, int64_t count) {
  int64_t pipeline = metalPipeline(device, source, name);
  if (pipeline == 0) {
    return (int8_t*) (intptr_t) 0;
  }
  int64_t out = metalEmptyBuffer(device, count * 4);
  if (metalDispatch(device, pipeline, count, metalBuffer(device, (int64_t) (intptr_t) settings, settingBytes), out, 0, 0) == 0) {
    return (int8_t*) (intptr_t) 0;
  }
  return (int8_t*) (intptr_t) metalContents(out);
}

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

static int64_t setSynth(float* p, float rate, int64_t partials, float total, float attack, float release, float volume) {
  p[0] = rate;
  p[1] = (float) partials;
  p[2] = attack;
  p[3] = release;
  p[4] = total;
  p[5] = 0.0f;
  p[6] = 0.0f;
  p[7] = volume;
  return 0;
}

static int64_t setPartial(float* p, int64_t which, float frequency, float amplitude) {
  p[8 + 2 * which] = frequency;
  p[9 + 2 * which] = amplitude;
  return 0;
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

static int64_t plotPair(const char* nameA, const char* nameB, float* signal_, int64_t shown, float* spectrum, int64_t bins, float signalHigh, float spectrumHigh, int64_t continuous) {
  float* pic = (int8_t*) (intptr_t) calloc(320 * 96, 4);
  if (continuous == 1) {
    drawWave(pic, 320, 96, signal_, shown, (float) (0.0 - (double) signalHigh), signalHigh);
  } else {
    drawStems(pic, 320, 96, signal_, shown, (float) (0.0 - (double) signalHigh), signalHigh);
  }
  savePicture(nameA, pic, 320, 96);
  drawStems(pic, 320, 96, spectrum, bins, 0.0f, spectrumHigh);
  savePicture(nameB, pic, 320, 96);
  return 0;
}

static int64_t main_(void) {
  int64_t device = metalDevice();
  if (device == 0) {
    return 10;
  }
  const char* source = metalReadFile("audio.metal");
  if (analysis(device, source) != 0) {
    return 13;
  }
  return sound(device, source);
}

static int64_t analysis(int64_t device, const char* source) {
  int64_t n = 2000;
  float* p = (int8_t*) (intptr_t) calloc(32, 4);
  setSynth(p, 8000.0f, 1, 0.25f, 0.0f, 0.0f, 1.0f);
  setPartial(p, 0, 400.0f, 1.0f);
  float* tone = synthesize(device, source, "synth", p, 128, n);
  if ((int64_t) (intptr_t) tone == 0) {
    return 1;
  }
  float* toneSpectrum = spectrumOf(device, source, tone, n);
  if (near(toneSpectrum[100], 1.0f, 0.02f) == 0 || near(toneSpectrum[125], 0.0f, 0.02f) == 0) {
    return 1;
  }
  plotPair("audio-1-sine.pgm", "audio-1-sine-spectrum.pgm", tone, 100, toneSpectrum, 200, 1.3f, 1.3f, 0);
  setSynth(p, 8000.0f, 3, 0.25f, 0.0f, 0.0f, 0.5f);
  setPartial(p, 0, 400.0f, 1.0f);
  setPartial(p, 1, 500.0f, 0.8f);
  setPartial(p, 2, 600.0f, 0.6f);
  float* chord = synthesize(device, source, "synth", p, 128, n);
  if ((int64_t) (intptr_t) chord == 0) {
    return 1;
  }
  float* chordSpectrum = spectrumOf(device, source, chord, n);
  if (near(chordSpectrum[100], 0.5f, 0.02f) == 0 || near(chordSpectrum[125], 0.4f, 0.02f) == 0 || near(chordSpectrum[150], 0.3f, 0.02f) == 0) {
    return 1;
  }
  plotPair("audio-2-chord.pgm", "audio-2-chord-spectrum.pgm", chord, 320, chordSpectrum, 200, 1.0f, 0.6f, 1);
  setSynth(p, 8000.0f, 2, 0.25f, 0.0f, 0.0f, 1.0f);
  setPartial(p, 0, 400.0f, 1.0f);
  setPartial(p, 1, 404.0f, 1.0f);
  float* beats = synthesize(device, source, "synth", p, 128, n);
  if ((int64_t) (intptr_t) beats == 0) {
    return 1;
  }
  float* beatSpectrum = spectrumOf(device, source, beats, n);
  if (near(beatSpectrum[100], 1.0f, 0.03f) == 0 || near(beatSpectrum[101], 1.0f, 0.03f) == 0) {
    return 1;
  }
  plotPair("audio-3-beats.pgm", "audio-3-beats-spectrum.pgm", beats, 2000, beatSpectrum, 200, 2.2f, 1.3f, 1);
  return squareAndSums(device, source);
}

static int64_t squareAndSums(int64_t device, const char* source) {
  int64_t n = 2000;
  float* p = (int8_t*) (intptr_t) calloc(8, 4);
  p[0] = 8000.0f;
  p[1] = 100.0f;
  float* square = synthesize(device, source, "squareWave", p, 32, n);
  if ((int64_t) (intptr_t) square == 0) {
    return 1;
  }
  float* squareSpectrum = spectrumOf(device, source, square, n);
  if (near(squareSpectrum[25], 1.2732f, 0.05f) == 0 || near(squareSpectrum[75], 0.4244f, 0.05f) == 0 || near(squareSpectrum[50], 0.0f, 0.05f) == 0) {
    return 1;
  }
  plotPair("audio-4-square.pgm", "audio-4-square-spectrum.pgm", square, 240, squareSpectrum, 200, 1.4f, 1.4f, 0);
  p[0] = 200.0f;
  p[1] = 0.0f;
  float* pic = (int8_t*) (intptr_t) calloc(320 * 96, 4);
  p[2] = 1.0f;
  drawWave(pic, 320, 96, synthesize(device, source, "partialSum", p, 32, 400), 400, (float) (0.0 - 1.5), 1.5f);
  savePicture("audio-5-square-1.pgm", pic, 320, 96);
  p[2] = 3.0f;
  drawWave(pic, 320, 96, synthesize(device, source, "partialSum", p, 32, 400), 400, (float) (0.0 - 1.5), 1.5f);
  savePicture("audio-5-square-3.pgm", pic, 320, 96);
  p[2] = 9.0f;
  float* nine = synthesize(device, source, "partialSum", p, 32, 400);
  drawWave(pic, 320, 96, nine, 400, (float) (0.0 - 1.5), 1.5f);
  savePicture("audio-5-square-9.pgm", pic, 320, 96);
  float highest = 0.0f;
  for (int64_t k = 0; k < 400; k = k + 1) {
    if (nine[k] > highest) {
      highest = nine[k];
    }
  }
  if (highest < 1.05f || highest > 1.25f) {
    return 1;
  }
  p[1] = 1.0f;
  drawWave(pic, 320, 96, synthesize(device, source, "partialSum", p, 32, 400), 400, (float) (0.0 - 1.5), 1.5f);
  savePicture("audio-6-sawtooth-9.pgm", pic, 320, 96);
  p[1] = 2.0f;
  drawWave(pic, 320, 96, synthesize(device, source, "partialSum", p, 32, 400), 400, (float) (0.0 - 1.5), 1.5f);
  savePicture("audio-6-triangle-9.pgm", pic, 320, 96);
  return 0;
}

static int64_t sound(int64_t device, const char* source) {
  int64_t rate = 44100;
  int64_t seconds = 1;
  float* p = (int8_t*) (intptr_t) calloc(32, 4);
  setSynth(p, 44100.0f, 3, 1.5f, 0.03f, 0.4f, 0.25f);
  setPartial(p, 0, noteFrequency(60.0f), 1.0f);
  setPartial(p, 1, noteFrequency(64.0f), 1.0f);
  setPartial(p, 2, noteFrequency(67.0f), 1.0f);
  int64_t chordCount = 66150;
  float* chord = synthesize(device, source, "synth", p, 128, chordCount);
  if ((int64_t) (intptr_t) chord == 0) {
    return 12;
  }
  Sound* chordSound = Sound_new(chord, chordCount, rate, 1);
  Sound_saveWav(chordSound, "audio-chord.wav");
  float* notes = (int8_t*) (intptr_t) calloc(48, 4);
  writeMelody(notes);
  float* melodySettings = (int8_t*) (intptr_t) calloc(4, 4);
  melodySettings[0] = 44100.0f;
  melodySettings[1] = 12.0f;
  melodySettings[2] = 0.12f;
  int64_t melodyCount = 149940;
  float* tune = playableMelody(device, source, notes, melodySettings, melodyCount);
  if ((int64_t) (intptr_t) tune == 0) {
    return 12;
  }
  for (int64_t i = 0; i < melodyCount; i = i + 997) {
    if (near(tune[i], melody((int32_t) i, notes, melodySettings), 0.01f) == 0) {
      return 14;
    }
  }
  Sound* tuneSound = Sound_new(tune, melodyCount, rate, 1);
  Sound_saveWav(tuneSound, "audio-melody.wav");
  float* shape = (int8_t*) (intptr_t) calloc(320 * 96, 4);
  drawWave(shape, 320, 96, tune, melodyCount, (float) (0.0 - 1.6), 1.6f);
  savePicture("audio-7-melody.pgm", shape, 320, 96);
  if (getenv("POLYIR_PLAY") != 0) {
    if (soundPlay(chordSound) != 0) {
      return 15;
    }
    if (soundPlay(tuneSound) != 0) {
      return 15;
    }
  }
  return 0;
}

static float* playableMelody(int64_t device, const char* source, float* notes, float* settings, int64_t count) {
  int64_t pipeline = metalPipeline(device, source, "melody");
  if (pipeline == 0) {
    return (int8_t*) (intptr_t) 0;
  }
  int64_t out = metalEmptyBuffer(device, count * 4);
  if (metalDispatch(device, pipeline, count, metalBuffer(device, (int64_t) (intptr_t) notes, 192), metalBuffer(device, (int64_t) (intptr_t) settings, 16), out, 0) == 0) {
    return (int8_t*) (intptr_t) 0;
  }
  return (int8_t*) (intptr_t) metalContents(out);
}

static int64_t writeMelody(float* notes) {
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
    notes[3 * k] = (float) k * 0.3f;
    notes[3 * k + 1] = 0.3f;
    notes[3 * k + 2] = noteFrequency(LIST_AT(keys, k, float));
  }
  List* chord = listFilled(sizeof(float), 4, (float[]){0.0f});
  LIST_AT(chord, 0, float) = 60.0f;
  LIST_AT(chord, 1, float) = 64.0f;
  LIST_AT(chord, 2, float) = 67.0f;
  LIST_AT(chord, 3, float) = 72.0f;
  for (int64_t k = 0; k < 4; k = k + 1) {
    notes[24 + 3 * k] = 2.4f;
    notes[25 + 3 * k] = 1.0f;
    notes[26 + 3 * k] = noteFrequency(LIST_AT(chord, k, float));
  }
  return 0;
}

int main(void) {
  return (int) main_();
}
