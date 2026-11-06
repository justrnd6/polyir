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

double vecMulSum(double a, double b);
int64_t sumI64Vec(int64_t a, int64_t b, int64_t c);
double vecMulSumViaVar(double a, double b);

inline double vecMulSum(double a, double b) {
  const std::array<double, 4> v1 = std::array<double, 4>{{a, a + 1.0, a + 2.0, a + 3.0}};
  const std::array<double, 4> v2 = std::array<double, 4>{{b, b, b, b}};
  const std::array<double, 4> p = lanes(v1, v2, [](auto x, auto y) { return x * y; });
  return p[0] + p[1] + p[2] + p[3];
}

inline int64_t sumI64Vec(int64_t a, int64_t b, int64_t c) {
  const std::array<int64_t, 3> v = std::array<int64_t, 3>{{a, b, c}};
  return v[0] + v[1] + v[2];
}

inline double vecMulSumViaVar(double a, double b) {
  const std::array<double, 4> v1 = std::array<double, 4>{{a, a + 1.0, a + 2.0, a + 3.0}};
  const std::array<double, 4> v2 = std::array<double, 4>{{b, b, b, b}};
  const std::array<double, 4> p = lanes(v1, v2, [](auto x, auto y) { return x * y; });
  double total = 0.0;
  for (int64_t i = 0; i < 4; i = i + 1) {
    total = total + p[i];
  }
  return total;
}
