#pragma once

#include <cstdint>
#include <functional>

int64_t doubleIt(int64_t x);
int64_t applyTwice(std::function<int64_t(int64_t)> f, int64_t x);
int64_t applyDoubleTwice(int64_t x);
int64_t directDoubleCheck(int64_t x);
int64_t addOne(int64_t x);
int64_t pickAndApply(int64_t x, int64_t useAddOne);
int64_t applyNTimes(std::function<int64_t(int64_t)> f, int64_t x, int64_t n);
int64_t doubleFourTimes(int64_t x);

inline int64_t doubleIt(int64_t x) {
  return x * 2;
}

inline int64_t applyTwice(std::function<int64_t(int64_t)> f, int64_t x) {
  const int64_t once = f(x);
  return f(once);
}

inline int64_t applyDoubleTwice(int64_t x) {
  return applyTwice(doubleIt, x);
}

inline int64_t directDoubleCheck(int64_t x) {
  return doubleIt(x);
}

inline int64_t addOne(int64_t x) {
  return x + 1;
}

inline int64_t pickAndApply(int64_t x, int64_t useAddOne) {
  std::function<int64_t(int64_t)> fn = doubleIt;
  if (useAddOne != 0) {
    fn = addOne;
  }
  return fn(x);
}

inline int64_t applyNTimes(std::function<int64_t(int64_t)> f, int64_t x, int64_t n) {
  int64_t acc = x;
  for (int64_t i = 0; i < n; i = i + 1) {
    acc = f(acc);
  }
  return acc;
}

inline int64_t doubleFourTimes(int64_t x) {
  return applyNTimes(doubleIt, x, 4);
}
