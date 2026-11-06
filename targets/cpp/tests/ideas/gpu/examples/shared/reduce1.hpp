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

float atomicAddFloat(List<float> sums, int32_t slot, float amount);
int32_t groupSize();
int32_t localIndex(int32_t i);
int32_t groupIndex(int32_t i);
int32_t barrier();
int32_t blockSum(int32_t i, List<float> src, int32_t n, List<float> tile, List<float> partial);
int32_t blockMax(int32_t i, List<float> src, int32_t n, List<float> tile, List<float> partial);
int32_t total(int32_t i, List<float> src, int32_t n, List<float> tile, List<float> sum);
int64_t groupsFor(int64_t n);
int64_t launchCount(int64_t n);

inline float atomicAddFloat(List<float> sums, int32_t slot, float amount) {
  const float old = sums->at(slot);
  sums->at(slot) = old + amount;
  return old;
}

inline int32_t groupSize() {
  return 1;
}

inline int32_t localIndex(int32_t i) {
  return 0;
}

inline int32_t groupIndex(int32_t i) {
  return i;
}

inline int32_t barrier() {
  return 0;
}

inline int32_t blockSum(int32_t i, List<float> src, int32_t n, List<float> tile, List<float> partial) {
  const int32_t lane = localIndex(i);
  if (i < n) {
    tile->at(lane) = src->at(i);
  } else {
    tile->at(lane) = 0.0f;
  }
  barrier();
  for (int32_t s = groupSize() / 2; s > 0; s = s / 2) {
    if (lane < s) {
      tile->at(lane) = tile->at(lane) + tile->at(lane + s);
    }
    barrier();
  }
  if (lane == 0) {
    partial->at(groupIndex(i)) = tile->at(0);
  }
  return 0;
}

inline int32_t blockMax(int32_t i, List<float> src, int32_t n, List<float> tile, List<float> partial) {
  const int32_t lane = localIndex(i);
  if (i < n) {
    tile->at(lane) = src->at(i);
  } else {
    tile->at(lane) = src->at(0);
  }
  barrier();
  for (int32_t s = groupSize() / 2; s > 0; s = s / 2) {
    if (lane < s) {
      tile->at(lane) = fmaxf(tile->at(lane), tile->at(lane + s));
    }
    barrier();
  }
  if (lane == 0) {
    partial->at(groupIndex(i)) = tile->at(0);
  }
  return 0;
}

inline int32_t total(int32_t i, List<float> src, int32_t n, List<float> tile, List<float> sum) {
  const int32_t lane = localIndex(i);
  if (i < n) {
    tile->at(lane) = src->at(i);
  } else {
    tile->at(lane) = 0.0f;
  }
  barrier();
  for (int32_t s = groupSize() / 2; s > 0; s = s / 2) {
    if (lane < s) {
      tile->at(lane) = tile->at(lane) + tile->at(lane + s);
    }
    barrier();
  }
  if (lane == 0) {
    atomicAddFloat(sum, 0, tile->at(0));
  }
  return 0;
}

inline int64_t groupsFor(int64_t n) {
  return (n + 255) / 256;
}

inline int64_t launchCount(int64_t n) {
  int64_t launches = 0;
  int64_t left = n;
  for (int64_t k = 0; k < 10; k = k + 1) {
    if (left > 1) {
      left = groupsFor(left);
      launches = launches + 1;
    }
  }
  return launches;
}
