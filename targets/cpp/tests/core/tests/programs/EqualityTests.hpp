#pragma once

#include <algorithm>
#include <any>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>
using namespace std::string_literals;

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

#ifndef POLYIR_COPIES_HPP
#define POLYIR_COPIES_HPP

// A copy holds the same items in a new list, map, set or struct; `with` makes one with one more.
template <typename T>
Ref<T> copyOf(const Ref<T>& value) {
  return std::make_shared<T>(*value);
}

template <typename T>
List<T> withItem(const List<T>& items, const std::type_identity_t<T>& item) {
  auto out = copyOf(items);
  out->push_back(item);
  return out;
}

template <typename T>
Set<T> withItem(const Set<T>& items, const std::type_identity_t<T>& item) {
  auto out = copyOf(items);
  out->insert(item);
  return out;
}

template <typename K, typename V>
Map<K, V> withEntry(const Map<K, V>& table, const std::type_identity_t<K>& key, const std::type_identity_t<V>& value) {
  auto out = copyOf(table);
  out->insert_or_assign(key, value);
  return out;
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

struct Point;
struct Line;
struct Named;
struct Counter;
struct Digit;
struct Wrapped;

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

struct Line {
  Ref<Point> a;
  Ref<Point> b;

  Line(Ref<Point> a, Ref<Point> b) : a(a), b(b) {}

  bool operator==(const Line& other) const {
    return allSame(a, other.a, b, other.b);
  }

  std::size_t hash() const {
    return hashAll(a, b);
  }
};

struct Named {
  std::string name;
  List<int64_t> tags;

  Named(std::string name, List<int64_t> tags) : name(name), tags(tags) {}

  bool operator==(const Named& other) const {
    return allSame(name, other.name, tags, other.tags);
  }

  std::size_t hash() const {
    return hashAll(name, tags);
  }
};

struct Counter {
  int64_t n;
  int64_t id;

  Counter(int64_t n, int64_t id) : n(n), id(id) {}

  bool operator==(const Counter& other) const {
    return allSame(n, other.n, id, other.id);
  }

  std::size_t hash() const {
    return hashAll(n, id);
  }
};

struct Digit {
  int64_t v;

  Digit(int64_t v) : v(v) {}

  bool equals(Ref<Digit> other_);

  bool operator==(const Digit& other) const {
    return allSame(v, other.v);
  }

  std::size_t hash() const {
    return hashAll(v);
  }
};

struct Wrapped {
  Either<int64_t, std::string> inner;

  Wrapped(Either<int64_t, std::string> inner) : inner(inner) {}

  bool operator==(const Wrapped& other) const {
    return allSame(inner, other.inner);
  }

  std::size_t hash() const {
    return hashAll(inner);
  }
};

double checkStructsAreEqualFieldByField();
double checkNestedStructsAreEqualRecursively();
double checkAStringAndAListInsideAStructAreComparedByContent();
double checkStringsCompareByCharacters();
double checkAStructWithVarFieldsIsComparedByWhatItHoldsNow();
double checkIdentityIsTheSameObject();
double checkAnImmutableStructComparedByIdentityIsAnObject();
double checkAValStructIsADifferentObjectThatIsEqual();
double checkAnEqualsMethodDecidesWhatEqualMeans();
double checkListsAreEqualElementByElement();
double checkListsOfStructsAreEqualDeeply();
double checkEithersAreEqualWhenTheSameCaseWithAnEqualPayload();
double checkVariantsAreEqualWhenTheSameCaseWithAnEqualPayload();
double checkAStructHoldingAnEitherIsComparedThroughIt();
double checkNanIsNotEqualToItself();

inline auto Digit::equals(Ref<Digit> other_) -> bool {
  return this->v % 10 == other_->v % 10;
}

inline double checkStructsAreEqualFieldByField() {
  const Ref<Point> p = std::make_shared<Point>(1, 2);
  const Ref<Point> q = std::make_shared<Point>(1, 2);
  const Ref<Point> r = std::make_shared<Point>(1, 3);
  int64_t score = 0;
  if (same(p, q)) {
    score = score + 1;
  }
  if (!same(p, r)) {
    score = score + 2;
  }
  if (same(p, r)) {
    score = score + 100;
  }
  return offInt(score, 3);
}

inline double checkNestedStructsAreEqualRecursively() {
  const Ref<Point> p = std::make_shared<Point>(1, 2);
  const Ref<Point> z = std::make_shared<Point>(3, 4);
  const Ref<Line> a = std::make_shared<Line>(p, z);
  const Ref<Line> b = std::make_shared<Line>(std::make_shared<Point>(1, 2), std::make_shared<Point>(3, 4));
  const Ref<Line> c = std::make_shared<Line>(z, p);
  int64_t score = 0;
  if (same(a, b)) {
    score = score + 1;
  }
  if (!same(a, c)) {
    score = score + 2;
  }
  return offInt(score, 3);
}

inline double checkAStringAndAListInsideAStructAreComparedByContent() {
  const List<int64_t> t1 = newList<int64_t>();
  t1->push_back(1);
  const List<int64_t> t2 = newList<int64_t>();
  t2->push_back(1);
  const Ref<Named> a = std::make_shared<Named>("a"s + "b"s, t1);
  const Ref<Named> b = std::make_shared<Named>("ab"s, t2);
  int64_t score = 0;
  if (same(a, b)) {
    score = score + 1;
  }
  t2->push_back(2);
  if (!same(a, b)) {
    score = score + 2;
  }
  return offInt(score, 3);
}

inline double checkStringsCompareByCharacters() {
  const std::string x = "match"s;
  const std::string y = "ma"s + "tch"s;
  int64_t score = 0;
  if (x == y) {
    score = score + 1;
  }
  if (x != "other"s) {
    score = score + 2;
  }
  if (x == y) {
    score = score + 100;
  }
  return offInt(score, 3);
}

inline double checkAStructWithVarFieldsIsComparedByWhatItHoldsNow() {
  const Ref<Counter> a = std::make_shared<Counter>(1, 2);
  const Ref<Counter> b = std::make_shared<Counter>(1, 2);
  int64_t score = 0;
  if (same(a, b)) {
    score = score + 1;
  }
  b->n = 5;
  if (!same(a, b)) {
    score = score + 2;
  }
  return offInt(score, 3);
}

inline double checkIdentityIsTheSameObject() {
  const Ref<Counter> a = std::make_shared<Counter>(1, 2);
  const Ref<Counter> b = std::make_shared<Counter>(1, 2);
  const Ref<Counter> alias = a;
  int64_t score = 0;
  if (a == alias) {
    score = score + 1;
  }
  if (a != b) {
    score = score + 2;
  }
  if (a == b) {
    score = score + 100;
  }
  return offInt(score, 3);
}

inline double checkAnImmutableStructComparedByIdentityIsAnObject() {
  const Ref<Point> p = std::make_shared<Point>(1, 2);
  const Ref<Point> q = std::make_shared<Point>(1, 2);
  const Ref<Point> same_ = p;
  int64_t score = 0;
  if (p == same_) {
    score = score + 1;
  }
  if (p != q) {
    score = score + 2;
  }
  if (same(p, q)) {
    score = score + 4;
  }
  return offInt(score, 7);
}

inline double checkAValStructIsADifferentObjectThatIsEqual() {
  const Ref<Counter> a = std::make_shared<Counter>(1, 2);
  const Ref<Counter> b = copyOf(a);
  int64_t score = 0;
  if (same(a, b)) {
    score = score + 1;
  }
  if (a != b) {
    score = score + 2;
  }
  return offInt(score, 3);
}

inline double checkAnEqualsMethodDecidesWhatEqualMeans() {
  const Ref<Digit> a = std::make_shared<Digit>(13);
  const Ref<Digit> b = std::make_shared<Digit>(23);
  const Ref<Digit> c = std::make_shared<Digit>(24);
  int64_t score = 0;
  if (same(a, b)) {
    score = score + 1;
  }
  if (!same(a, c)) {
    score = score + 2;
  }
  return offInt(score, 3);
}

inline double checkListsAreEqualElementByElement() {
  const List<int64_t> a = newList<int64_t>();
  const List<int64_t> b = newList<int64_t>();
  a->push_back(1);
  a->push_back(2);
  b->push_back(1);
  b->push_back(2);
  int64_t score = 0;
  if (same(a, b)) {
    score = score + 1;
  }
  b->push_back(3);
  if (!same(a, b)) {
    score = score + 2;
  }
  if (a == b) {
    score = score + 100;
  }
  return offInt(score, 3);
}

inline double checkListsOfStructsAreEqualDeeply() {
  const List<Ref<Point>> a = newList<Ref<Point>>();
  const List<Ref<Point>> b = newList<Ref<Point>>();
  a->push_back(std::make_shared<Point>(1, 2));
  b->push_back(std::make_shared<Point>(1, 2));
  int64_t score = 0;
  if (same(a, b)) {
    score = score + 1;
  }
  a->push_back(std::make_shared<Point>(5, 6));
  b->push_back(std::make_shared<Point>(5, 7));
  if (!same(a, b)) {
    score = score + 2;
  }
  return offInt(score, 3);
}

inline double checkEithersAreEqualWhenTheSameCaseWithAnEqualPayload() {
  const Either<int64_t, int64_t> a = Either<int64_t, int64_t>::ok(5);
  const Either<int64_t, int64_t> b = Either<int64_t, int64_t>::ok(5);
  const Either<int64_t, int64_t> c = Either<int64_t, int64_t>::err(5);
  const Either<int64_t, int64_t> d = Either<int64_t, int64_t>::ok(6);
  int64_t score = 0;
  if (same(a, b)) {
    score = score + 1;
  }
  if (!same(a, c)) {
    score = score + 2;
  }
  if (!same(a, d)) {
    score = score + 4;
  }
  return offInt(score, 7);
}

inline double checkVariantsAreEqualWhenTheSameCaseWithAnEqualPayload() {
  const Variant a = makeVariant(1, 2.5);
  const Variant b = makeVariant(1, 2.5);
  const Variant c = makeVariant(0, int64_t(2));
  int64_t score = 0;
  if (same(a, b)) {
    score = score + 1;
  }
  if (!same(a, c)) {
    score = score + 2;
  }
  return offInt(score, 3);
}

inline double checkAStructHoldingAnEitherIsComparedThroughIt() {
  const Ref<Wrapped> a = std::make_shared<Wrapped>(Either<int64_t, std::string>::ok(1));
  const Ref<Wrapped> b = std::make_shared<Wrapped>(Either<int64_t, std::string>::ok(1));
  const Ref<Wrapped> c = std::make_shared<Wrapped>(Either<int64_t, std::string>::err("no"s));
  int64_t score = 0;
  if (same(a, b)) {
    score = score + 1;
  }
  if (!same(a, c)) {
    score = score + 2;
  }
  return offInt(score, 3);
}

inline double checkNanIsNotEqualToItself() {
  const double nan = 0.0 / 0.0;
  int64_t score = 0;
  if (nan == nan) {
    score = score + 100;
  }
  if (nan != nan) {
    score = score + 1;
  }
  return offInt(score, 1);
}
