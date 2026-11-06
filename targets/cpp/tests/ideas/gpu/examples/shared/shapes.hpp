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

int32_t atomicAdd(List<int32_t> counts, int32_t slot, int32_t amount);
float atomicAddFloat(List<float> sums, int32_t slot, float amount);
int32_t atomicMin(List<int32_t> values, int32_t slot, int32_t candidate);
int32_t atomicMax(List<int32_t> values, int32_t slot, int32_t candidate);
float blend7(float a, float b, float c, float d, float e, float f, float g, float k);
float plasma(int32_t x, int32_t y, int32_t width, int32_t height, float phase);
float vignette(float pixel, int32_t x, int32_t y, int32_t width, int32_t height);
int32_t tally(int32_t i, List<int32_t> counts, List<float> sums, List<int32_t> lows, List<int32_t> highs);
int32_t reported(int32_t i);
int64_t nearly(float a, float b, float tolerance);

inline int32_t atomicAdd(List<int32_t> counts, int32_t slot, int32_t amount) {
  const int32_t old = counts->at(slot);
  counts->at(slot) = old + amount;
  return old;
}

inline float atomicAddFloat(List<float> sums, int32_t slot, float amount) {
  const float old = sums->at(slot);
  sums->at(slot) = old + amount;
  return old;
}

inline int32_t atomicMin(List<int32_t> values, int32_t slot, int32_t candidate) {
  const int32_t old = values->at(slot);
  if (candidate < old) {
    values->at(slot) = candidate;
  }
  return old;
}

inline int32_t atomicMax(List<int32_t> values, int32_t slot, int32_t candidate) {
  const int32_t old = values->at(slot);
  if (candidate > old) {
    values->at(slot) = candidate;
  }
  return old;
}

inline float blend7(float a, float b, float c, float d, float e, float f, float g, float k) {
  return k * (a + (float) (2.0 * (double) b) + (float) (3.0 * (double) c)) + (d - e) * (f + g);
}

inline float plasma(int32_t x, int32_t y, int32_t width, int32_t height, float phase) {
  const float u = (float) x / (float) width;
  const float v = (float) y / (float) height;
  const float a = sinf((float) (10.0 * (double) u + (double) phase));
  const float b = sinf((float) (8.0 * (double) v - (double) phase));
  const float c = sinf((float) (6.0 * (double) (u + v) + 2.0 * (double) phase));
  return (float) (0.5 + 0.1667 * (double) (a + b + c));
}

inline float vignette(float pixel, int32_t x, int32_t y, int32_t width, int32_t height) {
  const float dx = (float) x / (float) width - 0.5f;
  const float dy = (float) y / (float) height - 0.5f;
  const float edge = (float) (1.0 - 1.6 * (double) (dx * dx + dy * dy));
  return pixel * edge;
}

inline int32_t tally(int32_t i, List<int32_t> counts, List<float> sums, List<int32_t> lows, List<int32_t> highs) {
  const int32_t value = i * 7919 % 10007;
  atomicAdd(counts, i % 4, 1);
  atomicAddFloat(sums, i % 4, 0.5f);
  atomicMin(lows, 0, value);
  atomicMax(highs, 0, value);
  return 0;
}

inline int32_t reported(int32_t i) {
  return i * 7919 % 10007;
}

inline int64_t nearly(float a, float b, float tolerance) {
  float gap = a - b;
  if (gap < 0.0f) {
    gap = (float) (0.0 - (double) gap);
  }
  if (gap <= tolerance) {
    return 1;
  }
  return 0;
}
