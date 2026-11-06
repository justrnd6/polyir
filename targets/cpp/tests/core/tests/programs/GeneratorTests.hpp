#pragma once

#include <algorithm>
#include <any>
#include <coroutine>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <utility>
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

#ifndef POLYIR_VARIANT_HPP
#define POLYIR_VARIANT_HPP

// A variant: the number of its case and the value it holds, which it can compare and hash (`makeVariant` makes one).
struct Variant {
  int64_t tag = 0;
  std::any value;
  std::function<bool(const std::any&)> holds;
  std::function<std::size_t()> hasher;

  bool operator==(const Variant& other) const {
    return tag == other.tag && (holds ? holds(other.value) : !other.value.has_value());
  }

  std::size_t hash() const {
    return tag * 31 + (hasher ? hasher() : 0);
  }
};

template <typename T>
Variant makeVariant(int64_t tag, const T& value) {
  return Variant{tag, value, [value](const std::any& other) { return other.type() == typeid(T) && same(std::any_cast<T>(other), value); },
                 [value]() { return hashOf(value); }};
}
#endif

#ifndef POLYIR_COROUTINE_HPP
#define POLYIR_COROUTINE_HPP

// A coroutine: its body one resume at a time. `resume` gives case 0 with the value it yields, or case 1 when the body has
// finished; a `co_yield` hears whether the coroutine was closed.
template <typename T>
class Coroutine {
 public:
  struct promise_type {
    std::optional<T> value;
    bool closing = false;

    Coroutine get_return_object() {
      return Coroutine(std::coroutine_handle<promise_type>::from_promise(*this));
    }

    std::suspend_always initial_suspend() noexcept {
      return {};
    }

    std::suspend_always final_suspend() noexcept {
      return {};
    }

    void return_void() {}

    void unhandled_exception() {
      throw;
    }

    struct Heard {
      promise_type* promise;

      bool await_ready() noexcept {
        return false;
      }

      void await_suspend(std::coroutine_handle<>) noexcept {}

      bool await_resume() noexcept {
        return promise->closing;
      }
    };

    Heard yield_value(T next) {
      value = std::move(next);
      return Heard{this};
    }
  };

  explicit Coroutine(std::coroutine_handle<promise_type> handle) : handle(handle) {}

  Coroutine(Coroutine&& other) noexcept : handle(std::exchange(other.handle, {})) {}

  Coroutine(const Coroutine&) = delete;

  ~Coroutine() {
    if (handle) {
      handle.destroy();
    }
  }

  Variant resume() {
    if (finished || handle.done()) {
      finished = true;
      return Variant{1, std::any()};
    }
    started = true;
    handle.promise().value.reset();
    handle.resume();
    finished = handle.done();
    return finished ? Variant{1, std::any()} : makeVariant(0, *handle.promise().value);
  }

  bool done() const {
    return finished;
  }

  void close() {
    if (started && !finished && !handle.done()) {
      handle.promise().closing = true;
      handle.resume();
    }
    finished = true;
  }

 private:
  std::coroutine_handle<promise_type> handle;
  bool started = false;
  bool finished = false;
};
#endif

inline int64_t made = 0;

Coroutine<int64_t> upTo(int64_t limit);
Coroutine<int64_t> counted(int64_t limit);
double checkSumsEveryValue();
double checkEmptyCoroutineRunsNoTurns();
int64_t firstAbove(int64_t limit);
double checkReturnLeavesMidway();
double checkLoopsNest();
double checkSourceIsEvaluatedOnce();

inline Coroutine<int64_t> upTo(int64_t limit) {
  for (int64_t i = 0; i < limit; i = i + 1) {
    if (co_yield i) {
      co_return;
    }
  }
}

inline Coroutine<int64_t> counted(int64_t limit) {
  made = made + 1;
  return upTo(limit);
}

inline double checkSumsEveryValue() {
  int64_t total = 0;
  auto forGen_21_10 = upTo(5);
  Variant forStep_21_10 = forGen_21_10.resume();
  while (forStep_21_10.tag == 0) {
    const int64_t n = std::any_cast<int64_t>(forStep_21_10.value);
    total = total + n;
    forStep_21_10 = forGen_21_10.resume();
  }
  return offInt(total, 10);
}

inline double checkEmptyCoroutineRunsNoTurns() {
  int64_t turns = 0;
  auto forGen_29_10 = upTo(0);
  Variant forStep_29_10 = forGen_29_10.resume();
  while (forStep_29_10.tag == 0) {
    const int64_t n = std::any_cast<int64_t>(forStep_29_10.value);
    turns = turns + 1;
    forStep_29_10 = forGen_29_10.resume();
  }
  return offInt(turns, 0);
}

inline int64_t firstAbove(int64_t limit) {
  auto forGen_36_10 = upTo(100);
  Variant forStep_36_10 = forGen_36_10.resume();
  while (forStep_36_10.tag == 0) {
    const int64_t n = std::any_cast<int64_t>(forStep_36_10.value);
    if (n > limit) {
      forGen_36_10.close();
      return n;
    }
    forStep_36_10 = forGen_36_10.resume();
  }
  return -1;
}

inline double checkReturnLeavesMidway() {
  return offInt(firstAbove(6), 7);
}

inline double checkLoopsNest() {
  int64_t total = 0;
  auto forGen_50_10 = upTo(3);
  Variant forStep_50_10 = forGen_50_10.resume();
  while (forStep_50_10.tag == 0) {
    const int64_t a = std::any_cast<int64_t>(forStep_50_10.value);
    auto forGen_51_14 = upTo(2);
    Variant forStep_51_14 = forGen_51_14.resume();
    while (forStep_51_14.tag == 0) {
      const int64_t b = std::any_cast<int64_t>(forStep_51_14.value);
      total = total + a * 10 + b;
      forStep_51_14 = forGen_51_14.resume();
    }
    forStep_50_10 = forGen_50_10.resume();
  }
  return offInt(total, 63);
}

inline double checkSourceIsEvaluatedOnce() {
  made = 0;
  int64_t total = 0;
  auto forGen_61_10 = counted(4);
  Variant forStep_61_10 = forGen_61_10.resume();
  while (forStep_61_10.tag == 0) {
    const int64_t n = std::any_cast<int64_t>(forStep_61_10.value);
    total = total + n;
    forStep_61_10 = forGen_61_10.resume();
  }
  return offInt(made * 100 + total, 106);
}
