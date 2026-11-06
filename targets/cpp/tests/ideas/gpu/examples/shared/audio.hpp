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

#include "../../../../core/stdlib/Math.hpp"
#include "../../../audio/Wave.hpp"

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

float synth(int32_t i, List<float> p);
float squareWave(int32_t i, List<float> p);
float partialSum(int32_t i, List<float> p);
float melody(int32_t i, List<float> notes, List<float> p);

inline float synth(int32_t i, List<float> p) {
  const float t = (float) i / p->at(0);
  const float envelope = fade(t, p->at(2), p->at(3), p->at(4));
  const float wobble = p->at(6) * sine(p->at(5) * t);
  float sum = 0.0f;
  for (int64_t k = 0; k < (int64_t) (int32_t) (p->at(1)); k = k + 1) {
    const float cycles = p->at(8 + 2 * k) * t;
    sum = sum + p->at(9 + 2 * k) * sinf((float) (6.2831853 * (double) (cycles - floorf(cycles)) + (double) wobble));
  }
  return p->at(7) * envelope * sum;
}

inline float squareWave(int32_t i, List<float> p) {
  return sign(sine(p->at(1) * (float) i / p->at(0)));
}

inline float partialSum(int32_t i, List<float> p) {
  const float x = (float) (6.2831853 * (double) (float) (i % (int32_t) (p->at(0))) / (double) (p->at(0)));
  float sum = 0.0f;
  for (int64_t h = 1; h <= (int64_t) (int32_t) (p->at(2)); h = h + 1) {
    const float number = (float) h;
    const float wave = sinf(number * x);
    if (p->at(1) < 0.5f) {
      if (h % 2 == 1) {
        sum = sum + wave / number;
      }
    }
    if (p->at(1) > 0.5f && p->at(1) < 1.5f) {
      if (h % 2 == 1) {
        sum = sum + wave / number;
      }
      if (h % 2 == 0) {
        sum = sum - wave / number;
      }
    }
    if (p->at(1) > 1.5f) {
      if (h % 4 == 1) {
        sum = sum + wave / (number * number);
      }
      if (h % 4 == 3) {
        sum = sum - wave / (number * number);
      }
    }
  }
  if (p->at(1) < 0.5f) {
    return (float) (1.2732395 * (double) sum);
  }
  if (p->at(1) < 1.5f) {
    return (float) (0.6366198 * (double) sum);
  }
  return (float) (0.8105695 * (double) sum);
}

inline float melody(int32_t i, List<float> notes, List<float> p) {
  const float t = (float) i / p->at(0);
  float sum = 0.0f;
  for (int64_t n = 0; n < (int64_t) (int32_t) (p->at(1)); n = n + 1) {
    const float start = notes->at(3 * n);
    const float length = notes->at(3 * n + 1);
    const float local = t - start;
    if (local >= 0.0f && local < length) {
      const float envelope = fade(local, 0.02f, 0.08f, length);
      const float f = notes->at(3 * n + 2);
      sum = sum + envelope * (sine(f * t) + (float) (0.5 * (double) sine((float) (2.0 * (double) f * (double) t))) + (float) (0.25 * (double) sine((float) (3.0 * (double) f * (double) t))));
    }
  }
  return p->at(2) * sum;
}
