#pragma once

#import <math.h>

#import "Check.h"

static float sign(float x);
static float fract(float x);
static float clamp(float x, float low, float high);
static float mix(float a, float b, float t);
static double checkSign(void);
static double checkFract(void);
static double checkClamp(void);
static double checkMix(void);
static double checkNestedCalls(void);
static float rescale(float x);
static double checkOwnFunctionsStillWork(void);

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

static double checkSign(void) {
  return off(sign((float) (0.0 - 3.0)), (float) (0.0 - 1.0)) + off(sign(0.0f), 0.0f) + off(sign(2.0f), 1.0f);
}

static double checkFract(void) {
  return off(fract(2.75f), 0.75f) + off(fract((float) (0.0 - 0.25)), 0.75f);
}

static double checkClamp(void) {
  return off(clamp(5.0f, (float) (0.0 - 1.0), 2.0f), 2.0f) + off(clamp((float) (0.0 - 5.0), (float) (0.0 - 1.0), 2.0f), (float) (0.0 - 1.0));
}

static double checkMix(void) {
  return off(mix(10.0f, 20.0f, 0.25f), 12.5f);
}

static double checkNestedCalls(void) {
  return off(clamp(fract(1.75f) * 2.0f, (float) (0.0 - 1.0), 1.0f), 1.0f) + off(mix(sign(2.0f), fract(0.5f), 0.5f), 0.75f);
}

static float rescale(float x) {
  return x * 3.0f;
}

static double checkOwnFunctionsStillWork(void) {
  return off(rescale(fract(1.25f)), 0.75f);
}
