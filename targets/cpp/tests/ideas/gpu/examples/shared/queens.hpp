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

int32_t queens(int32_t t, List<int32_t> prefixes, List<int32_t> p, List<int32_t> stack);
int64_t listPrefixes(int64_t n, int64_t depth, List<int32_t> out);

inline int32_t queens(int32_t t, List<int32_t> prefixes, List<int32_t> p, List<int32_t> stack) {
  const int32_t n = p->at(0);
  const int32_t depth = p->at(1);
  const int32_t full = (int32_t) ((1 << (int64_t) n) - 1);
  const int32_t base = t * (n + 1) * 4;
  stack->at(base) = prefixes->at(t * 3);
  stack->at(base + 1) = prefixes->at(t * 3 + 1);
  stack->at(base + 2) = prefixes->at(t * 3 + 2);
  stack->at(base + 3) = full & ~(prefixes->at(t * 3) | prefixes->at(t * 3 + 1) | prefixes->at(t * 3 + 2));
  int32_t level = 0;
  int32_t count = 0;
  for (int64_t step = 0; level >= 0; step = step + 1) {
    const int32_t at = base + level * 4;
    const int32_t free = stack->at(at + 3);
    if (free == 0) {
      level = level - 1;
    } else {
      const int32_t bit = free & (int32_t) (0 - (int64_t) free);
      stack->at(at + 3) = free ^ bit;
      if (depth + level + 1 == n) {
        count = count + 1;
      } else {
        const int32_t nextCols = stack->at(at) | bit;
        const int32_t nextLeft = (stack->at(at + 1) | bit) << 1 & full;
        const int32_t nextRight = (stack->at(at + 2) | bit) >> 1;
        level = level + 1;
        const int32_t below = base + level * 4;
        stack->at(below) = nextCols;
        stack->at(below + 1) = nextLeft;
        stack->at(below + 2) = nextRight;
        stack->at(below + 3) = full & ~(nextCols | nextLeft | nextRight);
      }
    }
  }
  return count;
}

inline int64_t listPrefixes(int64_t n, int64_t depth, List<int32_t> out) {
  int64_t total = 1;
  int64_t r = 0;
  while (r < depth) {
    total = total * n;
    r = r + 1;
  }
  const int32_t full = (int32_t) ((1 << n) - 1);
  int64_t found = 0;
  for (int64_t code = 0; code < total; code = code + 1) {
    int32_t left = 0;
    int32_t right = 0;
    int32_t cols = 0;
    int64_t ok = 1;
    int64_t rest = code;
    for (int64_t r2 = 0; r2 < depth; r2 = r2 + 1) {
      const int64_t column = rest % n;
      rest = rest / n;
      const int32_t bit = (int32_t) (1 << column);
      if ((bit & (cols | left | right)) != 0) {
        ok = 0;
      }
      cols = cols | bit;
      left = (left | bit) << 1 & full;
      right = (right | bit) >> 1;
    }
    if (ok == 1) {
      out->at(found * 3) = cols;
      out->at(found * 3 + 1) = left;
      out->at(found * 3 + 2) = right;
      found = found + 1;
    }
  }
  return found;
}
