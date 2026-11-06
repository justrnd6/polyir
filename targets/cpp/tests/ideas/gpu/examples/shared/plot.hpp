#pragma once

#include <algorithm>
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

int64_t drawStems(List<float> pic, int64_t w, int64_t h, List<float> values, int64_t n, float low, float high);
int64_t minOf(int64_t a, int64_t b);
int64_t maxOf(int64_t a, int64_t b);
int64_t drawWave(List<float> pic, int64_t w, int64_t h, List<float> values, int64_t n, float low, float high);
int64_t drawCurve(List<float> pic, int64_t w, int64_t h, List<float> values, int64_t n, float low, float high);
int64_t curveRow(float value, int64_t h, float low, float high);

inline int64_t drawStems(List<float> pic, int64_t w, int64_t h, List<float> values, int64_t n, float low, float high) {
  for (int64_t k = 0; k < w * h; k = k + 1) {
    pic->at(k) = 0.96f;
  }
  const int64_t zero = (int64_t) (high / (high - low) * (float) h);
  for (int64_t x = 0; x < w; x = x + 1) {
    pic->at(zero * w + x) = 0.55f;
  }
  for (int64_t j = 0; j < n; j = j + 1) {
    const int64_t column = (int64_t) (((float) j + 0.5f) * (float) w / (float) n);
    int64_t top = (int64_t) ((high - values->at(j)) / (high - low) * (float) (h - 1));
    if (top < 0) {
      top = 0;
    }
    if (top > h - 1) {
      top = h - 1;
    }
    const int64_t from = minOf(top, zero);
    const int64_t to = maxOf(top, zero);
    for (int64_t y = from; y <= to; y = y + 1) {
      pic->at(y * w + column) = 0.1f;
    }
    for (int64_t dy = 0 - 1; dy <= 1; dy = dy + 1) {
      for (int64_t dx = 0 - 1; dx <= 1; dx = dx + 1) {
        const int64_t yy = top + dy;
        const int64_t xx = column + dx;
        if (yy >= 0 && yy < h && xx >= 0 && xx < w) {
          pic->at(yy * w + xx) = 0.1f;
        }
      }
    }
  }
  return 0;
}

inline int64_t minOf(int64_t a, int64_t b) {
  if (a < b) {
    return a;
  }
  return b;
}

inline int64_t maxOf(int64_t a, int64_t b) {
  if (a > b) {
    return a;
  }
  return b;
}

inline int64_t drawWave(List<float> pic, int64_t w, int64_t h, List<float> values, int64_t n, float low, float high) {
  for (int64_t k = 0; k < w * h; k = k + 1) {
    pic->at(k) = 0.96f;
  }
  const int64_t zero = (int64_t) (high / (high - low) * (float) h);
  for (int64_t x = 0; x < w; x = x + 1) {
    pic->at(zero * w + x) = 0.55f;
  }
  for (int64_t x = 0; x < w; x = x + 1) {
    const int64_t from = x * n / w;
    int64_t upto = (x + 1) * n / w;
    if (upto <= from) {
      upto = from + 1;
    }
    float lowest = values->at(from);
    float highest = values->at(from);
    for (int64_t j = from; j < upto; j = j + 1) {
      if (values->at(j) < lowest) {
        lowest = values->at(j);
      }
      if (values->at(j) > highest) {
        highest = values->at(j);
      }
    }
    int64_t top = (int64_t) ((high - highest) / (high - low) * (float) (h - 1));
    int64_t bottom = (int64_t) ((high - lowest) / (high - low) * (float) (h - 1));
    if (top < 0) {
      top = 0;
    }
    if (bottom > h - 1) {
      bottom = h - 1;
    }
    for (int64_t y = top; y <= bottom; y = y + 1) {
      pic->at(y * w + x) = 0.1f;
    }
  }
  return 0;
}

inline int64_t drawCurve(List<float> pic, int64_t w, int64_t h, List<float> values, int64_t n, float low, float high) {
  for (int64_t k = 0; k < w * h; k = k + 1) {
    pic->at(k) = 0.96f;
  }
  const int64_t zero = (int64_t) (high / (high - low) * (float) h);
  for (int64_t x = 0; x < w; x = x + 1) {
    pic->at(zero * w + x) = 0.55f;
  }
  int64_t previous = curveRow(values->at(0), h, low, high);
  for (int64_t x = 0; x < w; x = x + 1) {
    const int64_t row = curveRow(values->at(x * n / w), h, low, high);
    for (int64_t y = minOf(row, previous); y <= maxOf(row, previous); y = y + 1) {
      pic->at(y * w + x) = 0.1f;
    }
    previous = row;
  }
  return 0;
}

inline int64_t curveRow(float value, int64_t h, float low, float high) {
  int64_t row = (int64_t) ((high - value) / (high - low) * (float) (h - 1));
  if (row < 0) {
    row = 0;
  }
  if (row > h - 1) {
    row = h - 1;
  }
  return row;
}
