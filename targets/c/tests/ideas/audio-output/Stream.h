#pragma once

#include <setjmp.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../audio/Sound.h"

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

typedef struct Stream Stream;

struct Stream {
  __typeof__(int64_t (*)(int64_t, float*, int64_t, int64_t)) fill;
  int64_t state;
  int64_t channels;
  int64_t rate;
  int64_t limit;
  float volume;
  int64_t* progress;
};

static Stream* Stream_new(__typeof__(int64_t (*)(int64_t, float*, int64_t, int64_t)) fill, int64_t state, int64_t channels, int64_t rate, int64_t limit, float volume, int64_t* progress);
static bool Stream_equal(Stream* a, Stream* b);
static uint64_t Stream_hash(Stream* a);

static Stream* streamNew(__typeof__(int64_t (*)(int64_t, float*, int64_t, int64_t)) fill, int64_t state, int64_t channels, int64_t rate, float seconds, float volume);
static int64_t streamBufferFrames(void);
static int64_t streamBufferCount(void);
static int64_t streamFeed(Stream* stream, float* buf, int64_t frames);
static int64_t streamWanted(Stream* stream, int64_t first, int64_t frames);
static int64_t streamFinish(Stream* stream);
static int64_t streamScale(float* buf, int64_t count, float volume);
static bool streamFinished(Stream* stream);
static int64_t soundFill(int64_t state, float* buf, int64_t frames, int64_t first);
static Stream* soundStream(Sound* s, float volume);

static Stream* Stream_new(__typeof__(int64_t (*)(int64_t, float*, int64_t, int64_t)) fill, int64_t state, int64_t channels, int64_t rate, int64_t limit, float volume, int64_t* progress) {
  Stream* made = gcAlloc(sizeof(Stream));
  made->fill = fill;
  made->state = state;
  made->channels = channels;
  made->rate = rate;
  made->limit = limit;
  made->volume = volume;
  made->progress = progress;
  return made;
}

static bool Stream_equal(Stream* a, Stream* b) {
  if (a == b || a == NULL || b == NULL) {
    return a == b;
  }

  bool same = true;
  same = same && a->fill == b->fill;
  same = same && a->state == b->state;
  same = same && a->channels == b->channels;
  same = same && a->rate == b->rate;
  same = same && a->limit == b->limit;
  same = same && a->volume == b->volume;
  same = same && a->progress == b->progress;
  return same;
}

static uint64_t Stream_hash(Stream* a) {
  uint64_t hash = 17;
  hash = hash * 31 + hashBytes(&a->fill, sizeof a->fill);
  hash = hash * 31 + (uint64_t) a->state;
  hash = hash * 31 + (uint64_t) a->channels;
  hash = hash * 31 + (uint64_t) a->rate;
  hash = hash * 31 + (uint64_t) a->limit;
  hash = hash * 31 + hashBytes(&a->volume, sizeof a->volume);
  hash = hash * 31 + hashBytes(&a->progress, sizeof a->progress);
  return hash;
}

static Stream* streamNew(__typeof__(int64_t (*)(int64_t, float*, int64_t, int64_t)) fill, int64_t state, int64_t channels, int64_t rate, float seconds, float volume) {
  int64_t limit = (int64_t) (seconds * (float) rate);
  int64_t* counters = (int8_t*) (intptr_t) calloc(4, 8);
  return Stream_new(fill, state, channels, rate, limit, volume, counters);
}

static int64_t streamBufferFrames(void) {
  return 2048;
}

static int64_t streamBufferCount(void) {
  return 3;
}

static int64_t streamFeed(Stream* stream, float* buf, int64_t frames) {
  int64_t first = (stream->progress)[0];
  int64_t wanted = streamWanted(stream, first, frames);
  if (wanted <= 0) {
    return streamFinish(stream);
  }
  __typeof__(int64_t (*)(int64_t, float*, int64_t, int64_t)) fill = stream->fill;
  int64_t made_ = fill(stream->state, buf, wanted, first);
  if (made_ <= 0) {
    return streamFinish(stream);
  }
  streamScale(buf, made_ * stream->channels, stream->volume);
  (stream->progress)[0] = first + made_;
  (stream->progress)[3] = (stream->progress)[3] + 1;
  return made_;
}

static int64_t streamWanted(Stream* stream, int64_t first, int64_t frames) {
  if (stream->limit > 0 && stream->limit - first < frames) {
    return stream->limit - first;
  }
  return frames;
}

static int64_t streamFinish(Stream* stream) {
  (stream->progress)[1] = 1;
  return 0;
}

static int64_t streamScale(float* buf, int64_t count, float volume) {
  if (volume == 1.0f) {
    return 0;
  }
  for (int64_t i = 0; i < count; i = i + 1) {
    buf[i] = buf[i] * volume;
  }
  return 0;
}

static bool streamFinished(Stream* stream) {
  return (stream->progress)[1] == 1;
}

static int64_t soundFill(int64_t state, float* buf, int64_t frames, int64_t first) {
  Sound** view = (int8_t*) (intptr_t) state;
  Sound* sound = view[0];
  int64_t made_ = frames;
  if (first + made_ > sound->count) {
    made_ = sound->count - first;
  }
  if (made_ <= 0) {
    return 0;
  }
  for (int64_t i = 0; i < made_ * sound->channels; i = i + 1) {
    buf[i] = (sound->samples)[first * sound->channels + i];
  }
  return made_;
}

static Stream* soundStream(Sound* s, float volume) {
  Sound** slot = (int8_t*) (intptr_t) calloc(1, 32);
  slot[0] = s;
  __typeof__(int64_t (*)(int64_t, float*, int64_t, int64_t)) fill = soundFill;
  return streamNew(fill, (int64_t) (intptr_t) slot, s->channels, s->rate, 0.0f, volume);
}
