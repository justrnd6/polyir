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

struct Shape;

struct Shape {
  struct Size {
    int64_t area;

    Size(int64_t area) : area(area) {}

    bool operator==(const Size& other) const {
      return allSame(area, other.area);
    }

    std::size_t hash() const {
      return hashAll(area);
    }
  };

  struct Builder : std::enable_shared_from_this<Builder> {
    std::optional<int64_t> __side;
    std::string __name;
    List<std::string> __tags;

    Builder(std::optional<int64_t> __side, std::string __name, List<std::string> __tags) : __side(__side), __name(__name), __tags(__tags) {}

    Ref<Builder> side(int64_t value);
    Ref<Builder> name(std::string value);
    Ref<Builder> tags(List<std::string> value);
    Ref<Shape> build();

    bool operator==(const Builder& other) const {
      return allSame(__side, other.__side, __name, other.__name, __tags, other.__tags);
    }

    std::size_t hash() const {
      return hashAll(__side, __name, __tags);
    }
  };

  int64_t side;
  std::string name;
  List<std::string> tags;

  Shape(int64_t side, std::string name, List<std::string> tags) : side(side), name(name), tags(tags) {}

  Ref<Size> size();

  static Ref<Shape> square(int64_t side);
  static Ref<Builder> builder();

  bool operator==(const Shape& other) const {
    return allSame(side, other.side, name, other.name, tags, other.tags);
  }

  std::size_t hash() const {
    return hashAll(side, name, tags);
  }
};

Ref<Shape::Builder> tagged(Ref<Shape::Builder> b);

inline auto Shape::size() -> Ref<Size> {
  return std::make_shared<Size>(this->side * this->side);
}

inline auto Shape::square(int64_t side) -> Ref<Shape> {
  return builder()->side(side)->name("square"s)->build();
}

inline auto Shape::builder() -> Ref<Builder> {
  return std::make_shared<Builder>(std::optional<int64_t>(), "shape"s, newList<std::string>());
}

inline auto Shape::Builder::side(int64_t value) -> Ref<Builder> {
  this->__side = std::optional<int64_t>(value);
  return shared_from_this();
}

inline auto Shape::Builder::name(std::string value) -> Ref<Builder> {
  this->__name = value;
  return shared_from_this();
}

inline auto Shape::Builder::tags(List<std::string> value) -> Ref<Builder> {
  this->__tags = value;
  return shared_from_this();
}

inline auto Shape::Builder::build() -> Ref<Shape> {
  if (!(this->__side).has_value()) {
    fail("Shape.build: side is not set"s);
  }
  return std::make_shared<Shape>(present(this->__side), this->__name, this->__tags);
}

inline Ref<Shape::Builder> tagged(Ref<Shape::Builder> b) {
  const List<std::string> tags = listOf<std::string>({"red"s, "big"s});
  return b->tags(tags);
}

int main() {
  const Ref<Shape> first = tagged(Shape::builder())->side(2)->build();
  const Ref<Shape> second = Shape::square(3);
  const Ref<Shape::Size> size = second->size();
  println(str(first->name) + " "s + str(first->side) + " "s + str(first->size()->area) + " ["s + str(first->tags->at(0)) + ", "s + str(first->tags->at(1)) + "] "s + str(second->name) + " "s + str(second->side) + " "s + str(size->area));
}
