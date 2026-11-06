#import <pthread.h>
#import <setjmp.h>
#import <stdbool.h>
#import <stdint.h>
#import <stdio.h>
#import <stdlib.h>
#import <string.h>

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

#ifndef POLYIR_VARIANT_H
#define POLYIR_VARIANT_H

// A variant: the number of its case and the value it holds, kept on the heap with its size.
typedef struct Variant {
  int64_t tag;
  void* value;
  size_t size;
} Variant;

static Variant makeVariant(int64_t tag, const void* value, size_t size) {
  return (Variant){tag, size > 0 ? copyBytes(value, size) : NULL, size};
}

// Two variants are the same when they are of one case and hold the same bytes.
static bool variantSame(Variant a, Variant b) {
  return a.tag == b.tag && a.size == b.size && (a.size == 0 || memcmp(a.value, b.value, a.size) == 0);
}
#endif

#ifndef POLYIR_COROUTINE_H
#define POLYIR_COROUTINE_H

// A coroutine: its body runs on a thread of its own, only while a resume waits for it. `coroutineResume` gives case 0 with the
// value the body gave, or case 1 when it has finished; `coroutineGive` hands a value over and hears whether it was closed.
typedef struct Coroutine Coroutine;
typedef void (*CoroutineBody)(Coroutine* coroutine, void* env);

struct Coroutine {
  CoroutineBody body;
  void* env;
  size_t size;
  pthread_mutex_t lock;
  pthread_cond_t turned;
  bool bodysTurn;
  bool started;
  bool finished;
  bool closing;
  void* handed;
};

static Coroutine* coroutineNew(CoroutineBody body, void* env, size_t size) {
  Coroutine* coroutine = gcAlloc(sizeof(Coroutine));
  coroutine->body = body;
  coroutine->env = env;
  coroutine->size = size;
  pthread_mutex_init(&coroutine->lock, NULL);
  pthread_cond_init(&coroutine->turned, NULL);

  return coroutine;
}

// Hands the turn to the other side and waits until it comes back.
static void coroutineTurn(Coroutine* coroutine, bool toBody) {
  coroutine->bodysTurn = toBody;
  pthread_cond_broadcast(&coroutine->turned);

  while (coroutine->bodysTurn == toBody && !coroutine->finished) {
    pthread_cond_wait(&coroutine->turned, &coroutine->lock);
  }
}

static void* coroutineThread(void* raw) {
  Coroutine* coroutine = raw;
  pthread_mutex_lock(&coroutine->lock);
  coroutine->body(coroutine, coroutine->env);
  coroutine->finished = true;
  pthread_cond_broadcast(&coroutine->turned);
  pthread_mutex_unlock(&coroutine->lock);

  return NULL;
}

static void coroutineStart(Coroutine* coroutine) {
  pthread_t thread;
  coroutine->started = true;
  coroutine->bodysTurn = true;
  pthread_create(&thread, NULL, coroutineThread, coroutine);
  pthread_detach(thread);

  while (coroutine->bodysTurn && !coroutine->finished) {
    pthread_cond_wait(&coroutine->turned, &coroutine->lock);
  }
}

static Variant coroutineResume(Coroutine* coroutine) {
  if (coroutine->finished) {
    return makeVariant(1, NULL, 0);
  }

  pthread_mutex_lock(&coroutine->lock);
  coroutine->started ? coroutineTurn(coroutine, true) : coroutineStart(coroutine);
  const Variant step = coroutine->finished ? makeVariant(1, NULL, 0) : makeVariant(0, coroutine->handed, coroutine->size);
  pthread_mutex_unlock(&coroutine->lock);

  return step;
}

static bool coroutineGive(Coroutine* coroutine, const void* value) {
  coroutine->handed = (void*) value;
  coroutineTurn(coroutine, false);

  return coroutine->closing;
}

static bool coroutineDone(Coroutine* coroutine) {
  return coroutine->finished;
}

static void coroutineClose(Coroutine* coroutine) {
  if (coroutine->started && !coroutine->finished) {
    pthread_mutex_lock(&coroutine->lock);
    coroutine->closing = true;
    coroutineTurn(coroutine, true);
    pthread_mutex_unlock(&coroutine->lock);
  }

  coroutine->finished = true;
}
#endif

static Coroutine* fibonacci(int64_t count);
static int64_t main_(void);

typedef struct fibonacci_Env {
  int64_t count;
} fibonacci_Env;

static void fibonacci_run(Coroutine* coroutine, void* raw) {
  fibonacci_Env* env = raw;
  int64_t a = 0;
  int64_t b = 1;
  for (int64_t i = 0; i < env->count; i = i + 1) {
    if (coroutineGive(coroutine, (int64_t[]){a})) {
      return;
    }
    int64_t next = a + b;
    a = b;
    b = next;
  }
}

static Coroutine* fibonacci(int64_t count) {
  fibonacci_Env* env = gcAlloc(sizeof(fibonacci_Env));
  env->count = count;
  return coroutineNew(fibonacci_run, env, sizeof(int64_t));
}

static int64_t main_(void) {
  Coroutine* fib = fibonacci(7);
  int64_t sum = 0;
  bool done = false;
  for (int64_t i = 0; i < 8 && done == false; i = i + 1) {
    Variant step = coroutineResume(fib);
    if (step.tag == 1) {
      done = true;
    } else {
      int64_t value = (*(int64_t*) step.value);
      putchar(48 + value);
      sum = sum + value;
    }
  }
  putchar(10);
  if (coroutineDone(fib) == false) {
    return -1;
  }
  return sum;
}

int main(void) {
  return (int) main_();
}
