#include <pthread.h>
#include <setjmp.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

#ifndef POLYIR_CLOSURE_H
#define POLYIR_CLOSURE_H

// A closure: its function and its environment, which the function takes first. The function is kept as any function pointer
// and called as its own type.
typedef void (*AnyFn)(void);

typedef struct Closure {
  AnyFn fn;
  void* env;
} Closure;
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

#ifndef POLYIR_TEXT_H
#define POLYIR_TEXT_H

// Text is a `const char*`, made and joined on the heap. A string's positions count its bytes. A byte outside the string is 0,
// a slice keeps what lies inside the string, and a search starts inside it.
static const char* textJoin(const char* a, const char* b) {
  const size_t first = strlen(a);
  char* out = gcAlloc(first + strlen(b) + 1);
  memcpy(out, a, first);
  strcpy(out + first, b);

  return out;
}

static bool textSame(const char* a, const char* b) {
  return strcmp(a, b) == 0;
}

static const char* textOfInt(int64_t value) {
  char* out = gcAlloc(24);
  snprintf(out, 24, "%lld", (long long) value);

  return out;
}

static const char* textOfUint(uint64_t value) {
  char* out = gcAlloc(24);
  snprintf(out, 24, "%llu", (unsigned long long) value);

  return out;
}

static const char* textOfDouble(double value) {
  char* out = gcAlloc(400);
  snprintf(out, 400, "%f", value);

  return out;
}

static const char* textOfBool(bool flag) {
  return flag ? "true" : "false";
}

static const char* textOfChar(int8_t c) {
  char* out = gcAlloc(2);
  out[0] = (char) c;

  return out;
}

static int8_t textByte(const char* text, int64_t at) {
  return at >= 0 && at < (int64_t) strlen(text) ? (int8_t) text[at] : 0;
}

static const char* textSlice(const char* text, int64_t from, int64_t to) {
  const int64_t size = strlen(text);
  const int64_t start = from < 0 ? 0 : from > size ? size : from;
  const int64_t end = to < start ? start : to > size ? size : to;
  char* out = gcAlloc(end - start + 1);
  memcpy(out, text + start, end - start);

  return out;
}

static int64_t textFind(const char* text, const char* part, int64_t from) {
  const int64_t size = strlen(text);
  const char* found = strstr(text + (from < 0 ? 0 : from > size ? size : from), part);

  return found == NULL ? -1 : found - text;
}

// A string's hash: djb2 over its bytes, wrapping at 64 bits, as the native code computes it.
static int64_t textHash(const char* text) {
  uint64_t hash = 5381;

  for (const char* c = text; *c != 0; c++) {
    hash = hash * 33 + (unsigned char) *c;
  }

  return (int64_t) hash;
}
#endif

typedef struct Node Node;
typedef struct Holder Holder;

static bool flag = false;
static double ratio = 1.5;
static int64_t hits = 0;

struct Node {
  int64_t value;
  Node** next;
};

struct Holder {
  Closure op;
  const char* name;
};

static Node* Node_new(int64_t value, Node** next);
static bool Node_equal(Node* a, Node* b);
static uint64_t Node_hash(Node* a);

static Holder* Holder_new(Closure op, const char* name);
static bool Holder_equal(Holder* a, Holder* b);
static uint64_t Holder_hash(Holder* a);
static int64_t Holder_run(Holder* self, int64_t x);
static Closure Holder_viaLater(Holder* self);

static Coroutine* pairs(int64_t n);
static int64_t sumNodes(int64_t n);
static int64_t chain(void);
static int64_t nestedClosures(int64_t a);
static int64_t strClosure(const char* prefix);
static int64_t holders(void);
static int64_t globals(void);
static int64_t findFirst(int64_t limit);
static int64_t tryInLoop(int64_t n);
static const char* lines(const char* a, const char* b);
static int64_t lineLen(void);
static int64_t machineStrings(int64_t n);
static int64_t precedence(int64_t a, int64_t b);
static int64_t main_(void);

typedef struct closure1_Env {
  Holder* self;
} closure1_Env;

static closure1_Env* closure1_capture(Holder* self) {
  closure1_Env* env = gcAlloc(sizeof(closure1_Env));
  env->self = self;
  return env;
}

static int64_t closure1(void* raw, int64_t y) {
  closure1_Env* env = raw;
  Closure f = env->self->op;
  return ((int64_t (*)(void*, int64_t)) f.fn)(f.env, y) + (int64_t) strlen(env->self->name);
}

typedef struct closure2_Env {
  int64_t* total;
  int64_t x;
  int64_t a;
} closure2_Env;

static closure2_Env* closure2_capture(int64_t* total, int64_t x, int64_t a) {
  closure2_Env* env = gcAlloc(sizeof(closure2_Env));
  env->total = total;
  env->x = x;
  env->a = a;
  return env;
}

static int64_t closure2(void* raw, int64_t y) {
  closure2_Env* env = raw;
  (*env->total) = (*env->total) + y;
  return env->x * y + env->a;
}

typedef struct closure3_Env {
  int64_t* total;
  int64_t y;
  int64_t a;
} closure3_Env;

static closure3_Env* closure3_capture(int64_t* total, int64_t y, int64_t a) {
  closure3_Env* env = gcAlloc(sizeof(closure3_Env));
  env->total = total;
  env->y = y;
  env->a = a;
  return env;
}

static int64_t closure3(void* raw, int64_t x) {
  closure3_Env* env = raw;
  Closure inner = (Closure){(AnyFn) closure2, closure2_capture(env->total, x, env->a)};
  return ((int64_t (*)(void*, int64_t)) inner.fn)(inner.env, x + 1) + ((int64_t (*)(void*, int64_t)) inner.fn)(inner.env, 2);
}

typedef struct closure4_Env {
  const char* prefix;
  const char* tail;
} closure4_Env;

static closure4_Env* closure4_capture(const char* prefix, const char* tail) {
  closure4_Env* env = gcAlloc(sizeof(closure4_Env));
  env->prefix = prefix;
  env->tail = tail;
  return env;
}

static const char* closure4(void* raw, const char* s) {
  closure4_Env* env = raw;
  return textJoin(textJoin(env->prefix, s), env->tail);
}

static int64_t closure5(void* env, int64_t x) {
  (void) env;
  return x * x;
}

static Node* Node_new(int64_t value, Node** next) {
  Node* made = gcAlloc(sizeof(Node));
  made->value = value;
  made->next = next;
  return made;
}

static bool Node_equal(Node* a, Node* b) {
  if (a == b || a == NULL || b == NULL) {
    return a == b;
  }

  bool same = true;
  same = same && a->value == b->value;
  same = same && memcmp(&a->next, &b->next, sizeof a->next) == 0;
  return same;
}

static uint64_t Node_hash(Node* a) {
  uint64_t hash = 17;
  hash = hash * 31 + (uint64_t) a->value;
  hash = hash * 31 + hashBytes(&a->next, sizeof a->next);
  return hash;
}

static Holder* Holder_new(Closure op, const char* name) {
  Holder* made = gcAlloc(sizeof(Holder));
  made->op = op;
  made->name = name;
  return made;
}

static bool Holder_equal(Holder* a, Holder* b) {
  if (a == b || a == NULL || b == NULL) {
    return a == b;
  }

  bool same = true;
  same = same && memcmp(&a->op, &b->op, sizeof a->op) == 0;
  same = same && textSame(a->name, b->name);
  return same;
}

static uint64_t Holder_hash(Holder* a) {
  uint64_t hash = 17;
  hash = hash * 31 + hashBytes(&a->op, sizeof a->op);
  hash = hash * 31 + (uint64_t) textHash(a->name);
  return hash;
}

static int64_t Holder_run(Holder* self, int64_t x) {
  Closure f = self->op;
  return ((int64_t (*)(void*, int64_t)) f.fn)(f.env, x);
}

static Closure Holder_viaLater(Holder* self) {
  return (Closure){(AnyFn) closure1, closure1_capture(self)};
}

typedef struct pairs_Env {
  int64_t n;
} pairs_Env;

static void pairs_run(Coroutine* coroutine, void* raw) {
  pairs_Env* env = raw;
  for (int64_t i = 0; i < env->n; i = i + 1) {
    if (coroutineGive(coroutine, (Node*[]){Node_new(i, NULL)})) {
      return;
    }
  }
}

static Coroutine* pairs(int64_t n) {
  pairs_Env* env = gcAlloc(sizeof(pairs_Env));
  env->n = n;
  return coroutineNew(pairs_run, env, sizeof(Node*));
}

static int64_t sumNodes(int64_t n) {
  Coroutine* g = pairs(n);
  int64_t s = 0;
  bool done = false;
  for (int64_t k = 0; k < 20 && done == false; k = k + 1) {
    Variant r = coroutineResume(g);
    if (r.tag == 1) {
      done = true;
    } else {
      s = s + (*(Node**) r.value)->value;
    }
  }
  return s;
}

static int64_t chain(void) {
  Node* a = Node_new(1, NULL);
  Node* b = Node_new(2, copyBytes((Node*[]){a}, sizeof(Node*)));
  Node* c = Node_new(3, copyBytes((Node*[]){b}, sizeof(Node*)));
  int64_t sum = 0;
  Node** cur = copyBytes((Node*[]){c}, sizeof(Node*));
  for (int64_t i = 0; i < 5 && cur != NULL; i = i + 1) {
    Node* n = (*(Node**) present(cur));
    sum = sum + n->value;
    cur = n->next;
  }
  a->value = 10;
  return sum * 100 + (*(Node**) present(b->next))->value;
}

static int64_t nestedClosures(int64_t a) {
  int64_t* total = copyBytes((int64_t[]){0}, sizeof(int64_t));
  Closure outer = (Closure){(AnyFn) closure3, closure3_capture(total, y, a)};
  int64_t r = ((int64_t (*)(void*, int64_t)) outer.fn)(outer.env, 3);
  return r * 1000 + (*total);
}

static int64_t strClosure(const char* prefix) {
  const char* tail = "!";
  Closure f = (Closure){(AnyFn) closure4, closure4_capture(prefix, tail)};
  return (int64_t) strlen(((const char* (*)(void*, const char*)) f.fn)(f.env, "abc"));
}

static int64_t holders(void) {
  Holder* h = Holder_new((Closure){(AnyFn) closure5, NULL}, "hh");
  Closure later = Holder_viaLater(h);
  return Holder_run(h, 5) + ((int64_t (*)(void*, int64_t)) later.fn)(later.env, 2);
}

static int64_t globals(void) {
  hits = hits + 5;
  flag = true;
  ratio = ratio * 2.0;
  int64_t r = hits;
  if (flag) {
    r = r + 100;
  }
  if (ratio > 2.5) {
    r = r + 1000;
  }
  return r;
}

static int64_t findFirst(int64_t limit) {
  for (int64_t i = 0; i < limit; i = i + 1) {
    for (int64_t j = 0; j < limit; j = j + 1) {
      if (i * j == 12) {
        return i * 100 + j;
      }
    }
  }
  return 0 - 1;
}

static int64_t tryInLoop(int64_t n) {
  int64_t volatile s = 0;
  for (int64_t i = 0; i < n; i = i + 1) {
    {
      Handler tried = {.outer = handlers};
      handlers = &tried;

      if (setjmp(tried.jump) == 0) {
        if (i % 2 == 1) {
          throwCode(i);
        }
        s = s + 10;
        handlers = tried.outer;
      } else {
        handlers = tried.outer;
        const int64_t code = tried.code;
        s = s + code;
      }
    }
  }
  return s;
}

static const char* lines(const char* a, const char* b) {
  int64_t label = 0;
  const char* acc = "";
  int64_t n = 0;
  for (;;) {
    switch (label) {
      case 0: {
        acc = a;
        n = 0;
        label = 1;
        continue;
      }
      case 1: {
        if (n < 3) {
          label = 2;
          continue;
        } else {
          label = 3;
          continue;
        }
      }
      case 2: {
        acc = textJoin(acc, b);
        n = n + 1;
        label = 1;
        continue;
      }
      case 3: {
        return acc;
      }
    }
  }
}

static int64_t lineLen(void) {
  return (int64_t) strlen(lines("x", "yz"));
}

static int64_t machineStrings(int64_t n) {
  int64_t label = 0;
  int64_t p = 0;
  const char* s = "";
  int64_t q = 0;
  int64_t r = 0;
  for (;;) {
    switch (label) {
      case 0: {
        if (n == 0) {
          p = n;
          s = "p";
          label = 1;
          continue;
        } else {
          q = n;
          s = "q";
          label = 2;
          continue;
        }
      }
      case 1: {
        if (p > 4) {
          r = p;
          label = 3;
          continue;
        } else {
          q = p + 1;
          s = textJoin(s, "a");
          label = 2;
          continue;
        }
      }
      case 2: {
        if (q > 4) {
          r = q;
          label = 3;
          continue;
        } else {
          p = q + 1;
          s = textJoin(s, "b");
          label = 1;
          continue;
        }
      }
      case 3: {
        return r * 100 + (int64_t) strlen(s);
      }
    }
  }
}

static int64_t precedence(int64_t a, int64_t b) {
  int64_t x = (a + b) * (a - b) / 3;
  int64_t y = a & b | a ^ b;
  int64_t z = a << 2 + b >> 1;
  int64_t s = x + y + z;
  if ((a & 1) == 1 && b > 2 || a == 0) {
    s = s + 1;
  }
  int32_t w = (int32_t) a;
  if ((int64_t) w < 100 && (int32_t) (a + b) < 1000) {
    s = s + 2;
  }
  return s;
}

static int64_t main_(void) {
  int64_t score = sumNodes(5) + chain() + nestedClosures(2) + strClosure("ab");
  score = score + holders() + globals() + findFirst(10) + tryInLoop(6);
  score = score + lineLen() + machineStrings(0) + machineStrings(1) + precedence(13, 7);
  return score % 251;
}

int main(void) {
  return (int) main_();
}
