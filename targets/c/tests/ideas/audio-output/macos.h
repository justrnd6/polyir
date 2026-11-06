#pragma once

#include <setjmp.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "Stream.h"

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

int32_t AudioQueueNewOutput(int64_t format, int64_t callback, int64_t user, int64_t runLoop, int64_t runLoopMode, int64_t flags, int64_t out);
int32_t AudioQueueAllocateBuffer(int64_t queue, int64_t bytes, int64_t out);
int32_t AudioQueueEnqueueBuffer(int64_t queue, int64_t buf, int64_t packets, int64_t descriptions);
int32_t AudioQueueStart(int64_t queue, int64_t startTime);
int32_t AudioQueueStop(int64_t queue, int64_t immediately);
int32_t AudioQueueDispose(int64_t queue, int64_t immediately);
int32_t usleep(int64_t microseconds);

static int64_t audioFormat(int64_t rate, int64_t channels);
static int64_t audioRefill(int64_t user, int64_t queue, int64_t buf);
static int64_t audioFillAndQueue(Stream* stream, int64_t queue, int64_t buf);
static int64_t audioPrime(Stream* stream, int64_t queue);
static int64_t audioStreamRun(Stream* stream);
static int64_t audioStreamStart(Stream* stream, int64_t queue);
static int64_t audioWait(Stream* stream);

static int64_t audioFormat(int64_t rate, int64_t channels) {
  int64_t block = calloc(5, 8);
  double* rateField = (int8_t*) (intptr_t) block;
  rateField[0] = (double) rate;
  int32_t* fields = (int8_t*) (intptr_t) block;
  fields[2] = 1819304813;
  fields[3] = 9;
  fields[4] = (int32_t) (4 * channels);
  fields[5] = 1;
  fields[6] = (int32_t) (4 * channels);
  fields[7] = (int32_t) channels;
  fields[8] = 32;
  return block;
}

static int64_t audioRefill(int64_t user, int64_t queue, int64_t buf) {
  Stream** view = (int8_t*) (intptr_t) user;
  Stream* stream = view[0];
  (stream->progress)[2] = (stream->progress)[2] - 1;
  if (streamFinished(stream)) {
    return 0;
  }
  return audioFillAndQueue(stream, queue, buf);
}

static int64_t audioFillAndQueue(Stream* stream, int64_t queue, int64_t buf) {
  int64_t* words = (int8_t*) (intptr_t) buf;
  float* memory = (int8_t*) (intptr_t) words[1];
  int64_t made_ = streamFeed(stream, memory, streamBufferFrames());
  if (made_ == 0) {
    return 0;
  }
  int32_t* sizes = (int8_t*) (intptr_t) buf;
  sizes[4] = (int32_t) (made_ * stream->channels * 4);
  (stream->progress)[2] = (stream->progress)[2] + 1;
  AudioQueueEnqueueBuffer(queue, buf, 0, 0);
  return made_;
}

static int64_t audioPrime(Stream* stream, int64_t queue) {
  int64_t* slot = (int8_t*) (intptr_t) calloc(1, 8);
  int32_t status = AudioQueueAllocateBuffer(queue, streamBufferFrames() * stream->channels * 4, (int64_t) (intptr_t) slot);
  if (status != 0) {
    return 0;
  }
  audioFillAndQueue(stream, queue, slot[0]);
  return slot[0];
}

static int64_t audioStreamRun(Stream* stream) {
  int64_t* queueSlot = (int8_t*) (intptr_t) calloc(1, 8);
  Stream** streamSlot = (int8_t*) (intptr_t) calloc(1, 64);
  streamSlot[0] = stream;
  __typeof__(int64_t (*)(int64_t, int64_t, int64_t)) refill = audioRefill;
  int64_t format = audioFormat(stream->rate, stream->channels);
  int32_t status = AudioQueueNewOutput(format, (int64_t) (intptr_t) refill, (int64_t) (intptr_t) streamSlot, 0, 0, 0, (int64_t) (intptr_t) queueSlot);
  if (status != 0) {
    return (int64_t) status;
  }
  return audioStreamStart(stream, queueSlot[0]);
}

static int64_t audioStreamStart(Stream* stream, int64_t queue) {
  for (int64_t k = 0; k < streamBufferCount(); k = k + 1) {
    audioPrime(stream, queue);
  }
  int64_t code = (int64_t) AudioQueueStart(queue, 0);
  if (code == 0) {
    audioWait(stream);
  }
  AudioQueueStop(queue, 1);
  AudioQueueDispose(queue, 1);
  return code;
}

static int64_t audioWait(Stream* stream) {
  for (int64_t nap = 0; nap < 720000; nap = nap + 1) {
    if (streamFinished(stream) && (stream->progress)[2] <= 0) {
      return 0;
    }
    usleep(5000);
  }
  return 1;
}
