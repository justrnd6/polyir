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
struct Dep;

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

struct Dep {
  std::string name;
  std::string version;

  Dep(std::string name, std::string version) : name(name), version(version) {}

  bool operator==(const Dep& other) const {
    return allSame(name, other.name, version, other.version);
  }

  std::size_t hash() const {
    return hashAll(name, version);
  }
};

List<std::string> split(std::string s, std::string sep);
std::string join(List<std::string> parts, std::string sep);
bool isSpaceChar(int8_t c);
std::string trim(std::string s);
std::string repeat(std::string s, int64_t n);
std::string fill(std::string template_, Map<std::string, std::string> values);
std::string filledName(std::string placeholder, Map<std::string, std::string> values);
std::string cmakeLists(std::string name, List<std::string> sources, List<Ref<Dep>> deps);
int64_t checksum(std::string s);

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

inline std::string repeat(std::string s, int64_t n) {
  std::string out = ""s;
  for (int64_t i = 0; i < n; i = i + 1) {
    out = out + s;
  }
  return out;
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

inline std::string cmakeLists(std::string name, List<std::string> sources, List<Ref<Dep>> deps) {
  const Ref<StringBuilder> out = std::make_shared<StringBuilder>(newList<std::string>(), newList<int64_t>());
  for (auto d : *deps) {
    out->line("pkg_check_modules("s + str(d->name) + " REQUIRED IMPORTED_TARGET "s + str(d->name) + ">="s + str(d->version) + ")"s);
  }
  return "cmake_minimum_required(VERSION 3.20)"s + "\n"s + "project("s + str(name) + " LANGUAGES C)"s + "\n"s + "find_package(PkgConfig REQUIRED)"s + "\n"s + str(trim(out->text())) + "\n"s + "add_executable("s + str(name) + " "s + str(join(sources, " "s)) + ")"s + "\n"s;
}

inline int64_t checksum(std::string s) {
  int64_t sum = 0;
  for (int64_t i = 0; i < (int64_t) s.size(); i = i + 1) {
    sum = (sum * 31 + (int64_t) textByte(s, i)) % 1000003;
  }
  return sum;
}

int main() {
  const List<Ref<Dep>> deps = listOf<Ref<Dep>>({std::make_shared<Dep>("zlib"s, "1.2"s), std::make_shared<Dep>("sdl2"s, "2.0"s)});
  const std::string text = cmakeLists("demo"s, listOf<std::string>({"main.c"s, "util.c"s}), deps);
  const Map<std::string, std::string> values = newMap<std::string, std::string>();
  values->insert_or_assign("who"s, "polyir"s);
  const std::string hello = fill("hello ${who}"s, values);
  return (checksum(text) + (int64_t) split("a, b ,c"s, ","s)->size() * 7 + (int64_t) hello.size()) % 256;
}
