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
#include <utility>
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

bool startsWith(std::string s, std::string prefix);
bool endsWith(std::string s, std::string suffix);
std::string sizeOf(int64_t n);
std::string gradeOf(int64_t score);
std::string kindOf(std::string path);
std::string shown(std::optional<int64_t> found);
Either<int64_t, std::string> safeDivide(int64_t a, int64_t b);
std::string quotient(int64_t a, int64_t b);
std::string said(List<int64_t> xs);
void report(int64_t code);

template <typename T>
List<T> drop(List<T> xs, int64_t n);

inline bool startsWith(std::string s, std::string prefix) {
  if ((int64_t) prefix.size() > (int64_t) s.size()) {
    return false;
  }
  return textSlice(s, 0, (int64_t) prefix.size()) == prefix;
}

inline bool endsWith(std::string s, std::string suffix) {
  if ((int64_t) suffix.size() > (int64_t) s.size()) {
    return false;
  }
  return textSlice(s, (int64_t) s.size() - (int64_t) suffix.size(), (int64_t) s.size()) == suffix;
}

inline std::string sizeOf(int64_t n) {
  std::string matchValue_10_10 = ""s;
  switch (n) {
    case 0: {
      matchValue_10_10 = "none"s;
      break;
    }
    case 1: case 2: case 3: {
      matchValue_10_10 = "a few"s;
      break;
    }
    default: {
      matchValue_10_10 = "many"s;
      break;
    }
  }
  return matchValue_10_10;
}

inline std::string gradeOf(int64_t score) {
  std::string matchValue_19_10 = ""s;
  if (score >= 90) {
    matchValue_19_10 = "A"s;
  } else if (score >= 80) {
    matchValue_19_10 = "B"s;
  } else {
    matchValue_19_10 = "C"s;
  }
  return matchValue_19_10;
}

inline std::string kindOf(std::string path) {
  std::string matchValue_28_10 = ""s;
  if (path == "Ninafile"s || path == "Polyfile"s) {
    matchValue_28_10 = "build"s;
  } else if (endsWith(path, ".poly"s)) {
    matchValue_28_10 = "polyir"s;
  } else if (startsWith(path, "README"s)) {
    matchValue_28_10 = "docs"s;
  } else {
    matchValue_28_10 = "other"s;
  }
  return matchValue_28_10;
}

inline std::string shown(std::optional<int64_t> found) {
  std::string matchValue_38_10 = ""s;
  if (found.has_value() && present(found) > 100) {
    const int64_t n = present(found);
    matchValue_38_10 = "big "s + str(n);
  } else if (found.has_value()) {
    const int64_t n = present(found);
    matchValue_38_10 = "found "s + str(n);
  } else {
    matchValue_38_10 = "nothing"s;
  }
  return matchValue_38_10;
}

inline Either<int64_t, std::string> safeDivide(int64_t a, int64_t b) {
  if (b == 0) {
    return Either<int64_t, std::string>::err("divide by zero"s);
  }
  return Either<int64_t, std::string>::ok(a / b);
}

inline std::string quotient(int64_t a, int64_t b) {
  const Either<int64_t, std::string> match_54_10 = safeDivide(a, b);
  std::string matchValue_54_10 = ""s;
  if (match_54_10.isOk) {
    const int64_t q = match_54_10.okValue();
    matchValue_54_10 = "quotient "s + str(q);
  } else if (!match_54_10.isOk) {
    const std::string why = match_54_10.errValue();
    matchValue_54_10 = why;
  }
  return matchValue_54_10;
}

inline std::string said(List<int64_t> xs) {
  std::string matchValue_62_10 = ""s;
  if ((int64_t) xs->size() == 0) {
    matchValue_62_10 = "empty"s;
  } else if ((int64_t) xs->size() == 1) {
    const int64_t x = xs->at(0);
    matchValue_62_10 = "just "s + str(x);
  } else {
    const int64_t first = xs->at(0);
    const List<int64_t> rest = drop<int64_t>(xs, 1);
    matchValue_62_10 = str(first) + ", then "s + str((int64_t) rest->size()) + " more"s;
  }
  return matchValue_62_10;
}

inline void report(int64_t code) {
  if (code == 200 || code == 204) {
    println("done"s);
  } else if (code >= 500) {
    println("retry "s + str(code));
  } else if (code >= 400) {
    const int64_t s = code;
    println("failed with "s + str(s));
  } else {
    println("? "s + str(code));
  }
}

int main() {
  println(sizeOf(2));
  println(gradeOf(85));
  println(kindOf("main.poly"s));
  println(shown(std::optional<int64_t>(500)));
  println(quotient(7, 2));
  println(said(listOf<int64_t>({3, 4, 5})));
  report(404);
  return 0;
}

template <typename T>
inline List<T> drop(List<T> xs, int64_t n) {
  const List<T> out = newList<T>();
  for (int64_t i = n; i < (int64_t) xs->size(); i = i + 1) {
    out->push_back(xs->at(i));
  }
  return out;
}
