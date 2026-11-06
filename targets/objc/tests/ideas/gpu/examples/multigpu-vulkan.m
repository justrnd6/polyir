#import <setjmp.h>
#import <stdbool.h>
#import <stdint.h>
#import <stdio.h>
#import <stdlib.h>
#import <string.h>

#import "../Vulkan.h"
#import "shared/classics.h"
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

static int64_t startWorkers(int64_t instance, int64_t* workers);
static int64_t* pipelinesFor(int64_t* workers, int64_t count, const char* name);
static int64_t allBuilt(int64_t* pipes, int64_t count);
static int64_t finishAll(int64_t* workers, int64_t count);
static int64_t splitMonteCarlo(int64_t* workers, int64_t count);
static float* lifeInStrips(int64_t* workers, int64_t* pipes, int64_t count, float* board, int64_t n);
static int64_t splitLife(int64_t* workers, int64_t count);
static int64_t main_(void);

static int64_t startWorkers(int64_t instance, int64_t* workers) {
  int64_t gpus = vulkanGpuCount(instance);
  int64_t count = gpus;
  if (count > 4) {
    count = 4;
  }
  if (gpus < 2) {
    count = 2;
  }
  for (int64_t w = 0; w < count; w = w + 1) {
    int64_t which = gpus < 2 ? 0 : w;
    workers[w] = vulkanStartOn(instance, which);
    if (workers[w] == 0) {
      return 0;
    }
  }
  say("GPUs found: ");
  sayNumber(gpus);
  say(" (");
  say(vulkanGpuName(workers[0]));
  say(")");
  newline();
  say("workers: ");
  sayNumber(count);
  if (gpus < 2) {
    say("  (only one GPU, so these are separate logical devices on it: separate queues and memory, shared hardware)");
  } else {
    say("  (one for each GPU)");
  }
  newline();
  return count;
}

static int64_t* pipelinesFor(int64_t* workers, int64_t count, const char* name) {
  int64_t* pipes = (int8_t*) (intptr_t) calloc(count, 8);
  for (int64_t w = 0; w < count; w = w + 1) {
    pipes[w] = makePipeline(workers[w], "", name);
  }
  return pipes;
}

static int64_t allBuilt(int64_t* pipes, int64_t count) {
  for (int64_t w = 0; w < count; w = w + 1) {
    if (pipes[w] == 0) {
      return 0;
    }
  }
  return 1;
}

static int64_t finishAll(int64_t* workers, int64_t count) {
  for (int64_t w = 0; w < count; w = w + 1) {
    if (vulkanFinish(workers[w]) == 0) {
      return 0;
    }
  }
  return 1;
}

static int64_t splitMonteCarlo(int64_t* workers, int64_t count) {
  int64_t* pipes = pipelinesFor(workers, count, "hits");
  if (allBuilt(pipes, count) == 0) {
    return 11;
  }
  int64_t threads = 65536;
  int64_t each = threads / count;
  int64_t mark = wallMs();
  float* whole = launch(workers[0], pipes[0], spare(), 1, spare(), 1, settings(2048.0f, 0.0f, 0.0f), threads);
  int64_t wholeMs = wallMs() - mark;
  mark = wallMs();
  int64_t* parts = (int8_t*) (intptr_t) calloc(count, 8);
  for (int64_t w = 0; w < count; w = w + 1) {
    parts[w] = (int64_t) (intptr_t) launchStart(workers[w], pipes[w], spare(), 1, spare(), 1, settings(2048.0f, (float) (w * each), 0.0f), each);
  }
  if (finishAll(workers, count) == 0) {
    return 12;
  }
  int64_t splitMs = wallMs() - mark;
  for (int64_t w = 0; w < count; w = w + 1) {
    float* piece = (int8_t*) (intptr_t) parts[w];
    for (int64_t t = 0; t < each; t = t + 1) {
      if (near(piece[t], whole[w * each + t], 0.0f) == 0) {
        return 13;
      }
    }
  }
  say("Monte Carlo, 65,536 threads x 2,048 darts: one worker ");
  sayNumber(wholeMs);
  say(" ms, split over ");
  sayNumber(count);
  say(" workers ");
  sayNumber(splitMs);
  say(" ms; every thread's count equals the single worker's");
  newline();
  return 0;
}

static float* lifeInStrips(int64_t* workers, int64_t* pipes, int64_t count, float* board, int64_t n) {
  int64_t h = n / count;
  int64_t* blocks = (int8_t*) (intptr_t) calloc(count, 8);
  int64_t* outs = (int8_t*) (intptr_t) calloc(count, 8);
  float* opts = settings((float) n, (float) (h + 2), 0.0f);
  for (int64_t s = 0; s < count; s = s + 1) {
    float* block = (int8_t*) (intptr_t) calloc((h + 2) * n, 4);
    copyRows(board, (s * h + n - 1) % n, block, 0, n, 1);
    copyRows(board, s * h, block, 1, n, h);
    copyRows(board, (s + 1) * h % n, block, h + 1, n, 1);
    blocks[s] = (int64_t) (intptr_t) block;
    outs[s] = (int64_t) (intptr_t) launchStart(workers[s], pipes[s], block, (h + 2) * n, spare(), 1, opts, (h + 2) * n);
  }
  finishAll(workers, count);
  float* next = (int8_t*) (intptr_t) calloc(n * n, 4);
  for (int64_t s = 0; s < count; s = s + 1) {
    copyRows((int8_t*) (intptr_t) outs[s], 1, next, s * h, n, h);
  }
  return next;
}

static int64_t splitLife(int64_t* workers, int64_t count) {
  int64_t* pipes = pipelinesFor(workers, count, "lifeStep");
  if (allBuilt(pipes, count) == 0) {
    return 11;
  }
  int64_t n = 64;
  float* whole = (int8_t*) (intptr_t) calloc(n * n, 4);
  for (int64_t p = 0; p < n * n; p = p + 1) {
    if (pseudo(p + 7, 100) < 35) {
      whole[p] = 1.0f;
    }
  }
  float* strips = whole;
  float* opts = settings((float) n, (float) n, 0.0f);
  for (int64_t g = 0; g < 20; g = g + 1) {
    whole = launch(workers[0], pipes[0], whole, n * n, spare(), 1, opts, n * n);
    strips = lifeInStrips(workers, pipes, count, strips, n);
    if (sameNumbers(whole, strips, n * n, 0.0f) == 0) {
      return 13;
    }
  }
  say("Game of Life, 64 x 64 for 20 generations as ");
  sayNumber(count);
  say(" strips: the stitched board equals the whole board every generation");
  newline();
  say("    each generation the CPU carried ");
  sayNumber(count * 2);
  say(" boundary rows between the workers' separate memories");
  newline();
  return 0;
}

static int64_t main_(void) {
  int64_t instance = vulkanInstance();
  if (instance == 0) {
    return 10;
  }
  if (vulkanGpuCount(instance) == 0) {
    return 10;
  }
  int64_t* workers = (int8_t*) (intptr_t) calloc(4, 8);
  int64_t count = startWorkers(instance, workers);
  if (count == 0) {
    return 10;
  }
  int64_t code = splitMonteCarlo(workers, count);
  if (code != 0) {
    return code;
  }
  code = splitLife(workers, count);
  return code;
}

int main(void) {
  return (int) main_();
}
