#import <math.h>
#import <setjmp.h>
#import <stdbool.h>
#import <stdint.h>
#import <stdio.h>
#import <stdlib.h>
#import <string.h>

#import "../../audio/Sound.h"
#import "../../audio/Wave.h"
#import "../Metal.h"
#import "../SoundGpu.h"
#import "shared/picture.h"
#import "shared/plot.h"
#import "shared/print.h"

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

static Sound* roomResponse(float seconds, int64_t rate);
static Sound* dryBeep(float seconds, int64_t rate);
static int64_t main_(void);
static int64_t compareFft(int64_t device, int64_t direct);
static int64_t compare(int64_t device, int64_t pipeline, float roomSeconds);
static int64_t save(int64_t device, int64_t pipeline);

static Sound* roomResponse(float seconds, int64_t rate) {
  Sound* room = soundNew(seconds, rate);
  float* data = room->samples;
  for (int64_t j = 0; j < room->count; j = j + 1) {
    float t = (float) j / (float) rate;
    data[j] = (float) (0.5 * (double) noise((int32_t) j) * (double) expf((float) (0.0 - 6.9 * (double) t / (double) seconds)));
  }
  data[0] = 1.0f;
  return room;
}

static Sound* dryBeep(float seconds, int64_t rate) {
  Sound* s = soundNew(seconds, rate);
  Sound_tone(s, 0, 440.0f, 0.8f, 0.0f, 0.1f);
  return s;
}

static int64_t main_(void) {
  int64_t device = metalDevice();
  if (device == 0) {
    return 10;
  }
  int64_t pipeline = metalPipeline(device, metalReadFile("convolve.metal"), "convolveSample");
  if (pipeline == 0) {
    return 11;
  }
  say("room response   output samples   CPU (ms)   GPU (ms)");
  newline();
  int64_t code = 0;
  for (int64_t round = 0; round < 3; round = round + 1) {
    if (code == 0) {
      code = compare(device, pipeline, (float) (0.1 + 0.2 * (double) (float) round));
    }
  }
  if (code != 0) {
    return code;
  }
  code = compareFft(device, pipeline);
  if (code != 0) {
    return code;
  }
  return save(device, pipeline);
}

static int64_t compareFft(int64_t device, int64_t direct) {
  int64_t rate = 44100;
  int64_t* pipelines = fftPipelines(device, metalReadFile("convolve.metal"));
  say("room response   direct sum (GPU)   through the FFT (GPU)");
  newline();
  for (int64_t round = 0; round < 4; round = round + 1) {
    float seconds = 0.5f;
    for (int64_t k = 0; k < round; k = k + 1) {
      seconds = seconds * 2.0f;
    }
    Sound* dry = dryBeep(1.0f, rate);
    Sound* room = roomResponse(seconds, rate);
    int64_t start = wallMs();
    Sound* viaDirect = soundConvolveGpu(device, direct, dry, room);
    int64_t directMs = wallMs() - start;
    int64_t fftStart = wallMs();
    Sound* viaFft = soundConvolveFft(device, pipelines, dry, room);
    int64_t fftMs = wallMs() - fftStart;
    if (viaDirect->count == 0 || viaFft->count == 0) {
      return 12;
    }
    say("   ");
    sayRight((int64_t) (seconds * 1000.0f), 5);
    say(" ms        ");
    sayRight(directMs, 6);
    say(" ms             ");
    sayRight(fftMs, 6);
    say(" ms");
    newline();
    float* a = viaDirect->samples;
    float* b = viaFft->samples;
    float loudest = 0.0f;
    for (int64_t i = 0; i < viaDirect->count; i = i + 1) {
      float level = a[i];
      if (level < 0.0f) {
        level = (float) (0.0 - (double) level);
      }
      if (level > loudest) {
        loudest = level;
      }
    }
    for (int64_t i = 0; i < viaDirect->count; i = i + 53) {
      float gap = a[i] - b[i];
      if (gap < 0.0f) {
        gap = (float) (0.0 - (double) gap);
      }
      if (gap > (float) (0.002 * (double) loudest)) {
        return 13;
      }
    }
  }
  return 0;
}

static int64_t compare(int64_t device, int64_t pipeline, float roomSeconds) {
  int64_t rate = 44100;
  Sound* dry = dryBeep(0.5f, rate);
  Sound* room = roomResponse(roomSeconds, rate);
  int64_t cpuStart = wallMs();
  Sound* viaCpu = Sound_convolve(dry, room);
  int64_t cpuMs = wallMs() - cpuStart;
  int64_t gpuStart = wallMs();
  Sound* viaGpu = soundConvolveGpu(device, pipeline, dry, room);
  int64_t gpuMs = wallMs() - gpuStart;
  if (viaGpu->count == 0) {
    return 12;
  }
  say("   ");
  sayNumber((int64_t) (roomSeconds * 1000.0f));
  say(" ms");
  say("         ");
  sayNumber(viaGpu->count);
  say("          ");
  sayNumber(cpuMs);
  say("          ");
  sayNumber(gpuMs);
  newline();
  float* a = viaCpu->samples;
  float* b = viaGpu->samples;
  for (int64_t i = 0; i < viaGpu->count; i = i + 37) {
    float gap = a[i] - b[i];
    if (gap < 0.0f) {
      gap = (float) (0.0 - (double) gap);
    }
    if (gap > 0.002f) {
      return 13;
    }
  }
  return 0;
}

static int64_t save(int64_t device, int64_t pipeline) {
  int64_t rate = 44100;
  Sound* dry = dryBeep(0.5f, rate);
  Sound* room = roomResponse(0.7f, rate);
  Sound* wet = soundConvolveGpu(device, pipeline, dry, room);
  if (wet->count == 0) {
    return 12;
  }
  Sound_saveWav(dry, "convolve-dry.wav");
  Sound_normalize(wet, 0.9f);
  Sound_saveWav(wet, "convolve-wet.wav");
  float* pic = (int8_t*) (intptr_t) calloc(320 * 96, 4);
  drawWave(pic, 320, 96, room->samples, room->count, (float) (0.0 - 1.1), 1.1f);
  savePicture("convolve-1-room.pgm", pic, 320, 96);
  Sound* dryLong = soundNew(Sound_seconds(wet), rate);
  Sound_add(dryLong, dry, 0.0f, 1.0f);
  drawWave(pic, 320, 96, dryLong->samples, dryLong->count, (float) (0.0 - 1.1), 1.1f);
  savePicture("convolve-2-dry.pgm", pic, 320, 96);
  drawWave(pic, 320, 96, wet->samples, wet->count, (float) (0.0 - 1.1), 1.1f);
  savePicture("convolve-3-wet.pgm", pic, 320, 96);
  return 0;
}

int main(void) {
  return (int) main_();
}
