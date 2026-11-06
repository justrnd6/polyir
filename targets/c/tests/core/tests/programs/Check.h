#pragma once

#include <math.h>
#include <stdint.h>

static double off(float got, float expected);
static double offInt(int64_t got, int64_t expected);
static double offRelative(float got, float expected);
static float amplitudeAt(float* data, int64_t start, int64_t count, int64_t rate, float hz);

static double off(float got, float expected) {
  return (double) (got - expected);
}

static double offInt(int64_t got, int64_t expected) {
  return (double) (got - expected);
}

static double offRelative(float got, float expected) {
  return (double) ((got - expected) / expected);
}

static float amplitudeAt(float* data, int64_t start, int64_t count, int64_t rate, float hz) {
  double re = 0.0;
  double im = 0.0;
  for (int64_t k = 0; k < count; k = k + 1) {
    double angle = 6.283185307179586 * (double) hz * (double) (start + k) / (double) rate;
    re = re + (double) data[start + k] * cos(angle);
    im = im + (double) data[start + k] * sin(angle);
  }
  return (float) (2.0 * sqrt(re * re + im * im) / (double) count);
}
