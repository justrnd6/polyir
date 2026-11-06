#pragma once

#include <cstdint>

int64_t classify(int64_t x);
int64_t maxVal(int64_t a, int64_t b);
int64_t absViaIf(int64_t x);

inline int64_t classify(int64_t x) {
  return x < 0 ? 0 - 1 : x == 0 ? 0 : 1;
}

inline int64_t maxVal(int64_t a, int64_t b) {
  int64_t result = 0;
  if (a < b) {
    result = b;
  } else {
    result = a;
  }
  return result;
}

inline int64_t absViaIf(int64_t x) {
  int64_t r = x;
  if (r < 0) {
    r = 0 - r;
  }
  return r;
}
