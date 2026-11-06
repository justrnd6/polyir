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

typedef struct Acc Acc;
typedef struct Dog Dog;
typedef struct Bird Bird;

struct Acc {
  int64_t total;
  const char* tag;
  List* items;
};

static const char Dog_kind[] = "Dog";

struct Dog {
  const char* kind;
  int64_t age;
};

static const char Bird_kind[] = "Bird";

struct Bird {
  const char* kind;
  int64_t age;
};

static Acc* Acc_new(int64_t total, const char* tag, List* items);
static bool Acc_equal(Acc* a, Acc* b);
static uint64_t Acc_hash(Acc* a);
static int64_t Acc_add(Acc* self, int64_t x);
static Closure Acc_later(Acc* self);
static const char* Acc_describe(Acc* self);
static Acc* Acc_self_(Acc* self);

static int64_t Animal_legs(void* self, int64_t p0);
static const char* Animal_name(void* self);

static Dog* Dog_new(int64_t age);
static bool Dog_equal(Dog* a, Dog* b);
static uint64_t Dog_hash(Dog* a);
static int64_t Dog_legs(Dog* self, int64_t n);
static const char* Dog_name(Dog* self);

static Bird* Bird_new(int64_t age);
static bool Bird_equal(Bird* a, Bird* b);
static uint64_t Bird_hash(Bird* a);
static int64_t Bird_legs(Bird* self, int64_t n);
static const char* Bird_name(Bird* self);

static int64_t risky(int64_t x);
static int64_t tryReturn(int64_t x);
static int64_t nested(int64_t x);
static Closure makeAdder(int64_t n);
static int64_t twice(Closure f, int64_t x);
static int64_t counterPair(void);
static int64_t main_(void);

typedef struct closure1_Env {
  Acc* self;
} closure1_Env;

static closure1_Env* closure1_capture(Acc* self) {
  closure1_Env* env = gcAlloc(sizeof(closure1_Env));
  env->self = self;
  return env;
}

static int64_t closure1(void* raw) {
  closure1_Env* env = raw;
  return env->self->total * 2;
}

typedef struct closure2_Env {
  int64_t n;
} closure2_Env;

static closure2_Env* closure2_capture(int64_t n) {
  closure2_Env* env = gcAlloc(sizeof(closure2_Env));
  env->n = n;
  return env;
}

static int64_t closure2(void* raw, int64_t x) {
  closure2_Env* env = raw;
  return x + env->n;
}

typedef struct closure3_Env {
  int64_t* c;
} closure3_Env;

static closure3_Env* closure3_capture(int64_t* c) {
  closure3_Env* env = gcAlloc(sizeof(closure3_Env));
  env->c = c;
  return env;
}

static int64_t closure3(void* raw) {
  closure3_Env* env = raw;
  (*env->c) = (*env->c) + 1;
  return (*env->c);
}

typedef struct closure4_Env {
  int64_t* c;
} closure4_Env;

static closure4_Env* closure4_capture(int64_t* c) {
  closure4_Env* env = gcAlloc(sizeof(closure4_Env));
  env->c = c;
  return env;
}

static int64_t closure4(void* raw) {
  closure4_Env* env = raw;
  return (*env->c);
}

static Acc* Acc_new(int64_t total, const char* tag, List* items) {
  Acc* made = gcAlloc(sizeof(Acc));
  made->total = total;
  made->tag = tag;
  made->items = items;
  return made;
}

static bool Acc_equal(Acc* a, Acc* b) {
  if (a == b || a == NULL || b == NULL) {
    return a == b;
  }

  bool same = true;
  same = same && a->total == b->total;
  same = same && textSame(a->tag, b->tag);
  same = same && listSame(a->items, b->items, NULL);
  return same;
}

static uint64_t Acc_hash(Acc* a) {
  uint64_t hash = 17;
  hash = hash * 31 + (uint64_t) a->total;
  hash = hash * 31 + (uint64_t) textHash(a->tag);
  hash = hash * 31 + hashBytes(&a->items, sizeof a->items);
  return hash;
}

static int64_t Acc_add(Acc* self, int64_t x) {
  self->total = self->total + x;
  listPush(self->items, (int64_t[]){x});
  return self->total;
}

static Closure Acc_later(Acc* self) {
  return (Closure){(AnyFn) closure1, closure1_capture(self)};
}

static const char* Acc_describe(Acc* self) {
  return textJoin(textJoin(self->tag, ":"), self->tag);
}

static Acc* Acc_self_(Acc* self) {
  return self;
}

static int64_t Animal_legs(void* self, int64_t p0) {
  const char* kind = *(const char**) self;
  if (kind == Dog_kind) {
    return Dog_legs(self, p0);
  }
  if (kind == Bird_kind) {
    return Bird_legs(self, p0);
  }
  fail("no struct of this kind implements Animal");
}

static const char* Animal_name(void* self) {
  const char* kind = *(const char**) self;
  if (kind == Dog_kind) {
    return Dog_name(self);
  }
  if (kind == Bird_kind) {
    return Bird_name(self);
  }
  fail("no struct of this kind implements Animal");
}

static Dog* Dog_new(int64_t age) {
  Dog* made = gcAlloc(sizeof(Dog));
  made->kind = Dog_kind;
  made->age = age;
  return made;
}

static bool Dog_equal(Dog* a, Dog* b) {
  if (a == b || a == NULL || b == NULL) {
    return a == b;
  }

  bool same = true;
  same = same && a->age == b->age;
  return same;
}

static uint64_t Dog_hash(Dog* a) {
  uint64_t hash = 17;
  hash = hash * 31 + (uint64_t) a->age;
  return hash;
}

static int64_t Dog_legs(Dog* self, int64_t n) {
  return 4 * n + self->age;
}

static const char* Dog_name(Dog* self) {
  return "dog";
}

static Bird* Bird_new(int64_t age) {
  Bird* made = gcAlloc(sizeof(Bird));
  made->kind = Bird_kind;
  made->age = age;
  return made;
}

static bool Bird_equal(Bird* a, Bird* b) {
  if (a == b || a == NULL || b == NULL) {
    return a == b;
  }

  bool same = true;
  same = same && a->age == b->age;
  return same;
}

static uint64_t Bird_hash(Bird* a) {
  uint64_t hash = 17;
  hash = hash * 31 + (uint64_t) a->age;
  return hash;
}

static int64_t Bird_legs(Bird* self, int64_t n) {
  return 2 * n;
}

static const char* Bird_name(Bird* self) {
  return "bird";
}

static int64_t risky(int64_t x) {
  if (x < 0) {
    throwCode(7);
  }
  return x;
}

static int64_t tryReturn(int64_t x) {
  {
    Handler tried = {.outer = handlers};
    handlers = &tried;

    if (setjmp(tried.jump) == 0) {
      return risky(x) + 1;
      handlers = tried.outer;
    } else {
      handlers = tried.outer;
      const int64_t code = tried.code;
      return code * 100;
    }
  }
  return 0;
}

static int64_t nested(int64_t x) {
  int64_t volatile r = 0;
  {
    Handler tried = {.outer = handlers};
    handlers = &tried;

    if (setjmp(tried.jump) == 0) {
      {
        Handler tried = {.outer = handlers};
        handlers = &tried;

        if (setjmp(tried.jump) == 0) {
          r = risky(x);
          handlers = tried.outer;
        } else {
          handlers = tried.outer;
          const int64_t a = tried.code;
          r = a + 1;
          throwCode(r);
        }
      }
      handlers = tried.outer;
    } else {
      handlers = tried.outer;
      const int64_t b = tried.code;
      r = b * 10;
    }
  }
  return r;
}

static Closure makeAdder(int64_t n) {
  return (Closure){(AnyFn) closure2, closure2_capture(n)};
}

static int64_t twice(Closure f, int64_t x) {
  return ((int64_t (*)(void*, int64_t)) f.fn)(f.env, ((int64_t (*)(void*, int64_t)) f.fn)(f.env, x));
}

static int64_t counterPair(void) {
  int64_t* c = copyBytes((int64_t[]){0}, sizeof(int64_t));
  Closure inc = (Closure){(AnyFn) closure3, closure3_capture(c)};
  Closure get = (Closure){(AnyFn) closure4, closure4_capture(c)};
  ((int64_t (*)(void*)) inc.fn)(inc.env);
  ((int64_t (*)(void*)) inc.fn)(inc.env);
  return ((int64_t (*)(void*)) get.fn)(get.env) * 10 + (*c);
}

static int64_t main_(void) {
  int64_t score = 0;
  Acc* a = Acc_new(1, "ab", listNew(sizeof(int64_t)));
  score = score + Acc_add(a, 5) + Acc_add(a, 6);
  Closure f = Acc_later(a);
  score = score + ((int64_t (*)(void*)) f.fn)(f.env);
  score = score + a->items->count;
  if (textSame(Acc_describe(a), "ab:ab")) {
    score = score + 1000;
  }
  Acc* s = Acc_self_(a);
  if (s == a) {
    score = score + 2000;
  }
  a->tag = "zz";
  if (textSame(s->tag, "zz")) {
    score = score + 3000;
  }
  List* zoo = listNew(sizeof(void*));
  listPush(zoo, (void*[]){Dog_new(3)});
  listPush(zoo, (void*[]){Bird_new(1)});
  for (int64_t i = 0; i < zoo->count; i = i + 1) {
    score = score + Animal_legs(LIST_AT(zoo, i, void*), 2);
    if (textSame(Animal_name(LIST_AT(zoo, i, void*)), "bird")) {
      score = score + 7;
    }
  }
  score = score + tryReturn(3) + tryReturn(0 - 1);
  score = score + nested(4) + nested(0 - 2);
  Closure add3 = makeAdder(3);
  score = score + twice(add3, 10);
  score = score + counterPair();
  return score % 251;
}

int main(void) {
  return (int) main_();
}
