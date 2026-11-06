// A construct's forms (core/docs/translate.md). A form is one set of parts a construct comes with (`if(cond, then)`,
// `if(cond, then, else)`). A source's canon (Schema.hpp) lists every form of every construct its trees hold; a page has a
// rule for each form, or a word for why it has none.
#pragma once

#include "polyir/Std.hpp"
#include "polyir/Export.hpp"

namespace polyir::translate {

  // What fills a part: a name (renamed away from the target's reserved words), plain text, an expression, statements, a list of
  // items, a statement written in place, a type, or a fact (information a rule may use or leave out, like `isKey`).
  enum class PartKind { Name, Text, Value, Block, List, Statement, Type, Fact };

  struct PartSpec {
    String name;
    PartKind kind = PartKind::Value;
  };

  struct FormSpec {
    String section;
    String construct;
    Vector<PartSpec> parts;
    bool isType = false;
  };

  POLYIR_API const PartSpec* FindPart(const FormSpec& form, const String& name);

  POLYIR_API String FormHead(const FormSpec& form);  // `if(cond, then, else)`

  POLYIR_API bool MayLeaveOut(PartKind kind);  // a type or a fact: a rule may name it or not

}  // namespace polyir::translate
