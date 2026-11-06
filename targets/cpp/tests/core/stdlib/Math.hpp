#pragma once

#include <cmath>

float sign(float x);
float fract(float x);
float clamp(float x, float low, float high);
float mix(float a, float b, float t);

inline float sign(float x) {
  if (x > 0.0f) {
    return 1.0f;
  }
  if (x < 0.0f) {
    return (float) (0.0 - 1.0);
  }
  return 0.0f;
}

inline float fract(float x) {
  return x - floorf(x);
}

inline float clamp(float x, float low, float high) {
  if (x < low) {
    return low;
  }
  if (x > high) {
    return high;
  }
  return x;
}

inline float mix(float a, float b, float t) {
  return a + (b - a) * t;
}
