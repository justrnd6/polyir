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

int32_t wrap(int32_t n, int32_t size, int32_t mode);
float texel(List<float> tex, int32_t offset, int32_t size, int32_t x, int32_t y, int32_t mode);
float nearest(List<float> tex, int32_t offset, int32_t size, float u, float v, int32_t mode);
float lerp(float a, float b, float t);
float bilinear(List<float> tex, int32_t offset, int32_t size, float u, float v, int32_t mode);
int32_t levelOffset(int32_t base, int32_t level);
int32_t levelSize(int32_t base, int32_t level);
float mipSample(List<float> tex, int32_t base, int32_t levels, float u, float v, float level, int32_t mode);
int64_t pyramidSize(int64_t base);
int64_t fillBase(List<float> tex, int64_t base);
int64_t buildPyramid(List<float> tex, int64_t base);
int64_t shrink(List<float> tex, int64_t from, int64_t size);

inline int32_t wrap(int32_t n, int32_t size, int32_t mode) {
  if (mode == 0) {
    if (n < 0) {
      return 0;
    }
    if (n > size - 1) {
      return size - 1;
    }
    return n;
  }
  if (mode == 1) {
    return (n % size + size) % size;
  }
  const int32_t period = size * 2;
  const int32_t m = (n % period + period) % period;
  if (m >= size) {
    return period - 1 - m;
  }
  return m;
}

inline float texel(List<float> tex, int32_t offset, int32_t size, int32_t x, int32_t y, int32_t mode) {
  return tex->at(offset + wrap(y, size, mode) * size + wrap(x, size, mode));
}

inline float nearest(List<float> tex, int32_t offset, int32_t size, float u, float v, int32_t mode) {
  const int32_t x = (int32_t) floorf(u * (float) size);
  const int32_t y = (int32_t) floorf(v * (float) size);
  return texel(tex, offset, size, x, y, mode);
}

inline float lerp(float a, float b, float t) {
  return a + (b - a) * t;
}

inline float bilinear(List<float> tex, int32_t offset, int32_t size, float u, float v, int32_t mode) {
  const float sx = u * (float) size - 0.5f;
  const float sy = v * (float) size - 0.5f;
  const int32_t x0 = (int32_t) floorf(sx);
  const int32_t y0 = (int32_t) floorf(sy);
  const float fx = sx - (float) x0;
  const float fy = sy - (float) y0;
  const float top = lerp(texel(tex, offset, size, x0, y0, mode), texel(tex, offset, size, x0 + 1, y0, mode), fx);
  const float bottom = lerp(texel(tex, offset, size, x0, y0 + 1, mode), texel(tex, offset, size, x0 + 1, y0 + 1, mode), fx);
  return lerp(top, bottom, fy);
}

inline int32_t levelOffset(int32_t base, int32_t level) {
  int32_t offset = 0;
  int32_t size = base;
  for (int64_t k = 0; k < (int64_t) level; k = k + 1) {
    offset = offset + size * size;
    size = size / 2;
  }
  return offset;
}

inline int32_t levelSize(int32_t base, int32_t level) {
  int32_t size = base;
  for (int64_t k = 0; k < (int64_t) level; k = k + 1) {
    size = size / 2;
  }
  return size;
}

inline float mipSample(List<float> tex, int32_t base, int32_t levels, float u, float v, float level, int32_t mode) {
  int32_t lo = (int32_t) floorf(level);
  if (lo < 0) {
    lo = 0;
  }
  if (lo > levels - 1) {
    lo = levels - 1;
  }
  int32_t hi = lo + 1;
  if (hi > levels - 1) {
    hi = levels - 1;
  }
  float amount = level - (float) lo;
  if (amount < 0.0f) {
    amount = 0.0f;
  }
  if (amount > 1.0f) {
    amount = 1.0f;
  }
  const float a = bilinear(tex, levelOffset(base, lo), levelSize(base, lo), u, v, mode);
  const float b = bilinear(tex, levelOffset(base, hi), levelSize(base, hi), u, v, mode);
  return lerp(a, b, amount);
}

inline int64_t pyramidSize(int64_t base) {
  int64_t total = 0;
  int64_t size = base;
  for (int64_t k = 0; k < 16; k = k + 1) {
    if (size >= 1) {
      total = total + size * size;
      size = size / 2;
    }
  }
  return total;
}

inline int64_t fillBase(List<float> tex, int64_t base) {
  for (int64_t y = 0; y < base; y = y + 1) {
    for (int64_t x = 0; x < base; x = x + 1) {
      float level = (x / 4 + y / 4) % 2 == 1 ? 0.45f : 0.85f;
      if (x % 4 == 0 || y % 4 == 0) {
        level = level * 0.6f;
      }
      tex->at(y * base + x) = level;
    }
  }
  return 0;
}

inline int64_t buildPyramid(List<float> tex, int64_t base) {
  fillBase(tex, base);
  int64_t from = 0;
  int64_t size = base;
  for (int64_t level = 0; level < 16; level = level + 1) {
    if (size > 1) {
      shrink(tex, from, size);
      from = from + size * size;
      size = size / 2;
    }
  }
  return 0;
}

inline int64_t shrink(List<float> tex, int64_t from, int64_t size) {
  const int64_t next = from + size * size;
  const int64_t half = size / 2;
  for (int64_t y = 0; y < half; y = y + 1) {
    for (int64_t x = 0; x < half; x = x + 1) {
      const float a = tex->at(from + 2 * y * size + 2 * x);
      const float b = tex->at(from + 2 * y * size + 2 * x + 1);
      const float c = tex->at(from + (2 * y + 1) * size + 2 * x);
      const float d = tex->at(from + (2 * y + 1) * size + 2 * x + 1);
      tex->at(next + y * half + x) = (a + b + c + d) * 0.25f;
    }
  }
  return 0;
}
