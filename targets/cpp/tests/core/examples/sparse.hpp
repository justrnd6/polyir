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

#ifndef POLYIR_SPARSE_HPP
#define POLYIR_SPARSE_HPP

// A sparse grid: the cells written, as rows, columns and values. A read finds the last write to its cell; a cell never
// written reads as the element's zero. `set(value, row, col)` writes one.
template <typename T>
struct Sparse {
  std::vector<int64_t> rows;
  std::vector<int64_t> cols;
  std::vector<T> values;
  T zero;

  explicit Sparse(const T& zero) : zero(zero) {}

  int64_t size() const {
    return values.size();
  }

  T get(int64_t row, int64_t col) const {
    T found = zero;
    for (std::size_t i = 0; i < values.size(); i++) {
      if (rows[i] == row && cols[i] == col) {
        found = values[i];
      }
    }
    return found;
  }

  void set(const T& value, int64_t row, int64_t col) {
    rows.push_back(row);
    cols.push_back(col);
    values.push_back(value);
  }
};
#endif

int64_t writeAndReadBack();
int64_t unsetCellIsZero();
int64_t lastWriteWins();
int64_t oneDimensionalRoundTrip();
int64_t lenAfterWrites();

inline int64_t writeAndReadBack() {
  const Ref<Sparse<int64_t>> sp = std::make_shared<Sparse<int64_t>>(0);
  sp->set(10, 0, 0);
  sp->set(20, 2, 3);
  sp->set(30, 5, 1);
  return sp->get(2, 3) * 100 + sp->get(5, 1) * 10 + sp->get(0, 0);
}

inline int64_t unsetCellIsZero() {
  const Ref<Sparse<int64_t>> sp = std::make_shared<Sparse<int64_t>>(0);
  sp->set(99, 1, 1);
  return sp->get(9, 9);
}

inline int64_t lastWriteWins() {
  const Ref<Sparse<int64_t>> sp = std::make_shared<Sparse<int64_t>>(0);
  sp->set(1, 3, 3);
  sp->set(2, 3, 3);
  sp->set(3, 3, 3);
  return sp->get(3, 3);
}

inline int64_t oneDimensionalRoundTrip() {
  const Ref<Sparse<int64_t>> sp = std::make_shared<Sparse<int64_t>>(0);
  sp->set(100, 0, 0);
  sp->set(200, 7, 0);
  sp->set(300, 3, 0);
  return sp->get(7, 0) * 100 + sp->get(3, 0) * 10 + sp->get(0, 0);
}

inline int64_t lenAfterWrites() {
  const Ref<Sparse<int64_t>> sp = std::make_shared<Sparse<int64_t>>(0);
  sp->set(1, 0, 0);
  sp->set(2, 1, 1);
  sp->set(3, 1, 1);
  return (int64_t) sp->size();
}
