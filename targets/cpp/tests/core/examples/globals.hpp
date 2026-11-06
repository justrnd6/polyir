#pragma once

#include <cstdint>

inline int64_t counter = 0;

int64_t bump();

inline int64_t bump() {
  counter = counter + 1;
  return counter;
}
