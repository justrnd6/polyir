// Strings in polyir source (lang/src/Strings.nina): the source pass for text blocks, interpolation and `else if` (the first step of ParseSource,
// before the lexer), and the decoded value of a literal.
#pragma once

#include "polyir/Std.hpp"

namespace polyir::front {

  // The source with its text blocks (`"""…"""`) and interpolated strings (`"v${n}"`) written as plain one-line literals joined by `+`. Each
  // expression is wrapped in `str(…)`: `"v${n}"` is `("v" + str(n))`. It also turns `else if (c) {…}` into `else { if (c) {…} }`. The line
  // count is kept, so later positions still point at the right line.
  String DesugaredSource(String source);

  // A string literal's value: the quotes taken off and the escapes (`\n \t \r \" \' \\ \$`) decoded.
  String LiteralValue(StringView token);

  // A character literal's code: `'a'` is 97, `'\n'` is 10.
  int64_t CharValue(StringView token);

}  // namespace polyir::front
