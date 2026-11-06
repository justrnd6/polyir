#pragma once

#include <cstdint>

int64_t bitwisePrecedenceCheck();
int64_t shiftPrecedenceCheck();

inline int64_t bitwisePrecedenceCheck() {
  const int64_t a = 6;
  const int64_t b = 3;
  const int64_t c = 1;
  const int64_t d = 2;
  return a | b & c ^ d;
}

inline int64_t shiftPrecedenceCheck() {
  const int64_t x = 1;
  return x << 2 & 3;
}
