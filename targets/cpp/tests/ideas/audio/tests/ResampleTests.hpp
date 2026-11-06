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

#include "../../../core/tests/programs/Check.hpp"
#include "../Resample.hpp"
#include "../Sound.hpp"

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

Ref<Sound> sineSound(float hz, float seconds, int64_t rate);
double checkCountGoingUp();
double checkCountGoingDown();
double checkCountRounds();
double checkSoundKeepsItsLength();
double checkUpsamplingFollowsTheWave();
double checkDownsamplingRemovesAliases();
double checkDownsamplingKeepsLowTones();
double checkRoundTrip();
double checkConstantStaysConstant();
double checkStereoResampling();
double checkSameRateIsACopy();

inline Ref<Sound> sineSound(float hz, float seconds, int64_t rate) {
  const Ref<Sound> s = soundNew(seconds, rate);
  for (int64_t i = 0; i < s->count; i = i + 1) {
    s->samples->at(i) = (float) sin(6.283185307179586 * (double) hz * (double) i / (double) rate);
  }
  return s;
}

inline double checkCountGoingUp() {
  return offInt(resampledCount(44100, 44100, 48000), 48000);
}

inline double checkCountGoingDown() {
  return offInt(resampledCount(1000, 8000, 4000), 500);
}

inline double checkCountRounds() {
  return offInt(resampledCount(10, 3, 4), 13);
}

inline double checkSoundKeepsItsLength() {
  const Ref<Sound> s = sineSound(440.0f, 0.5f, 8000)->resample(12000);
  return offInt(s->rate, 12000) + offInt(s->count, 6000) + offInt(s->channels, 1) + off(s->seconds(), 0.5f);
}

inline double checkUpsamplingFollowsTheWave() {
  const Ref<Sound> s = sineSound(440.0f, 1.0f, 8000)->resample(12000);
  double worst = 0.0;
  for (int64_t j = 1000; j < 11000; j = j + 7) {
    const double expected = sin(6.283185307179586 * 440.0 * (double) j / 12000.0);
    double gap = (double) (s->samples->at(j)) - expected;
    if (gap < 0.0) {
      gap = 0.0 - gap;
    }
    if (gap > worst) {
      worst = gap;
    }
  }
  return worst;
}

inline double checkDownsamplingRemovesAliases() {
  const Ref<Sound> s = sineSound(3000.0f, 1.0f, 8000)->resample(4000);
  return (double) amplitudeAt(s->samples, 500, 2000, 4000, 1000.0f);
}

inline double checkDownsamplingKeepsLowTones() {
  const Ref<Sound> s = sineSound(1000.0f, 1.0f, 8000)->resample(4000);
  return off(amplitudeAt(s->samples, 500, 2000, 4000, 1000.0f), 1.0f);
}

inline double checkRoundTrip() {
  const Ref<Sound> original = sineSound(300.0f, 1.0f, 8000);
  const Ref<Sound> back = original->resample(12000)->resample(8000);
  double worst = 0.0;
  for (int64_t j = 1000; j < 7000; j = j + 11) {
    double gap = (double) (back->samples->at(j) - original->samples->at(j));
    if (gap < 0.0) {
      gap = 0.0 - gap;
    }
    if (gap > worst) {
      worst = gap;
    }
  }
  return worst;
}

inline double checkConstantStaysConstant() {
  const Ref<Sound> s = soundNew(0.5f, 8000);
  for (int64_t i = 0; i < s->count; i = i + 1) {
    s->samples->at(i) = 1.0f;
  }
  const Ref<Sound> up = s->resample(22050);
  const Ref<Sound> down = s->resample(3000);
  return off(up->samples->at(up->count / 2), 1.0f) + off(down->samples->at(down->count / 2), 1.0f);
}

inline double checkStereoResampling() {
  const Ref<Sound> left = sineSound(440.0f, 0.5f, 8000);
  const Ref<Sound> right = sineSound(880.0f, 0.5f, 8000);
  const Ref<Sound> out = soundStereo(left, right)->resample(16000);
  return offInt(out->channels, 2) + offInt(out->count, 8000) + off(amplitudeAt(out->channel(0)->samples, 2000, 4000, 16000, 440.0f), 1.0f) + off(amplitudeAt(out->channel(1)->samples, 2000, 4000, 16000, 880.0f), 1.0f);
}

inline double checkSameRateIsACopy() {
  const Ref<Sound> s = sineSound(440.0f, 0.1f, 8000);
  const Ref<Sound> copy = s->resample(8000);
  double worst = 0.0;
  for (int64_t i = 100; i < 700; i = i + 1) {
    double gap = (double) (copy->samples->at(i) - s->samples->at(i));
    if (gap < 0.0) {
      gap = 0.0 - gap;
    }
    if (gap > worst) {
      worst = gap;
    }
  }
  return worst + offInt(copy->count, s->count);
}
