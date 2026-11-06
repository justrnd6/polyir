// The JSON reader behind hir/Json.hpp: a recursive-descent parser over the whole text. `value` looks at the next
// character and hands off to the reader for an object, an array, a string, or a literal/number; objects and arrays
// call `value` again for what they hold. A malformed value throws with the offset where reading stopped; the reader is
// lenient about commas (optional, and a trailing one is allowed) and about how a number is spelled (strtod reads it).
#include "polyir/Std.hpp"
#include "hir/Json.hpp"

#include <cstdlib>

namespace polyir::json {

  namespace {

    // The text being read, and how far the reader is.
    struct Reader {
      const String& text;
      size_t at = 0;
    };

    Value value(Reader& reader);

    [[noreturn]] void fail(const Reader& reader, const String& what) {
      throw RuntimeError("JSON: " + what + " at offset " + to_string(reader.at));
    }

    bool isSpaceAt(const Reader& reader) {
      return reader.at < reader.text.size() && String(" \n\t\r").find(reader.text[reader.at]) != String::npos;
    }

    void skipSpace(Reader& reader) {
      while (isSpaceAt(reader)) {
        reader.at++;
      }
    }

    char peek(Reader& reader) {
      skipSpace(reader);

      return reader.at < reader.text.size() ? reader.text[reader.at] : '\0';
    }

    void expect(Reader& reader, char c) {
      if (peek(reader) != c) {
        fail(reader, String("expected '") + c + "'");
      }

      reader.at++;
    }

    // Reads `expected` and answers true when the text continues with it; otherwise reads nothing and answers false.
    bool readWord(Reader& reader, const char* expected) {
      const String wanted = expected;

      if (reader.text.compare(reader.at, wanted.size(), wanted) != 0) {
        return false;
      }

      reader.at += wanted.size();

      return true;
    }

    Value boolValue(bool boolean) {
      Value v;
      v.kind = Kind::Bool;
      v.boolean = boolean;

      return v;
    }

    bool isNumberChar(const Reader& reader) {
      return reader.at < reader.text.size() && String("+-0123456789.eE").find(reader.text[reader.at]) != String::npos;
    }

    String numberText(Reader& reader) {
      const size_t start = reader.at;

      while (isNumberChar(reader)) {
        reader.at++;
      }

      if (start == reader.at) {
        fail(reader, "unexpected character");
      }

      return reader.text.substr(start, reader.at - start);
    }

    // A number keeps both readings: as a double, and as an exact integer when it was written as one (`whole`). So a
    // 64-bit constant survives without rounding through a double.
    Value numberValue(const String& digits, bool whole) {
      Value v;
      v.kind = Kind::Number;
      v.number = strtod(digits.c_str(), nullptr);
      v.integer = whole ? strtoll(digits.c_str(), nullptr, 10) : static_cast<int64_t>(v.number);

      return v;
    }

    Value number(Reader& reader) {
      const String digits = numberText(reader);

      return numberValue(digits, digits.find_first_of(".eE") == String::npos);
    }

    Value literalOrNumber(Reader& reader) {
      Value v;

      if (readWord(reader, "true")) {
        return boolValue(true);
      }

      if (readWord(reader, "false")) {
        return boolValue(false);
      }

      if (readWord(reader, "null")) {
        return v;
      }

      return number(reader);
    }

    // The character after a backslash: `\n`, `\t` and `\r` are control characters, anything else (`\"`, `\\`) is itself.
    char escaped(Reader& reader) {
      const char c = reader.text[reader.at++];

      switch (c) {
        case 'n':
          return '\n';
        case 't':
          return '\t';
        case 'r':
          return '\r';
        default:
          return c;
      }
    }

    char stringChar(Reader& reader) {
      const char c = reader.text[reader.at++];

      return c == '\\' && reader.at < reader.text.size() ? escaped(reader) : c;
    }

    String rawString(Reader& reader) {
      expect(reader, '"');
      String out;

      while (reader.at < reader.text.size() && reader.text[reader.at] != '"') {
        out += stringChar(reader);
      }

      expect(reader, '"');

      return out;
    }

    Value stringValue(Reader& reader) {
      Value v;
      v.kind = Kind::String;
      v.text = rawString(reader);

      return v;
    }

    void skipComma(Reader& reader) {
      if (peek(reader) == ',') {
        reader.at++;
      }
    }

    void elements(Reader& reader, Array& out) {
      while (peek(reader) != ']') {
        out.push_back(value(reader));
        skipComma(reader);
      }

      expect(reader, ']');
    }

    Value array(Reader& reader) {
      Value v;
      v.kind = Kind::Array;
      v.items = make_shared<Array>();
      expect(reader, '[');
      elements(reader, *v.items);

      return v;
    }

    void member(Reader& reader, Object& out) {
      const String key = rawString(reader);
      expect(reader, ':');
      out[key] = value(reader);
      skipComma(reader);
    }

    void members(Reader& reader, Object& out) {
      while (peek(reader) != '}') {
        member(reader, out);
      }

      expect(reader, '}');
    }

    Value object(Reader& reader) {
      Value v;
      v.kind = Kind::Object;
      v.members = make_shared<Object>();
      expect(reader, '{');
      members(reader, *v.members);

      return v;
    }

    Value value(Reader& reader) {
      const char c = peek(reader);

      if (c == '{') {
        return object(reader);
      }

      if (c == '[') {
        return array(reader);
      }

      if (c == '"') {
        return stringValue(reader);
      }

      return literalOrNumber(reader);
    }

    Value document(Reader& reader) {
      Value v = value(reader);
      skipSpace(reader);

      if (reader.at != reader.text.size()) {
        fail(reader, "unexpected text after the value");
      }

      return v;
    }

  }  // namespace

  const Value& At(const Value& v, const String& key) {
    if (v.kind != Kind::Object || v.members->count(key) == 0) {
      throw RuntimeError("JSON: missing key '" + key + "'");
    }

    return v.members->at(key);
  }

  const Array& AsArray(const Value& v) {
    if (v.kind != Kind::Array) {
      throw RuntimeError("JSON: an array was expected");
    }

    return *v.items;
  }

  const String& AsString(const Value& v) {
    if (v.kind != Kind::String) {
      throw RuntimeError("JSON: a string was expected");
    }

    return v.text;
  }

  int64_t AsInt(const Value& v) {
    if (v.kind != Kind::Number) {
      throw RuntimeError("JSON: a number was expected");
    }

    return v.integer;
  }

  // The HIR writers spell every float as a string (FloatText: exact hex, nan, inf), which strtod reads back exactly. A
  // plain JSON number (hand-written HIR) is accepted too.
  double AsDouble(const Value& v) {
    if (v.kind == Kind::String) {
      return strtod(v.text.c_str(), nullptr);
    }

    if (v.kind != Kind::Number) {
      throw RuntimeError("JSON: a number was expected");
    }

    return v.number;
  }

  bool AsBool(const Value& v) {
    if (v.kind != Kind::Bool) {
      throw RuntimeError("JSON: a boolean was expected");
    }

    return v.boolean;
  }

  Value Parse(const String& text) {
    Reader reader{text};

    return document(reader);
  }

}  // namespace polyir::json
