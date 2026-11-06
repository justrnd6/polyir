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
