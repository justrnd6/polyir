#pragma once

#include <math.h>

static float sign(float x);
static float fract(float x);
static float clamp(float x, float low, float high);
static float mix(float a, float b, float t);

static float sign(float x) {
  if (x > 0.0f) {
    return 1.0f;
  }
  if (x < 0.0f) {
    return (float) (0.0 - 1.0);
  }
  return 0.0f;
}

static float fract(float x) {
  return x - floorf(x);
}

static float clamp(float x, float low, float high) {
  if (x < low) {
    return low;
  }
  if (x > high) {
    return high;
  }
  return x;
}

static float mix(float a, float b, float t) {
  return a + (b - a) * t;
}
