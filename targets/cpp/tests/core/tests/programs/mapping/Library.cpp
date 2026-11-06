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

struct Account;
struct Vec2;

template <typename T>
struct Box;

inline int64_t visits = 0;

struct Account {
  int64_t balance;
  std::string owner;

  Account(int64_t balance, std::string owner) : balance(balance), owner(owner) {}

  int64_t deposit(int64_t amount);
  int64_t depositTwice(int64_t amount);

  bool operator==(const Account& other) const {
    return allSame(balance, other.balance, owner, other.owner);
  }

  std::size_t hash() const {
    return hashAll(balance, owner);
  }
};

struct Vec2 {
  double x;
  double y;

  Vec2(double x, double y) : x(x), y(y) {}

  double length2();
  Ref<Vec2> scaled(double k);

  bool operator==(const Vec2& other) const {
    return allSame(x, other.x, y, other.y);
  }

  std::size_t hash() const {
    return hashAll(x, y);
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

int64_t fib(int64_t n);
int64_t classify(int64_t n);
int64_t bump();
std::string greeting(std::string name);
int32_t narrow(int32_t a, int32_t b);
int64_t apply(std::function<int64_t(int64_t)> f, int64_t x);

inline auto Account::deposit(int64_t amount) -> int64_t {
  this->balance = this->balance + amount;
  return this->balance;
}

inline auto Account::depositTwice(int64_t amount) -> int64_t {
  this->deposit(amount);
  return this->deposit(amount);
}

inline auto Vec2::length2() -> double {
  return this->x * this->x + this->y * this->y;
}

inline auto Vec2::scaled(double k) -> Ref<Vec2> {
  return std::make_shared<Vec2>(this->x * k, this->y * k);
}

inline int64_t fib(int64_t n) {
  return n < 2 ? n : fib(n - 1) + fib(n - 2);
}

inline int64_t classify(int64_t n) {
  return n < 0 ? 1 : n == 0 ? 2 : n < 10 ? 3 : 4;
}

inline int64_t bump() {
  visits = visits + 1;
  return visits;
}

inline std::string greeting(std::string name) {
  return "hello, "s + name;
}

inline int32_t narrow(int32_t a, int32_t b) {
  return a * b + 7;
}

inline int64_t apply(std::function<int64_t(int64_t)> f, int64_t x) {
  return f(x) + f(x + 1);
}

int main() {
  int64_t score = 0;
  score = score + fib(10);
  const Ref<Account> acct = std::make_shared<Account>(100, "ann"s);
  score = score + acct->depositTwice(5);
  const Ref<Vec2> v = std::make_shared<Vec2>(3.0, 4.0);
  const Ref<Vec2> w = v->scaled(2.0);
  score = score + (int64_t) (w->length2());
  if (greeting("bo"s) == "hello, bo"s) {
    score = score + 1;
  }
  score = score + (int64_t) narrow(6, 7);
  const Set<int64_t> seen = newSet<int64_t>();
  for (int64_t i = 0; i < 20; i = i + 1) {
    seen->insert(i % 7);
  }
  score = score + (int64_t) seen->size();
  const List<int64_t> nums = newList<int64_t>();
  {
    int64_t i = 0;
    int64_t j = 10;
    while (i < 5) {
      nums->push_back(i * j);
      i = i + 1;
      j = j - 1;
    }
  }
  nums->at(0) = 100;
  score = score + nums->at(0) + nums->at(4) + (int64_t) nums->size();
  const Map<std::string, int64_t> ages = newMap<std::string, int64_t>();
  ages->insert_or_assign("ann"s, 31);
  if (ages->contains("ann"s)) {
    score = score + getOr(ages, "ann"s, 0);
  }
  if (ages->contains("zed"s) == false) {
    score = score + 1;
  }
  const int64_t factor = 3;
  score = score + apply([=](int64_t x) { return x * factor; }, 4);
  const Ref<Box<double>> b = std::make_shared<Box<double>>(2.5);
  score = score + (int64_t) (b->value * 4.0);
  score = score + classify(0 - 5) + classify(0) + classify(7) + classify(70);
  score = score + bump() + bump();
  return score;
}
