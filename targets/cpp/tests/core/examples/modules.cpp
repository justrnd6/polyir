#include <algorithm>
#include <cstdint>
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

#ifndef POLYIR_TEXT_HPP
#define POLYIR_TEXT_HPP

// Text: `str` writes a value (a flag as `true`, a char as its byte), and a string's positions count its bytes. A byte outside
// the string is 0, a slice keeps what lies inside the string, and a search starts inside it.
inline std::string str(const std::string& text) {
  return text;
}

inline std::string str(bool flag) {
  return flag ? "true" : "false";
}

inline std::string str(int8_t c) {
  return std::string(1, (char) c);
}

inline std::string str(uint8_t c) {
  return std::string(1, (char) c);
}

template <typename T>
  requires std::is_arithmetic_v<T>
std::string str(T number) {
  return std::to_string(number);
}

inline void println(const std::string& text) {
  std::cout << text << '\n';
}

inline int8_t textByte(const std::string& text, int64_t at) {
  return at >= 0 && at < (int64_t) text.size() ? (int8_t) text[at] : 0;
}

inline std::string textSlice(const std::string& text, int64_t from, int64_t to) {
  const int64_t size = text.size();
  const int64_t start = std::clamp<int64_t>(from, 0, size);
  return text.substr(start, std::clamp<int64_t>(to, start, size) - start);
}

inline int64_t textFind(const std::string& text, const std::string& part, int64_t from) {
  const std::size_t at = text.find(part, std::clamp<int64_t>(from, 0, text.size()));
  return at == std::string::npos ? -1 : (int64_t) at;
}
#endif

namespace Geometry {
  struct Point;

  namespace Shapes {
    struct Shape;
    struct Square;
    struct Rect;
  }
}

namespace Geometry {
  struct Point {
    int64_t x;
    int64_t y;

    Point(int64_t x, int64_t y) : x(x), y(y) {}

    int64_t manhattan();
    int64_t absolute(int64_t n);

    bool operator==(const Point& other) const {
      return allSame(x, other.x, y, other.y);
    }

    std::size_t hash() const {
      return hashAll(x, y);
    }
  };

  namespace Shapes {
    struct Shape {
      virtual ~Shape() = default;

      virtual int64_t area() = 0;
    };

    struct Square : public Shape {
      int64_t side;

      Square(int64_t side) : side(side) {}

      int64_t area() override;

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

      bool operator==(const Rect& other) const {
        return allSame(w, other.w, h, other.h);
      }

      std::size_t hash() const {
        return hashAll(w, h);
      }
    };
  }
}

namespace Geometry {
  int64_t clamp(int64_t n, int64_t low, int64_t high);
  int64_t distance(Ref<Point> a, Ref<Point> b);

  namespace Shapes {
    int64_t total(List<Ref<Shape>> shapes);
  }
}

template <typename T>
T larger(T a, T b, bool aFirst);

template <typename T>
T firstOf(List<T> xs);

namespace Geometry {
  inline auto Geometry::Point::manhattan() -> int64_t {
    return this->absolute(this->x) + this->absolute(this->y);
  }

  inline auto Geometry::Point::absolute(int64_t n) -> int64_t {
    return n < 0 ? 0 - n : n;
  }

  inline int64_t clamp(int64_t n, int64_t low, int64_t high) {
    return n < low ? low : n > high ? high : n;
  }

  inline int64_t distance(Ref<Point> a, Ref<Point> b) {
    const Ref<Point> step = std::make_shared<Point>(a->x - b->x, a->y - b->y);
    return clamp(step->manhattan(), 0, 100);
  }

  namespace Shapes {
    inline auto Geometry::Shapes::Square::area() -> int64_t {
      return this->side * this->side;
    }

    inline auto Geometry::Shapes::Rect::area() -> int64_t {
      return this->w * this->h;
    }

    inline int64_t total(List<Ref<Shape>> shapes) {
      int64_t sum = 0;
      for (auto s : *shapes) {
        sum = sum + s->area();
      }
      return sum;
    }
  }
}

int main() {
  const Ref<Geometry::Point> a = std::make_shared<Geometry::Point>(1, 2);
  const Ref<Geometry::Point> b = std::make_shared<Geometry::Point>(4, -2);
  const List<Ref<Geometry::Shapes::Shape>> shapes = listOf<Ref<Geometry::Shapes::Shape>>({std::make_shared<Geometry::Shapes::Square>(3), std::make_shared<Geometry::Shapes::Rect>(2, 5)});
  const std::string size = Geometry::Shapes::total(shapes) > 10 ? "big"s : "small"s;
  println(str(Geometry::distance(a, b)) + " "s + str(size) + " "s + str(larger<int64_t>(3, 7, false)) + " "s + str(larger<std::string>("x"s, "y"s, true)) + " "s + str(firstOf<std::string>(listOf<std::string>({"first"s, "second"s}))));
}

template <typename T>
inline T larger(T a, T b, bool aFirst) {
  return aFirst ? a : b;
}

template <typename T>
inline T firstOf(List<T> xs) {
  return xs->at(0);
}
