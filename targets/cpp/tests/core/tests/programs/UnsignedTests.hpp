#pragma once

#include <algorithm>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "Check.hpp"

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

#ifndef POLYIR_LISTS_HPP
#define POLYIR_LISTS_HPP

// A push used as a value gives the list; `contains` asks a list for a value; a map gives a key it does not hold its zero.
template <typename T>
List<T> pushed(List<T> items, std::type_identity_t<T> item) {
  items->push_back(item);
  return items;
}

template <typename T>
bool contains(const List<T>& items, const std::type_identity_t<T>& item) {
  return std::any_of(items->begin(), items->end(), [&](const T& each) { return same(each, item); });
}

template <typename K, typename V>
V getOr(const Map<K, V>& table, const std::type_identity_t<K>& key, const std::type_identity_t<V>& zero) {
  const auto found = table->find(key);
  return found == table->end() ? zero : found->second;
}

template <typename K, typename V>
std::vector<K> keysOf(const Map<K, V>& table) {
  std::vector<K> keys;
  for (const auto& entry : *table) {
    keys.push_back(entry.first);
  }
  return keys;
}

template <typename T>
List<T> truncate(List<T> items, int64_t count) {
  if (count < (int64_t) items->size()) {
    items->resize(std::max<int64_t>(count, 0));
  }
  return items;
}

template <typename T>
List<T> concat(const List<T>& a, const List<T>& b) {
  auto out = std::make_shared<std::vector<T>>(*a);
  out->insert(out->end(), b->begin(), b->end());
  return out;
}
#endif

struct Pixel;

struct Pixel {
  uint8_t r;
  uint8_t g;
  uint8_t b;

  Pixel(uint8_t r, uint8_t g, uint8_t b) : r(r), g(g), b(b) {}

  bool operator==(const Pixel& other) const {
    return allSame(r, other.r, g, other.g, b, other.b);
  }

  std::size_t hash() const {
    return hashAll(r, g, b);
  }
};

double checkAnUnsignedComparisonDoesNotSeeTheTopBitAsASign();
double checkAnUnsignedDivisionAndRemainder();
double checkAnUnsignedRightShiftFillsWithZeros();
double checkAnUnsignedWidensWithZeroBits();
double checkNarrowUnsignedArithmeticWrapsAtItsSize();
double checkAnUnsignedGoesToAFloatAsItsValue();
double checkTheLargestU64IsLargerThanFive();
double checkAnUnsignedFieldKeepsItsValueAndStructsCompare();
double checkUnsignedKeysAreOrdinaryKeys();

inline double checkAnUnsignedComparisonDoesNotSeeTheTopBitAsASign() {
  const uint32_t big = 4000000000;
  int64_t seen = 0;
  if (big > 3000000000) {
    seen = seen + 1;
  }
  if (big >= big) {
    seen = seen + 10;
  }
  if (big < 5) {
    seen = seen + 100;
  }
  return offInt(seen, 11);
}

inline double checkAnUnsignedDivisionAndRemainder() {
  const uint32_t big = 4000000000;
  const uint32_t half = big / 2;
  const uint32_t rest = big % 7;
  return offInt((int64_t) half, 2000000000) + offInt((int64_t) rest, 3);
}

inline double checkAnUnsignedRightShiftFillsWithZeros() {
  const uint32_t big = 4000000000;
  const uint32_t shifted = big >> 28;
  const uint64_t m = (uint64_t) (0 - 1);
  return offInt((int64_t) shifted, 14) + offInt((int64_t) (m >> 63), 1);
}

inline double checkAnUnsignedWidensWithZeroBits() {
  const uint32_t big = 4000000000;
  const uint8_t small = 200;
  return offInt((int64_t) big, 4000000000) + offInt((int64_t) small, 200);
}

inline double checkNarrowUnsignedArithmeticWrapsAtItsSize() {
  const uint8_t b = 200;
  const uint8_t c = 100;
  const uint8_t sum = (uint8_t) (b + c);
  const uint8_t under = (uint8_t) (c - b);
  return offInt((int64_t) sum, 44) + offInt((int64_t) under, 156);
}

inline double checkAnUnsignedGoesToAFloatAsItsValue() {
  const uint32_t big = 4000000000;
  const double asDouble = (double) big;
  return offInt((int64_t) asDouble, 4000000000);
}

inline double checkTheLargestU64IsLargerThanFive() {
  const uint64_t m = (uint64_t) (0 - 1);
  int64_t seen = 0;
  if (m > 5) {
    seen = seen + 1;
  }
  if (m / 2 == 9223372036854775807) {
    seen = seen + 10;
  }
  return offInt(seen, 11);
}

inline double checkAnUnsignedFieldKeepsItsValueAndStructsCompare() {
  const Ref<Pixel> a = std::make_shared<Pixel>(250, 128, 7);
  const Ref<Pixel> b = std::make_shared<Pixel>(250, 128, 7);
  int64_t seen = 0;
  if (same(a, b)) {
    seen = seen + 1;
  }
  seen = seen + (int64_t) (a->g);
  return offInt(seen, 129);
}

inline double checkUnsignedKeysAreOrdinaryKeys() {
  const Map<uint32_t, int64_t> m = newMap<uint32_t, int64_t>();
  const uint32_t k = 4000000000;
  m->insert_or_assign(k, 7);
  m->insert_or_assign(5, 9);
  return offInt(getOr(m, k, 0), 7) + offInt(getOr(m, 5, 0), 9);
}
