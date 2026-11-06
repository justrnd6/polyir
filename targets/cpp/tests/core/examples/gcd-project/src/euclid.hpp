#pragma once

#include <cstdint>

int64_t gcd(int64_t a, int64_t b);
int64_t lcm(int64_t a, int64_t b);

inline int64_t gcd(int64_t a, int64_t b) {
  int64_t x = a;
  int64_t y = b;
  while (y != 0) {
    const int64_t r = x % y;
    x = y;
    y = r;
  }
  return x;
}

inline int64_t lcm(int64_t a, int64_t b) {
  return a / gcd(a, b) * b;
}
