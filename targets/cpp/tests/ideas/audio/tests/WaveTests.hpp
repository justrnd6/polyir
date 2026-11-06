#pragma once

#include <cstdint>

#include "../../../core/stdlib/Math.hpp"
#include "../../../core/tests/programs/Check.hpp"
#include "../Wave.hpp"

double checkSignOfNegative();
double checkSignOfZero();
double checkSignOfPositive();
double checkFractOfPositive();
double checkFractOfNegative();
double checkClampAbove();
double checkClampBelow();
double checkMix();
double checkSinePeak();
double checkSineTrough();
double checkSineZeroCrossing();
double checkSquareHigh();
double checkSquareLow();
double checkSawtoothStart();
double checkSawtoothThreeQuarters();
double checkTriangleStart();
double checkTrianglePeak();
double checkTriangleMiddle();
double checkConcertA();
double checkOctaveAboveConcertA();
double checkFadeInHalfway();
double checkFadeHolds();
double checkFadeOutHalfway();
double checkNoiseRange();
double checkNoiseRepeats();
double checkNoiseVaries();

inline double checkSignOfNegative() {
  return off(sign((float) (0.0 - 3.0)), (float) (0.0 - 1.0));
}

inline double checkSignOfZero() {
  return off(sign(0.0f), 0.0f);
}

inline double checkSignOfPositive() {
  return off(sign(2.0f), 1.0f);
}

inline double checkFractOfPositive() {
  return off(fract(2.75f), 0.75f);
}

inline double checkFractOfNegative() {
  return off(fract((float) (0.0 - 0.25)), 0.75f);
}

inline double checkClampAbove() {
  return off(clamp(5.0f, (float) (0.0 - 1.0), 2.0f), 2.0f);
}

inline double checkClampBelow() {
  return off(clamp((float) (0.0 - 5.0), (float) (0.0 - 1.0), 2.0f), (float) (0.0 - 1.0));
}

inline double checkMix() {
  return off(mix(10.0f, 20.0f, 0.25f), 12.5f);
}

inline double checkSinePeak() {
  return off(sine(0.25f), 1.0f);
}

inline double checkSineTrough() {
  return off(sine(0.75f), (float) (0.0 - 1.0));
}

inline double checkSineZeroCrossing() {
  return off(sine(1.5f), 0.0f);
}

inline double checkSquareHigh() {
  return off(square(0.25f), 1.0f);
}

inline double checkSquareLow() {
  return off(square(0.75f), (float) (0.0 - 1.0));
}

inline double checkSawtoothStart() {
  return off(sawtooth(0.0f), (float) (0.0 - 1.0));
}

inline double checkSawtoothThreeQuarters() {
  return off(sawtooth(0.75f), 0.5f);
}

inline double checkTriangleStart() {
  return off(triangle(0.0f), (float) (0.0 - 1.0));
}

inline double checkTrianglePeak() {
  return off(triangle(0.5f), 1.0f);
}

inline double checkTriangleMiddle() {
  return off(triangle(0.25f), 0.0f);
}

inline double checkConcertA() {
  return off(noteFrequency(69.0f), 440.0f);
}

inline double checkOctaveAboveConcertA() {
  return off(noteFrequency(81.0f), 880.0f);
}

inline double checkFadeInHalfway() {
  return off(fade(0.5f, 1.0f, 1.0f, 4.0f), 0.5f);
}

inline double checkFadeHolds() {
  return off(fade(2.0f, 1.0f, 1.0f, 4.0f), 1.0f);
}

inline double checkFadeOutHalfway() {
  return off(fade(3.5f, 1.0f, 1.0f, 4.0f), 0.5f);
}

inline double checkNoiseRange() {
  int64_t outside = 0;
  for (int64_t seed = 0; seed < 1000; seed = seed + 1) {
    if (noise((int32_t) seed) > 1.0f || noise((int32_t) seed) < (float) (0.0 - 1.0)) {
      outside = outside + 1;
    }
  }
  return offInt(outside, 0);
}

inline double checkNoiseRepeats() {
  return off(noise(42) - noise(42), 0.0f);
}

inline double checkNoiseVaries() {
  if (noise(1) == noise(2) && noise(2) == noise(3)) {
    return 1.0;
  }
  return 0.0;
}
