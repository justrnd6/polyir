#pragma once

#include <cmath>
#include <cstdint>

#include "Check.hpp"

double checkFloatRemainder();
double checkFloatRemainderKeepsTheSignOfTheLeftNumber();
double checkFloatRemainderIgnoresTheSignOfTheRightNumber();
double checkFloatRemainderOfNumbersThatDivideEvenly();
double checkFloatRemainderOfZero();
double checkFloatRemainderOfAFractionOfOne();
double checkWholeNumberRemainder();

inline double checkFloatRemainder() {
  const float a = 7.5f;
  const float b = 2.0f;
  return off(std::fmod(a, b), 1.5f);
}

inline double checkFloatRemainderKeepsTheSignOfTheLeftNumber() {
  const float a = (float) (0.0 - 7.5);
  const float b = 2.0f;
  return off(std::fmod(a, b), (float) (0.0 - 1.5));
}

inline double checkFloatRemainderIgnoresTheSignOfTheRightNumber() {
  const float a = 7.5f;
  const float b = (float) (0.0 - 2.0);
  return off(std::fmod(a, b), 1.5f);
}

inline double checkFloatRemainderOfNumbersThatDivideEvenly() {
  const float b = 47.0f;
  const float c = 97.0f;
  return off((float) std::fmod(2867.0, (double) b), 0.0f) + off((float) std::fmod(5969.0, (double) b), 0.0f) + off((float) std::fmod(9409.0, (double) c), 0.0f);
}

inline double checkFloatRemainderOfZero() {
  const float a = 0.0f;
  const float b = 5.0f;
  return off(std::fmod(a, b), 0.0f);
}

inline double checkFloatRemainderOfAFractionOfOne() {
  const float a = 0.75f;
  const float b = 1.0f;
  return off(std::fmod(a, b), 0.75f);
}

inline double checkWholeNumberRemainder() {
  const int64_t a = 17;
  const int64_t b = 5;
  return offInt(a % b, 2) + offInt((0 - a) % b, 0 - 2);
}
