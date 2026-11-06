#pragma once

#include <stdint.h>

static int64_t gcd(int64_t a, int64_t b);
static int64_t lcm(int64_t a, int64_t b);

static int64_t gcd(int64_t a, int64_t b) {
  int64_t x = a;
  int64_t y = b;
  while (y != 0) {
    int64_t r = x % y;
    x = y;
    y = r;
  }
  return x;
}

static int64_t lcm(int64_t a, int64_t b) {
  return a / gcd(a, b) * b;
}
