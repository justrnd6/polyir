#include <algorithm>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
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

#ifndef POLYIR_THROWN_HPP
#define POLYIR_THROWN_HPP

// What `throw` throws: the code thrown.
struct Thrown {
  int64_t value;
};
#endif

struct Acc;
struct Animal;
struct Dog;
struct Bird;

struct Acc : std::enable_shared_from_this<Acc> {
  int64_t total;
  std::string tag;
  List<int64_t> items;

  Acc(int64_t total, std::string tag, List<int64_t> items) : total(total), tag(tag), items(items) {}

  int64_t add(int64_t x);
  std::function<int64_t()> later();
  std::string describe();
  Ref<Acc> self();

  bool operator==(const Acc& other) const {
    return allSame(total, other.total, tag, other.tag, items, other.items);
  }

  std::size_t hash() const {
    return hashAll(total, tag, items);
  }
};

struct Animal {
  virtual ~Animal() = default;

  virtual int64_t legs(int64_t p0) = 0;
  virtual std::string name() = 0;
};

struct Dog : public Animal {
  int64_t age;

  Dog(int64_t age) : age(age) {}

  int64_t legs(int64_t n) override;
  std::string name() override;

  bool operator==(const Dog& other) const {
    return allSame(age, other.age);
  }

  std::size_t hash() const {
    return hashAll(age);
  }
};

struct Bird : public Animal {
  int64_t age;

  Bird(int64_t age) : age(age) {}

  int64_t legs(int64_t n) override;
  std::string name() override;

  bool operator==(const Bird& other) const {
    return allSame(age, other.age);
  }

  std::size_t hash() const {
    return hashAll(age);
  }
};

int64_t risky(int64_t x);
int64_t tryReturn(int64_t x);
int64_t nested(int64_t x);
std::function<int64_t(int64_t)> makeAdder(int64_t n);
int64_t twice(std::function<int64_t(int64_t)> f, int64_t x);
int64_t counterPair();

inline auto Acc::add(int64_t x) -> int64_t {
  this->total = this->total + x;
  this->items->push_back(x);
  return this->total;
}

inline auto Acc::later() -> std::function<int64_t()> {
  return [=]() { return this->total * 2; };
}

inline auto Acc::describe() -> std::string {
  return this->tag + ":"s + this->tag;
}

inline auto Acc::self() -> Ref<Acc> {
  return shared_from_this();
}

inline auto Dog::legs(int64_t n) -> int64_t {
  return 4 * n + this->age;
}

inline auto Dog::name() -> std::string {
  return "dog"s;
}

inline auto Bird::legs(int64_t n) -> int64_t {
  return 2 * n;
}

inline auto Bird::name() -> std::string {
  return "bird"s;
}

inline int64_t risky(int64_t x) {
  if (x < 0) {
    throw Thrown{7};
  }
  return x;
}

inline int64_t tryReturn(int64_t x) {
  try {
    return risky(x) + 1;
  } catch (const Thrown& thrown) {
    const int64_t code = thrown.value;
    return code * 100;
  }
  return 0;
}

inline int64_t nested(int64_t x) {
  int64_t r = 0;
  try {
    try {
      r = risky(x);
    } catch (const Thrown& thrown) {
      const int64_t a = thrown.value;
      r = a + 1;
      throw Thrown{r};
    }
  } catch (const Thrown& thrown) {
    const int64_t b = thrown.value;
    r = b * 10;
  }
  return r;
}

inline std::function<int64_t(int64_t)> makeAdder(int64_t n) {
  return [=](int64_t x) { return x + n; };
}

inline int64_t twice(std::function<int64_t(int64_t)> f, int64_t x) {
  return f(f(x));
}

inline int64_t counterPair() {
  auto c = std::make_shared<int64_t>(0);
  const std::function<int64_t()> inc = [=]() -> int64_t {
    (*c) = (*c) + 1;
    return (*c);
  };
  const std::function<int64_t()> get = [=]() { return (*c); };
  inc();
  inc();
  return get() * 10 + (*c);
}

int main() {
  int64_t score = 0;
  const Ref<Acc> a = std::make_shared<Acc>(1, "ab"s, newList<int64_t>());
  score = score + a->add(5) + a->add(6);
  const std::function<int64_t()> f = a->later();
  score = score + f();
  score = score + (int64_t) (a->items)->size();
  if (a->describe() == "ab:ab"s) {
    score = score + 1000;
  }
  const Ref<Acc> s = a->self();
  if (s == a) {
    score = score + 2000;
  }
  a->tag = "zz"s;
  if (s->tag == "zz"s) {
    score = score + 3000;
  }
  const List<Ref<Animal>> zoo = newList<Ref<Animal>>();
  zoo->push_back(std::make_shared<Dog>(3));
  zoo->push_back(std::make_shared<Bird>(1));
  for (int64_t i = 0; i < (int64_t) zoo->size(); i = i + 1) {
    score = score + zoo->at(i)->legs(2);
    if (zoo->at(i)->name() == "bird"s) {
      score = score + 7;
    }
  }
  score = score + tryReturn(3) + tryReturn(0 - 1);
  score = score + nested(4) + nested(0 - 2);
  const std::function<int64_t(int64_t)> add3 = makeAdder(3);
  score = score + twice(add3, 10);
  score = score + counterPair();
  return score % 251;
}
