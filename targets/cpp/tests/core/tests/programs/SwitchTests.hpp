#pragma once

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

bool endsWith(std::string s, std::string suffix);
double offText(std::string got, std::string want);
std::string dayName(int64_t day);
double checkNumbers();
std::string classOf(int8_t c);
double checkChars();
std::string verb(std::string command);
double checkStrings();
double checkBooleans();
std::string outcome(Either<int64_t, std::string> r);
double checkSums();
std::string status(int64_t code);
double checkConditionsAmongKeys();
int64_t counter();
double checkCallSubject();
std::string step(std::string name);
double checkConditions();

inline bool endsWith(std::string s, std::string suffix) {
  if ((int64_t) suffix.size() > (int64_t) s.size()) {
    return false;
  }
  return textSlice(s, (int64_t) s.size() - (int64_t) suffix.size(), (int64_t) s.size()) == suffix;
}

inline double offText(std::string got, std::string want) {
  if (got == want) {
    return 0.0;
  }
  return 1.0;
}

inline std::string dayName(int64_t day) {
  std::string name = "?"s;
  switch (day) {
    case 0: case 6: {
      name = "weekend"s;
      break;
    }
    case 1: {
      name = "monday"s;
      break;
    }
    default: {
      name = "weekday"s;
      break;
    }
  }
  return name;
}

inline double checkNumbers() {
  return offText(dayName(0) + dayName(6) + dayName(1) + dayName(3), "weekendweekendmondayweekday"s);
}

inline std::string classOf(int8_t c) {
  std::string kind = "other"s;
  switch (c) {
    case 97: case 101: case 105: case 111: case 117: {
      kind = "vowel"s;
      break;
    }
    case 32: {
      kind = "space"s;
      break;
    }
  }
  return kind;
}

inline double checkChars() {
  return offText(classOf(101) + classOf(32) + classOf(122), "vowelspaceother"s);
}

inline std::string verb(std::string command) {
  std::string done = ""s;
  if (command == "start"s || command == "go"s) {
    done = "began"s;
  } else if (command == "stop"s) {
    done = "halted"s;
  } else {
    done = "?"s;
  }
  return done;
}

inline double checkStrings() {
  return offText(verb("go"s) + verb("start"s) + verb("stop"s) + verb("x"s), "beganbeganhalted?"s);
}

inline double checkBooleans() {
  std::string said = ""s;
  for (int64_t i = 0; i < 2; i = i + 1) {
    const bool match_57_9 = i == 0;
    switch (match_57_9) {
      case true: {
        said = said + "yes"s;
        break;
      }
      case false: {
        said = said + "no"s;
        break;
      }
    }
  }
  return offText(said, "yesno"s);
}

inline std::string outcome(Either<int64_t, std::string> r) {
  std::string said = ""s;
  if (r.isOk) {
    const int64_t v = r.okValue();
    said = "ok "s + str(v);
  } else if (!r.isOk) {
    const std::string e = r.errValue();
    said = "err "s + str(e);
  }
  return said;
}

inline double checkSums() {
  int64_t count = 0;
  const std::optional<int64_t> found = std::optional<int64_t>(4);
  if (found.has_value()) {
    const int64_t n = present(found);
    count = n;
  } else if (!found.has_value()) {
    count = -1;
  }
  return offText(outcome(Either<int64_t, std::string>::ok(3)) + ", "s + outcome(Either<int64_t, std::string>::err("no"s)) + (", "s + str(count)), "ok 3, err no, 4"s);
}

inline std::string status(int64_t code) {
  std::string said = ""s;
  if (code == 200 || code == 204) {
    said = "done"s;
  } else if (code >= 500) {
    said = "retry "s + str(code);
  } else if (code >= 400) {
    const int64_t s = code;
    said = "fail "s + str(s);
  } else {
    said = "? "s + str(code);
  }
  return said;
}

inline double checkConditionsAmongKeys() {
  return offText(status(204) + "; "s + status(503) + "; "s + status(404) + "; "s + status(301), "done; retry 503; fail 404; ? 301"s);
}

inline int64_t counter() {
  return 500;
}

inline double checkCallSubject() {
  std::string said = ""s;
  const int64_t match_107_5 = counter();
  if (match_107_5 == 200) {
    said = "ok"s;
  } else if (match_107_5 >= 500) {
    const int64_t s = match_107_5;
    said = "server "s + str(s);
  } else {
    said = "other"s;
  }
  return offText(said, "server 500"s);
}

inline std::string step(std::string name) {
  std::string said = ""s;
  if (endsWith(name, ".poly"s)) {
    said = "compile "s + name + ";"s;
  } else {
    said = "skip "s + name + ";"s;
  }
  return said;
}

inline double checkConditions() {
  return offText(step("a.poly"s) + step("b.md"s), "compile a.poly;skip b.md;"s);
}
