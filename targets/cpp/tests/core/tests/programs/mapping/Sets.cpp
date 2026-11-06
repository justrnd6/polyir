#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <iostream>
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

struct Point;
struct Named;
struct Cell2;

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
  std::optional<std::string> tag;

  Named(std::string name, std::optional<std::string> tag) : name(name), tag(tag) {}

  bool operator==(const Named& other) const {
    return allSame(name, other.name, tag, other.tag);
  }

  std::size_t hash() const {
    return hashAll(name, tag);
  }
};

struct Cell2 {
  int64_t v;
  std::string note;

  Cell2(int64_t v, std::string note) : v(v), note(note) {}

  int64_t bump();

  bool operator==(const Cell2& other) const {
    return allSame(v, other.v, note, other.note);
  }

  std::size_t hash() const {
    return hashAll(v, note);
  }
};

int64_t double_(int64_t x);
int64_t inc(int64_t x);
int64_t sets();
int64_t maps();
int64_t strs();
int64_t opts();
int64_t bufs();
int64_t swaps();
int64_t choose(int64_t k);
int64_t loops(int64_t n);
int64_t unsigned_();
int64_t floats();

inline auto Cell2::bump() -> int64_t {
  this->v = this->v + 1;
  return this->v;
}

inline int64_t double_(int64_t x) {
  return x * 2;
}

inline int64_t inc(int64_t x) {
  return x + 1;
}

inline int64_t sets() {
  const Set<Ref<Point>> seen = newSet<Ref<Point>>();
  seen->insert(std::make_shared<Point>(1, 2));
  seen->insert(std::make_shared<Point>(3, 4));
  seen->insert(std::make_shared<Point>(1, 2));
  int64_t score = (int64_t) seen->size();
  if (seen->contains(std::make_shared<Point>(3, 4))) {
    score = score + 10;
  }
  const Set<std::string> words = newSet<std::string>();
  words->insert("a"s);
  words->insert("b"s);
  words->insert("a"s);
  score = score + (int64_t) words->size() * 100;
  return score;
}

inline int64_t maps() {
  const Map<std::string, int64_t> m = newMap<std::string, int64_t>();
  m->insert_or_assign("x"s, 5);
  m->insert_or_assign("y"s, 6);
  m->insert_or_assign("x"s, getOr(m, "x"s, 0) + 1);
  const Map<Ref<Point>, std::string> byPoint = newMap<Ref<Point>, std::string>();
  byPoint->insert_or_assign(std::make_shared<Point>(1, 1), "p"s);
  int64_t score = getOr(m, "x"s, 0) * 10 + getOr(m, "y"s, 0) + (int64_t) m->size();
  if (byPoint->contains(std::make_shared<Point>(1, 1))) {
    score = score + 100;
  }
  if (getOr(m, "none"s, 0) == 0) {
    score = score + 1000;
  }
  return score;
}

inline int64_t strs() {
  const std::string a = "ab"s;
  const std::string b = a + "cd"s + a;
  int64_t score = (int64_t) b.size();
  if (b == "abcdab"s) {
    score = score + 10;
  }
  if (b != a) {
    score = score + 100;
  }
  const std::string q = "say hi {x}"s;
  score = score + (int64_t) q.size();
  return score;
}

inline int64_t opts() {
  const Ref<Named> n = std::make_shared<Named>("a"s, std::optional<std::string>("t"s));
  const Ref<Named> m = std::make_shared<Named>("a"s, std::optional<std::string>("t"s));
  const Ref<Named> o = std::make_shared<Named>("a"s, std::optional<std::string>());
  int64_t score = 0;
  if (same(n, m)) {
    score = score + 1;
  }
  if (same(n, o)) {
    score = score + 10;
  }
  if (!(o->tag).has_value()) {
    score = score + 100;
  }
  return score + (int64_t) present(n->tag).size();
}

inline int64_t bufs() {
  const List<int64_t> b = newFilled<int64_t>(4, 0);
  b->at(0) = 7;
  b->at(3) = 9;
  int64_t s = 0;
  for (int64_t i = 0; i < 4; i = i + 1) {
    s = s + b->at(i);
  }
  return s;
}

inline int64_t swaps() {
  const Ref<Cell2> c = std::make_shared<Cell2>(1, "n"s);
  const Ref<Cell2> d = c;
  d->bump();
  c->note = c->note + "!"s;
  if (d->note == "n!"s) {
    return c->v * 10 + (int64_t) (d->note).size();
  }
  return 0;
}

inline int64_t choose(int64_t k) {
  int64_t label = 0;
  for (;;) {
    switch (label) {
      case 0: {
        if (k == 0) {
          label = 1;
          continue;
        } else if (k == 1) {
          label = 2;
          continue;
        } else {
          label = 3;
          continue;
        }
      }
      case 1: {
        return 100;
      }
      case 2: {
        return 200;
      }
      case 3: {
        return 300;
      }
    }
  }
}

inline int64_t loops(int64_t n) {
  int64_t s = 0;
  {
    int64_t i = 0;
    int64_t j = n;
    while (i < j) {
      s = s + i * j;
      i = i + 1;
      j = j - 1;
    }
  }
  int64_t k = 0;
  for (int64_t t = 0; t < 4; t = t + 1) {
    k = k + 1;
  }
  return s + k;
}

inline int64_t unsigned_() {
  const uint8_t a = 250;
  const uint8_t b = (uint8_t) (a + 10);
  const uint32_t c = 4000000000;
  const uint32_t d = c + c;
  const int32_t e = 2147483647;
  const int32_t f = e + 1;
  return (int64_t) b + (int64_t) d + (int64_t) f % 1000;
}

inline int64_t floats() {
  const double x = 7.5;
  const double y = 2.25;
  const double z = x / 2.0 + y * 2.0;
  if (z > 8.0 && x != y) {
    return (int64_t) z;
  }
  return 0;
}

int main() {
  int64_t score = 0;
  score = score + sets() + maps() + strs() + opts() + bufs() + swaps();
  score = score + choose(0) + choose(1) + choose(5) + loops(6) + unsigned_() + floats();
  return score % 251;
}
