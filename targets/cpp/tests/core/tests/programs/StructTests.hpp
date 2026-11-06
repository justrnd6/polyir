#pragma once

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

struct Counter;
struct Pt;
struct Holder;
struct Reading;
struct Gauge;
struct Dot;
struct Span;
struct Meter;
struct Plate;

template <typename T>
struct Cell;

template <typename T>
struct Wrapper;

struct Counter {
  int64_t n;
  int64_t id;

  Counter(int64_t n, int64_t id) : n(n), id(id) {}

  int64_t bump();

  bool operator==(const Counter& other) const {
    return allSame(n, other.n, id, other.id);
  }

  std::size_t hash() const {
    return hashAll(n, id);
  }
};

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

struct Holder {
  List<int64_t> items;
  Ref<Counter> inner;

  Holder(List<int64_t> items, Ref<Counter> inner) : items(items), inner(inner) {}

  bool operator==(const Holder& other) const {
    return allSame(items, other.items, inner, other.inner);
  }

  std::size_t hash() const {
    return hashAll(items, inner);
  }
};

struct Reading {
  virtual ~Reading() = default;

  virtual int64_t read() = 0;
};

struct Gauge : public Reading {
  int64_t level;

  Gauge(int64_t level) : level(level) {}

  int64_t read() override;

  bool operator==(const Gauge& other) const {
    return allSame(level, other.level);
  }

  std::size_t hash() const {
    return hashAll(level);
  }
};

struct Dot {
  int64_t x;
  int64_t y;

  Dot(int64_t x, int64_t y) : x(x), y(y) {}

  bool operator==(const Dot& other) const {
    return allSame(x, other.x, y, other.y);
  }

  std::size_t hash() const {
    return hashAll(x, y);
  }
};

struct Span {
  int64_t lo;
  int64_t hi;

  Span(int64_t lo, int64_t hi) : lo(lo), hi(hi) {}

  int64_t width();

  bool operator==(const Span& other) const {
    return allSame(lo, other.lo, hi, other.hi);
  }

  std::size_t hash() const {
    return hashAll(lo, hi);
  }
};

struct Meter {
  int64_t reading;
  int64_t id;

  Meter(int64_t reading, int64_t id) : reading(reading), id(id) {}

  int64_t bump();

  bool operator==(const Meter& other) const {
    return allSame(reading, other.reading, id, other.id);
  }

  std::size_t hash() const {
    return hashAll(reading, id);
  }
};

struct Plate : public Reading {
  int64_t code;

  Plate(int64_t code) : code(code) {}

  int64_t read() override;

  bool operator==(const Plate& other) const {
    return allSame(code, other.code);
  }

  std::size_t hash() const {
    return hashAll(code);
  }
};

template <typename T>
struct Cell {
  T v;

  Cell(T v) : v(v) {}

  bool operator==(const Cell& other) const {
    return allSame(v, other.v);
  }

  std::size_t hash() const {
    return hashAll(v);
  }
};

template <typename T>
struct Wrapper {
  T value;

  Wrapper(T value) : value(value) {}

  bool operator==(const Wrapper& other) const {
    return allSame(value, other.value);
  }

  std::size_t hash() const {
    return hashAll(value);
  }
};

int64_t addTen(Ref<Counter> c);
int64_t addTenOnACopy(Ref<Counter> c);
double checkTwoNamesForOneObjectShareIt();
double checkACalleeChangesTheCallersObject();
double checkAValParameterIsTheCalleesOwnCopy();
double checkValInFrontOfAnExpressionCopies();
double checkCopyMethodAndValAgree();
double checkAMethodChangesThis();
double checkAnImmutableStructIsAValue();
double checkAFieldHoldingAnotherReferenceStruct();
double checkAFieldCanBeReplacedWithAnotherObject();
double checkAGenericStructWithAVarField();
double checkAnInterfaceValueSeesLaterChanges();
double checkAStructInADarrayIsTheSameObject();
double checkAStructInAClosureIsShared();
Ref<Counter> makeCounter(int64_t start);
int64_t lend(Ref<Counter> c);
int64_t keepIn(List<Ref<Counter>> store, Ref<Counter> c);
double checkAReturnedObjectSurvives();
double checkAnObjectLentToACalleeIsUsableAfterwards();
double checkAnObjectAnotherCalleeKeepsSurvives();
double checkAValParameterCopyIsReleasedAndTheOriginalIsNot();
int64_t earlyReturnWithObjects(int64_t n);
double checkAReturnInsideAnIfReleasesTheObjectAndKeepsTheValue();
double checkLetVarAndValOnReferenceStructs();
double checkValOnAContainerIsAnIndependentCopy();
double checkShorthandStructsWork();
double checkAShorthandStructWithAVarFieldIsAReference();
double checkAHashmapOfObjectsIsUsableThenReleased();
double checkASparseOfStringsIsUsableThenReleased();

inline auto Counter::bump() -> int64_t {
  this->n = this->n + 1;
  return this->n;
}

inline auto Gauge::read() -> int64_t {
  return this->level;
}

inline int64_t addTen(Ref<Counter> c) {
  c->n = c->n + 10;
  return 0;
}

inline int64_t addTenOnACopy(Ref<Counter> c) {
  const Ref<Counter> c2 = copyOf(c);
  c2->n = c2->n + 10;
  return c2->n;
}

inline double checkTwoNamesForOneObjectShareIt() {
  const Ref<Counter> a = std::make_shared<Counter>(1, 7);
  const Ref<Counter> b = a;
  b->n = 5;
  return offInt(a->n, 5) + offInt(a->id, 7);
}

inline double checkACalleeChangesTheCallersObject() {
  const Ref<Counter> a = std::make_shared<Counter>(1, 0);
  addTen(a);
  return offInt(a->n, 11);
}

inline double checkAValParameterIsTheCalleesOwnCopy() {
  const Ref<Counter> a = std::make_shared<Counter>(1, 0);
  const int64_t seen = addTenOnACopy(a);
  return offInt(seen, 11) + offInt(a->n, 1);
}

inline double checkValInFrontOfAnExpressionCopies() {
  const Ref<Counter> a = std::make_shared<Counter>(1, 3);
  const Ref<Counter> b = copyOf(a);
  b->n = 50;
  return offInt(a->n, 1) + offInt(b->n, 50) + offInt(b->id, 3);
}

inline double checkCopyMethodAndValAgree() {
  const Ref<Counter> a = std::make_shared<Counter>(4, 0);
  const Ref<Counter> b = copyOf(a);
  a->n = 9;
  return offInt(b->n, 4);
}

inline double checkAMethodChangesThis() {
  const Ref<Counter> a = std::make_shared<Counter>(0, 0);
  a->bump();
  a->bump();
  return offInt(a->n, 2);
}

inline double checkAnImmutableStructIsAValue() {
  const Ref<Pt> p = std::make_shared<Pt>(1, 2);
  const Ref<Pt> q = copyOf(p);
  const Ref<Pt> r = copyOf(p);
  return offInt(q->x + r->y, 3);
}

inline double checkAFieldHoldingAnotherReferenceStruct() {
  const Ref<Holder> h = std::make_shared<Holder>(newList<int64_t>(), std::make_shared<Counter>(1, 0));
  const Ref<Holder> alias = h;
  alias->inner->n = 8;
  alias->items->push_back(5);
  return offInt(h->inner->n, 8) + offInt((int64_t) (h->items)->size(), 1);
}

inline double checkAFieldCanBeReplacedWithAnotherObject() {
  const Ref<Holder> h = std::make_shared<Holder>(newList<int64_t>(), std::make_shared<Counter>(1, 0));
  const Ref<Counter> keep = h->inner;
  h->inner = std::make_shared<Counter>(2, 0);
  return offInt(keep->n, 1) + offInt(h->inner->n, 2);
}

inline double checkAGenericStructWithAVarField() {
  const Ref<Cell<int64_t>> c = std::make_shared<Cell<int64_t>>(1);
  const Ref<Cell<int64_t>> d = c;
  d->v = 9;
  return offInt(c->v, 9);
}

inline double checkAnInterfaceValueSeesLaterChanges() {
  const Ref<Gauge> g = std::make_shared<Gauge>(1);
  const Ref<Reading> r = g;
  g->level = 42;
  return offInt(r->read(), 42);
}

inline double checkAStructInADarrayIsTheSameObject() {
  const List<Ref<Counter>> all = newList<Ref<Counter>>();
  const Ref<Counter> a = std::make_shared<Counter>(1, 0);
  all->push_back(a);
  a->n = 6;
  return offInt(all->at(0)->n, 6);
}

inline double checkAStructInAClosureIsShared() {
  const Ref<Counter> a = std::make_shared<Counter>(1, 0);
  const std::function<int64_t()> f = [=]() -> int64_t {
    a->n = a->n + 1;
    return a->n;
  };
  f();
  return offInt(a->n, 2);
}

inline Ref<Counter> makeCounter(int64_t start) {
  const Ref<Counter> c = std::make_shared<Counter>(start, 1);
  c->n = c->n + 1;
  return c;
}

inline int64_t lend(Ref<Counter> c) {
  return c->n * 2;
}

inline int64_t keepIn(List<Ref<Counter>> store, Ref<Counter> c) {
  store->push_back(c);
  return 0;
}

inline double checkAReturnedObjectSurvives() {
  const Ref<Counter> a = makeCounter(5);
  const Ref<Counter> b = makeCounter(10);
  return offInt(a->n * 100 + b->n, 611);
}

inline double checkAnObjectLentToACalleeIsUsableAfterwards() {
  int64_t total = 0;
  for (int64_t i = 0; i < 100; i = i + 1) {
    const Ref<Counter> c = std::make_shared<Counter>(i, 0);
    total = total + lend(c) + c->n;
    c->n = 0;
  }
  return offInt(total, 3 * 4950);
}

inline double checkAnObjectAnotherCalleeKeepsSurvives() {
  const List<Ref<Counter>> store = newList<Ref<Counter>>();
  for (int64_t i = 0; i < 4; i = i + 1) {
    const Ref<Counter> c = std::make_shared<Counter>(i * 10, 0);
    keepIn(store, c);
  }
  return offInt(store->at(0)->n + store->at(1)->n + store->at(2)->n + store->at(3)->n, 60);
}

inline double checkAValParameterCopyIsReleasedAndTheOriginalIsNot() {
  const Ref<Counter> a = std::make_shared<Counter>(1, 0);
  int64_t total = 0;
  for (int64_t i = 0; i < 50; i = i + 1) {
    total = total + addTenOnACopy(a);
  }
  return offInt(total, 50 * 11) + offInt(a->n, 1);
}

inline int64_t earlyReturnWithObjects(int64_t n) {
  const Ref<Counter> c = std::make_shared<Counter>(n, 0);
  if (n > 5) {
    return c->n * 2;
  }
  c->bump();
  return c->n;
}

inline double checkAReturnInsideAnIfReleasesTheObjectAndKeepsTheValue() {
  return offInt(earlyReturnWithObjects(9) * 100 + earlyReturnWithObjects(2), 1800 + 3);
}

inline double checkLetVarAndValOnReferenceStructs() {
  const Ref<Counter> a = std::make_shared<Counter>(1, 0);
  const Ref<Counter> alias = a;
  const Ref<Counter> copy = copyOf(a);
  Ref<Counter> moving = a;
  a->n = 10;
  moving = std::make_shared<Counter>(100, 0);
  moving->n = moving->n + 1;
  return offInt(alias->n, 10) + offInt(copy->n, 1) + offInt(moving->n, 101) + offInt(a->n, 10);
}

inline double checkValOnAContainerIsAnIndependentCopy() {
  const List<int64_t> d = newList<int64_t>();
  d->push_back(4);
  const List<int64_t> e = copyOf(d);
  d->push_back(5);
  return offInt((int64_t) e->size(), 1) + offInt((int64_t) d->size(), 2);
}

inline auto Span::width() -> int64_t {
  return this->hi - this->lo;
}

inline auto Meter::bump() -> int64_t {
  this->reading = this->reading + 1;
  return this->reading;
}

inline auto Plate::read() -> int64_t {
  return this->code;
}

inline double checkShorthandStructsWork() {
  const Ref<Dot> d = std::make_shared<Dot>(3, 4);
  const Ref<Span> s = std::make_shared<Span>(2, 9);
  const Ref<Wrapper<int64_t>> w = std::make_shared<Wrapper<int64_t>>(5);
  const Ref<Reading> r = std::make_shared<Plate>(7);
  return offInt(d->x + d->y, 7) + offInt(s->width(), 7) + offInt(w->value, 5) + offInt(r->read(), 7);
}

inline double checkAShorthandStructWithAVarFieldIsAReference() {
  const Ref<Meter> m = std::make_shared<Meter>(1, 2);
  const Ref<Meter> alias = m;
  alias->bump();
  return offInt(m->reading, 2) + offInt(m->id, 2);
}

inline double checkAHashmapOfObjectsIsUsableThenReleased() {
  int64_t total = 0;
  for (int64_t i = 0; i < 100; i = i + 1) {
    const Map<int64_t, Ref<Counter>> m = newMap<int64_t, Ref<Counter>>();
    m->insert_or_assign(1, std::make_shared<Counter>(i, 1));
    m->insert_or_assign(2, std::make_shared<Counter>(5, 2));
    m->insert_or_assign(2, std::make_shared<Counter>(6, 2));
    getOr(m, 1, nullptr)->bump();
    total = total + getOr(m, 1, nullptr)->n - i + getOr(m, 2, nullptr)->n + (int64_t) m->size();
  }
  return offInt(total, 100 * (1 + 6 + 2));
}

inline double checkASparseOfStringsIsUsableThenReleased() {
  int64_t total = 0;
  for (int64_t i = 0; i < 100; i = i + 1) {
    const Ref<Sparse<std::string>> s = std::make_shared<Sparse<std::string>>(""s);
    s->set("a"s + "b"s, 0, 0);
    s->set("c"s + "d"s, 2, 3);
    if (s->get(2, 3) == "cd"s) {
      total = total + (int64_t) s->size();
    }
  }
  return offInt(total, 200);
}
