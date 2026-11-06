#pragma once

#include <math.h>
#include <stdint.h>

#include "../../../../core/stdlib/Math.h"
#include "../../../audio/Wave.h"

static float synth(int32_t i, float* p);
static float squareWave(int32_t i, float* p);
static float partialSum(int32_t i, float* p);
static float melody(int32_t i, float* notes, float* p);

static float synth(int32_t i, float* p) {
  float t = (float) i / p[0];
  float envelope = fade(t, p[2], p[3], p[4]);
  float wobble = p[6] * sine(p[5] * t);
  float sum = 0.0f;
  for (int64_t k = 0; k < (int64_t) (int32_t) p[1]; k = k + 1) {
    float cycles = p[8 + 2 * k] * t;
    sum = sum + p[9 + 2 * k] * sinf((float) (6.2831853 * (double) (cycles - floorf(cycles)) + (double) wobble));
  }
  return p[7] * envelope * sum;
}

static float squareWave(int32_t i, float* p) {
  return sign(sine(p[1] * (float) i / p[0]));
}

static float partialSum(int32_t i, float* p) {
  float x = (float) (6.2831853 * (double) (float) (i % (int32_t) p[0]) / (double) p[0]);
  float sum = 0.0f;
  for (int64_t h = 1; h <= (int64_t) (int32_t) p[2]; h = h + 1) {
    float number = (float) h;
    float wave = sinf(number * x);
    if (p[1] < 0.5f) {
      if (h % 2 == 1) {
        sum = sum + wave / number;
      }
    }
    if (p[1] > 0.5f && p[1] < 1.5f) {
      if (h % 2 == 1) {
        sum = sum + wave / number;
      }
      if (h % 2 == 0) {
        sum = sum - wave / number;
      }
    }
    if (p[1] > 1.5f) {
      if (h % 4 == 1) {
        sum = sum + wave / (number * number);
      }
      if (h % 4 == 3) {
        sum = sum - wave / (number * number);
      }
    }
  }
  if (p[1] < 0.5f) {
    return (float) (1.2732395 * (double) sum);
  }
  if (p[1] < 1.5f) {
    return (float) (0.6366198 * (double) sum);
  }
  return (float) (0.8105695 * (double) sum);
}

static float melody(int32_t i, float* notes, float* p) {
  float t = (float) i / p[0];
  float sum = 0.0f;
  for (int64_t n = 0; n < (int64_t) (int32_t) p[1]; n = n + 1) {
    float start = notes[3 * n];
    float length = notes[3 * n + 1];
    float local = t - start;
    if (local >= 0.0f && local < length) {
      float envelope = fade(local, 0.02f, 0.08f, length);
      float f = notes[3 * n + 2];
      sum = sum + envelope * (sine(f * t) + (float) (0.5 * (double) sine((float) (2.0 * (double) f * (double) t))) + (float) (0.25 * (double) sine((float) (3.0 * (double) f * (double) t))));
    }
  }
  return p[2] * sum;
}
