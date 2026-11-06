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

int32_t fftStage(int32_t t, List<float> src, List<int32_t> p, List<float> dst);
float fftMagnitude(int32_t k, List<float> data, List<int32_t> p);
int32_t fftFrames(int32_t t, List<float> signal, List<int32_t> p, List<float> dst);
float spectrogramPixel(int32_t i, List<float> data, List<int32_t> p);
int32_t complexMultiply(int32_t k, List<float> a, List<float> b, List<float> out);
float realScaled(int32_t i, List<float> data, List<float> p);

inline int32_t fftStage(int32_t t, List<float> src, List<int32_t> p, List<float> dst) {
  const int32_t total = p->at(0);
  const int32_t n = p->at(1);
  const int32_t s = p->at(2);
  const int32_t half = total / 2;
  const int32_t batch = t / half;
  const int32_t local = t % half;
  const int32_t position = local / s;
  const int32_t column = local % s;
  const int32_t base = batch * total * 2;
  const int32_t a = base + (int32_t) (2 * (int64_t) (column + s * position));
  const int32_t b = base + (int32_t) (2 * (int64_t) (column + s * (position + n / 2)));
  const float sumRe = src->at(a) + src->at(b);
  const float sumIm = src->at(a + 1) + src->at(b + 1);
  const float diffRe = src->at(a) - src->at(b);
  const float diffIm = src->at(a + 1) - src->at(b + 1);
  const float angle = (float) ((0.0 - 6.2831853) * (double) (float) (p->at(3)) * (double) (float) position / (double) (float) n);
  const float twiddleRe = cosf(angle);
  const float twiddleIm = sinf(angle);
  const int32_t first = base + (int32_t) (2 * (int64_t) (column + s * (int32_t) (2 * (int64_t) position)));
  const int32_t second = base + (int32_t) (2 * (int64_t) (column + s * (int32_t) (2 * (int64_t) position + 1)));
  dst->at(first) = sumRe;
  dst->at(first + 1) = sumIm;
  dst->at(second) = diffRe * twiddleRe - diffIm * twiddleIm;
  dst->at(second + 1) = diffRe * twiddleIm + diffIm * twiddleRe;
  return 0;
}

inline float fftMagnitude(int32_t k, List<float> data, List<int32_t> p) {
  const int32_t total = p->at(0);
  const float re = data->at(2 * (int64_t) k);
  const float im = data->at(2 * (int64_t) k + 1);
  return (float) (2.0 * (double) sqrtf(re * re + im * im) / (double) (float) total);
}

inline int32_t fftFrames(int32_t t, List<float> signal, List<int32_t> p, List<float> dst) {
  const int32_t total = p->at(0);
  const int32_t frame = t / total;
  const int32_t k = t % total;
  const float window = (float) (0.5 - 0.5 * (double) cosf((float) (6.2831853 * (double) (float) k / (double) (float) (total - 1))));
  dst->at(2 * (int64_t) t) = signal->at(frame * p->at(1) + k) * window;
  dst->at(2 * (int64_t) t + 1) = 0.0f;
  return 0;
}

inline float spectrogramPixel(int32_t i, List<float> data, List<int32_t> p) {
  const int32_t total = p->at(0);
  const int32_t frames = p->at(1);
  const int32_t rows = p->at(2);
  const int32_t x = i % frames;
  const int32_t bin = rows - 1 - i / frames;
  const int32_t at = x * total * 2 + (int32_t) (2 * (int64_t) bin);
  const float re = data->at(at);
  const float im = data->at(at + 1);
  const float magnitude = (float) (4.0 * (double) sqrtf(re * re + im * im) / (double) (float) total);
  float level = (float) ((20.0 * (double) logf(magnitude + 0.0000001f) / 2.3025851 + 70.0) / 70.0);
  if (level < 0.0f) {
    level = 0.0f;
  }
  if (level > 1.0f) {
    level = 1.0f;
  }
  return level;
}

inline int32_t complexMultiply(int32_t k, List<float> a, List<float> b, List<float> out) {
  out->at(2 * (int64_t) k) = a->at(2 * (int64_t) k) * b->at(2 * (int64_t) k) - a->at(2 * (int64_t) k + 1) * b->at(2 * (int64_t) k + 1);
  out->at(2 * (int64_t) k + 1) = a->at(2 * (int64_t) k) * b->at(2 * (int64_t) k + 1) + a->at(2 * (int64_t) k + 1) * b->at(2 * (int64_t) k);
  return 0;
}

inline float realScaled(int32_t i, List<float> data, List<float> p) {
  return data->at(2 * (int64_t) i) / p->at(0);
}
