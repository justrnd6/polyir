#pragma once

#import <math.h>
#import <stdint.h>

static float scale(float x, float k);
static float norm(float a, float b);
static int32_t sumTo(int32_t n);
static float lane(float __attribute__((vector_size(4 * sizeof(float)))) v);
static float smooth(int32_t i, float* src, int32_t n);

static float scale(float x, float k) {
  return x * k;
}

static float norm(float a, float b) {
  float s = scale(a, 2.0f) + b * b;
  return sqrtf(s);
}

static int32_t sumTo(int32_t n) {
  int32_t acc = 0;
  for (int64_t i = 0; i < (int64_t) n; i = i + 1) {
    if (i % 2 == 0) {
      acc = acc + (int32_t) i;
    }
  }
  return acc;
}

static float lane(float __attribute__((vector_size(4 * sizeof(float)))) v) {
  return v[0] + v[3];
}

static float smooth(int32_t i, float* src, int32_t n) {
  if (i == 0) {
    return src[i];
  }
  if (i == n - 1) {
    return src[i];
  }
  return (src[i - 1] + src[i] + src[i + 1]) / 3.0f;
}
