#include <algorithm>
#include <cstdint>
#include <iostream>
#include <string>
#include <type_traits>
using namespace std::string_literals;

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

bool isSpaceChar(int8_t c);
std::string trim(std::string s);

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

int main() {
  println("hello from polyir"s);
  const std::string name = "café"s;
  const int64_t n = 3;
  println(str(name) + " has "s + str((int64_t) name.size()) + " bytes; "s + str(n) + " + 1 = "s + str(n + 1));
  println("a tab:\tand a quote: \"done\""s);
  println(trim("a text block"s + "\n"s + "  keeps its inner indent"s + "\n"s));
  println(textSlice(name, 0, 3));
  println(str(textFind(name, "é"s, 0)));
  println(str(textFind(name, "tea"s, 0)));
  println(str((int64_t) name.size() * 10));
  println(str(name == "café"s));
  const int8_t first = textByte(name, 0);
  println(str(first));
  println(str(first));
  return (int64_t) name.size() + textFind(name, "é"s, 0) * 10;
}
