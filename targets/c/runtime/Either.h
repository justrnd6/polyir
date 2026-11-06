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
