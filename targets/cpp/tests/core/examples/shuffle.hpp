#pragma once

#include <array>
#include <cstdint>

#ifndef POLYIR_LANES_HPP
#define POLYIR_LANES_HPP

// An operator lane by lane on two vectors; the lanes of a vector in a new order.
template <typename T, std::size_t N, typename Op>
std::array<T, N> lanes(const std::array<T, N>& a, const std::array<T, N>& b, Op op) {
  std::array<T, N> out{};
  for (std::size_t i = 0; i < N; i++) {
    out[i] = op(a[i], b[i]);
  }
  return out;
}

template <typename T, std::size_t N, typename... Lanes>
std::array<T, sizeof...(Lanes)> shuffle(const std::array<T, N>& v, Lanes... picked) {
  return {v[picked]...};
}
#endif

int64_t reverse4(int64_t a, int64_t b, int64_t c);
int64_t duplicateFirstTwo(int64_t a, int64_t b, int64_t c);

inline int64_t reverse4(int64_t a, int64_t b, int64_t c) {
  const std::array<int64_t, 4> v = std::array<int64_t, 4>{{a, b, c, 40}};
  const std::array<int64_t, 4> r = shuffle(v, 3, 2, 1, 0);
  return r[0] * 1000 + r[1] * 100 + r[2] * 10 + r[3];
}

inline int64_t duplicateFirstTwo(int64_t a, int64_t b, int64_t c) {
  const std::array<int64_t, 4> v = std::array<int64_t, 4>{{a, b, c, 4}};
  const std::array<int64_t, 4> r = shuffle(v, 0, 0, 1, 1);
  return r[0] * 1000 + r[1] * 100 + r[2] * 10 + r[3];
}
