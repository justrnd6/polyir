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

struct Pt;
struct Counter;
struct Shape;
struct Sq;
struct Rect;

template <typename T>
struct Box;

struct Pt {
  int64_t x;
  int64_t y;

  Pt(int64_t x, int64_t y) : x(x), y(y) {}

  bool operator==(const Pt& other) const {
    return allSame(x, other.x, y, other.y);
  }

  std::size_t hash() const {
    return hashAll(x, y);
  }
};

struct Counter {
  int64_t n;
  std::string name;

  Counter(int64_t n, std::string name) : n(n), name(name) {}

  bool operator==(const Counter& other) const {
    return allSame(n, other.n, name, other.name);
  }

  std::size_t hash() const {
    return hashAll(n, name);
  }
};

struct Shape {
  virtual ~Shape() = default;

  virtual int64_t area() = 0;
};

struct Sq : public Shape {
  int64_t side;

  Sq(int64_t side) : side(side) {}

  int64_t area() override;

  bool operator==(const Sq& other) const {
    return allSame(side, other.side);
  }

  std::size_t hash() const {
    return hashAll(side);
  }
};

struct Rect : public Shape {
  int64_t w;
  int64_t h;

  Rect(int64_t w, int64_t h) : w(w), h(h) {}

  int64_t area() override;

  bool operator==(const Rect& other) const {
    return allSame(w, other.w, h, other.h);
  }

  std::size_t hash() const {
    return hashAll(w, h);
  }
};

template <typename T>
struct Box {
  T value;

  Box(T value) : value(value) {}

  bool operator==(const Box& other) const {
    return allSame(value, other.value);
  }

  std::size_t hash() const {
    return hashAll(value);
  }
};

int64_t totalArea(List<Ref<Shape>> shapes);
int64_t risky(int64_t x);
int64_t guarded(int64_t x);

inline auto Sq::area() -> int64_t {
  return this->side * this->side;
}

inline auto Rect::area() -> int64_t {
  return this->w * this->h;
}

inline int64_t totalArea(List<Ref<Shape>> shapes) {
  int64_t sum = 0;
  for (int64_t i = 0; i < (int64_t) shapes->size(); i = i + 1) {
    sum = sum + shapes->at(i)->area();
  }
  return sum;
}

inline int64_t risky(int64_t x) {
  if (x < 0) {
    throw Thrown{7};
  }
  return x;
}

inline int64_t guarded(int64_t x) {
  int64_t out = 0;
  try {
    out = risky(x);
  } catch (const Thrown& thrown) {
    const int64_t code = thrown.value;
    out = code * 10;
  }
  return out;
}

int main() {
  int64_t score = 0;
  const Ref<Pt> p = std::make_shared<Pt>(3, 4);
  const Ref<Pt> q = std::make_shared<Pt>(3, 4);
  if (same(p, q)) {
    score = score + 1;
  }
  const Ref<Counter> c = std::make_shared<Counter>(5, "hits"s);
  const Ref<Counter> d = c;
  d->n = d->n + 1;
  if (c == d) {
    score = score + c->n;
  }
  const List<Ref<Shape>> shapes = newList<Ref<Shape>>();
  shapes->push_back(std::make_shared<Sq>(3));
  shapes->push_back(std::make_shared<Rect>(2, 5));
  score = score + totalArea(shapes);
  auto calls = std::make_shared<int64_t>(0);
  const std::function<int64_t()> tick = [=]() -> int64_t {
    (*calls) = (*calls) + 1;
    return (*calls);
  };
  const int64_t first = tick();
  const int64_t second = tick();
  score = score + first + second + (*calls);
  const Map<std::string, int64_t> stock = newMap<std::string, int64_t>();
  stock->insert_or_assign("apple"s, 3);
  stock->insert_or_assign("pear"s, 4);
  score = score + getOr(stock, "apple"s, 0) * getOr(stock, "pear"s, 0);
  const Ref<Box<int64_t>> b = std::make_shared<Box<int64_t>>(7);
  score = score + b->value;
  score = score + guarded(5) + guarded(0 - 1);
  return score;
}
