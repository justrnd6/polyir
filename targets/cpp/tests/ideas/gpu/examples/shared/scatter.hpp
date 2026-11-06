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
int32_t reverseCopy(int32_t i, List<float> src, int32_t n, List<float> dst);
int32_t histogram(int32_t i, List<int32_t> data, int32_t bins, List<int32_t> counts);
int32_t histogramRacy(int32_t i, List<int32_t> data, int32_t bins, List<int32_t> counts);
int32_t splat(int32_t i, List<float> p, List<int32_t> cells);

inline int32_t atomicAdd(List<int32_t> counts, int32_t slot, int32_t amount) {
  const int32_t old = counts->at(slot);
  counts->at(slot) = old + amount;
  return old;
}

inline int32_t reverseCopy(int32_t i, List<float> src, int32_t n, List<float> dst) {
  dst->at(n - 1 - i) = src->at(i);
  return 0;
}

inline int32_t histogram(int32_t i, List<int32_t> data, int32_t bins, List<int32_t> counts) {
  const int32_t bin = data->at(i) % bins;
  atomicAdd(counts, bin, 1);
  return 0;
}

inline int32_t histogramRacy(int32_t i, List<int32_t> data, int32_t bins, List<int32_t> counts) {
  const int32_t bin = data->at(i) % bins;
  counts->at(bin) = counts->at(bin) + 1;
  return 0;
}

inline int32_t splat(int32_t i, List<float> p, List<int32_t> cells) {
  const int32_t w = (int32_t) (p->at(0));
  const int32_t arm = i % 2;
  const float t = (float) (i / 2 % 20000) / 20000.0f;
  const float angle = t * 9.0f + (float) arm * 3.14159265f;
  const float radius = (float) (0.08 + 0.85 * (double) t);
  const int32_t h1 = (i * 75 + 74) % 65537;
  const int32_t h2 = (h1 * 75 + 74) % 65537;
  const float spread = (float) (0.015 + 0.09 * (double) t);
  const float x = radius * cosf(angle) + ((float) h1 / 65537.0f - 0.5f) * spread * 2.0f;
  const float y = radius * sinf(angle) + ((float) h2 / 65537.0f - 0.5f) * spread * 2.0f;
  const int32_t px = (int32_t) floorf((x * 0.5f + 0.5f) * (float) w);
  const int32_t py = (int32_t) floorf((y * 0.5f + 0.5f) * (float) w);
  if (px >= 0 && px < w && py >= 0 && py < w) {
    atomicAdd(cells, py * w + px, 1);
  }
  return 0;
}
