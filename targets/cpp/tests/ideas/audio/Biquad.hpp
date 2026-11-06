#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#ifndef POLYIR_CORE_HPP
#define POLYIR_CORE_HPP

// What polyir shares (a struct, a list, a map, a set) is held by `Ref`, a shared pointer, so two names for one value see
// each other's changes. `same` compares what two values hold and `hashOf` hashes it, for the maps and sets that hold them.
template <typename T>
using Ref = std::shared_ptr<T>;

struct Same;
struct Hash;

template <typename T>
using List = Ref<std::vector<T>>;

template <typename K, typename V>
using Map = Ref<std::unordered_map<K, V, Hash, Same>>;

template <typename T>
using Set = Ref<std::unordered_set<T, Hash, Same>>;

template <typename T>
bool same(const T& a, const T& b);
template <typename T>
bool same(const std::shared_ptr<T>& a, const std::shared_ptr<T>& b);
template <typename T>
bool same(const std::vector<T>& a, const std::vector<T>& b);
template <typename K, typename V, typename H, typename E>
bool same(const std::unordered_map<K, V, H, E>& a, const std::unordered_map<K, V, H, E>& b);
template <typename T, typename H, typename E>
bool same(const std::unordered_set<T, H, E>& a, const std::unordered_set<T, H, E>& b);
template <typename T>
bool same(const std::optional<T>& a, const std::optional<T>& b);
template <typename T>
std::size_t hashOf(const T& value);
template <typename T>
std::size_t hashOf(const std::shared_ptr<T>& value);
template <typename T>
std::size_t hashOf(const std::vector<T>& items);

struct Same {
  template <typename T>
  bool operator()(const T& a, const T& b) const {
    return same(a, b);
  }
};

struct Hash {
  template <typename T>
  std::size_t operator()(const T& value) const {
    return hashOf(value);
  }
};

template <typename T>
bool same(const T& a, const T& b) {
  return a == b;
}

// Two shared values are the same when they hold the same; an interface's value, with no `==`, only when it is one value.
template <typename T>
bool same(const std::shared_ptr<T>& a, const std::shared_ptr<T>& b) {
  if constexpr (requires { *a == *b; }) {
    return a == b || (a != nullptr && b != nullptr && *a == *b);
  } else {
    return a == b;
  }
}

template <typename T>
bool same(const std::vector<T>& a, const std::vector<T>& b) {
  return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin(), Same{});
}

template <typename K, typename V, typename H, typename E>
bool same(const std::unordered_map<K, V, H, E>& a, const std::unordered_map<K, V, H, E>& b) {
  return a.size() == b.size() && std::all_of(a.begin(), a.end(), [&](const auto& entry) {
    const auto found = b.find(entry.first);
    return found != b.end() && same(entry.second, found->second);
  });
}

template <typename T, typename H, typename E>
bool same(const std::unordered_set<T, H, E>& a, const std::unordered_set<T, H, E>& b) {
  return a.size() == b.size() && std::all_of(a.begin(), a.end(), [&](const T& item) { return b.contains(item); });
}

template <typename T>
bool same(const std::optional<T>& a, const std::optional<T>& b) {
  return a.has_value() == b.has_value() && (!a.has_value() || same(*a, *b));
}

template <typename T>
std::size_t hashOf(const T& value) {
  if constexpr (requires { value.hash(); }) {
    return value.hash();
  } else {
    return std::hash<T>{}(value);
  }
}

template <typename T>
std::size_t hashOf(const std::shared_ptr<T>& value) {
  if constexpr (requires { value->hash(); }) {
    return value == nullptr ? 0 : value->hash();
  } else {
    return std::hash<std::shared_ptr<T>>{}(value);
  }
}

template <typename T>
std::size_t hashOf(const std::vector<T>& items) {
  std::size_t hash = items.size();
  for (const T& item : items) {
    hash = hash * 31 + hashOf(item);
  }
  return hash;
}

// A struct's `==` and hash: its fields, each compared with the other's (`allSame(x, other.x, y, other.y)`).
inline bool allSame() {
  return true;
}

template <typename T, typename... Rest>
bool allSame(const T& a, const T& b, const Rest&... rest) {
  return same(a, b) && allSame(rest...);
}

inline std::size_t hashAll() {
  return 0;
}

template <typename T, typename... Rest>
std::size_t hashAll(const T& first, const Rest&... rest) {
  return hashOf(first) * 31 + hashAll(rest...);
}

// New lists, maps and sets.
template <typename T>
List<T> newList() {
  return std::make_shared<std::vector<T>>();
}

template <typename T>
List<T> listOf(std::initializer_list<T> items) {
  return std::make_shared<std::vector<T>>(items);
}

template <typename T>
List<T> newFilled(int64_t count, const T& zero) {
  return std::make_shared<std::vector<T>>(count, zero);
}

template <typename K, typename V>
Map<K, V> newMap() {
  return std::make_shared<std::unordered_map<K, V, Hash, Same>>();
}

template <typename T>
Set<T> newSet() {
  return std::make_shared<std::unordered_set<T, Hash, Same>>();
}
#endif

struct Biquad;

struct Biquad {
  double b0;
  double b1;
  double b2;
  double a1;
  double a2;

  Biquad(double b0, double b1, double b2, double a1, double a2) : b0(b0), b1(b1), b2(b2), a1(a1), a2(a2) {}

  int64_t run(List<float> data, int64_t count);
  float gain(float hz, int64_t rate);

  bool operator==(const Biquad& other) const {
    return allSame(b0, other.b0, b1, other.b1, b2, other.b2, a1, other.a1, a2, other.a2);
  }

  std::size_t hash() const {
    return hashAll(b0, b1, b2, a1, a2);
  }
};

Ref<Biquad> biquadFrom(double b0, double b1, double b2, double a0, double a1, double a2);
double biquadAngle(int64_t rate, float hz);
Ref<Biquad> biquadLowPass(int64_t rate, float hz, float q);
Ref<Biquad> biquadHighPass(int64_t rate, float hz, float q);
Ref<Biquad> biquadBandPass(int64_t rate, float hz, float q);
Ref<Biquad> biquadNotch(int64_t rate, float hz, float q);
Ref<Biquad> biquadBell(int64_t rate, float hz, float q, float gainDb);
Ref<Biquad> biquadLowShelf(int64_t rate, float hz, float gainDb);
Ref<Biquad> biquadHighShelf(int64_t rate, float hz, float gainDb);

inline auto Biquad::run(List<float> data, int64_t count) -> int64_t {
  double x1 = 0.0;
  double x2 = 0.0;
  double y1 = 0.0;
  double y2 = 0.0;
  for (int64_t i = 0; i < count; i = i + 1) {
    const double x = (double) (data->at(i));
    const double y = this->b0 * x + this->b1 * x1 + this->b2 * x2 - this->a1 * y1 - this->a2 * y2;
    x2 = x1;
    x1 = x;
    y2 = y1;
    y1 = y;
    data->at(i) = (float) y;
  }
  return 0;
}

inline auto Biquad::gain(float hz, int64_t rate) -> float {
  const double w = 6.283185307179586 * (double) hz / (double) rate;
  const double c1 = cos(w);
  const double s1 = sin(w);
  const double c2 = cos(2.0 * w);
  const double s2 = sin(2.0 * w);
  const double topReal = this->b0 + this->b1 * c1 + this->b2 * c2;
  const double topImag = 0.0 - (this->b1 * s1 + this->b2 * s2);
  const double bottomReal = 1.0 + this->a1 * c1 + this->a2 * c2;
  const double bottomImag = 0.0 - (this->a1 * s1 + this->a2 * s2);
  const double strength = (topReal * topReal + topImag * topImag) / (bottomReal * bottomReal + bottomImag * bottomImag);
  return (float) sqrt(strength);
}

inline Ref<Biquad> biquadFrom(double b0, double b1, double b2, double a0, double a1, double a2) {
  return std::make_shared<Biquad>(b0 / a0, b1 / a0, b2 / a0, a1 / a0, a2 / a0);
}

inline double biquadAngle(int64_t rate, float hz) {
  return 6.283185307179586 * (double) hz / (double) rate;
}

inline Ref<Biquad> biquadLowPass(int64_t rate, float hz, float q) {
  const double w = biquadAngle(rate, hz);
  const double c = cos(w);
  const double alpha = sin(w) / (2.0 * (double) q);
  return biquadFrom((1.0 - c) / 2.0, 1.0 - c, (1.0 - c) / 2.0, 1.0 + alpha, 0.0 - 2.0 * c, 1.0 - alpha);
}

inline Ref<Biquad> biquadHighPass(int64_t rate, float hz, float q) {
  const double w = biquadAngle(rate, hz);
  const double c = cos(w);
  const double alpha = sin(w) / (2.0 * (double) q);
  return biquadFrom((1.0 + c) / 2.0, 0.0 - (1.0 + c), (1.0 + c) / 2.0, 1.0 + alpha, 0.0 - 2.0 * c, 1.0 - alpha);
}

inline Ref<Biquad> biquadBandPass(int64_t rate, float hz, float q) {
  const double w = biquadAngle(rate, hz);
  const double c = cos(w);
  const double alpha = sin(w) / (2.0 * (double) q);
  return biquadFrom(alpha, 0.0, 0.0 - alpha, 1.0 + alpha, 0.0 - 2.0 * c, 1.0 - alpha);
}

inline Ref<Biquad> biquadNotch(int64_t rate, float hz, float q) {
  const double w = biquadAngle(rate, hz);
  const double c = cos(w);
  const double alpha = sin(w) / (2.0 * (double) q);
  return biquadFrom(1.0, 0.0 - 2.0 * c, 1.0, 1.0 + alpha, 0.0 - 2.0 * c, 1.0 - alpha);
}

inline Ref<Biquad> biquadBell(int64_t rate, float hz, float q, float gainDb) {
  const double w = biquadAngle(rate, hz);
  const double c = cos(w);
  const double alpha = sin(w) / (2.0 * (double) q);
  const double a = pow(10.0, (double) gainDb / 40.0);
  return biquadFrom(1.0 + alpha * a, 0.0 - 2.0 * c, 1.0 - alpha * a, 1.0 + alpha / a, 0.0 - 2.0 * c, 1.0 - alpha / a);
}

inline Ref<Biquad> biquadLowShelf(int64_t rate, float hz, float gainDb) {
  const double w = biquadAngle(rate, hz);
  const double c = cos(w);
  const double a = pow(10.0, (double) gainDb / 40.0);
  const double beta = sqrt(2.0 * a) * sin(w);
  return biquadFrom(a * (a + 1.0 - (a - 1.0) * c + beta), 2.0 * a * (a - 1.0 - (a + 1.0) * c), a * (a + 1.0 - (a - 1.0) * c - beta), a + 1.0 + (a - 1.0) * c + beta, 0.0 - 2.0 * (a - 1.0 + (a + 1.0) * c), a + 1.0 + (a - 1.0) * c - beta);
}

inline Ref<Biquad> biquadHighShelf(int64_t rate, float hz, float gainDb) {
  const double w = biquadAngle(rate, hz);
  const double c = cos(w);
  const double a = pow(10.0, (double) gainDb / 40.0);
  const double beta = sqrt(2.0 * a) * sin(w);
  return biquadFrom(a * (a + 1.0 + (a - 1.0) * c + beta), 0.0 - 2.0 * a * (a - 1.0 + (a + 1.0) * c), a * (a + 1.0 + (a - 1.0) * c - beta), a + 1.0 - (a - 1.0) * c + beta, 2.0 * (a - 1.0 - (a + 1.0) * c), a + 1.0 - (a - 1.0) * c - beta);
}
