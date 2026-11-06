#pragma once

#include <algorithm>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "../../../core/tests/programs/Check.hpp"
#include "../Sound.hpp"
#include "../Wave.hpp"

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

Ref<Sound> quietTone();
double checkNewSoundIsMono();
double checkNewSoundLength();
double checkSeconds();
double checkTonePeak();
double checkSquareTonePeak();
double checkScale();
double checkMixAdds();
double checkNormalize();
double checkDelayedMixKeepsLength();
double checkDelayedMixIsSilentBefore();
double checkDelayedMixStartsLater();
double checkToneFadesIn();
double checkNoiseAmplitude();
float filterGain(int64_t kind, float hz, float cutoff);
double checkLowPassCutsHighTone();
double checkLowPassKeepsLowTone();
double checkHighPassKeepsHighTone();
double checkHighPassCutsLowTone();
double checkEchoFirstRepeat();
double checkEchoSecondRepeat();
Ref<Sound> convolved();
double checkConvolutionLength();
double checkConvolutionValues();
Ref<Sound> reverbOfImpulse();
double checkReverbStart();
double checkReverbFirstEcho();

inline Ref<Sound> quietTone() {
  const Ref<Sound> s = soundNew(0.1f, 1000);
  s->tone(0, 125.0f, 0.5f, 0.0f, 0.1f);
  return s;
}

inline double checkNewSoundIsMono() {
  return offInt(soundNew(1.0f, 8000)->channels, 1);
}

inline double checkNewSoundLength() {
  return offInt(soundNew(2.5f, 8000)->count, 20000);
}

inline double checkSeconds() {
  return off(soundNew(2.5f, 8000)->seconds(), 2.5f);
}

inline double checkTonePeak() {
  return off(quietTone()->peak(), 0.5f);
}

inline double checkSquareTonePeak() {
  const Ref<Sound> s = soundNew(0.1f, 1000);
  s->tone(1, 125.0f, 0.4f, 0.0f, 0.1f);
  return off(s->peak(), 0.4f);
}

inline double checkScale() {
  const Ref<Sound> s = quietTone();
  s->scale(0.5f);
  return off(s->peak(), 0.25f);
}

inline double checkMixAdds() {
  const Ref<Sound> a = quietTone();
  a->add(quietTone(), 0.0f, 1.0f);
  return off(a->peak(), 1.0f);
}

inline double checkNormalize() {
  const Ref<Sound> s = soundNew(0.1f, 1000);
  s->tone(0, 125.0f, 0.3f, 0.0f, 0.1f);
  s->normalize(0.9f);
  return off(s->peak(), 0.9f);
}

inline double checkDelayedMixKeepsLength() {
  const Ref<Sound> a = soundNew(1.0f, 1000);
  a->add(quietTone(), 0.5f, 1.0f);
  return offInt(a->count, 1000);
}

inline double checkDelayedMixIsSilentBefore() {
  const Ref<Sound> a = soundNew(1.0f, 1000);
  a->add(quietTone(), 0.5f, 1.0f);
  return off(a->samples->at(499), 0.0f);
}

inline double checkDelayedMixStartsLater() {
  const Ref<Sound> a = soundNew(1.0f, 1000);
  a->add(quietTone(), 0.5f, 1.0f);
  return off(a->samples->at(525), 0.35355339f);
}

inline double checkToneFadesIn() {
  const Ref<Sound> s = soundNew(0.5f, 8000);
  s->tone(0, 440.0f, 1.0f, 0.0f, 0.5f);
  return off(s->samples->at(0), 0.0f);
}

inline double checkNoiseAmplitude() {
  const Ref<Sound> s = soundNew(0.1f, 8000);
  s->noise(0.25f, 7);
  const float loudest = s->peak();
  if (loudest > 0.25f) {
    return 1.0;
  }
  if (loudest < 0.2f) {
    return 1.0;
  }
  return 0.0;
}

inline float filterGain(int64_t kind, float hz, float cutoff) {
  const Ref<Sound> s = soundNew(1.0f, 8000);
  const List<float> data = s->samples;
  for (int64_t i = 0; i < 8000; i = i + 1) {
    data->at(i) = sine(hz * (float) i / 8000.0f);
  }
  if (kind == 0) {
    s->lowPass(cutoff);
  } else {
    s->highPass(cutoff);
  }
  float loudest = 0.0f;
  for (int64_t i = 6000; i < 8000; i = i + 1) {
    float level = data->at(i);
    if (level < 0.0f) {
      level = (float) (0.0 - (double) level);
    }
    if (level > loudest) {
      loudest = level;
    }
  }
  return loudest;
}

inline double checkLowPassCutsHighTone() {
  return off(filterGain(0, 1000.0f, 500.0f), 0.3987f);
}

inline double checkLowPassKeepsLowTone() {
  return off(filterGain(0, 200.0f, 500.0f), 0.9031f);
}

inline double checkHighPassKeepsHighTone() {
  return off(filterGain(1, 1000.0f, 500.0f), 0.7225f);
}

inline double checkHighPassCutsLowTone() {
  return off(filterGain(1, 200.0f, 500.0f), 0.3613f);
}

inline double checkEchoFirstRepeat() {
  const Ref<Sound> s = soundNew(0.5f, 1000);
  s->samples->at(0) = 1.0f;
  s->echo(0.1f, 0.5f);
  return off(s->samples->at(100), 0.5f);
}

inline double checkEchoSecondRepeat() {
  const Ref<Sound> s = soundNew(0.5f, 1000);
  s->samples->at(0) = 1.0f;
  s->echo(0.1f, 0.5f);
  return off(s->samples->at(200), 0.25f);
}

inline Ref<Sound> convolved() {
  const Ref<Sound> a = soundNew(0.002f, 1000);
  const Ref<Sound> r = soundNew(0.003f, 1000);
  a->samples->at(0) = 1.0f;
  a->samples->at(1) = 2.0f;
  r->samples->at(0) = 1.0f;
  r->samples->at(2) = 3.0f;
  return a->convolve(r);
}

inline double checkConvolutionLength() {
  return offInt(convolved()->count, 4);
}

inline double checkConvolutionValues() {
  const List<float> y = convolved()->samples;
  return off(y->at(0), 1.0f) + off(y->at(1), 2.0f) + off(y->at(2), 3.0f) + off(y->at(3), 6.0f);
}

inline Ref<Sound> reverbOfImpulse() {
  const Ref<Sound> s = soundNew(1.0f, 10000);
  s->samples->at(0) = 1.0f;
  s->reverb(1.0f);
  return s;
}

inline double checkReverbStart() {
  return off(reverbOfImpulse()->samples->at(0), 0.20525f);
}

inline double checkReverbFirstEcho() {
  return off(reverbOfImpulse()->samples->at(297), 0.03924f);
}
