#pragma once

#include <algorithm>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <vector>
using namespace std::string_literals;

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

#ifndef POLYIR_COPIES_HPP
#define POLYIR_COPIES_HPP

// A copy holds the same items in a new list, map, set or struct; `with` makes one with one more.
template <typename T>
Ref<T> copyOf(const Ref<T>& value) {
  return std::make_shared<T>(*value);
}

template <typename T>
List<T> withItem(const List<T>& items, const std::type_identity_t<T>& item) {
  auto out = copyOf(items);
  out->push_back(item);
  return out;
}

template <typename T>
Set<T> withItem(const Set<T>& items, const std::type_identity_t<T>& item) {
  auto out = copyOf(items);
  out->insert(item);
  return out;
}

template <typename K, typename V>
Map<K, V> withEntry(const Map<K, V>& table, const std::type_identity_t<K>& key, const std::type_identity_t<V>& value) {
  auto out = copyOf(table);
  out->insert_or_assign(key, value);
  return out;
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

struct Point;
struct Named;

struct Point {
  int64_t x;
  int64_t y;

  Point(int64_t x, int64_t y) : x(x), y(y) {}

  bool operator==(const Point& other) const {
    return allSame(x, other.x, y, other.y);
  }

  std::size_t hash() const {
    return hashAll(x, y);
  }
};

struct Named {
  std::string name;
  Ref<Point> at;

  Named(std::string name, Ref<Point> at) : name(name), at(at) {}

  bool operator==(const Named& other) const {
    return allSame(name, other.name, at, other.at);
  }

  std::size_t hash() const {
    return hashAll(name, at);
  }
};

double checkAnArrayGrowsByMakingANewOne();
double checkThawAndFreezeCopy();
double checkAMapGrowsByMakingANewOne();
double checkWithReplacesAnExistingKey();
double checkHasTellsPresentFromAbsent();
double checkASetKeepsEachMemberOnce();
double checkAMutsetChangesInPlace();
double checkAMutmapChangesInPlace();
double checkAStructCanBeAKey();
double checkAStructWithAStringIsAKeyByContent();
double checkAnImmutableArrayIsAKey();
double checkMapsAreEqualWhateverTheOrderTheyWereBuiltIn();
double checkSetsAreEqualWhateverTheOrderTheyWereBuiltIn();
double checkMutableAndImmutableListsAreEqualByContent();
double checkRepeatedWithInALoopIsNotQuadraticInMemory();

inline double checkAnArrayGrowsByMakingANewOne() {
  const List<int64_t> a0 = newList<int64_t>();
  const List<int64_t> a1 = withItem(a0, 10);
  const List<int64_t> a2 = withItem(a1, 20);
  return offInt((int64_t) a0->size(), 0) + offInt((int64_t) a1->size(), 1) + offInt((int64_t) a2->size(), 2) + offInt(a2->at(0), 10) + offInt(a2->at(1), 20);
}

inline double checkThawAndFreezeCopy() {
  const List<int64_t> a = listOf<int64_t>({1, 2});
  const List<int64_t> m = copyOf(a);
  m->push_back(3);
  const List<int64_t> f = copyOf(m);
  m->push_back(4);
  return offInt((int64_t) a->size(), 2) + offInt((int64_t) m->size(), 4) + offInt((int64_t) f->size(), 3) + offInt(f->at(2), 3);
}

inline double checkAMapGrowsByMakingANewOne() {
  const Map<int64_t, int64_t> m0 = newMap<int64_t, int64_t>();
  const Map<int64_t, int64_t> m1 = withEntry(m0, 1, 100);
  Map<int64_t, int64_t> m = m1;
  for (int64_t i = 2; i < 30; i = i + 1) {
    m = withEntry(m, i, i * 100);
  }
  return offInt((int64_t) m0->size(), 0) + offInt((int64_t) m1->size(), 1) + offInt((int64_t) m->size(), 29) + offInt(getOr(m, 1, 0), 100) + offInt(getOr(m, 29, 0), 2900) + offInt(getOr(m1, 2, 0), 0);
}

inline double checkWithReplacesAnExistingKey() {
  const Map<int64_t, int64_t> m = withEntry(withEntry(newMap<int64_t, int64_t>(), 1, 10), 1, 11);
  return offInt((int64_t) m->size(), 1) + offInt(getOr(m, 1, 0), 11);
}

inline double checkHasTellsPresentFromAbsent() {
  const Map<std::string, int64_t> m = withEntry(newMap<std::string, int64_t>(), "a"s, 0);
  int64_t score = 0;
  if (m->contains("a"s)) {
    score = score + 1;
  }
  if (m->contains("b"s) == false) {
    score = score + 2;
  }
  return offInt(score, 3);
}

inline double checkASetKeepsEachMemberOnce() {
  const Set<int64_t> s = withItem(withItem(withItem(newSet<int64_t>(), 7), 8), 7);
  int64_t score = 0;
  if (s->contains(7)) {
    score = score + 1;
  }
  if (s->contains(9) == false) {
    score = score + 2;
  }
  return offInt((int64_t) s->size(), 2) + offInt(score, 3);
}

inline double checkAMutsetChangesInPlace() {
  const Set<std::string> s = newSet<std::string>();
  s->insert("x"s);
  s->insert("y"s);
  s->insert("x"s);
  int64_t score = 0;
  if (s->contains("y"s)) {
    score = score + 1;
  }
  if (s->contains("z"s) == false) {
    score = score + 2;
  }
  return offInt((int64_t) s->size(), 2) + offInt(score, 3);
}

inline double checkAMutmapChangesInPlace() {
  const Map<int64_t, int64_t> m = newMap<int64_t, int64_t>();
  m->insert_or_assign(1, 10);
  m->insert_or_assign(2, 20);
  m->insert_or_assign(1, 11);
  return offInt((int64_t) m->size(), 2) + offInt(getOr(m, 1, 0), 11) + offInt(getOr(m, 2, 0), 20);
}

inline double checkAStructCanBeAKey() {
  const Map<Ref<Point>, int64_t> seen = newMap<Ref<Point>, int64_t>();
  seen->insert_or_assign(std::make_shared<Point>(1, 2), 10);
  seen->insert_or_assign(std::make_shared<Point>(3, 4), 20);
  seen->insert_or_assign(std::make_shared<Point>(1, 2), 11);
  int64_t score = 0;
  if (seen->contains(std::make_shared<Point>(3, 4))) {
    score = score + 1;
  }
  if (seen->contains(std::make_shared<Point>(9, 9)) == false) {
    score = score + 2;
  }
  return offInt((int64_t) seen->size(), 2) + offInt(getOr(seen, std::make_shared<Point>(1, 2), 0), 11) + offInt(score, 3);
}

inline double checkAStructWithAStringIsAKeyByContent() {
  const Set<Ref<Named>> names = withItem(withItem(withItem(newSet<Ref<Named>>(), std::make_shared<Named>("a"s + "b"s, std::make_shared<Point>(1, 1))), std::make_shared<Named>("ab"s, std::make_shared<Point>(1, 1))), std::make_shared<Named>("c"s, std::make_shared<Point>(2, 2)));
  int64_t score = 0;
  if (names->contains(std::make_shared<Named>("c"s, std::make_shared<Point>(2, 2)))) {
    score = score + 1;
  }
  return offInt((int64_t) names->size(), 2) + offInt(score, 1);
}

inline double checkAnImmutableArrayIsAKey() {
  const Map<List<int64_t>, std::string> lists = withEntry(withEntry(newMap<List<int64_t>, std::string>(), listOf<int64_t>({1, 2}), "one-two"s), listOf<int64_t>({3}), "three"s);
  int64_t score = 0;
  if (getOr(lists, listOf<int64_t>({1, 2}), ""s) == "one-two"s) {
    score = score + 1;
  }
  if (getOr(lists, listOf<int64_t>({3}), ""s) == "three"s) {
    score = score + 2;
  }
  return offInt(score, 3);
}

inline double checkMapsAreEqualWhateverTheOrderTheyWereBuiltIn() {
  const Map<int64_t, std::string> a = withEntry(withEntry(newMap<int64_t, std::string>(), 1, "a"s), 2, "b"s);
  const Map<int64_t, std::string> b = withEntry(withEntry(newMap<int64_t, std::string>(), 2, "b"s), 1, "a"s);
  const Map<int64_t, std::string> c = withEntry(withEntry(newMap<int64_t, std::string>(), 2, "b"s), 1, "z"s);
  int64_t score = 0;
  if (same(a, b)) {
    score = score + 1;
  }
  if (!same(a, c)) {
    score = score + 2;
  }
  return offInt(score, 3);
}

inline double checkSetsAreEqualWhateverTheOrderTheyWereBuiltIn() {
  const Set<std::string> a = withItem(withItem(newSet<std::string>(), "x"s), "y"s);
  const Set<std::string> b = withItem(withItem(newSet<std::string>(), "y"s), "x"s);
  int64_t score = 0;
  if (same(a, b)) {
    score = score + 1;
  }
  if (!same(a, withItem(newSet<std::string>(), "x"s))) {
    score = score + 2;
  }
  return offInt(score, 3);
}

inline double checkMutableAndImmutableListsAreEqualByContent() {
  const List<int64_t> a = listOf<int64_t>({1, 2});
  const List<int64_t> b = listOf<int64_t>({1, 2});
  int64_t score = 0;
  if (same(a, b)) {
    score = score + 1;
  }
  if (a == b) {
    score = score + 100;
  }
  return offInt(score, 1);
}

inline double checkRepeatedWithInALoopIsNotQuadraticInMemory() {
  int64_t total = 0;
  for (int64_t r = 0; r < 200; r = r + 1) {
    List<int64_t> a = newList<int64_t>();
    Map<int64_t, int64_t> m = newMap<int64_t, int64_t>();
    Set<int64_t> s = newSet<int64_t>();
    for (int64_t i = 0; i < 20; i = i + 1) {
      a = withItem(a, i);
      m = withEntry(m, i, i * 2);
      s = withItem(s, i % 7);
    }
    total = total + (int64_t) a->size() + getOr(m, 19, 0) + (int64_t) s->size();
  }
  return offInt(total, 200 * 65);
}
