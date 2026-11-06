#pragma once

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

struct StringBuilder;

struct StringBuilder : std::enable_shared_from_this<StringBuilder> {
  List<std::string> pieces;
  List<int64_t> indents;

  StringBuilder(List<std::string> pieces, List<int64_t> indents) : pieces(pieces), indents(indents) {}

  Ref<StringBuilder> add(std::string s);
  Ref<StringBuilder> line(std::string s);
  Ref<StringBuilder> blank();
  Ref<StringBuilder> lines(List<std::string> all);
  Ref<StringBuilder> indent(int64_t n);
  Ref<StringBuilder> dedent();
  int64_t indentNow();
  int64_t length();
  bool isEmpty();
  std::string text();

  bool operator==(const StringBuilder& other) const {
    return allSame(pieces, other.pieces, indents, other.indents);
  }

  std::size_t hash() const {
    return hashAll(pieces, indents);
  }
};

bool contains_(std::string s, std::string part);
bool startsWith(std::string s, std::string prefix);
bool endsWith(std::string s, std::string suffix);
List<std::string> split(std::string s, std::string sep);
List<std::string> lines(std::string s);
std::string join(List<std::string> parts, std::string sep);
bool isSpaceChar(int8_t c);
std::string trim(std::string s);
std::string replace(std::string s, std::string old, std::string by);
std::string repeat(std::string s, int64_t n);
std::string indent(std::string s, int64_t n);
std::string indentRest(std::string s, int64_t n);
std::string fill(std::string template_, Map<std::string, std::string> values);
std::string filledName(std::string placeholder, Map<std::string, std::string> values);
double offText(std::string got, std::string want);
double offInt(int64_t got, int64_t want);
double checkEscapes();
double checkCharEscapes();
double checkInterpolation();
double checkInterpolatedMethodCalls();
double checkTextBlock();
double checkTextBlockWithoutFinalNewline();
double checkTextBlockKeepsIndentOfMultiLineValue();
double checkStrOfValues();
double checkCharAt();
double checkSliceAndFind();
double checkSliceClamps();
double checkFindFrom();
double checkListLiteralAndForIn();
double checkSplitAndJoin();
double checkTrimAndContains();
double checkStartsAndEnds();
double checkReplaceRepeatIndent();
double checkLines();
double checkFill();
void writeDeps(Ref<StringBuilder> out, List<std::string> deps);
double checkStringBuilder();

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

inline List<std::string> split(std::string s, std::string sep) {
  const List<std::string> parts = newList<std::string>();
  if ((int64_t) sep.size() == 0) {
    parts->push_back(s);
    return parts;
  }
  int64_t from = 0;
  for (int64_t at = textFind(s, sep, 0); at >= 0; at = textFind(s, sep, from)) {
    parts->push_back(textSlice(s, from, at));
    from = at + (int64_t) sep.size();
  }
  parts->push_back(textSlice(s, from, (int64_t) s.size()));
  return parts;
}

inline List<std::string> lines(std::string s) {
  return split(s, "\n"s);
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

inline bool isSpaceChar(int8_t c) {
  return c == 32 || c == 9 || c == 10 || c == 13;
}

inline std::string trim(std::string s) {
  int64_t from = 0;
  while (from < (int64_t) s.size() && isSpaceChar(textByte(s, from))) {
    from = from + 1;
  }
  int64_t to = (int64_t) s.size();
  while (to > from && isSpaceChar(textByte(s, to - 1))) {
    to = to - 1;
  }
  return textSlice(s, from, to);
}

inline std::string replace(std::string s, std::string old, std::string by) {
  return join(split(s, old), by);
}

inline std::string repeat(std::string s, int64_t n) {
  std::string out = ""s;
  for (int64_t i = 0; i < n; i = i + 1) {
    out = out + s;
  }
  return out;
}

inline std::string indent(std::string s, int64_t n) {
  const List<std::string> all = lines(s);
  const std::string pad = repeat(" "s, n);
  for (int64_t i = 0; i < (int64_t) all->size(); i = i + 1) {
    if ((int64_t) (all->at(i)).size() > 0) {
      all->at(i) = pad + all->at(i);
    }
  }
  return join(all, "\n"s);
}

inline std::string indentRest(std::string s, int64_t n) {
  const int64_t first = textFind(s, "\n"s, 0);
  if (first < 0) {
    return s;
  }
  return textSlice(s, 0, first + 1) + indent(textSlice(s, first + 1, (int64_t) s.size()), n);
}

inline std::string fill(std::string template_, Map<std::string, std::string> values) {
  std::string out = ""s;
  int64_t from = 0;
  for (int64_t at = textFind(template_, "${"s, 0); at >= 0; at = textFind(template_, "${"s, from)) {
    const int64_t close = textFind(template_, "}"s, at);
    if (close < 0) {
      return out + textSlice(template_, from, (int64_t) template_.size());
    }
    out = out + textSlice(template_, from, at) + filledName(textSlice(template_, at, close + 1), values);
    from = close + 1;
  }
  return out + textSlice(template_, from, (int64_t) template_.size());
}

inline std::string filledName(std::string placeholder, Map<std::string, std::string> values) {
  const std::string name = trim(textSlice(placeholder, 2, (int64_t) placeholder.size() - 1));
  if (values->contains(name)) {
    return getOr(values, name, ""s);
  }
  return placeholder;
}

inline auto StringBuilder::add(std::string s) -> Ref<StringBuilder> {
  this->pieces->push_back(s);
  return shared_from_this();
}

inline auto StringBuilder::line(std::string s) -> Ref<StringBuilder> {
  if ((int64_t) s.size() > 0) {
    this->pieces->push_back(repeat(" "s, this->indentNow()));
  }
  this->pieces->push_back(s);
  this->pieces->push_back("\n"s);
  return shared_from_this();
}

inline auto StringBuilder::blank() -> Ref<StringBuilder> {
  this->pieces->push_back("\n"s);
  return shared_from_this();
}

inline auto StringBuilder::lines(List<std::string> all) -> Ref<StringBuilder> {
  for (int64_t i = 0; i < (int64_t) all->size(); i = i + 1) {
    this->line(all->at(i));
  }
  return shared_from_this();
}

inline auto StringBuilder::indent(int64_t n) -> Ref<StringBuilder> {
  this->indents->push_back(this->indentNow() + n);
  return shared_from_this();
}

inline auto StringBuilder::dedent() -> Ref<StringBuilder> {
  if ((int64_t) (this->indents)->size() > 0) {
    truncate(this->indents, (int64_t) (this->indents)->size() - 1);
  }
  return shared_from_this();
}

inline auto StringBuilder::indentNow() -> int64_t {
  if ((int64_t) (this->indents)->size() == 0) {
    return 0;
  }
  return this->indents->at((int64_t) (this->indents)->size() - 1);
}

inline auto StringBuilder::length() -> int64_t {
  return (int64_t) (this->text()).size();
}

inline auto StringBuilder::isEmpty() -> bool {
  return this->length() == 0;
}

inline auto StringBuilder::text() -> std::string {
  return join(this->pieces, ""s);
}

inline double offText(std::string got, std::string want) {
  if (got == want) {
    return 0.0;
  }
  return 1.0;
}

inline double offInt(int64_t got, int64_t want) {
  return (double) (got - want);
}

inline double checkEscapes() {
  const std::string s = "a\tb\n\"c\" \\ $"s;
  return offInt((int64_t) s.size(), 11);
}

inline double checkCharEscapes() {
  return offInt((int64_t) 10 + (int64_t) 39 + (int64_t) 92, 10 + 39 + 92);
}

inline double checkInterpolation() {
  const std::string name = "zlib"s;
  const int64_t n = 42;
  return offText("lib "s + str(name) + " v"s + str(n + 1) + " ok="s + str(n > 1), "lib zlib v43 ok=true"s);
}

inline double checkInterpolatedMethodCalls() {
  const List<std::string> words = listOf<std::string>({"a"s, "b"s, "c"s});
  return offText("["s + str(join(words, ", "s)) + "]"s, "[a, b, c]"s);
}

inline double checkTextBlock() {
  const std::string name = "demo"s;
  const std::string text = "project("s + str(name) + ")"s + "\n"s + "  add(\"x\")"s + "\n"s + "\n"s + "done"s + "\n"s;
  return offText(text, "project(demo)\n  add(\"x\")\n\ndone\n"s);
}

inline double checkTextBlockWithoutFinalNewline() {
  const std::string text = "one"s + "\n"s + "two"s;
  return offText(text, "one\ntwo"s);
}

inline double checkTextBlockKeepsIndentOfMultiLineValue() {
  const std::string inner = "a\nb"s;
  const std::string text = "list:"s + "\n"s + "  "s + str(indentRest(str(inner), 2)) + "\n"s + "end"s + "\n"s;
  return offText(text, "list:\n  a\n  b\nend\n"s);
}

inline double checkStrOfValues() {
  const int8_t c = 120;
  return offText(str(0 - 7) + str(true) + str(false) + str(c) + str("s"s) + str(12), "-7truefalsexs12"s);
}

inline double checkCharAt() {
  const std::string s = "hello"s;
  return offInt((int64_t) textByte(s, 1), 101);
}

inline double checkSliceAndFind() {
  const std::string s = "key=value"s;
  const int64_t at = textFind(s, "="s, 0);
  return offText(textSlice(s, 0, at) + "|"s + textSlice(s, at + 1, (int64_t) s.size()), "key|value"s) + offInt(textFind(s, "z"s, 0), 0 - 1);
}

inline double checkSliceClamps() {
  const std::string s = "abc"s;
  return offText(textSlice(s, 0 - 5, 99) + textSlice(s, 2, 1), "abc"s);
}

inline double checkFindFrom() {
  const std::string s = "a.b.c"s;
  return offInt(textFind(s, "."s, 2), 3);
}

inline double checkListLiteralAndForIn() {
  int64_t total = 0;
  for (auto w : *(listOf<std::string>({"cmake"s, "-S"s, "src"s}))) {
    total = total + (int64_t) w.size();
  }
  return offInt(total, 10);
}

inline double checkSplitAndJoin() {
  const List<std::string> parts = split("a,b,,c"s, ","s);
  return offInt((int64_t) parts->size(), 4) + offText(join(parts, "+"s), "a+b++c"s);
}

inline double checkTrimAndContains() {
  const std::string s = "  \tpadded \n"s;
  return offText(trim(s), "padded"s) + offInt((int64_t) contains_(s, "dd"s), 1) + offInt((int64_t) contains_(s, "x"s), 0);
}

inline double checkStartsAndEnds() {
  const std::string s = "CMakeLists.txt"s;
  const bool both = startsWith(s, "CMake"s) && endsWith(s, ".txt"s) && !endsWith(s, "CMake"s);
  return offInt((int64_t) both, 1);
}

inline double checkReplaceRepeatIndent() {
  const std::string s = replace("a-b-c"s, "-"s, "::"s);
  return offText(s + "|"s + repeat("ab"s, 3) + "|"s + indent("x\n\ny"s, 2), "a::b::c|ababab|  x\n\n  y"s);
}

inline double checkLines() {
  return offInt((int64_t) lines("one\ntwo\nthree"s)->size(), 3);
}

inline double checkFill() {
  const Map<std::string, std::string> values = newMap<std::string, std::string>();
  values->insert_or_assign("name"s, "gcd"s);
  values->insert_or_assign("lang"s, "C"s);
  return offText(fill("project(${name} LANGUAGES ${lang}) ${missing}"s, values), "project(gcd LANGUAGES C) ${missing}"s);
}

inline void writeDeps(Ref<StringBuilder> out, List<std::string> deps) {
  out->indent(4);
  for (auto d : *deps) {
    out->line("implementation(\""s + str(d) + "\")"s);
  }
  out->dedent();
}

inline double checkStringBuilder() {
  const Ref<StringBuilder> out = std::make_shared<StringBuilder>(newList<std::string>(), newList<int64_t>());
  out->line("dependencies {"s);
  writeDeps(out, listOf<std::string>({"a:b:1"s, "c:d:2"s}));
  out->line("}"s);
  const std::string want = "dependencies {"s + "\n"s + "    implementation(\"a:b:1\")"s + "\n"s + "    implementation(\"c:d:2\")"s + "\n"s + "}"s + "\n"s;
  return offText(out->text(), want) + offInt((int64_t) (out->isEmpty()), 0);
}
