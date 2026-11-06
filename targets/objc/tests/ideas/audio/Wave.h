#pragma once

#import <math.h>
#import <stdint.h>

#import "../../core/stdlib/Math.h"

static float sine(float phase);
static float square(float phase);
static float sawtooth(float phase);
static float triangle(float phase);
static float noise(int32_t seed);
static float fade(float t, float attack, float release, float total);
static float noteFrequency(float m);

static float sine(float phase) {
  return sinf((float) (6.2831853 * (double) fract(phase)));
}

static float square(float phase) {
  if (fract(phase) < 0.5f) {
    return 1.0f;
  }
  return (float) (0.0 - 1.0);
}

static float sawtooth(float phase) {
  return (float) (2.0 * (double) fract(phase) - 1.0);
}

static float triangle(float phase) {
  return (float) (1.0 - 4.0 * (double) fabsf(fract(phase) - 0.5f));
}

static float noise(int32_t seed) {
  int32_t a = (seed * 75 + 74) % 65537;
  int32_t b = (a * 75 + 74) % 65537;
  return (float) (2.0 * (double) (float) b / 65537.0 - 1.0);
}

static float fade(float t, float attack, float release, float total) {
  float level = t < attack ? t / attack : 1.0f;
  if (total - t < release) {
    level = level * clamp((total - t) / release, 0.0f, 1.0f);
  }
  return level;
}

static float noteFrequency(float m) {
  return (float) (440.0 * (double) powf(2.0f, (m - 69.0f) / 12.0f));
}
