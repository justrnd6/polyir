// Pages and canons compiled ahead into C++ (core/docs/translate.md). polyir-translatec checks a page by its source's
// canon: a problem in it, or a form it does not mention, stops the build. It then writes a function that makes the page's
// rules (Rules.hpp) without reading its text. For a `.canon` file it writes one that makes the schema.
#pragma once

#include "polyir/translate/Page.hpp"

namespace polyir::translate {

  // What stops a page from being compiled: its problems, and each form of its source it has no rule for.
  POLYIR_API Vector<String> CompileProblems(const Target& page);

  // C++ defining `const TranslateRules& <function>()`, which makes the rules as they are.
  POLYIR_API String CompiledRulesText(const TranslateRules& rules, const String& function, const String& from);

  // C++ defining `const Schema& <function>()`, which makes the schema as it is.
  POLYIR_API String CompiledSchemaText(const Schema& schema, const String& function, const String& from);

}  // namespace polyir::translate
