#pragma once

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

#ifndef POLYIR_EITHER_H
#define POLYIR_EITHER_H

// An `either`: whether it holds its value as ok or as an error, and the value, kept on the heap with its size.
typedef struct Either {
  bool isOk;
  void* held;
  size_t size;
} Either;

static Either makeEither(bool isOk, const void* value, size_t size) {
  return (Either){isOk, copyBytes(value, size), size};
}

// Two `either`s are the same when both are ok (or both errors) and hold the same bytes.
static bool eitherSame(Either a, Either b) {
  return a.isOk == b.isOk && a.size == b.size && memcmp(a.held, b.held, a.size) == 0;
}
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

static bool contains(const char* s, const char* part);
static bool startsWith(const char* s, const char* prefix);
static bool endsWith(const char* s, const char* suffix);
static const char* join(List* parts, const char* sep);
static double offText(const char* got, const char* want);
static const char* sizeOf(int64_t n);
static double checkNumbers(void);
static const char* signOf(int64_t n);
static double checkComparisons(void);
static const char* gradeOf(int64_t score);
static double checkGuardsOnTheSubject(void);
static bool yes(const char* reply);
static double checkStrings(void);
static const char* kindOf(const char* path);
static double checkConditions(void);
static const char* word(bool ready);
static double checkBooleans(void);
static const char* shown(int64_t* found);
static double checkOptionals(void);
static Either safeDivide(int64_t a, int64_t b);
static const char* message(int64_t a, int64_t b);
static double checkEithers(void);
static const char* textOf(Variant shape);
static double checkVariants(void);
static const char* zeroOr(int64_t* found);
static double checkPayloadPatterns(void);
static const char* said(List* xs);
static double checkLists(void);
static const char* pairOf(List* xs);
static double checkListGuards(void);
static const char* command(const char* c);
static double checkStatements(void);
static double checkNested(void);
static List* drop_i64(List* xs, int64_t n);

static bool contains(const char* s, const char* part) {
  return textFind(s, part, 0) >= 0;
}

static bool startsWith(const char* s, const char* prefix) {
  if ((int64_t) strlen(prefix) > (int64_t) strlen(s)) {
    return false;
  }
  return textSame(textSlice(s, 0, (int64_t) strlen(prefix)), prefix);
}

static bool endsWith(const char* s, const char* suffix) {
  if ((int64_t) strlen(suffix) > (int64_t) strlen(s)) {
    return false;
  }
  return textSame(textSlice(s, (int64_t) strlen(s) - (int64_t) strlen(suffix), (int64_t) strlen(s)), suffix);
}

static const char* join(List* parts, const char* sep) {
  const char* out = "";
  for (int64_t i = 0; i < parts->count; i = i + 1) {
    if (i > 0) {
      out = textJoin(out, sep);
    }
    out = textJoin(out, LIST_AT(parts, i, const char*));
  }
  return out;
}

static double offText(const char* got, const char* want) {
  if (textSame(got, want)) {
    return 0.0;
  }
  return 1.0;
}

static const char* sizeOf(int64_t n) {
  const char* matchValue_16_12 = "";
  switch (n) {
    case 0: {
      matchValue_16_12 = "none";
      break;
    }
    case 1: case 2: case 3: {
      matchValue_16_12 = "a few";
      break;
    }
    case -1: {
      matchValue_16_12 = "minus one";
      break;
    }
    default: {
      matchValue_16_12 = "many";
      break;
    }
  }
  return matchValue_16_12;
}

static double checkNumbers(void) {
  return offText(textJoin(textJoin(textJoin(sizeOf(0), sizeOf(2)), sizeOf(9)), sizeOf(-1)), "nonea fewmanyminus one");
}

static const char* signOf(int64_t n) {
  const char* matchValue_30_16 = "";
  if (n < 0) {
    matchValue_30_16 = "negative";
  } else if (n == 0) {
    matchValue_30_16 = "zero";
  } else if (n >= 100) {
    matchValue_30_16 = "large";
  } else {
    matchValue_30_16 = "positive";
  }
  const char* sign = matchValue_30_16;
  return sign;
}

static double checkComparisons(void) {
  return offText(textJoin(textJoin(textJoin(textJoin(textJoin(textJoin(signOf(-3), " "), signOf(0)), " "), signOf(150)), " "), signOf(5)), "negative zero large positive");
}

static const char* gradeOf(int64_t score) {
  const char* matchValue_45_12 = "";
  if (score == 100) {
    matchValue_45_12 = "perfect";
  } else if (score >= 90) {
    int64_t s = score;
    matchValue_45_12 = textJoin("A ", textOfInt(s));
  } else if (score >= 80) {
    matchValue_45_12 = "B";
  } else {
    matchValue_45_12 = "C";
  }
  return matchValue_45_12;
}

static double checkGuardsOnTheSubject(void) {
  return offText(textJoin(textJoin(textJoin(gradeOf(100), gradeOf(93)), gradeOf(85)), gradeOf(10)), "perfectA 93BC");
}

static bool yes(const char* reply) {
  bool matchValue_60_12 = false;
  if (textSame(reply, "y") || textSame(reply, "yes")) {
    matchValue_60_12 = true;
  } else if (textSame(reply, "n") || textSame(reply, "no")) {
    matchValue_60_12 = false;
  } else {
    matchValue_60_12 = false;
  }
  return matchValue_60_12;
}

static double checkStrings(void) {
  return offText(textJoin(textJoin(textJoin(textJoin(textJoin(textJoin(textOfBool(yes("y")), " "), textOfBool(yes("yes"))), " "), textOfBool(yes("no"))), " "), textOfBool(yes("maybe"))), "true true false false");
}

static const char* kindOf(const char* path) {
  const char* matchValue_73_12 = "";
  if (textSame(path, "Ninafile") || textSame(path, "Polyfile")) {
    matchValue_73_12 = "build";
  } else if (endsWith(path, ".poly")) {
    matchValue_73_12 = "polyir";
  } else if (startsWith(path, "README")) {
    matchValue_73_12 = "docs";
  } else if (contains(path, "test") && !endsWith(path, ".md")) {
    matchValue_73_12 = "a test";
  } else if (endsWith(path, ".md")) {
    const char* p = path;
    matchValue_73_12 = textJoin("markdown ", (p));
  } else {
    matchValue_73_12 = "other";
  }
  return matchValue_73_12;
}

static double checkConditions(void) {
  List* kinds = LIST_OF(const char*, kindOf("Polyfile"), kindOf("a.poly"), kindOf("README.md"), kindOf("my_test.c"), kindOf("notes.md"), kindOf("x.c"));
  return offText(join(kinds, ","), "build,polyir,docs,a test,markdown notes.md,other");
}

static const char* word(bool ready) {
  const char* matchValue_89_12 = "";
  switch (ready) {
    case true: {
      matchValue_89_12 = "go";
      break;
    }
    case false: {
      matchValue_89_12 = "wait";
      break;
    }
  }
  return matchValue_89_12;
}

static double checkBooleans(void) {
  return offText(textJoin(word(true), word(false)), "gowait");
}

static const char* shown(int64_t* found) {
  const char* matchValue_102_12 = "";
  if (found != NULL && (*(int64_t*) present(found)) > 100) {
    int64_t n = (*(int64_t*) present(found));
    matchValue_102_12 = textJoin("big ", textOfInt(n));
  } else if (found != NULL) {
    int64_t n = (*(int64_t*) present(found));
    matchValue_102_12 = textJoin("found ", textOfInt(n));
  } else {
    matchValue_102_12 = "nothing";
  }
  return matchValue_102_12;
}

static double checkOptionals(void) {
  return offText(textJoin(textJoin(textJoin(textJoin(shown(copyBytes((int64_t[]){500}, sizeof(int64_t))), ", "), shown(copyBytes((int64_t[]){5}, sizeof(int64_t)))), ", "), shown(NULL)), "big 500, found 5, nothing");
}

static Either safeDivide(int64_t a, int64_t b) {
  if (b == 0) {
    return makeEither(false, (const char*[]){"divide by zero"}, sizeof(const char*));
  }
  return makeEither(true, (int64_t[]){a / b}, sizeof(int64_t));
}

static const char* message(int64_t a, int64_t b) {
  Either match_122_12 = safeDivide(a, b);
  const char* matchValue_122_12 = "";
  if (match_122_12.isOk) {
    int64_t q = (*(int64_t*) match_122_12.held);
    matchValue_122_12 = textJoin("quotient ", textOfInt(q));
  } else if (!match_122_12.isOk) {
    const char* why = (*(const char**) match_122_12.held);
    matchValue_122_12 = textJoin("error ", (why));
  }
  return matchValue_122_12;
}

static double checkEithers(void) {
  return offText(textJoin(textJoin(message(7, 2), "; "), message(7, 0)), "quotient 3; error divide by zero");
}

static const char* textOf(Variant shape) {
  const char* matchValue_133_12 = "";
  if (shape.tag == 0) {
    int64_t r = (*(int64_t*) shape.value);
    matchValue_133_12 = textJoin("circle ", textOfInt(r));
  } else if (shape.tag == 1) {
    int64_t s = (*(int64_t*) shape.value);
    matchValue_133_12 = textJoin("square ", textOfInt(s));
  } else if (shape.tag == 2) {
    const char* name = (*(const char**) shape.value);
    matchValue_133_12 = name;
  }
  return matchValue_133_12;
}

static double checkVariants(void) {
  const char* circle = textOf(makeVariant(0, (int64_t[]){2}, sizeof(int64_t)));
  return offText(textJoin(textJoin(textJoin(textJoin(circle, ", "), textOf(makeVariant(1, (int64_t[]){4}, sizeof(int64_t)))), ", "), textOf(makeVariant(2, (const char*[]){"blob"}, sizeof(const char*)))), "circle 2, square 4, blob");
}

static const char* zeroOr(int64_t* found) {
  const char* matchValue_147_12 = "";
  if (found != NULL && (*(int64_t*) present(found)) == 0) {
    matchValue_147_12 = "zero";
  } else if (found != NULL) {
    matchValue_147_12 = "other";
  } else {
    matchValue_147_12 = "none";
  }
  return matchValue_147_12;
}

static double checkPayloadPatterns(void) {
  return offText(textJoin(textJoin(zeroOr(copyBytes((int64_t[]){0}, sizeof(int64_t))), zeroOr(copyBytes((int64_t[]){3}, sizeof(int64_t)))), zeroOr(NULL)), "zeroothernone");
}

static const char* said(List* xs) {
  const char* matchValue_161_12 = "";
  if (xs->count == 0) {
    matchValue_161_12 = "empty";
  } else if (xs->count == 1) {
    int64_t x = LIST_AT(xs, 0, int64_t);
    matchValue_161_12 = textJoin("just ", textOfInt(x));
  } else if (LIST_AT(xs, 0, int64_t) == 0) {
    matchValue_161_12 = "starts at zero";
  } else {
    int64_t first = LIST_AT(xs, 0, int64_t);
    List* rest = drop_i64(xs, 1);
    matchValue_161_12 = textJoin(textJoin(textJoin(textOfInt(first), ", then "), textOfInt(rest->count)), " more");
  }
  return matchValue_161_12;
}

static double checkLists(void) {
  List* none = listNew(sizeof(int64_t));
  return offText(textJoin(textJoin(textJoin(textJoin(textJoin(textJoin(said(none), "; "), said(LIST_OF(int64_t, 7))), "; "), said(LIST_OF(int64_t, 0, 1, 2))), "; "), said(LIST_OF(int64_t, 3, 4, 5))), "empty; just 7; starts at zero; 3, then 2 more");
}

static const char* pairOf(List* xs) {
  const char* matchValue_176_12 = "";
  if (xs->count == 2 && LIST_AT(xs, 0, int64_t) == LIST_AT(xs, 1, int64_t)) {
    int64_t a = LIST_AT(xs, 0, int64_t);
    matchValue_176_12 = textJoin("a pair of ", textOfInt(a));
  } else if (xs->count == 2) {
    int64_t a = LIST_AT(xs, 0, int64_t);
    int64_t b = LIST_AT(xs, 1, int64_t);
    matchValue_176_12 = textJoin(textJoin(textOfInt(a), " and "), textOfInt(b));
  } else if (xs->count >= 1 && LIST_AT(xs, 0, int64_t) < 0) {
    matchValue_176_12 = "starts below zero";
  } else {
    matchValue_176_12 = "something else";
  }
  return matchValue_176_12;
}

static double checkListGuards(void) {
  List* same_ = LIST_OF(int64_t, 4, 4);
  List* two = LIST_OF(int64_t, 1, 2);
  List* low = LIST_OF(int64_t, -1, 2, 3);
  return offText(textJoin(textJoin(textJoin(textJoin(textJoin(textJoin(pairOf(same_), "; "), pairOf(two)), "; "), pairOf(low)), "; "), pairOf(LIST_OF(int64_t, 5))), "a pair of 4; 1 and 2; starts below zero; something else");
}

static const char* command(const char* c) {
  const char* said = "";
  if (textSame(c, "quit") || textSame(c, "exit")) {
    return "bye";
  } else if (startsWith(c, "say ")) {
    said = textSlice(c, 4, (int64_t) strlen(c));
  } else if (textSame(c, "noop")) {}
  return said;
}

static double checkStatements(void) {
  return offText(textJoin(textJoin(textJoin(textJoin(textJoin(textJoin(command("say hi"), "|"), command("quit")), "|"), command("noop")), "|"), command("huh")), "hi|bye||");
}

static double checkNested(void) {
  int64_t a = 2;
  int64_t b = 5;
  const char* matchValue_212_21 = "";
  switch (a) {
    case 1: {
      matchValue_212_21 = "one";
      break;
    }
    case 2: {
      const char* matchValue_212_49 = "";
      switch (b) {
        case 5: {
          matchValue_212_49 = "two-five";
          break;
        }
        default: {
          matchValue_212_49 = "two";
          break;
        }
      }
      matchValue_212_21 = matchValue_212_49;
      break;
    }
    default: {
      matchValue_212_21 = "many";
      break;
    }
  }
  const char* text = (matchValue_212_21);
  return offText(text, "two-five");
}

static List* drop_i64(List* xs, int64_t n) {
  List* out = listNew(sizeof(int64_t));
  for (int64_t i = n; i < xs->count; i = i + 1) {
    listPush(out, (int64_t[]){LIST_AT(xs, i, int64_t)});
  }
  return out;
}
