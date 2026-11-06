#pragma once

#include <stdint.h>

static int64_t reverse4(int64_t a, int64_t b, int64_t c);
static int64_t duplicateFirstTwo(int64_t a, int64_t b, int64_t c);

static int64_t reverse4(int64_t a, int64_t b, int64_t c) {
  int64_t __attribute__((vector_size(4 * sizeof(int64_t)))) v = (int64_t __attribute__((vector_size(4 * sizeof(int64_t))))){a, b, c, 40};
  int64_t __attribute__((vector_size(4 * sizeof(int64_t)))) r = __builtin_shufflevector(v, v, 3, 2, 1, 0);
  return r[0] * 1000 + r[1] * 100 + r[2] * 10 + r[3];
}

static int64_t duplicateFirstTwo(int64_t a, int64_t b, int64_t c) {
  int64_t __attribute__((vector_size(4 * sizeof(int64_t)))) v = (int64_t __attribute__((vector_size(4 * sizeof(int64_t))))){a, b, c, 4};
  int64_t __attribute__((vector_size(4 * sizeof(int64_t)))) r = __builtin_shufflevector(v, v, 0, 0, 1, 1);
  return r[0] * 1000 + r[1] * 100 + r[2] * 10 + r[3];
}
