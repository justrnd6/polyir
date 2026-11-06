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

int64_t sumPushed(int64_t a, int64_t b, int64_t c);
int64_t lenCheck(int64_t a, int64_t b, int64_t c);
int64_t sumSquaresUpTo(int64_t n);
int64_t sumEvensBelow(int64_t n);
int64_t fillSquares(List<int64_t> arr, int64_t count);
int64_t squaresThroughAHelper(int64_t count);
int64_t pushGivesTheArrayBack();
int64_t sumEvensBelowExplicit(int64_t n);

inline int64_t sumPushed(int64_t a, int64_t b, int64_t c) {
  const List<int64_t> arr = newList<int64_t>();
  arr->push_back(a);
  arr->push_back(b);
  arr->push_back(c);
  arr->push_back(a + b + c);
  return arr->at(0) + arr->at(1) + arr->at(2) + arr->at(3);
}

inline int64_t lenCheck(int64_t a, int64_t b, int64_t c) {
  const List<int64_t> arr = newList<int64_t>();
  arr->push_back(a);
  arr->push_back(b);
  arr->push_back(c);
  return (int64_t) arr->size();
}

inline int64_t sumSquaresUpTo(int64_t n) {
  const List<int64_t> arr = newList<int64_t>();
  for (int64_t i = 0; i < n; i = i + 1) {
    arr->push_back(i * i);
  }
  int64_t total = 0;
  for (int64_t i = 0; i < (int64_t) arr->size(); i = i + 1) {
    total = total + arr->at(i);
  }
  return total;
}

inline int64_t sumEvensBelow(int64_t n) {
  const List<int64_t> arr = newList<int64_t>();
  for (int64_t i = 0; i < n; i = i + 1) {
    if (i % 2 == 0) {
      arr->push_back(i);
    }
  }
  int64_t total = 0;
  for (int64_t i = 0; i < (int64_t) arr->size(); i = i + 1) {
    total = total + arr->at(i);
  }
  return total;
}

inline int64_t fillSquares(List<int64_t> arr, int64_t count) {
  for (int64_t i = 0; i < count; i = i + 1) {
    arr->push_back(i * i);
  }
  return count;
}

inline int64_t squaresThroughAHelper(int64_t count) {
  const List<int64_t> arr = newList<int64_t>();
  fillSquares(arr, count);
  return (int64_t) arr->size() * 1000 + arr->at(count - 1);
}

inline int64_t pushGivesTheArrayBack() {
  List<int64_t> arr = newList<int64_t>();
  arr = pushed(arr, 10);
  arr = pushed(arr, 20);
  return arr->at(0) + arr->at(1);
}

inline int64_t sumEvensBelowExplicit(int64_t n) {
  int64_t label = 0;
  int64_t i = 0;
  List<int64_t> arr = newList<int64_t>();
  for (;;) {
    switch (label) {
      case 0: {
        const List<int64_t> empty = newList<int64_t>();
        i = 0;
        arr = empty;
        label = 1;
        continue;
      }
      case 1: {
        if (i < n) {
          label = 2;
          continue;
        } else {
          label = 5;
          continue;
        }
      }
      case 2: {
        if (i % 2 == 0) {
          label = 3;
          continue;
        } else {
          label = 4;
          continue;
        }
      }
      case 3: {
        int64_t next_i = i + 1;
        List<int64_t> next_arr = pushed(arr, i);
        i = next_i;
        arr = next_arr;
        label = 1;
        continue;
      }
      case 4: {
        i = i + 1;
        label = 1;
        continue;
      }
      case 5: {
        int64_t i2 = 0;
        int64_t total = 0;
        while (i2 < (int64_t) arr->size()) {
          i2 = i2 + 1;
          total = total + arr->at(i2);
        }
        return total;
      }
    }
  }
}
