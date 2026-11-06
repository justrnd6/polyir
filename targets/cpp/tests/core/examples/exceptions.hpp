#pragma once

#include <cstdint>

#ifndef POLYIR_THROWN_HPP
#define POLYIR_THROWN_HPP

// What `throw` throws: the code thrown.
struct Thrown {
  int64_t value;
};
#endif

int64_t riskyDouble(int64_t x);
int64_t safeDouble(int64_t x);
int64_t nestedTryCheck(int64_t x);

inline int64_t riskyDouble(int64_t x) {
  if (x < 0) {
    throw Thrown{99};
  }
  return x * 2;
}

inline int64_t safeDouble(int64_t x) {
  int64_t result = 0;
  try {
    result = riskyDouble(x);
  } catch (const Thrown& thrown) {
    const int64_t code = thrown.value;
    result = code;
  }
  return result;
}

inline int64_t nestedTryCheck(int64_t x) {
  int64_t result = 0;
  try {
    try {
      result = riskyDouble(x);
    } catch (const Thrown& thrown) {
      const int64_t innerCode = thrown.value;
      result = innerCode + 1;
      throw Thrown{result};
    }
  } catch (const Thrown& thrown) {
    const int64_t outerCode = thrown.value;
    result = outerCode * 10;
  }
  return result;
}
