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

int32_t atomicMin(List<int32_t> values, int32_t slot, int32_t candidate);
int32_t emptyKey();
int32_t rasterize(int32_t t, List<float> tris, int32_t width, int32_t height, List<int32_t> depth);
float shade(int32_t p, List<int32_t> depth, List<float> tris);
int64_t makeScene(List<float> tris, int64_t count, int64_t width, int64_t height);
int64_t nextSeed(int64_t seed);
float nextRandom(int64_t seed);

inline int32_t atomicMin(List<int32_t> values, int32_t slot, int32_t candidate) {
  const int32_t old = values->at(slot);
  if (candidate < old) {
    values->at(slot) = candidate;
  }
  return old;
}

inline int32_t emptyKey() {
  return 2147483647;
}

inline int32_t rasterize(int32_t t, List<float> tris, int32_t width, int32_t height, List<int32_t> depth) {
  const int32_t base = t * 10;
  const float x0 = tris->at(base);
  const float y0 = tris->at(base + 1);
  const float z0 = tris->at(base + 2);
  const float x1 = tris->at(base + 3);
  const float y1 = tris->at(base + 4);
  const float z1 = tris->at(base + 5);
  const float x2 = tris->at(base + 6);
  const float y2 = tris->at(base + 7);
  const float z2 = tris->at(base + 8);
  const float area = (x1 - x0) * (y2 - y0) - (x2 - x0) * (y1 - y0);
  const int32_t left = (int32_t) fmaxf(0.0f, floorf(fminf(x0, fminf(x1, x2))));
  const int32_t right = (int32_t) fminf((float) width - 1.0f, ceilf(fmaxf(x0, fmaxf(x1, x2))));
  const int32_t top = (int32_t) fmaxf(0.0f, floorf(fminf(y0, fminf(y1, y2))));
  const int32_t bottom = (int32_t) fminf((float) height - 1.0f, ceilf(fmaxf(y0, fmaxf(y1, y2))));
  for (int32_t y = top; y <= bottom; y = y + 1) {
    for (int32_t x = left; x <= right; x = x + 1) {
      const float px = (float) x + 0.5f;
      const float py = (float) y + 0.5f;
      const float w0 = ((x1 - px) * (y2 - py) - (x2 - px) * (y1 - py)) / area;
      const float w1 = ((x2 - px) * (y0 - py) - (x0 - px) * (y2 - py)) / area;
      const float w2 = (float) (1.0 - (double) w0 - (double) w1);
      if (w0 >= 0.0f && w1 >= 0.0f && w2 >= 0.0f) {
        const float z = w0 * z0 + w1 * z1 + w2 * z2;
        atomicMin(depth, y * width + x, (int32_t) (z * 1048575.0f) * 512 + t);
      }
    }
  }
  return 0;
}

inline float shade(int32_t p, List<int32_t> depth, List<float> tris) {
  const int32_t key = depth->at(p);
  if (key == 2147483647) {
    return 0.06f;
  }
  const int32_t id = key % 512;
  const float z = (float) (key / 512) / 1048575.0f;
  return tris->at(id * 10 + 9) * (float) (1.0 - 0.45 * (double) z);
}

inline int64_t makeScene(List<float> tris, int64_t count, int64_t width, int64_t height) {
  int64_t seed = 12345;
  for (int64_t t = 0; t < count; t = t + 1) {
    const float cx = nextRandom(seed) * (float) width;
    seed = nextSeed(seed);
    const float cy = nextRandom(seed) * (float) height;
    seed = nextSeed(seed);
    const float size = (float) (30.0 + (double) (nextRandom(seed) * 110.0f));
    seed = nextSeed(seed);
    for (int64_t corner = 0; corner < 3; corner = corner + 1) {
      tris->at(t * 10 + corner * 3) = cx + (nextRandom(seed) - 0.5f) * 2.0f * size;
      seed = nextSeed(seed);
      tris->at(t * 10 + corner * 3 + 1) = cy + (nextRandom(seed) - 0.5f) * 2.0f * size;
      seed = nextSeed(seed);
      tris->at(t * 10 + corner * 3 + 2) = nextRandom(seed);
      seed = nextSeed(seed);
    }
    tris->at(t * 10 + 9) = (float) (0.25 + 0.75 * (double) nextRandom(seed));
    seed = nextSeed(seed);
  }
  return 0;
}

inline int64_t nextSeed(int64_t seed) {
  return (seed * 1103515245 + 12345) % 2147483648;
}

inline float nextRandom(int64_t seed) {
  return (float) (seed / 65536 % 32768) / 32768.0f;
}
