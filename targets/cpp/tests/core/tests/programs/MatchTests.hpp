#pragma once

#include <algorithm>
#include <any>
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

bool contains_(std::string s, std::string part);
bool startsWith(std::string s, std::string prefix);
bool endsWith(std::string s, std::string suffix);
std::string join(List<std::string> parts, std::string sep);
double offText(std::string got, std::string want);
std::string sizeOf(int64_t n);
double checkNumbers();
std::string signOf(int64_t n);
double checkComparisons();
std::string gradeOf(int64_t score);
double checkGuardsOnTheSubject();
bool yes(std::string reply);
double checkStrings();
std::string kindOf(std::string path);
double checkConditions();
std::string word(bool ready);
double checkBooleans();
std::string shown(std::optional<int64_t> found);
double checkOptionals();
Either<int64_t, std::string> safeDivide(int64_t a, int64_t b);
std::string message(int64_t a, int64_t b);
double checkEithers();
std::string textOf(Variant shape);
double checkVariants();
std::string zeroOr(std::optional<int64_t> found);
double checkPayloadPatterns();
std::string said(List<int64_t> xs);
double checkLists();
std::string pairOf(List<int64_t> xs);
double checkListGuards();
std::string command(std::string c);
double checkStatements();
double checkNested();

template <typename T>
List<T> drop(List<T> xs, int64_t n);

inline bool contains_(std::string s, std::string part) {
  return textFind(s, part, 0) >= 0;
}

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

inline std::string join(List<std::string> parts, std::string sep) {
  std::string out = ""s;
  for (int64_t i = 0; i < (int64_t) parts->size(); i = i + 1) {
    if (i > 0) {
      out = out + sep;
    }
    out = out + parts->at(i);
  }
  return out;
}

inline double offText(std::string got, std::string want) {
  if (got == want) {
    return 0.0;
  }
  return 1.0;
}

inline std::string sizeOf(int64_t n) {
  std::string matchValue_16_12 = ""s;
  switch (n) {
    case 0: {
      matchValue_16_12 = "none"s;
      break;
    }
    case 1: case 2: case 3: {
      matchValue_16_12 = "a few"s;
      break;
    }
    case -1: {
      matchValue_16_12 = "minus one"s;
      break;
    }
    default: {
      matchValue_16_12 = "many"s;
      break;
    }
  }
  return matchValue_16_12;
}

inline double checkNumbers() {
  return offText(sizeOf(0) + sizeOf(2) + sizeOf(9) + sizeOf(-1), "nonea fewmanyminus one"s);
}

inline std::string signOf(int64_t n) {
  std::string matchValue_30_16 = ""s;
  if (n < 0) {
    matchValue_30_16 = "negative"s;
  } else if (n == 0) {
    matchValue_30_16 = "zero"s;
  } else if (n >= 100) {
    matchValue_30_16 = "large"s;
  } else {
    matchValue_30_16 = "positive"s;
  }
  const std::string sign = matchValue_30_16;
  return sign;
}

inline double checkComparisons() {
  return offText(signOf(-3) + " "s + signOf(0) + " "s + signOf(150) + " "s + signOf(5), "negative zero large positive"s);
}

inline std::string gradeOf(int64_t score) {
  std::string matchValue_45_12 = ""s;
  if (score == 100) {
    matchValue_45_12 = "perfect"s;
  } else if (score >= 90) {
    const int64_t s = score;
    matchValue_45_12 = "A "s + str(s);
  } else if (score >= 80) {
    matchValue_45_12 = "B"s;
  } else {
    matchValue_45_12 = "C"s;
  }
  return matchValue_45_12;
}

inline double checkGuardsOnTheSubject() {
  return offText(gradeOf(100) + gradeOf(93) + gradeOf(85) + gradeOf(10), "perfectA 93BC"s);
}

inline bool yes(std::string reply) {
  bool matchValue_60_12 = false;
  if (reply == "y"s || reply == "yes"s) {
    matchValue_60_12 = true;
  } else if (reply == "n"s || reply == "no"s) {
    matchValue_60_12 = false;
  } else {
    matchValue_60_12 = false;
  }
  return matchValue_60_12;
}

inline double checkStrings() {
  return offText(str(yes("y"s)) + " "s + str(yes("yes"s)) + " "s + str(yes("no"s)) + " "s + str(yes("maybe"s)), "true true false false"s);
}

inline std::string kindOf(std::string path) {
  std::string matchValue_73_12 = ""s;
  if (path == "Ninafile"s || path == "Polyfile"s) {
    matchValue_73_12 = "build"s;
  } else if (endsWith(path, ".poly"s)) {
    matchValue_73_12 = "polyir"s;
  } else if (startsWith(path, "README"s)) {
    matchValue_73_12 = "docs"s;
  } else if (contains_(path, "test"s) && !endsWith(path, ".md"s)) {
    matchValue_73_12 = "a test"s;
  } else if (endsWith(path, ".md"s)) {
    const std::string p = path;
    matchValue_73_12 = "markdown "s + str(p);
  } else {
    matchValue_73_12 = "other"s;
  }
  return matchValue_73_12;
}

inline double checkConditions() {
  const List<std::string> kinds = listOf<std::string>({kindOf("Polyfile"s), kindOf("a.poly"s), kindOf("README.md"s), kindOf("my_test.c"s), kindOf("notes.md"s), kindOf("x.c"s)});
  return offText(join(kinds, ","s), "build,polyir,docs,a test,markdown notes.md,other"s);
}

inline std::string word(bool ready) {
  std::string matchValue_89_12 = ""s;
  switch (ready) {
    case true: {
      matchValue_89_12 = "go"s;
      break;
    }
    case false: {
      matchValue_89_12 = "wait"s;
      break;
    }
  }
  return matchValue_89_12;
}

inline double checkBooleans() {
  return offText(word(true) + word(false), "gowait"s);
}

inline std::string shown(std::optional<int64_t> found) {
  std::string matchValue_102_12 = ""s;
  if (found.has_value() && present(found) > 100) {
    const int64_t n = present(found);
    matchValue_102_12 = "big "s + str(n);
  } else if (found.has_value()) {
    const int64_t n = present(found);
    matchValue_102_12 = "found "s + str(n);
  } else {
    matchValue_102_12 = "nothing"s;
  }
  return matchValue_102_12;
}

inline double checkOptionals() {
  return offText(shown(std::optional<int64_t>(500)) + ", "s + shown(std::optional<int64_t>(5)) + ", "s + shown(std::optional<int64_t>()), "big 500, found 5, nothing"s);
}

inline Either<int64_t, std::string> safeDivide(int64_t a, int64_t b) {
  if (b == 0) {
    return Either<int64_t, std::string>::err("divide by zero"s);
  }
  return Either<int64_t, std::string>::ok(a / b);
}

inline std::string message(int64_t a, int64_t b) {
  const Either<int64_t, std::string> match_122_12 = safeDivide(a, b);
  std::string matchValue_122_12 = ""s;
  if (match_122_12.isOk) {
    const int64_t q = match_122_12.okValue();
    matchValue_122_12 = "quotient "s + str(q);
  } else if (!match_122_12.isOk) {
    const std::string why = match_122_12.errValue();
    matchValue_122_12 = "error "s + str(why);
  }
  return matchValue_122_12;
}

inline double checkEithers() {
  return offText(message(7, 2) + "; "s + message(7, 0), "quotient 3; error divide by zero"s);
}

inline std::string textOf(Variant shape) {
  std::string matchValue_133_12 = ""s;
  if (shape.tag == 0) {
    const int64_t r = std::any_cast<int64_t>(shape.value);
    matchValue_133_12 = "circle "s + str(r);
  } else if (shape.tag == 1) {
    const int64_t s = std::any_cast<int64_t>(shape.value);
    matchValue_133_12 = "square "s + str(s);
  } else if (shape.tag == 2) {
    const std::string name = std::any_cast<std::string>(shape.value);
    matchValue_133_12 = name;
  }
  return matchValue_133_12;
}

inline double checkVariants() {
  const std::string circle = textOf(makeVariant(0, int64_t(2)));
  return offText(circle + ", "s + textOf(makeVariant(1, int64_t(4))) + ", "s + textOf(makeVariant(2, "blob"s)), "circle 2, square 4, blob"s);
}

inline std::string zeroOr(std::optional<int64_t> found) {
  std::string matchValue_147_12 = ""s;
  if (found.has_value() && present(found) == 0) {
    matchValue_147_12 = "zero"s;
  } else if (found.has_value()) {
    matchValue_147_12 = "other"s;
  } else {
    matchValue_147_12 = "none"s;
  }
  return matchValue_147_12;
}

inline double checkPayloadPatterns() {
  return offText(zeroOr(std::optional<int64_t>(0)) + zeroOr(std::optional<int64_t>(3)) + zeroOr(std::optional<int64_t>()), "zeroothernone"s);
}

inline std::string said(List<int64_t> xs) {
  std::string matchValue_161_12 = ""s;
  if ((int64_t) xs->size() == 0) {
    matchValue_161_12 = "empty"s;
  } else if ((int64_t) xs->size() == 1) {
    const int64_t x = xs->at(0);
    matchValue_161_12 = "just "s + str(x);
  } else if (xs->at(0) == 0) {
    matchValue_161_12 = "starts at zero"s;
  } else {
    const int64_t first = xs->at(0);
    const List<int64_t> rest = drop<int64_t>(xs, 1);
    matchValue_161_12 = str(first) + ", then "s + str((int64_t) rest->size()) + " more"s;
  }
  return matchValue_161_12;
}

inline double checkLists() {
  const List<int64_t> none = newList<int64_t>();
  return offText(said(none) + "; "s + said(listOf<int64_t>({7})) + "; "s + said(listOf<int64_t>({0, 1, 2})) + "; "s + said(listOf<int64_t>({3, 4, 5})), "empty; just 7; starts at zero; 3, then 2 more"s);
}

inline std::string pairOf(List<int64_t> xs) {
  std::string matchValue_176_12 = ""s;
  if ((int64_t) xs->size() == 2 && xs->at(0) == xs->at(1)) {
    const int64_t a = xs->at(0);
    matchValue_176_12 = "a pair of "s + str(a);
  } else if ((int64_t) xs->size() == 2) {
    const int64_t a = xs->at(0);
    const int64_t b = xs->at(1);
    matchValue_176_12 = str(a) + " and "s + str(b);
  } else if ((int64_t) xs->size() >= 1 && xs->at(0) < 0) {
    matchValue_176_12 = "starts below zero"s;
  } else {
    matchValue_176_12 = "something else"s;
  }
  return matchValue_176_12;
}

inline double checkListGuards() {
  const List<int64_t> same_ = listOf<int64_t>({4, 4});
  const List<int64_t> two = listOf<int64_t>({1, 2});
  const List<int64_t> low = listOf<int64_t>({-1, 2, 3});
  return offText(pairOf(same_) + "; "s + pairOf(two) + "; "s + pairOf(low) + "; "s + pairOf(listOf<int64_t>({5})), "a pair of 4; 1 and 2; starts below zero; something else"s);
}

inline std::string command(std::string c) {
  std::string said = ""s;
  if (c == "quit"s || c == "exit"s) {
    return "bye"s;
  } else if (startsWith(c, "say "s)) {
    said = textSlice(c, 4, (int64_t) c.size());
  } else if (c == "noop"s) {}
  return said;
}

inline double checkStatements() {
  return offText(command("say hi"s) + "|"s + command("quit"s) + "|"s + command("noop"s) + "|"s + command("huh"s), "hi|bye||"s);
}

inline double checkNested() {
  const int64_t a = 2;
  const int64_t b = 5;
  std::string matchValue_212_21 = ""s;
  switch (a) {
    case 1: {
      matchValue_212_21 = "one"s;
      break;
    }
    case 2: {
      std::string matchValue_212_49 = ""s;
      switch (b) {
        case 5: {
          matchValue_212_49 = "two-five"s;
          break;
        }
        default: {
          matchValue_212_49 = "two"s;
          break;
        }
      }
      matchValue_212_21 = matchValue_212_49;
      break;
    }
    default: {
      matchValue_212_21 = "many"s;
      break;
    }
  }
  const std::string text = str(matchValue_212_21);
  return offText(text, "two-five"s);
}

template <typename T>
inline List<T> drop(List<T> xs, int64_t n) {
  const List<T> out = newList<T>();
  for (int64_t i = n; i < (int64_t) xs->size(); i = i + 1) {
    out->push_back(xs->at(i));
  }
  return out;
}
