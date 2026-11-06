#pragma once

#include <cstdint>
#include <functional>

int64_t counterTwice();
std::function<int64_t(int64_t)> makeMultiplier(int64_t factor);
int64_t tripleOf(int64_t x);
int64_t sharedCounter();

inline int64_t counterTwice() {
  auto count = std::make_shared<int64_t>(0);
  const std::function<int64_t()> inc = [=]() -> int64_t {
    (*count) = (*count) + 1;
    return (*count);
  };
  const int64_t a = inc();
  const int64_t b = inc();
  return a * 100 + b;
}

inline std::function<int64_t(int64_t)> makeMultiplier(int64_t factor) {
  const std::function<int64_t(int64_t)> mul = [=](int64_t x) { return x * factor; };
  return mul;
}

inline int64_t tripleOf(int64_t x) {
  const std::function<int64_t(int64_t)> triple = makeMultiplier(3);
  return triple(x);
}

inline int64_t sharedCounter() {
  auto shared = std::make_shared<int64_t>(0);
  const std::function<int64_t()> bumpBy1 = [=]() -> int64_t {
    (*shared) = (*shared) + 1;
    return (*shared);
  };
  const std::function<int64_t()> bumpBy10 = [=]() -> int64_t {
    (*shared) = (*shared) + 10;
    return (*shared);
  };
  const int64_t a = bumpBy1();
  const int64_t b = bumpBy10();
  const int64_t c = bumpBy1();
  return a * 10000 + b * 100 + c;
}
