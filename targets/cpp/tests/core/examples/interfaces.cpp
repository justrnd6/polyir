#include <algorithm>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <unordered_map>
#include <unordered_set>
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

struct Shape;
struct Named;
struct Combiner;
struct Square;
struct Rect;
struct Summer;
struct Holder;

struct Shape {
  virtual ~Shape() = default;

  virtual int64_t area() = 0;
  virtual int64_t scaled(int64_t p0) = 0;
};

struct Named {
  virtual ~Named() = default;

  virtual int64_t id() = 0;
};

struct Combiner {
  virtual ~Combiner() = default;

  virtual int64_t combine(Ref<Shape> p0) = 0;
};

struct Square : public Shape, public Named {
  int64_t side;

  Square(int64_t side) : side(side) {}

  int64_t area() override;
  int64_t scaled(int64_t k) override;
  int64_t id() override;

  bool operator==(const Square& other) const {
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
  int64_t scaled(int64_t k) override;

  bool operator==(const Rect& other) const {
    return allSame(w, other.w, h, other.h);
  }

  std::size_t hash() const {
    return hashAll(w, h);
  }
};

struct Summer : public Combiner {
  int64_t base;

  Summer(int64_t base) : base(base) {}

  int64_t combine(Ref<Shape> other_) override;

  bool operator==(const Summer& other) const {
    return allSame(base, other.base);
  }

  std::size_t hash() const {
    return hashAll(base);
  }
};

struct Holder {
  Ref<Shape> shape;
  int64_t tag;

  Holder(Ref<Shape> shape, int64_t tag) : shape(shape), tag(tag) {}

  bool operator==(const Holder& other) const {
    return allSame(shape, other.shape, tag, other.tag);
  }

  std::size_t hash() const {
    return hashAll(shape, tag);
  }
};

int64_t totalArea(Ref<Shape> s);
int64_t directCall();
Ref<Shape> pick(int64_t which);
int64_t chosen();
int64_t sumAll();
int64_t viaField();
int64_t twoInterfaces();
int64_t interfaceArgument();

inline auto Square::area() -> int64_t {
  return this->side * this->side;
}

inline auto Square::scaled(int64_t k) -> int64_t {
  return this->side * k;
}

inline auto Square::id() -> int64_t {
  return 1;
}

inline auto Rect::area() -> int64_t {
  return this->w * this->h;
}

inline auto Rect::scaled(int64_t k) -> int64_t {
  return this->w * k;
}

inline auto Summer::combine(Ref<Shape> other_) -> int64_t {
  return this->base + other_->area();
}

inline int64_t totalArea(Ref<Shape> s) {
  return s->area();
}

inline int64_t directCall() {
  return totalArea(std::make_shared<Square>(3)) * 100 + totalArea(std::make_shared<Rect>(4, 5));
}

inline Ref<Shape> pick(int64_t which) {
  if (which == 0) {
    return std::make_shared<Square>(6);
  }
  return std::make_shared<Rect>(2, 7);
}

inline int64_t chosen() {
  const Ref<Shape> a = pick(0);
  const Ref<Shape> b = pick(1);
  return a->area() * 1000 + b->area() + b->scaled(10);
}

inline int64_t sumAll() {
  const List<Ref<Shape>> shapes = newList<Ref<Shape>>();
  shapes->push_back(std::make_shared<Square>(2));
  shapes->push_back(std::make_shared<Rect>(3, 4));
  shapes->push_back(std::make_shared<Square>(5));
  int64_t total = 0;
  for (int64_t i = 0; i < (int64_t) shapes->size(); i = i + 1) {
    total = total + shapes->at(i)->area();
  }
  return total;
}

inline int64_t viaField() {
  const Ref<Holder> h = std::make_shared<Holder>(std::make_shared<Rect>(5, 6), 7);
  return h->shape->area() * 10 + h->tag;
}

inline int64_t twoInterfaces() {
  const Ref<Square> sq = std::make_shared<Square>(4);
  const Ref<Named> n = sq;
  const Ref<Shape> s = sq;
  return n->id() * 1000 + s->area();
}

inline int64_t interfaceArgument() {
  const Ref<Combiner> c = std::make_shared<Summer>(100);
  return c->combine(std::make_shared<Rect>(3, 3));
}

int main() {
  return directCall() + chosen() + sumAll() + viaField() + twoInterfaces() + interfaceArgument();
}
