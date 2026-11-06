#pragma once

#include <stdint.h>

#include "../../../core/stdlib/Math.h"
#include "../../../core/tests/programs/Check.h"
#include "../Wave.h"

static double checkSignOfNegative(void);
static double checkSignOfZero(void);
static double checkSignOfPositive(void);
static double checkFractOfPositive(void);
static double checkFractOfNegative(void);
static double checkClampAbove(void);
static double checkClampBelow(void);
static double checkMix(void);
static double checkSinePeak(void);
static double checkSineTrough(void);
static double checkSineZeroCrossing(void);
static double checkSquareHigh(void);
static double checkSquareLow(void);
static double checkSawtoothStart(void);
static double checkSawtoothThreeQuarters(void);
static double checkTriangleStart(void);
static double checkTrianglePeak(void);
static double checkTriangleMiddle(void);
static double checkConcertA(void);
static double checkOctaveAboveConcertA(void);
static double checkFadeInHalfway(void);
static double checkFadeHolds(void);
static double checkFadeOutHalfway(void);
static double checkNoiseRange(void);
static double checkNoiseRepeats(void);
static double checkNoiseVaries(void);

static double checkSignOfNegative(void) {
  return off(sign((float) (0.0 - 3.0)), (float) (0.0 - 1.0));
}

static double checkSignOfZero(void) {
  return off(sign(0.0f), 0.0f);
}

static double checkSignOfPositive(void) {
  return off(sign(2.0f), 1.0f);
}

static double checkFractOfPositive(void) {
  return off(fract(2.75f), 0.75f);
}

static double checkFractOfNegative(void) {
  return off(fract((float) (0.0 - 0.25)), 0.75f);
}

static double checkClampAbove(void) {
  return off(clamp(5.0f, (float) (0.0 - 1.0), 2.0f), 2.0f);
}

static double checkClampBelow(void) {
  return off(clamp((float) (0.0 - 5.0), (float) (0.0 - 1.0), 2.0f), (float) (0.0 - 1.0));
}

static double checkMix(void) {
  return off(mix(10.0f, 20.0f, 0.25f), 12.5f);
}

static double checkSinePeak(void) {
  return off(sine(0.25f), 1.0f);
}

static double checkSineTrough(void) {
  return off(sine(0.75f), (float) (0.0 - 1.0));
}

static double checkSineZeroCrossing(void) {
  return off(sine(1.5f), 0.0f);
}

static double checkSquareHigh(void) {
  return off(square(0.25f), 1.0f);
}

static double checkSquareLow(void) {
  return off(square(0.75f), (float) (0.0 - 1.0));
}

static double checkSawtoothStart(void) {
  return off(sawtooth(0.0f), (float) (0.0 - 1.0));
}

static double checkSawtoothThreeQuarters(void) {
  return off(sawtooth(0.75f), 0.5f);
}

static double checkTriangleStart(void) {
  return off(triangle(0.0f), (float) (0.0 - 1.0));
}

static double checkTrianglePeak(void) {
  return off(triangle(0.5f), 1.0f);
}

static double checkTriangleMiddle(void) {
  return off(triangle(0.25f), 0.0f);
}

static double checkConcertA(void) {
  return off(noteFrequency(69.0f), 440.0f);
}

static double checkOctaveAboveConcertA(void) {
  return off(noteFrequency(81.0f), 880.0f);
}

static double checkFadeInHalfway(void) {
  return off(fade(0.5f, 1.0f, 1.0f, 4.0f), 0.5f);
}

static double checkFadeHolds(void) {
  return off(fade(2.0f, 1.0f, 1.0f, 4.0f), 1.0f);
}

static double checkFadeOutHalfway(void) {
  return off(fade(3.5f, 1.0f, 1.0f, 4.0f), 0.5f);
}

static double checkNoiseRange(void) {
  int64_t outside = 0;
  for (int64_t seed = 0; seed < 1000; seed = seed + 1) {
    if (noise((int32_t) seed) > 1.0f || noise((int32_t) seed) < (float) (0.0 - 1.0)) {
      outside = outside + 1;
    }
  }
  return offInt(outside, 0);
}

static double checkNoiseRepeats(void) {
  return off(noise(42) - noise(42), 0.0f);
}

static double checkNoiseVaries(void) {
  if (noise(1) == noise(2) && noise(2) == noise(3)) {
    return 1.0;
  }
  return 0.0;
}
