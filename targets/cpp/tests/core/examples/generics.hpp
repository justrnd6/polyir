#pragma once

#include <algorithm>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <utility>
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

#ifndef POLYIR_EITHER_HPP
#define POLYIR_EITHER_HPP

// An `either`: an ok value or an error, and which one it holds.
template <typename T, typename E>
struct Either {
  bool isOk = false;
  std::optional<T> okHeld;
  std::optional<E> errHeld;

  static Either ok(T value) {
    Either out;
    out.isOk = true;
    out.okHeld = std::move(value);
    return out;
  }

  static Either err(E value) {
    Either out;
    out.errHeld = std::move(value);
    return out;
  }

  T okValue() const {
    return *okHeld;
  }

  E errValue() const {
    return *errHeld;
  }

  bool operator==(const Either& other) const {
    return isOk == other.isOk && same(okHeld, other.okHeld) && same(errHeld, other.errHeld);
  }

  std::size_t hash() const {
    return isOk ? hashOf(*okHeld) : hashOf(*errHeld) * 31 + 1;
  }
};
#endif

template <typename T>
struct Box;

struct Pair_i64_double;
struct Doubler_i64;

template <typename T>
struct Container;

struct Pair_i64_i64;

template <typename T>
struct Box {
  T value;

  Box(T value) : value(value) {}

  T getValue() {
    return this->value;
  }

  bool operator==(const Box& other) const {
    return allSame(value, other.value);
  }

  std::size_t hash() const {
    return hashAll(value);
  }
};

struct Pair_i64_double {
  int64_t a;
  double b;

  Pair_i64_double(int64_t a, double b) : a(a), b(b) {}

  int64_t sumAsInt();

  bool operator==(const Pair_i64_double& other) const {
    return allSame(a, other.a, b, other.b);
  }

  std::size_t hash() const {
    return hashAll(a, b);
  }
};

struct Doubler_i64 {
  int64_t value;

  Doubler_i64(int64_t value) : value(value) {}

  int64_t doubled();

  bool operator==(const Doubler_i64& other) const {
    return allSame(value, other.value);
  }

  std::size_t hash() const {
    return hashAll(value);
  }
};

template <typename T>
struct Container {
  Either<T, int64_t> value;

  Container(Either<T, int64_t> value) : value(value) {}

  T unwrapOrDefault(T def) {
    if (!(this->value).isOk) {
      return def;
    }
    return (this->value).okValue();
  }

  bool operator==(const Container& other) const {
    return allSame(value, other.value);
  }

  std::size_t hash() const {
    return hashAll(value);
  }
};

struct Pair_i64_i64 {
  int64_t a;
  int64_t b;

  Pair_i64_i64(int64_t a, int64_t b) : a(a), b(b) {}

  int64_t sumAsInt();

  bool operator==(const Pair_i64_i64& other) const {
    return allSame(a, other.a, b, other.b);
  }

  std::size_t hash() const {
    return hashAll(a, b);
  }
};

int64_t boxedI64();
int64_t boxedBoth();
int64_t pairOfIntAndDouble();
int64_t doubledI64();
int64_t containerOk();
Either<Ref<Pair_i64_i64>, int64_t> makePairResult(int64_t ok);
int64_t pairResultOrErr(int64_t ok);

inline int64_t boxedI64() {
  const Ref<Box<int64_t>> b = std::make_shared<Box<int64_t>>(42);
  return b->getValue();
}

inline int64_t boxedBoth() {
  const Ref<Box<int64_t>> bi = std::make_shared<Box<int64_t>>(100);
  const Ref<Box<double>> bd = std::make_shared<Box<double>>(2.0);
  return bi->getValue() + (int64_t) (bd->getValue());
}

inline int64_t pairOfIntAndDouble() {
  const Ref<Pair_i64_double> p = std::make_shared<Pair_i64_double>(7, 8.5);
  return p->sumAsInt();
}

inline int64_t doubledI64() {
  const Ref<Doubler_i64> d = std::make_shared<Doubler_i64>(21);
  return d->doubled();
}

inline int64_t containerOk() {
  const Ref<Container<int64_t>> c = std::make_shared<Container<int64_t>>(Either<int64_t, int64_t>::ok(42));
  return c->unwrapOrDefault(0);
}

inline Either<Ref<Pair_i64_i64>, int64_t> makePairResult(int64_t ok) {
  if (ok == 0) {
    return Either<Ref<Pair_i64_i64>, int64_t>::err(-1);
  }
  return Either<Ref<Pair_i64_i64>, int64_t>::ok(std::make_shared<Pair_i64_i64>(11, 22));
}

inline int64_t pairResultOrErr(int64_t ok) {
  const Either<Ref<Pair_i64_i64>, int64_t> r = makePairResult(ok);
  if (!r.isOk) {
    return r.errValue();
  }
  const Ref<Pair_i64_i64> p = r.okValue();
  return p->a * 100 + p->b;
}

inline auto Pair_i64_double::sumAsInt() -> int64_t {
  return (int64_t) (this->a) + (int64_t) (this->b);
}

inline auto Doubler_i64::doubled() -> int64_t {
  return this->value + this->value;
}

inline auto Pair_i64_i64::sumAsInt() -> int64_t {
  return (int64_t) (this->a) + (int64_t) (this->b);
}
