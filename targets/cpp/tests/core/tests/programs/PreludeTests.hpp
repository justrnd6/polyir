#pragma once

#include <cmath>

#include "Check.hpp"

float sign(float x);
float fract(float x);
float clamp(float x, float low, float high);
float mix(float a, float b, float t);
double checkSign();
double checkFract();
double checkClamp();
double checkMix();
double checkNestedCalls();
float rescale(float x);
double checkOwnFunctionsStillWork();

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

inline double checkSign() {
  return off(sign((float) (0.0 - 3.0)), (float) (0.0 - 1.0)) + off(sign(0.0f), 0.0f) + off(sign(2.0f), 1.0f);
}

inline double checkFract() {
  return off(fract(2.75f), 0.75f) + off(fract((float) (0.0 - 0.25)), 0.75f);
}

inline double checkClamp() {
  return off(clamp(5.0f, (float) (0.0 - 1.0), 2.0f), 2.0f) + off(clamp((float) (0.0 - 5.0), (float) (0.0 - 1.0), 2.0f), (float) (0.0 - 1.0));
}

inline double checkMix() {
  return off(mix(10.0f, 20.0f, 0.25f), 12.5f);
}

inline double checkNestedCalls() {
  return off(clamp(fract(1.75f) * 2.0f, (float) (0.0 - 1.0), 1.0f), 1.0f) + off(mix(sign(2.0f), fract(0.5f), 0.5f), 0.75f);
}

inline float rescale(float x) {
  return x * 3.0f;
}

inline double checkOwnFunctionsStillWork() {
  return off(rescale(fract(1.25f)), 0.75f);
}
