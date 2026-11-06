#pragma once

#include <stdint.h>

static float pairSum(int32_t i, float* src, int32_t n);
static int32_t pairSumInt(int32_t i, int32_t* src, int32_t n);
static float pairMax(int32_t i, float* src, int32_t n);
static int64_t passCount(int64_t n);
static float cpuSum(float* values, int64_t n);
static float cpuMax(float* values, int64_t n);
static int64_t fillTestData(float* values, int64_t n);

static float pairSum(int32_t i, float* src, int32_t n) {
  int32_t a = (int32_t) (2 * (int64_t) i);
  float total = src[a];
  if (a + 1 < n) {
    total = total + src[a + 1];
  }
  return total;
}

static int32_t pairSumInt(int32_t i, int32_t* src, int32_t n) {
  int32_t a = (int32_t) (2 * (int64_t) i);
  int32_t total = src[a];
  if (a + 1 < n) {
    total = total + src[a + 1];
  }
  return total;
}

static float pairMax(int32_t i, float* src, int32_t n) {
  int32_t a = (int32_t) (2 * (int64_t) i);
  float best = src[a];
  if (a + 1 < n) {
    if (src[a + 1] > best) {
      best = src[a + 1];
    }
  }
  return best;
}

static int64_t passCount(int64_t n) {
  int64_t passes = 0;
  int64_t left = n;
  for (int64_t k = 0; k < 40; k = k + 1) {
    if (left > 1) {
      left = (left + 1) / 2;
      passes = passes + 1;
    }
  }
  return passes;
}

static float cpuSum(float* values, int64_t n) {
  float total = 0.0f;
  for (int64_t i = 0; i < n; i = i + 1) {
    total = total + values[i];
  }
  return total;
}

static float cpuMax(float* values, int64_t n) {
  float best = values[0];
  for (int64_t i = 1; i < n; i = i + 1) {
    if (values[i] > best) {
      best = values[i];
    }
  }
  return best;
}

static int64_t fillTestData(float* values, int64_t n) {
  for (int64_t i = 0; i < n; i = i + 1) {
    values[i] = (float) (i % 7);
  }
  values[n / 3] = 1000.0f;
  return 0;
}
