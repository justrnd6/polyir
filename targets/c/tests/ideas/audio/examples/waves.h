#pragma once

#include <stdint.h>

#include "../Wave.h"

static int64_t concertAHeight(void);
static int64_t softAttack(void);
static float chordSample(float t);

static int64_t concertAHeight(void) {
  float quarterCycle = (float) (0.25 / (double) noteFrequency(69.0f));
  return (int64_t) (sine(noteFrequency(69.0f) * quarterCycle) * 1000.0f);
}

static int64_t softAttack(void) {
  return (int64_t) (fade(0.5f, 1.0f, 1.0f, 4.0f) * 1000.0f);
}

static float chordSample(float t) {
  return (float) (0.3 * (double) (sine(noteFrequency(60.0f) * t) + sine(noteFrequency(64.0f) * t) + sine(noteFrequency(67.0f) * t)));
}
