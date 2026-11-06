#pragma once

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

static bool endsWith(const char* s, const char* suffix);
static double offText(const char* got, const char* want);
static const char* dayName(int64_t day);
static double checkNumbers(void);
static const char* classOf(int8_t c);
static double checkChars(void);
static const char* verb(const char* command);
static double checkStrings(void);
static double checkBooleans(void);
static const char* outcome(Either r);
static double checkSums(void);
static const char* status(int64_t code);
static double checkConditionsAmongKeys(void);
static int64_t counter(void);
static double checkCallSubject(void);
static const char* step(const char* name);
static double checkConditions(void);

static bool endsWith(const char* s, const char* suffix) {
  if ((int64_t) strlen(suffix) > (int64_t) strlen(s)) {
    return false;
  }
  return textSame(textSlice(s, (int64_t) strlen(s) - (int64_t) strlen(suffix), (int64_t) strlen(s)), suffix);
}

static double offText(const char* got, const char* want) {
  if (textSame(got, want)) {
    return 0.0;
  }
  return 1.0;
}

static const char* dayName(int64_t day) {
  const char* name = "?";
  switch (day) {
    case 0: case 6: {
      name = "weekend";
      break;
    }
    case 1: {
      name = "monday";
      break;
    }
    default: {
      name = "weekday";
      break;
    }
  }
  return name;
}

static double checkNumbers(void) {
  return offText(textJoin(textJoin(textJoin(dayName(0), dayName(6)), dayName(1)), dayName(3)), "weekendweekendmondayweekday");
}

static const char* classOf(int8_t c) {
  const char* kind_ = "other";
  switch (c) {
    case 97: case 101: case 105: case 111: case 117: {
      kind_ = "vowel";
      break;
    }
    case 32: {
      kind_ = "space";
      break;
    }
  }
  return kind_;
}

static double checkChars(void) {
  return offText(textJoin(textJoin(classOf(101), classOf(32)), classOf(122)), "vowelspaceother");
}

static const char* verb(const char* command) {
  const char* done = "";
  if (textSame(command, "start") || textSame(command, "go")) {
    done = "began";
  } else if (textSame(command, "stop")) {
    done = "halted";
  } else {
    done = "?";
  }
  return done;
}

static double checkStrings(void) {
  return offText(textJoin(textJoin(textJoin(verb("go"), verb("start")), verb("stop")), verb("x")), "beganbeganhalted?");
}

static double checkBooleans(void) {
  const char* said = "";
  for (int64_t i = 0; i < 2; i = i + 1) {
    bool match_57_9 = i == 0;
    switch (match_57_9) {
      case true: {
        said = textJoin(said, "yes");
        break;
      }
      case false: {
        said = textJoin(said, "no");
        break;
      }
    }
  }
  return offText(said, "yesno");
}

static const char* outcome(Either r) {
  const char* said = "";
  if (r.isOk) {
    int64_t v = (*(int64_t*) r.held);
    said = textJoin("ok ", textOfInt(v));
  } else if (!r.isOk) {
    const char* e = (*(const char**) r.held);
    said = textJoin("err ", (e));
  }
  return said;
}

static double checkSums(void) {
  int64_t count = 0;
  int64_t* found = copyBytes((int64_t[]){4}, sizeof(int64_t));
  if (found != NULL) {
    int64_t n = (*(int64_t*) present(found));
    count = n;
  } else if (found == NULL) {
    count = -1;
  }
  return offText(textJoin(textJoin(textJoin(outcome(makeEither(true, (int64_t[]){3}, sizeof(int64_t))), ", "), outcome(makeEither(false, (const char*[]){"no"}, sizeof(const char*)))), textJoin(", ", textOfInt(count))), "ok 3, err no, 4");
}

static const char* status(int64_t code) {
  const char* said = "";
  if (code == 200 || code == 204) {
    said = "done";
  } else if (code >= 500) {
    said = textJoin("retry ", textOfInt(code));
  } else if (code >= 400) {
    int64_t s = code;
    said = textJoin("fail ", textOfInt(s));
  } else {
    said = textJoin("? ", textOfInt(code));
  }
  return said;
}

static double checkConditionsAmongKeys(void) {
  return offText(textJoin(textJoin(textJoin(textJoin(textJoin(textJoin(status(204), "; "), status(503)), "; "), status(404)), "; "), status(301)), "done; retry 503; fail 404; ? 301");
}

static int64_t counter(void) {
  return 500;
}

static double checkCallSubject(void) {
  const char* said = "";
  int64_t match_107_5 = counter();
  if (match_107_5 == 200) {
    said = "ok";
  } else if (match_107_5 >= 500) {
    int64_t s = match_107_5;
    said = textJoin("server ", textOfInt(s));
  } else {
    said = "other";
  }
  return offText(said, "server 500");
}

static const char* step(const char* name) {
  const char* said = "";
  if (endsWith(name, ".poly")) {
    said = textJoin(textJoin("compile ", name), ";");
  } else {
    said = textJoin(textJoin("skip ", name), ";");
  }
  return said;
}

static double checkConditions(void) {
  return offText(textJoin(step("a.poly"), step("b.md")), "compile a.poly;skip b.md;");
}
