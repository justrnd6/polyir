#include <algorithm>
#include <any>
#include <coroutine>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>
using namespace std::string_literals;

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

#ifndef POLYIR_FAIL_HPP
#define POLYIR_FAIL_HPP

// Stops the program with a message, as polyir's `fail` does.
[[noreturn]] inline void fail(const std::string& message) {
  std::cerr << "trap: " << message << '\n';
  std::exit(1);
}

// The value an optional holds; an empty one stops the program.
template <typename T>
T present(const std::optional<T>& value) {
  if (!value.has_value()) {
    fail("unwrap of an empty optional");
  }
  return *value;
}
#endif

#ifndef POLYIR_THROWN_HPP
#define POLYIR_THROWN_HPP

// What `throw` throws: the code thrown.
struct Thrown {
  int64_t value;
};
#endif

struct Node;
struct Holder;

inline bool flag = false;
inline double ratio = 1.5;
inline int64_t hits = 0;

struct Node {
  int64_t value;
  std::optional<Ref<Node>> next;

  Node(int64_t value, std::optional<Ref<Node>> next) : value(value), next(next) {}

  bool operator==(const Node& other) const {
    return allSame(value, other.value, next, other.next);
  }

  std::size_t hash() const {
    return hashAll(value, next);
  }
};

struct Holder {
  std::function<int64_t(int64_t)> op;
  std::string name;

  Holder(std::function<int64_t(int64_t)> op, std::string name) : op(op), name(name) {}

  int64_t run(int64_t x);
  std::function<int64_t(int64_t)> viaLater();

  bool operator==(const Holder& other) const {
    return allSame(op, other.op, name, other.name);
  }

  std::size_t hash() const {
    return hashAll(op, name);
  }
};

Coroutine<Ref<Node>> pairs(int64_t n);
int64_t sumNodes(int64_t n);
int64_t chain();
int64_t nestedClosures(int64_t a);
int64_t strClosure(std::string prefix);
int64_t holders();
int64_t globals();
int64_t findFirst(int64_t limit);
int64_t tryInLoop(int64_t n);
std::string lines(std::string a, std::string b);
int64_t lineLen();
int64_t machineStrings(int64_t n);
int64_t precedence(int64_t a, int64_t b);

inline auto Holder::run(int64_t x) -> int64_t {
  const std::function<int64_t(int64_t)> f = this->op;
  return f(x);
}

inline auto Holder::viaLater() -> std::function<int64_t(int64_t)> {
  return [=](int64_t y) -> int64_t {
    const std::function<int64_t(int64_t)> f = this->op;
    return f(y) + (int64_t) (this->name).size();
  };
}

inline Coroutine<Ref<Node>> pairs(int64_t n) {
  for (int64_t i = 0; i < n; i = i + 1) {
    if (co_yield std::make_shared<Node>(i, std::optional<Ref<Node>>())) {
      co_return;
    }
  }
}

inline int64_t sumNodes(int64_t n) {
  auto g = pairs(n);
  int64_t s = 0;
  bool done = false;
  for (int64_t k = 0; k < 20 && done == false; k = k + 1) {
    const Variant r = g.resume();
    if (r.tag == 1) {
      done = true;
    } else {
      s = s + std::any_cast<Ref<Node>>(r.value)->value;
    }
  }
  return s;
}

inline int64_t chain() {
  const Ref<Node> a = std::make_shared<Node>(1, std::optional<Ref<Node>>());
  const Ref<Node> b = std::make_shared<Node>(2, std::optional<Ref<Node>>(a));
  const Ref<Node> c = std::make_shared<Node>(3, std::optional<Ref<Node>>(b));
  int64_t sum = 0;
  std::optional<Ref<Node>> cur = std::optional<Ref<Node>>(c);
  for (int64_t i = 0; i < 5 && cur.has_value(); i = i + 1) {
    const Ref<Node> n = present(cur);
    sum = sum + n->value;
    cur = n->next;
  }
  a->value = 10;
  return sum * 100 + present(b->next)->value;
}

inline int64_t nestedClosures(int64_t a) {
  auto total = std::make_shared<int64_t>(0);
  const std::function<int64_t(int64_t)> outer = [=](int64_t x) -> int64_t {
    const std::function<int64_t(int64_t)> inner = [=](int64_t y) -> int64_t {
      (*total) = (*total) + y;
      return x * y + a;
    };
    return inner(x + 1) + inner(2);
  };
  const int64_t r = outer(3);
  return r * 1000 + (*total);
}

inline int64_t strClosure(std::string prefix) {
  const std::string tail = "!"s;
  const std::function<std::string(std::string)> f = [=](std::string s) { return prefix + s + tail; };
  return (int64_t) f("abc"s).size();
}

inline int64_t holders() {
  const Ref<Holder> h = std::make_shared<Holder>([=](int64_t x) { return x * x; }, "hh"s);
  const std::function<int64_t(int64_t)> later = h->viaLater();
  return h->run(5) + later(2);
}

inline int64_t globals() {
  hits = hits + 5;
  flag = true;
  ratio = ratio * 2.0;
  int64_t r = hits;
  if (flag) {
    r = r + 100;
  }
  if (ratio > 2.5) {
    r = r + 1000;
  }
  return r;
}

inline int64_t findFirst(int64_t limit) {
  for (int64_t i = 0; i < limit; i = i + 1) {
    for (int64_t j = 0; j < limit; j = j + 1) {
      if (i * j == 12) {
        return i * 100 + j;
      }
    }
  }
  return 0 - 1;
}

inline int64_t tryInLoop(int64_t n) {
  int64_t s = 0;
  for (int64_t i = 0; i < n; i = i + 1) {
    try {
      if (i % 2 == 1) {
        throw Thrown{i};
      }
      s = s + 10;
    } catch (const Thrown& thrown) {
      const int64_t code = thrown.value;
      s = s + code;
    }
  }
  return s;
}

inline std::string lines(std::string a, std::string b) {
  int64_t label = 0;
  std::string acc = ""s;
  int64_t n = 0;
  for (;;) {
    switch (label) {
      case 0: {
        acc = a;
        n = 0;
        label = 1;
        continue;
      }
      case 1: {
        if (n < 3) {
          label = 2;
          continue;
        } else {
          label = 3;
          continue;
        }
      }
      case 2: {
        acc = acc + b;
        n = n + 1;
        label = 1;
        continue;
      }
      case 3: {
        return acc;
      }
    }
  }
}

inline int64_t lineLen() {
  return (int64_t) lines("x"s, "yz"s).size();
}

inline int64_t machineStrings(int64_t n) {
  int64_t label = 0;
  int64_t p = 0;
  std::string s = ""s;
  int64_t q = 0;
  int64_t r = 0;
  for (;;) {
    switch (label) {
      case 0: {
        if (n == 0) {
          p = n;
          s = "p"s;
          label = 1;
          continue;
        } else {
          q = n;
          s = "q"s;
          label = 2;
          continue;
        }
      }
      case 1: {
        if (p > 4) {
          r = p;
          label = 3;
          continue;
        } else {
          q = p + 1;
          s = s + "a"s;
          label = 2;
          continue;
        }
      }
      case 2: {
        if (q > 4) {
          r = q;
          label = 3;
          continue;
        } else {
          p = q + 1;
          s = s + "b"s;
          label = 1;
          continue;
        }
      }
      case 3: {
        return r * 100 + (int64_t) s.size();
      }
    }
  }
}

inline int64_t precedence(int64_t a, int64_t b) {
  const int64_t x = (a + b) * (a - b) / 3;
  const int64_t y = a & b | a ^ b;
  const int64_t z = a << 2 + b >> 1;
  int64_t s = x + y + z;
  if ((a & 1) == 1 && b > 2 || a == 0) {
    s = s + 1;
  }
  const int32_t w = (int32_t) a;
  if ((int64_t) w < 100 && (int32_t) (a + b) < 1000) {
    s = s + 2;
  }
  return s;
}

int main() {
  int64_t score = sumNodes(5) + chain() + nestedClosures(2) + strClosure("ab"s);
  score = score + holders() + globals() + findFirst(10) + tryInLoop(6);
  score = score + lineLen() + machineStrings(0) + machineStrings(1) + precedence(13, 7);
  return score % 251;
}
