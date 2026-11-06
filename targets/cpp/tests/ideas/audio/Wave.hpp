#pragma once

#include <cmath>
#include <cstdint>

#include "../../core/stdlib/Math.hpp"

float sine(float phase);
float square(float phase);
float sawtooth(float phase);
float triangle(float phase);
float noise(int32_t seed);
float fade(float t, float attack, float release, float total);
float noteFrequency(float m);

inline float sine(float phase) {
  return sinf((float) (6.2831853 * (double) fract(phase)));
}

inline float square(float phase) {
  if (fract(phase) < 0.5f) {
    return 1.0f;
  }
  return (float) (0.0 - 1.0);
}

inline float sawtooth(float phase) {
  return (float) (2.0 * (double) fract(phase) - 1.0);
}

inline float triangle(float phase) {
  return (float) (1.0 - 4.0 * (double) fabsf(fract(phase) - 0.5f));
}

inline float noise(int32_t seed) {
  const int32_t a = (seed * 75 + 74) % 65537;
  const int32_t b = (a * 75 + 74) % 65537;
  return (float) (2.0 * (double) (float) b / 65537.0 - 1.0);
}

inline float fade(float t, float attack, float release, float total) {
  float level = t < attack ? t / attack : 1.0f;
  if (total - t < release) {
    level = level * clamp((total - t) / release, 0.0f, 1.0f);
  }
  return level;
}

inline float noteFrequency(float m) {
  return (float) (440.0 * (double) powf(2.0f, (m - 69.0f) / 12.0f));
}
