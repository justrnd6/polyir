// A small JSON reader for the HIR files (internal): objects, arrays, strings, numbers, booleans and null.
#pragma once

#include "polyir/Std.hpp"
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace polyir::json {

  struct Value;
  using Array = Vector<Value>;
  using Object = Map<String, Value>;

  enum class Kind { Null, Bool, Number, String, Array, Object };

  struct Value {
    Kind kind = Kind::Null;
    bool boolean = false;
    double number = 0;
    int64_t integer = 0;  // the number when it was written without a fraction or exponent
    String text;
    SharedPtr<Array> items;
    SharedPtr<Object> members;
  };

  const Value& At(const Value& v, const String& key);  // throws when `v` is not an object with the key

  const Array& AsArray(const Value& v);

  const String& AsString(const Value& v);

  int64_t AsInt(const Value& v);

  double AsDouble(const Value& v);

  bool AsBool(const Value& v);

  Value Parse(const String& text);  // throws RuntimeError with the position of the mistake

  inline bool IsNull(const Value& v) {
    return v.kind == Kind::Null;
  }

  inline bool Has(const Value& v, const String& key) {
    return v.kind == Kind::Object && v.members->count(key) != 0;
  }

}  // namespace polyir::json
