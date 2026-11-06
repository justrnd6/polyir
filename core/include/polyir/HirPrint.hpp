// Compact text for the HIR, the same notation the Nina implementation prints (`polyir --emit-hir`): types, expressions, statements and
// declarations as plain strings, so a program built here can be compared with what the checker produced.
//
//   `x` a local, `x@` a `var` slot read, `@x` a global, `&f` a function used as a value, `(Add:i64 a b)` an operator with its result type,
//   `f:i64(a,b)` a call with its result type, `(cast:i32 v)`, `s.#2` field number 2, `?what?` a construct the checker has not translated.
#pragma once

#include "polyir/Std.hpp"
#include "polyir/Export.hpp"
#include "polyir/Hir.hpp"

#include <string>

namespace polyir {

  POLYIR_API String ExprText(const Expr& expr);

  POLYIR_API String StmtText(const Stmt& stmt);

  POLYIR_API String BlockText(const StmtList& body);

  POLYIR_API String ModuleText(const Module& module);  // one declaration per line: structs, interfaces, host types, externs, globals, functions

}  // namespace polyir
