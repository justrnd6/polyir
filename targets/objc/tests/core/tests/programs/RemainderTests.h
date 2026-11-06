#pragma once

#import <math.h>
#import <stdint.h>

#import "Check.h"

static double checkFloatRemainder(void);
static double checkFloatRemainderKeepsTheSignOfTheLeftNumber(void);
static double checkFloatRemainderIgnoresTheSignOfTheRightNumber(void);
static double checkFloatRemainderOfNumbersThatDivideEvenly(void);
static double checkFloatRemainderOfZero(void);
static double checkFloatRemainderOfAFractionOfOne(void);
static double checkWholeNumberRemainder(void);

static double checkFloatRemainder(void) {
  float a = 7.5f;
  float b = 2.0f;
  return off(fmod(a, b), 1.5f);
}

static double checkFloatRemainderKeepsTheSignOfTheLeftNumber(void) {
  float a = (float) (0.0 - 7.5);
  float b = 2.0f;
  return off(fmod(a, b), (float) (0.0 - 1.5));
}

static double checkFloatRemainderIgnoresTheSignOfTheRightNumber(void) {
  float a = 7.5f;
  float b = (float) (0.0 - 2.0);
  return off(fmod(a, b), 1.5f);
}

static double checkFloatRemainderOfNumbersThatDivideEvenly(void) {
  float b = 47.0f;
  float c = 97.0f;
  return off((float) fmod(2867.0, (double) b), 0.0f) + off((float) fmod(5969.0, (double) b), 0.0f) + off((float) fmod(9409.0, (double) c), 0.0f);
}

static double checkFloatRemainderOfZero(void) {
  float a = 0.0f;
  float b = 5.0f;
  return off(fmod(a, b), 0.0f);
}

static double checkFloatRemainderOfAFractionOfOne(void) {
  float a = 0.75f;
  float b = 1.0f;
  return off(fmod(a, b), 0.75f);
}

static double checkWholeNumberRemainder(void) {
  int64_t a = 17;
  int64_t b = 5;
  return offInt(a % b, 2) + offInt((0 - a) % b, 0 - 2);
}
