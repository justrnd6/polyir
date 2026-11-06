#pragma once

#include <stdint.h>

static float convolveSample(int32_t i, float* x, float* h, int32_t* p);

static float convolveSample(int32_t i, float* x, float* h, int32_t* p) {
  int32_t nx = p[0];
  int32_t nh = p[1];
  int32_t first = i - (nx - 1);
  if (first < 0) {
    first = 0;
  }
  int32_t last = i;
  if (last > nh - 1) {
    last = nh - 1;
  }
  float sum = 0.0f;
  for (int32_t j = first; j <= last; j = j + 1) {
    sum = sum + x[i - j] * h[j];
  }
  return sum;
}
