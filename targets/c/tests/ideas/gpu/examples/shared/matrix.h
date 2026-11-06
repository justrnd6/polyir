#pragma once

#include <stdint.h>

static float __attribute__((vector_size(4 * sizeof(float)))) transform(float __attribute__((vector_size(4 * sizeof(float)))) p, float* m);
static int64_t identity(float* m);
static int64_t multiply4(float* a, float* b, float* out);

static float __attribute__((vector_size(4 * sizeof(float)))) transform(float __attribute__((vector_size(4 * sizeof(float)))) p, float* m) {
  return (float __attribute__((vector_size(4 * sizeof(float))))){m[0] * p[0] + m[1] * p[1] + m[2] * p[2] + m[3] * p[3], m[4] * p[0] + m[5] * p[1] + m[6] * p[2] + m[7] * p[3], m[8] * p[0] + m[9] * p[1] + m[10] * p[2] + m[11] * p[3], m[12] * p[0] + m[13] * p[1] + m[14] * p[2] + m[15] * p[3]};
}

static int64_t identity(float* m) {
  for (int64_t k = 0; k < 16; k = k + 1) {
    m[k] = 0.0f;
  }
  m[0] = 1.0f;
  m[5] = 1.0f;
  m[10] = 1.0f;
  m[15] = 1.0f;
  return 0;
}

static int64_t multiply4(float* a, float* b, float* out) {
  for (int64_t row = 0; row < 4; row = row + 1) {
    for (int64_t col = 0; col < 4; col = col + 1) {
      float sum = 0.0f;
      for (int64_t k = 0; k < 4; k = k + 1) {
        sum = sum + a[row * 4 + k] * b[k * 4 + col];
      }
      out[row * 4 + col] = sum;
    }
  }
  return 0;
}
