// What the engine knows of pages beyond Page.hpp and Rules.hpp: a page read with its base, where its source's canon comes
// from, and what is wrong in it (Pages.cpp, PageChecks.cpp).
#pragma once

#include "polyir/translate/Page.hpp"

namespace polyir::translate {

  // A page's rules after those of its base, and the support files of both (Pages.cpp).
  struct Page {
    TranslateRules rules;
    Map<String, String> files;
    Vector<String> names;  // the target's name, then its base's, as host bindings name them (`ts` uses `js` bindings)
  };

  SharedPtr<const Page> PageOf(const Target& target);  // read once for each page's text (and support files), then kept

  // The canon of `source` for a page: a `.canon` file beside it, else what its `findSchema` finds.
  Optional<Schema> SchemaFor(const Target& target, const String& source);

  // What is wrong in a page, read with its base as `rules`, by its source's canon.
  Vector<String> PageProblems(const TranslateRules& rules, const Schema& schema);

  // The extension of a program's file: the style's `library` one for a file with no `main` of its own. Else it is the style's
  // `extension`, else the target's name.
  String FileExtension(const Style& style, const String& name, bool library);

}  // namespace polyir::translate
