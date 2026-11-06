#pragma once

#include <cstdint>

#include "../Wave.hpp"

int64_t concertAHeight();
int64_t softAttack();
float chordSample(float t);

inline int64_t concertAHeight() {
  const float quarterCycle = (float) (0.25 / (double) noteFrequency(69.0f));
  return (int64_t) (sine(noteFrequency(69.0f) * quarterCycle) * 1000.0f);
}

inline int64_t softAttack() {
  return (int64_t) (fade(0.5f, 1.0f, 1.0f, 4.0f) * 1000.0f);
}

inline float chordSample(float t) {
  return (float) (0.3 * (double) (sine(noteFrequency(60.0f) * t) + sine(noteFrequency(64.0f) * t) + sine(noteFrequency(67.0f) * t)));
}
