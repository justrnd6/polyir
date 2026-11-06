#pragma once

#import <setjmp.h>
#import <stdbool.h>
#import <stdint.h>
#import <stdio.h>
#import <stdlib.h>
#import <string.h>

#import "../../../core/tests/programs/Check.h"
#import "../Stream.h"
#import "../../audio/Sound.h"

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

static int64_t numbering(int64_t state, float* buf, int64_t frames, int64_t first);
static int64_t shortSound(int64_t state, float* buf, int64_t frames, int64_t first);
static Stream* counting(float seconds, float volume);
static double checkFeedAsksForConsecutiveFrames(void);
static double checkProgressCounts(void);
static double checkTimeLimitCutsLastBuffer(void);
static double checkFinishedOnlyAfterTheEnd(void);
static double checkFillFunctionEndsTheSound(void);
static double checkVolumeScales(void);
static double checkSilentVolume(void);
static double checkSoundStreamPlaysTheSound(void);

static int64_t numbering(int64_t state, float* buf, int64_t frames, int64_t first) {
  int64_t* calls = (int8_t*) (intptr_t) state;
  calls[0] = calls[0] + 1;
  for (int64_t i = 0; i < frames; i = i + 1) {
    buf[i * 2] = (float) (first + i);
    buf[i * 2 + 1] = (float) (first + i);
  }
  return frames;
}

static int64_t shortSound(int64_t state, float* buf, int64_t frames, int64_t first) {
  int64_t made_ = frames;
  if (first + made_ > 5) {
    made_ = 5 - first;
  }
  if (made_ < 0) {
    made_ = 0;
  }
  for (int64_t i = 0; i < made_; i = i + 1) {
    buf[i] = 1.0f;
  }
  return made_;
}

static Stream* counting(float seconds, float volume) {
  __typeof__(int64_t (*)(int64_t, float*, int64_t, int64_t)) fill = numbering;
  return streamNew(fill, calloc(1, 8), 2, 1000, seconds, volume);
}

static double checkFeedAsksForConsecutiveFrames(void) {
  Stream* stream = counting(0.0f, 1.0f);
  float* buf = (int8_t*) (intptr_t) calloc(8, 4);
  int64_t first = streamFeed(stream, buf, 4);
  int64_t second = streamFeed(stream, buf, 4);
  return offInt(first, 4) + offInt(second, 4) + off(buf[0], 4.0f) + off(buf[1], 4.0f) + off(buf[6], 7.0f);
}

static double checkProgressCounts(void) {
  Stream* stream = counting(0.0f, 1.0f);
  float* buf = (int8_t*) (intptr_t) calloc(8, 4);
  streamFeed(stream, buf, 4);
  streamFeed(stream, buf, 3);
  return offInt((stream->progress)[0], 7) + offInt((stream->progress)[3], 2);
}

static double checkTimeLimitCutsLastBuffer(void) {
  Stream* stream = counting(0.01f, 1.0f);
  float* buf = (int8_t*) (intptr_t) calloc(8, 4);
  int64_t a = streamFeed(stream, buf, 4);
  int64_t b = streamFeed(stream, buf, 4);
  int64_t c = streamFeed(stream, buf, 4);
  int64_t d = streamFeed(stream, buf, 4);
  return offInt(a, 4) + offInt(b, 4) + offInt(c, 2) + offInt(d, 0);
}

static double checkFinishedOnlyAfterTheEnd(void) {
  Stream* stream = counting(0.004f, 1.0f);
  float* buf = (int8_t*) (intptr_t) calloc(8, 4);
  int64_t wrong = 0;
  if (streamFinished(stream)) {
    wrong = wrong + 1;
  }
  streamFeed(stream, buf, 4);
  if (streamFinished(stream)) {
    wrong = wrong + 1;
  }
  streamFeed(stream, buf, 4);
  if (streamFinished(stream) == false) {
    wrong = wrong + 1;
  }
  return offInt(wrong, 0);
}

static double checkFillFunctionEndsTheSound(void) {
  __typeof__(int64_t (*)(int64_t, float*, int64_t, int64_t)) fill = shortSound;
  Stream* stream = streamNew(fill, 0, 1, 1000, 0.0f, 1.0f);
  float* buf = (int8_t*) (intptr_t) calloc(8, 4);
  int64_t a = streamFeed(stream, buf, 4);
  int64_t b = streamFeed(stream, buf, 4);
  int64_t c = streamFeed(stream, buf, 4);
  return offInt(a, 4) + offInt(b, 1) + offInt(c, 0) + offInt((stream->progress)[1], 1);
}

static double checkVolumeScales(void) {
  Stream* stream = counting(0.0f, 0.5f);
  float* buf = (int8_t*) (intptr_t) calloc(8, 4);
  streamFeed(stream, buf, 4);
  return off(buf[6], 1.5f) + off(buf[7], 1.5f);
}

static double checkSilentVolume(void) {
  Stream* stream = counting(0.0f, 0.0f);
  float* buf = (int8_t*) (intptr_t) calloc(8, 4);
  streamFeed(stream, buf, 4);
  return off(buf[6], 0.0f);
}

static double checkSoundStreamPlaysTheSound(void) {
  Sound* s = soundNewChannels(0.005f, 1000, 2);
  for (int64_t i = 0; i < 5; i = i + 1) {
    (s->samples)[i * 2] = (float) (i + 1);
    (s->samples)[i * 2 + 1] = (float) (i + 1) * 10.0f;
  }
  Stream* stream = soundStream(s, 1.0f);
  float* buf = (int8_t*) (intptr_t) calloc(8, 4);
  int64_t a = streamFeed(stream, buf, 4);
  double firstBuffer = off(buf[0], 1.0f) + off(buf[1], 10.0f) + off(buf[6], 4.0f) + off(buf[7], 40.0f);
  int64_t b = streamFeed(stream, buf, 4);
  double secondBuffer = off(buf[0], 5.0f) + off(buf[1], 50.0f);
  int64_t c = streamFeed(stream, buf, 4);
  return offInt(stream->channels, 2) + offInt(stream->rate, 1000) + offInt(a, 4) + offInt(b, 1) + offInt(c, 0) + firstBuffer + secondBuffer;
}
