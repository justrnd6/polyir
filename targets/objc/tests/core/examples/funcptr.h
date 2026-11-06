#pragma once

#import <stdint.h>

static int64_t doubleIt(int64_t x);
static int64_t applyTwice(__typeof__(int64_t (*)(int64_t)) f, int64_t x);
static int64_t applyDoubleTwice(int64_t x);
static int64_t directDoubleCheck(int64_t x);
static int64_t addOne(int64_t x);
static int64_t pickAndApply(int64_t x, int64_t useAddOne);
static int64_t applyNTimes(__typeof__(int64_t (*)(int64_t)) f, int64_t x, int64_t n);
static int64_t doubleFourTimes(int64_t x);

static int64_t doubleIt(int64_t x) {
  return x * 2;
}

static int64_t applyTwice(__typeof__(int64_t (*)(int64_t)) f, int64_t x) {
  int64_t once = f(x);
  return f(once);
}

static int64_t applyDoubleTwice(int64_t x) {
  return applyTwice(doubleIt, x);
}

static int64_t directDoubleCheck(int64_t x) {
  return doubleIt(x);
}

static int64_t addOne(int64_t x) {
  return x + 1;
}

static int64_t pickAndApply(int64_t x, int64_t useAddOne) {
  __typeof__(int64_t (*)(int64_t)) fn = doubleIt;
  if (useAddOne != 0) {
    fn = addOne;
  }
  return fn(x);
}

static int64_t applyNTimes(__typeof__(int64_t (*)(int64_t)) f, int64_t x, int64_t n) {
  int64_t acc = x;
  for (int64_t i = 0; i < n; i = i + 1) {
    acc = f(acc);
  }
  return acc;
}

static int64_t doubleFourTimes(int64_t x) {
  return applyNTimes(doubleIt, x, 4);
}
