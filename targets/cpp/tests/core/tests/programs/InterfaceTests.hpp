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

#include "../../examples/interfaces.cpp"
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

struct Scaler;
struct Halver;
struct Weighted;

template <typename T>
struct Cell;

struct Scaler {
  virtual ~Scaler() = default;

  virtual float half(float p0) = 0;
  virtual int64_t offset(int32_t p0) = 0;
};

struct Halver : public Scaler {
  int64_t base;

  Halver(int64_t base) : base(base) {}

  float half(float x) override;
  int64_t offset(int32_t by) override;

  bool operator==(const Halver& other) const {
    return allSame(base, other.base);
  }

  std::size_t hash() const {
    return hashAll(base);
  }
};

struct Weighted {
  virtual ~Weighted() = default;

  virtual int64_t weight() = 0;
};

template <typename T>
struct Cell : public Weighted {
  T value;

  Cell(T value) : value(value) {}

  int64_t weight() {
    return 7;
  }

  T get() {
    return this->value;
  }

  bool operator==(const Cell& other) const {
    return allSame(value, other.value);
  }

  std::size_t hash() const {
    return hashAll(value);
  }
};

double checkConversionAtACall();
double checkConversionAtAReturn();
double checkDarrayOfInterfaceValues();
double checkInterfaceAsAStructField();
double checkOneStructTwoInterfaces();
double checkInterfaceMethodTakingAnInterface();
double checkDispatchFollowsTheValue();
double checkSameMethodNameOnTwoStructs();
double checkConvertedValueIsACopy();
double checkArgumentWidthsFollowTheInterface();
double checkInterfaceInAnEither();
double checkInterfaceCapturedByAClosure();
double checkInterfaceThroughAVar();
int64_t totalWeight(Ref<Weighted> a, Ref<Weighted> b);
double checkGenericStructConvertsToAnInterface();
double checkGenericInterfaceValuesInADarray();

inline double checkConversionAtACall() {
  return offInt(directCall(), 920);
}

inline double checkConversionAtAReturn() {
  return offInt(chosen(), 36034);
}

inline double checkDarrayOfInterfaceValues() {
  return offInt(sumAll(), 41);
}

inline double checkInterfaceAsAStructField() {
  return offInt(viaField(), 307);
}

inline double checkOneStructTwoInterfaces() {
  return offInt(twoInterfaces(), 1016);
}

inline double checkInterfaceMethodTakingAnInterface() {
  return offInt(interfaceArgument(), 109);
}

inline double checkDispatchFollowsTheValue() {
  const Ref<Shape> a = std::make_shared<Square>(5);
  const Ref<Shape> b = std::make_shared<Rect>(5, 2);
  return offInt(a->area(), 25) + offInt(b->area(), 10);
}

inline double checkSameMethodNameOnTwoStructs() {
  const Ref<Square> s = std::make_shared<Square>(3);
  const Ref<Rect> r = std::make_shared<Rect>(3, 4);
  return offInt(s->area() + r->area(), 21);
}

inline double checkConvertedValueIsACopy() {
  const Ref<Square> s = std::make_shared<Square>(4);
  const Ref<Shape> a = s;
  const Ref<Shape> b = s;
  return offInt(a->area() + b->area(), 32);
}

inline auto Halver::half(float x) -> float {
  return x / 2.0f;
}

inline auto Halver::offset(int32_t by) -> int64_t {
  return this->base + (int64_t) by;
}

inline double checkArgumentWidthsFollowTheInterface() {
  const Ref<Scaler> s = std::make_shared<Halver>(40);
  return off(s->half(9.0f), 4.5f) + offInt(s->offset(2), 42);
}

inline double checkInterfaceInAnEither() {
  const Either<Ref<Shape>, int64_t> r = Either<Ref<Shape>, int64_t>::ok(std::make_shared<Rect>(4, 4));
  if (!r.isOk) {
    return 1.0;
  }
  const Ref<Shape> s = r.okValue();
  return offInt(s->area(), 16);
}

inline double checkInterfaceCapturedByAClosure() {
  const Ref<Shape> s = std::make_shared<Square>(7);
  const std::function<int64_t()> f = [=]() { return s->area(); };
  return offInt(f(), 49);
}

inline double checkInterfaceThroughAVar() {
  Ref<Shape> s = std::make_shared<Square>(2);
  const int64_t first = s->area();
  s = std::make_shared<Rect>(3, 5);
  return offInt(first * 100 + s->area(), 415);
}

inline int64_t totalWeight(Ref<Weighted> a, Ref<Weighted> b) {
  return a->weight() + b->weight();
}

inline double checkGenericStructConvertsToAnInterface() {
  const Ref<Cell<int64_t>> a = std::make_shared<Cell<int64_t>>(5);
  const Ref<Cell<double>> b = std::make_shared<Cell<double>>(2.5);
  return offInt(totalWeight(a, b), 14) + offInt(a->get(), 5) + off((float) (b->get()), 2.5f);
}

inline double checkGenericInterfaceValuesInADarray() {
  const List<Ref<Weighted>> all = newList<Ref<Weighted>>();
  const Ref<Cell<int64_t>> a = std::make_shared<Cell<int64_t>>(1);
  all->push_back(a);
  all->push_back(a);
  return offInt(all->at(0)->weight() + all->at(1)->weight(), 14);
}
