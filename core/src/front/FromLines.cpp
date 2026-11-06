// The `//# from FILE:LINE[:COLUMN] [CODE]` lines of a program's text (Module::fromLines): each says where the line after it came
// from, as C's `#line` does, and may give that line's code. A program another front end wrote (nina-lite's `.poly`) can so be
// mapped back to its own source. To any other reader the line is a comment.
#include "front/Pipeline.hpp"

namespace polyir::front {

  namespace {

    bool isWholeNumber(const String& text) {
      return !text.empty() && all_of(text.begin(), text.end(), [](char c) { return c >= '0' && c <= '9'; });
    }

    Vector<String> colonParts(const String& text) {
      Vector<String> parts;
      IStringStream in(text);

      for (String part; getline(in, part, ':');) {
        parts.push_back(part);
      }

      return parts;
    }

    String joinedFirst(const Vector<String>& parts, size_t count) {
      String text;

      for (size_t i = 0; i < count; i++) {
        text += (i == 0 ? "" : ":") + parts[i];
      }

      return text;
    }

    // `Hops.nina:12:5` or `Hops.nina:12` on line `line` of file `in`: the file, its line and its column (0 when it gives none). A
    // place it cannot read gives a FromLine of line 0, which is left out.
    FromLine placeOf(const String& place, const String& in, int line) {
      const Vector<String> parts = colonParts(place);
      const size_t count = parts.size();
      const bool withColumn = count >= 3 && isWholeNumber(parts[count - 1]) && isWholeNumber(parts[count - 2]);
      const size_t numbers = withColumn ? 2 : 1;

      if (count < 2 || !isWholeNumber(parts[count - numbers])) {
        return FromLine{};
      }

      return FromLine{in, line, joinedFirst(parts, count - numbers), stoi(parts[count - numbers]), withColumn ? stoi(parts[count - 1]) : 0, ""};
    }

    // What follows `//# from `: the place up to the first space, then the line's code, if any.
    FromLine fromLineOf(const String& rest, const String& in, int line) {
      const size_t space = rest.find(' ');
      FromLine from = placeOf(rest.substr(0, space), in, line);
      const size_t code = space == String::npos ? String::npos : rest.find_first_not_of(' ', space);
      from.code = code == String::npos ? "" : rest.substr(code, rest.find_last_not_of(" \t\r") - code + 1);

      return from;
    }

  }  // namespace

  Vector<FromLine> FromLinesOf(const String& text, const String& in) {
    const String mark = "//# from ";
    Vector<FromLine> lines;
    IStringStream reading(text);
    int number = 0;

    for (String line; getline(reading, line);) {
      number++;
      const size_t start = line.find_first_not_of(" \t");

      if (start != String::npos && line.compare(start, mark.size(), mark) == 0) {
        const FromLine from = fromLineOf(line.substr(start + mark.size()), in, number);
        from.line > 0 ? lines.push_back(from) : void();
      }
    }

    return lines;
  }

}  // namespace polyir::front
