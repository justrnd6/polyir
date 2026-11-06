// The C++ parser against the Nina one: every example program (front_examples.txt) parses without a syntax error, the programs the Nina front end
// rejects at parse time are rejected with its messages, and a table of broken sources gets the message the Nina parser gives (the expected
// strings were produced by running lang/cli/build/out/polyir-lang on each source; regenerate them there if the grammar changes). A few small
// programs check the shape of the tree, since the checker is written against it.
#include "polyir/Std.hpp"
#include "polyir/Frontend.hpp"
#include "front/Ast.hpp"
#include "front/Lint.hpp"

using namespace polyir;
using namespace polyir::front;

namespace {

  int failures = 0;

  struct Case {
    const char* name;
    const char* source;
    const char* expected;  // "" when the source has no syntax error
  };

  // Sources with the "line:column: message" the Nina front end prints for them (its `file:` prefix removed).
  const Case kCases[] = {
    {"eof in a block",
     "func main() -> i64 { return 1 ",
     "1:31: expected '}' (found '')"},
    {"let with a colon and no type",
     "func main() -> i64 { let x: = 1 return x }",
     "1:29: expected 'array', 'buffer', 'closure', 'coro', 'either', 'func', 'grid', IDENT, 'map', 'mutarray', 'mutmap', 'mutset', 'optional', 'ptr', 'set', 'sparse', 'tensor', 'variant' or 'vec' (found '=')"},
    {"binary operator without a right side",
     "func main() -> i64 { return 1 + }",
     "1:33: expected 'array', '!', 'buffer', CHAR_LIT, 'closure', 'eitherErr', 'eitherOk', 'false', FLOAT_LIT, IDENT, 'if', INT_LIT, '[', '(', 'map', 'match', '-', 'mutarray', 'mutmap', 'mutset', 'None', 'set', 'Some', 'sparse', STRING_LIT, '~', 'true', 'val', 'variant' or 'vec' (found '}')"},
    {"junk at the start",
     "banana",
     "1:1: expected 'bridge', 'coro', end of input, 'extern', 'func', 'global', 'import', 'interface', 'kernel', 'meta' or 'struct' (found 'banana')"},
    {"junk after a declaration",
     "func main() -> i64 { return 1 } banana",
     "1:33: expected end of input (found 'banana')"},
    {"parameter list not closed",
     "func main( -> i64 { }",
     "1:12: expected ')' (found '->')"},
    {"parameter without a type",
     "func f(x) -> i64 { return 1 }",
     "1:9: expected ':' (found ')')"},
    {"parameter with a bad mark",
     "func f(uniform) -> i64 { return 1 }",
     "1:15: expected IDENT (found ')')"},
    {"val parameter without a type",
     "func f(x: val) -> i64 { return 1 }",
     "1:14: expected 'array', 'buffer', 'closure', 'coro', 'either', 'func', 'grid', IDENT, 'map', 'mutarray', 'mutmap', 'mutset', 'optional', 'ptr', 'set', 'sparse', 'tensor', 'variant' or 'vec' (found ')')"},
    {"missing arrow",
     "func f() i64 { return 1 }",
     "1:10: expected '->' (found 'i64')"},
    {"missing return type",
     "func f() -> { return 1 }",
     "1:13: expected 'array', 'buffer', 'closure', 'coro', 'either', 'func', 'grid', IDENT, 'map', 'mutarray', 'mutmap', 'mutset', 'optional', 'ptr', 'set', 'sparse', 'tensor', 'variant' or 'vec' (found '{')"},
    {"missing body",
     "func f() -> i64",
     "1:16: expected '{' (found '')"},
    {"kernel without func",
     "kernel f() -> i64 { return 1 }",
     "1:8: expected 'func' (found 'f')"},
    {"coro kernel",
     "coro kernel func f() -> i64 { yield 1 }",
     "1:6: expected 'func' (found 'kernel')"},
    {"struct field without a type",
     "struct P { x: }",
     "1:15: expected 'array', 'buffer', 'closure', 'coro', 'either', 'func', 'grid', IDENT, 'map', 'mutarray', 'mutmap', 'mutset', 'optional', 'ptr', 'set', 'sparse', 'tensor', 'variant' or 'vec' (found '}')"},
    {"struct field without a colon",
     "struct P { x i64 }",
     "1:14: expected ':' (found 'i64')"},
    {"primary field without a type",
     "struct P(x) { }",
     "1:11: expected ':' (found ')')"},
    {"primary field list not closed",
     "struct P(x: i64 { }",
     "1:17: expected ')' (found '{')"},
    {"empty type parameters",
     "struct P<> { }",
     "1:10: expected IDENT (found '>')"},
    {"implements nothing",
     "struct P : { }",
     "1:12: expected IDENT (found '{')"},
    {"struct member junk",
     "struct P { 5 }",
     "1:12: expected '}' (found '5')"},
    {"struct body not closed",
     "struct P { x: i64",
     "1:18: expected '}' (found '')"},
    {"var field without a name",
     "struct S { var }",
     "1:16: expected IDENT (found '}')"},
    {"interface method without func",
     "interface I { f() -> i64 }",
     "1:15: expected '}' (found 'f')"},
    {"interface method without a return type",
     "interface I { func f() }",
     "1:24: expected '->' (found '}')"},
    {"extern func without a return type",
     "extern func f()",
     "1:16: expected '->' (found '')"},
    {"extern binding without a target",
     "extern func f() -> i64 = \"x\"",
     "1:26: expected IDENT (found '\"x\"')"},
    {"extern binding without a string",
     "extern func f() -> i64 = java",
     "1:30: expected STRING_LIT (found '')"},
    {"extern type member without a name",
     "extern type T = java \"x\" { func }",
     "1:33: expected IDENT (found '}')"},
    {"extern type member junk",
     "extern type T { 5 }",
     "1:17: expected '}' (found '5')"},
    {"extern interface without a body",
     "extern interface I",
     "1:19: expected '{' (found '')"},
    {"bridge without an arrow",
     "bridge array<i64> java \"long[]\"",
     "1:19: expected '<->' (found 'java')"},
    {"bridge rule without a string",
     "bridge array<i64> <-> java \"long[]\" { toHost = }",
     "1:48: expected STRING_LIT (found '}')"},
    {"global without an initializer",
     "global g: i64 =",
     "1:16: expected 'false', FLOAT_LIT, INT_LIT or 'true' (found '')"},
    {"global with a name as initializer",
     "global g: i64 = x",
     "1:17: expected 'false', FLOAT_LIT, INT_LIT or 'true' (found 'x')"},
    {"global without a type",
     "global g = 1",
     "1:10: expected ':' (found '=')"},
    {"import of a number",
     "import 5",
     "1:8: expected STRING_LIT (found '5')"},
    {"meta without a value",
     "meta \"a\" = 1",
     "1:12: expected STRING_LIT (found '1')"},
    {"if without parentheses",
     "func f() -> i64 { if x { } return 1 }",
     "1:22: expected '(' (found 'x')"},
    {"if with a non-block body",
     "func f() -> i64 { if (x) return 1 }",
     "1:26: expected '{' (found 'return')"},
    {"else without a block",
     "func f() -> i64 { if (x) { } else return 1 }",
     "1:35: expected '{' (found 'return')"},
    {"for with a missing part",
     "func f() -> i64 { for (i = 0; i < 3) { } }",
     "1:36: expected ';' (found ')')"},
    {"for binding without a value",
     "func f() -> i64 { for (i = ; i < 3; i = i + 1) { } }",
     "1:28: expected 'array', '!', 'buffer', CHAR_LIT, 'closure', 'eitherErr', 'eitherOk', 'false', FLOAT_LIT, IDENT, 'if', INT_LIT, '[', '(', 'map', 'match', '-', 'mutarray', 'mutmap', 'mutset', 'None', 'set', 'Some', 'sparse', STRING_LIT, '~', 'true', 'val', 'variant' or 'vec' (found ';')"},
    {"for step after a comma",
     "func f() -> i64 { for (i = 0; i < 3; i = i + 1, ) { } }",
     "1:49: expected IDENT (found ')')"},
    {"switch without a default",
     "func f() -> i64 { switch (1) { 1 => a() } }",
     "1:41: expected 'else' (found '}')"},
    {"switch case without an arrow",
     "func f() -> i64 { switch (1) { 1 a() else => b() } }",
     "1:34: expected '=>' (found 'a')"},
    {"switch case with a name",
     "func f() -> i64 { switch (1) { x => a() else => b() } }",
     "1:37: expected '{' (found 'a')"},
    {"label without a colon",
     "func f() -> i64 { label l() }",
     "1:29: expected ':' (found '}')"},
    {"label parameters not closed",
     "func f() -> i64 { label l(: }",
     "1:27: expected ')' (found ':')"},
    {"goto without a target",
     "func f() -> i64 { goto }",
     "1:24: expected IDENT (found '}')"},
    {"goto target without parentheses",
     "func f() -> i64 { goto l }",
     "1:26: expected '(' (found '}')"},
    {"branch without a question mark",
     "func f() -> i64 { branch 1 a() : b() }",
     "1:28: expected '?' (found 'a')"},
    {"branch without a second target",
     "func f() -> i64 { branch 1 ? a() : }",
     "1:36: expected IDENT (found '}')"},
    {"try without catch",
     "func f() -> i64 { try { } }",
     "1:27: expected 'catch' (found '}')"},
    {"catch without parentheses",
     "func f() -> i64 { try { } catch x { } }",
     "1:33: expected '(' (found 'x')"},
    {"throw without a value",
     "func f() -> i64 { throw }",
     "1:25: expected 'array', '!', 'buffer', CHAR_LIT, 'closure', 'eitherErr', 'eitherOk', 'false', FLOAT_LIT, IDENT, 'if', INT_LIT, '[', '(', 'map', 'match', '-', 'mutarray', 'mutmap', 'mutset', 'None', 'set', 'Some', 'sparse', STRING_LIT, '~', 'true', 'val', 'variant' or 'vec' (found '}')"},
    {"a bare return (the checker decides whether the function may have one)",
     "func f() -> i64 { return }",
     ""},
    {"yield without a value",
     "coro func f() -> i64 { yield }",
     "1:30: expected 'array', '!', 'buffer', CHAR_LIT, 'closure', 'eitherErr', 'eitherOk', 'false', FLOAT_LIT, IDENT, 'if', INT_LIT, '[', '(', 'map', 'match', '-', 'mutarray', 'mutmap', 'mutset', 'None', 'set', 'Some', 'sparse', STRING_LIT, '~', 'true', 'val', 'variant' or 'vec' (found '}')"},
    {"assignment without a value",
     "func f() -> i64 { x = }",
     "1:23: expected 'array', '!', 'buffer', CHAR_LIT, 'closure', 'eitherErr', 'eitherOk', 'false', FLOAT_LIT, IDENT, 'if', INT_LIT, '[', '(', 'map', 'match', '-', 'mutarray', 'mutmap', 'mutset', 'None', 'set', 'Some', 'sparse', STRING_LIT, '~', 'true', 'val', 'variant' or 'vec' (found '}')"},
    {"two values in a row",
     "func f() -> i64 { x = 1 2 }",
     ""},
    {"statement starting with a minus",
     "func f() -> i64 { -1 }",
     "1:19: expected '}' (found '-')"},
    {"statement starting with a bang",
     "func f() -> i64 { !x }",
     "1:19: expected '}' (found '!')"},
    {"statement starting with a string is fine",
     "func f() -> i64 { \"abc\" }",
     ""},
    {"let value missing",
     "func f() -> i64 { let x: i64 = }",
     "1:32: expected 'array', '!', 'buffer', CHAR_LIT, 'closure', 'eitherErr', 'eitherOk', 'false', FLOAT_LIT, IDENT, 'if', INT_LIT, '[', '(', 'map', 'match', '-', 'mutarray', 'mutmap', 'mutset', 'None', 'set', 'Some', 'sparse', STRING_LIT, '~', 'true', 'val', 'variant' or 'vec' (found '}')"},
    {"array type not closed",
     "func f() -> i64 { let a: array<i64 = 1 }",
     "1:36: expected '>' (found '=')"},
    {"nested generic closes with >>",
     "func f() -> i64 { let a: array<array<i64>> = 1 }",
     ""},
    {"a >> with one list of types open",
     "func f() -> i64 { let a: array<i64>> = 1 }",
     "1:35: expected '>' (found '>>')"},
    {"vec type without a count",
     "func f() -> i64 { let a: vec<i64> = 1 }",
     "1:33: expected ',' (found '>')"},
    {"map type with one argument",
     "func f() -> i64 { let m: map<i64> = 1 }",
     "1:33: expected ',' (found '>')"},
    {"tensor type without dimensions",
     "func f() -> i64 { let t: tensor<i64> = 1 }",
     "1:36: expected ',' (found '>')"},
    {"grid type with one size",
     "func f() -> i64 { let t: grid<i64, 2> = 1 }",
     "1:37: expected ',' (found '>')"},
    {"func type without an arrow",
     "func f() -> i64 { let g: func(i64) = 1 }",
     "1:36: expected '->' (found '=')"},
    {"closure type without a return type",
     "func f() -> i64 { let g: closure(i64) -> = 1 }",
     "1:42: expected 'array', 'buffer', 'closure', 'coro', 'either', 'func', 'grid', IDENT, 'map', 'mutarray', 'mutmap', 'mutset', 'optional', 'ptr', 'set', 'sparse', 'tensor', 'variant' or 'vec' (found '=')"},
    {"variant type with nothing",
     "func f() -> i64 { let v: variant<> = 1 }",
     "1:34: expected 'array', 'buffer', 'closure', 'coro', 'either', 'func', 'grid', IDENT, 'map', 'mutarray', 'mutmap', 'mutset', 'optional', 'ptr', 'set', 'sparse', 'tensor', 'variant' or 'vec' (found '>')"},
    {"generic type not closed",
     "func f() -> i64 { let b: Box<i64 = 1 }",
     "1:34: expected '>' (found '=')"},
    {"array size not closed",
     "func f() -> i64 { let a: i64[3 = 1 }",
     "1:32: expected ']' (found '=')"},
    {"a type that is not one",
     "func f() -> i64 { let a: 5 = 1 }",
     "1:26: expected 'array', 'buffer', 'closure', 'coro', 'either', 'func', 'grid', IDENT, 'map', 'mutarray', 'mutmap', 'mutset', 'optional', 'ptr', 'set', 'sparse', 'tensor', 'variant' or 'vec' (found '5')"},
    {"the renamed darray",
     "func f() -> i64 { let x: darray<i64> = 1 }",
     "1:26: expected 'array', 'buffer', 'closure', 'coro', 'either', 'func', 'grid', IDENT, 'map', 'mutarray', 'mutmap', 'mutset', 'optional', 'ptr', 'set', 'sparse', 'tensor', 'variant' or 'vec' (found 'darray') — darray was renamed: write mutarray<T> (changeable) or array<T> (unchangeable)"},
    {"the renamed hashmap",
     "func f() -> i64 { let x: hashmap<i64, i64> = 1 }",
     "1:26: expected 'array', 'buffer', 'closure', 'coro', 'either', 'func', 'grid', IDENT, 'map', 'mutarray', 'mutmap', 'mutset', 'optional', 'ptr', 'set', 'sparse', 'tensor', 'variant' or 'vec' (found 'hashmap') — hashmap was renamed: write mutmap<K,V> (changeable) or map<K,V> (unchangeable)"},
    {"method name missing",
     "func f() -> i64 { a. }",
     "1:22: expected IDENT (found '}')"},
    {"field name is a number",
     "func f() -> i64 { a.5 }",
     "1:21: expected IDENT (found '5')"},
    {"method call on a number name",
     "func f() -> i64 { a.5(1) }",
     "1:21: expected IDENT (found '5')"},
    {"empty index",
     "func f() -> i64 { a[] }",
     "1:21: expected 'array', '!', 'buffer', CHAR_LIT, 'closure', 'eitherErr', 'eitherOk', 'false', FLOAT_LIT, IDENT, 'if', INT_LIT, '[', '(', 'map', 'match', '-', 'mutarray', 'mutmap', 'mutset', 'None', 'set', 'Some', 'sparse', STRING_LIT, '~', 'true', 'val', 'variant' or 'vec' (found ']')"},
    {"index not closed",
     "func f() -> i64 { a[1 }",
     "1:23: expected ']' (found '}')"},
    {"call with a trailing comma",
     "func f() -> i64 { f(1, ) }",
     "1:24: expected 'array', '!', 'buffer', CHAR_LIT, 'closure', 'eitherErr', 'eitherOk', 'false', FLOAT_LIT, IDENT, 'if', INT_LIT, '[', '(', 'map', 'match', '-', 'mutarray', 'mutmap', 'mutset', 'None', 'set', 'Some', 'sparse', STRING_LIT, '~', 'true', 'val', 'variant' or 'vec' (found ')')"},
    {"call not closed",
     "func f() -> i64 { f(1 }",
     "1:23: expected ')' (found '}')"},
    {"struct literal field without a value",
     "func f() -> i64 { P { x: } }",
     "1:26: expected 'array', '!', 'buffer', CHAR_LIT, 'closure', 'eitherErr', 'eitherOk', 'false', FLOAT_LIT, IDENT, 'if', INT_LIT, '[', '(', 'map', 'match', '-', 'mutarray', 'mutmap', 'mutset', 'None', 'set', 'Some', 'sparse', STRING_LIT, '~', 'true', 'val', 'variant' or 'vec' (found '}')"},
    {"struct literal field without a colon",
     "func f() -> i64 { P { x 1 } }",
     "1:25: expected ':' (found '1')"},
    {"struct literal with a trailing comma",
     "func f() -> i64 { P { x: 1, } }",
     "1:29: expected IDENT (found '}')"},
    {"vec literal with a trailing comma",
     "func f() -> i64 { vec[1, ] }",
     "1:26: expected 'array', '!', 'buffer', CHAR_LIT, 'closure', 'eitherErr', 'eitherOk', 'false', FLOAT_LIT, IDENT, 'if', INT_LIT, '[', '(', 'map', 'match', '-', 'mutarray', 'mutmap', 'mutset', 'None', 'set', 'Some', 'sparse', STRING_LIT, '~', 'true', 'val', 'variant' or 'vec' (found ']')"},
    {"mutarray with an argument",
     "func f() -> i64 { mutarray<i64>(1) }",
     "1:33: expected ')' (found '1')"},
    {"buffer without a size",
     "func f() -> i64 { buffer<i64>() }",
     "1:29: expected ',' (found '>')"},
    {"variant with a name as the tag",
     "func f() -> i64 { variant<i64>(x, 1) }",
     "1:32: expected INT_LIT (found 'x')"},
    {"Some without a value",
     "func f() -> i64 { Some<i64>() }",
     "1:29: expected 'array', '!', 'buffer', CHAR_LIT, 'closure', 'eitherErr', 'eitherOk', 'false', FLOAT_LIT, IDENT, 'if', INT_LIT, '[', '(', 'map', 'match', '-', 'mutarray', 'mutmap', 'mutset', 'None', 'set', 'Some', 'sparse', STRING_LIT, '~', 'true', 'val', 'variant' or 'vec' (found ')')"},
    {"None with a value",
     "func f() -> i64 { None<i64>(1) }",
     "1:29: expected ')' (found '1')"},
    {"eitherOk with one type",
     "func f() -> i64 { eitherOk<i64>(1) }",
     "1:31: expected ',' (found '>')"},
    {"closure without a return type",
     "func f() -> i64 { closure(x: i64) { } }",
     "1:35: expected '->' (found '{')"},
    {"closure without a body",
     "func f() -> i64 { closure(x: i64) -> i64 }",
     "1:42: expected '{' (found '}')"},
    {"parenthesis not closed",
     "func f() -> i64 { (1 + 2 }",
     "1:26: expected ')' (found '}')"},
    {"empty parentheses",
     "func f() -> i64 { () }",
     "1:20: expected 'array', '!', 'buffer', CHAR_LIT, 'closure', 'eitherErr', 'eitherOk', 'false', FLOAT_LIT, IDENT, 'if', INT_LIT, '[', '(', 'map', 'match', '-', 'mutarray', 'mutmap', 'mutset', 'None', 'set', 'Some', 'sparse', STRING_LIT, '~', 'true', 'val', 'variant' or 'vec' (found ')')"},
    {"a dollar sign",
     "func f() -> i64 { return $ }",
     "1:26: unexpected character '$'"},
    {"a string with no end",
     "func f() -> i64 { let s: string = \"abc }",
     "1:35: unexpected character '\"'"},
    {"a character with two letters",
     "func f() -> i64 { let c: i8 = 'ab' }",
     "1:31: unexpected character '''"},
    {"an empty character",
     "func f() -> i64 { let c: i8 = '' }",
     "1:31: unexpected character '''"},
    {"a float with no digits after the dot",
     "func f() -> i64 { return 1. }",
     "1:29: expected IDENT (found '}')"},
    {"an annotation with no name",
     "@func",
     "1:2: expected IDENT (found 'func')"},
    {"a character the lexer does not know",
     "func f() -> i64 { return 1 $ 2 }",
     "1:28: unexpected character '$'"},
    {"a lone slash is an operator",
     "func f() -> i64 { return 1 / }",
     "1:30: expected 'array', '!', 'buffer', CHAR_LIT, 'closure', 'eitherErr', 'eitherOk', 'false', FLOAT_LIT, IDENT, 'if', INT_LIT, '[', '(', 'map', 'match', '-', 'mutarray', 'mutmap', 'mutset', 'None', 'set', 'Some', 'sparse', STRING_LIT, '~', 'true', 'val', 'variant' or 'vec' (found '}')"},
    {"a comment hides the rest",
     "func f() -> i64 { return 1 // }\n}",
     ""},
    {"an error on a later line",
     "func f() -> i64 {\n  let a: i64 = 1\n  let b: = 2\n}",
     "3:10: expected 'array', 'buffer', 'closure', 'coro', 'either', 'func', 'grid', IDENT, 'map', 'mutarray', 'mutmap', 'mutset', 'optional', 'ptr', 'set', 'sparse', 'tensor', 'variant' or 'vec' (found '=')"},
    {"an error after tabs",
     "func f() -> i64 {\n\treturn )\n}",
     "2:9: expected '}' (found ')')"},
    {"an error on the last line without a newline",
     "func f() -> i64 {\n  return 1\n",
     "3:1: expected '}' (found '')"},
    {"operators chain",
     "func f() -> i64 { return 1 + 2 * 3 - 4 << 1 & 5 ^ 6 | 7 == 8 && 9 || 10 }",
     ""},
    {"empty input",
     "",
     ""},
    {"only a comment",
     "// nothing here",
     ""},
  };

  struct RejectedFile {
    const char* path;      // under core/tests/programs/fixtures/diagnostics
    const char* message;
  };

  // The files the Nina front end rejects at parse time. (None of core/examples is one: all 122 of them parse.)
  const RejectedFile kRejected[] = {
    {"lib/broken.poly", "1:12: expected ')' (found '->')"},
    {"parse-entry.poly",
     "3:1: expected 'array', '!', 'buffer', CHAR_LIT, 'closure', 'eitherErr', 'eitherOk', 'false', FLOAT_LIT, IDENT, 'if', INT_LIT, '[', '(', 'map', 'match', '-', "
     "'mutarray', 'mutmap', 'mutset', 'None', 'set', 'Some', 'sparse', STRING_LIT, '~', 'true', 'val', 'variant' or 'vec' (found '}')"}};

  void fail(const String& name, const String& got, const String& expected) {
    failures++;
    cout << "FAIL " << name << "\n  got      " << got << "\n  expected " << expected << "\n";
  }

  void expectEqual(const String& name, const String& got, const String& expected) {
    if (got != expected) {
      fail(name, got, expected);
    }
  }

  String firstError(const ParseResult& result) {
    return result.errors.empty() ? String() : result.errors[0];
  }

  String readFile(const String& path) {
    IFStream in(path);
    StringStream buffer;
    buffer << in.rdbuf();

    return buffer.str();
  }

  Vector<String> linesOf(const String& path) {
    Vector<String> lines;
    IFStream in(path);

    for (String line; getline(in, line);) {
      if (!line.empty()) {
        lines.push_back(line);
      }
    }

    return lines;
  }

  void checkCases() {
    for (const Case& c : kCases) {
      expectEqual(c.name, firstError(ParseSource(c.source)), c.expected);
    }

    cout << "checked " << sizeof(kCases) / sizeof(kCases[0]) << " sources against the Nina messages\n";
  }

  void checkExample(const String& path) {
    const ParseResult result = ParseSource(readFile(path));

    if (!Ok(result)) {
      fail(path, firstError(result), "no syntax error");
      return;
    }

    const String tree = DumpProgram(result.program);

    if (tree.size() < 10 || tree != DumpProgram(result.program)) {
      fail(path, "an empty or unstable tree dump", "a tree");
    }
  }

  // polyir-check's build writes the example list (front_examples.txt, in the build directory the test runs in).
  void checkExamples() {
    const Vector<String> files = linesOf("front_examples.txt");

    for (const String& path : files) {
      checkExample(path);
    }

    cout << "parsed " << files.size() << " example programs\n";

    if (files.empty()) {
      fail("front_examples.txt", "no examples listed", "the programs of core/examples");
    }
  }

  void checkRejectedFiles() {
    for (const RejectedFile& file : kRejected) {
      const String path = String(POLYIR_SOURCE_DIR) + "/tests/programs/fixtures/diagnostics/" + file.path;
      expectEqual(file.path, firstError(ParseSource(readFile(path))), file.message);
    }
  }

  // ── the shape of the tree ───────────────────────────────────────────────────────────────────────────────────────────────────────────────

  const AstExpr& returnedExpr(const Program& program) {
    const AstFuncDecl& func = program.funcs.items[0];
    const AstStmt& ret = program.stmts[program.stmtRefs[func.body.stmts][0]];

    return program.exprs[ret.value];
  }

  // The tree's enums are compared as their numbers, so a mismatch prints the two numbers.
  void expectOperator(const String& what, Operator got, Operator want) {
    expectEqual(what, to_string(static_cast<int>(got)), to_string(static_cast<int>(want)));
  }

  void expectSuffixKind(const String& what, AstSuffixKind got, AstSuffixKind want) {
    expectEqual(what, to_string(static_cast<int>(got)), to_string(static_cast<int>(want)));
  }

  // `1 + 2 * 3 << 4` climbs: `+` binds looser than `*` and tighter than `<<`, all left associative.
  void checkPrecedence() {
    const ParseResult result = ParseSource("func f() -> i64 { return 1 + 2 * 3 << 4 - 5 }");
    const Program& p = result.program;
    const AstExpr& top = returnedExpr(p);

    expectOperator("top operator is <<", top.op, Operator::Shl);
    expectOperator("left of << is +", p.exprs[top.first].op, Operator::Add);
    expectOperator("right of << is -", p.exprs[top.second].op, Operator::Sub);
    expectEqual("operator position", to_string(top.opPos.column), "36");
  }

  // `a.b(c)[d]` is a Postfix: base `a`, suffixes method b, index.
  void checkPostfix() {
    const ParseResult result = ParseSource("func f() -> i64 { return a.b(c)[d].e }");
    const AstExpr& e = returnedExpr(result.program);
    const View<AstSuffix> suffixes = result.program.suffixes[e.suffixes];

    expectEqual("postfix suffix count", to_string(suffixes.size()), "3");
    expectSuffixKind("first suffix is a method", suffixes[0].kind, AstSuffixKind::Method);
    expectEqual("method name", String(TextOf(result.program, suffixes[0].name.name)), "b");
    expectSuffixKind("last suffix is a field", suffixes[2].kind, AstSuffixKind::Field);
  }

  const char* const kDumpSource = "struct P(x: i64) {\n  func get() -> i64 { return x }\n}\nglobal g: f64 = 2.5\n";
  const char* const kDumpExpected =
      "program\n"
      "  struct P @1:8 (x @1:10: i64) @1:1\n"
      "    func get @2:8() -> i64 @2:3\n"
      "      body @2:21\n"
      "        return @2:23\n"
      "          name x @2:30\n"
      "  global g @4:8: f64 = 2.5 @4:1\n";

  // The dump of a small program is stable text (positions included).
  void checkDump() {
    expectEqual("dump", DumpProgram(ParseSource(kDumpSource).program), kDumpExpected);
  }

  // Names are interned: two uses of one name are one Symbol, and the interner maps it back.
  void checkSymbols() {
    Interner names;
    const ParseResult result = ParseSource("func f() -> i64 { return f(f) }", names);
    const AstFuncDecl& func = result.program.funcs.items[0];
    const AstExpr& call = returnedExpr(result.program);

    expectEqual("one symbol for one name", to_string(func.name.name.id), to_string(call.name.name.id));
    expectEqual("symbol text", String(TextOf(names, func.name.name)), "f");
  }

  void runAll() {
    checkCases();
    checkExamples();
    checkRejectedFiles();

    checkPrecedence();
    checkPostfix();
    checkDump();
    checkSymbols();
  }

}  // namespace

int main() {
  runAll();
  cout << (failures == 0 ? "front parse tests passed" : "front parse tests FAILED") << "\n";

  return failures == 0 ? 0 : 1;
}
