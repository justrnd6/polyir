// A checked program written in another language by that language's construct rules (docs/translate.md). A target is one
// `.translate` page: a rule per polyir construct, `construct(parts) := template`, applied to the program's constructs. polyir
// is one source of the translate library (polyir/translate/Page.hpp); this is that library with polyir's checked HIR as the
// tree and the pages built into libpolyir. polyir only writes the text; building or running it is the target's own job.
//
//   const Optional<Target> js = FindTarget("js", "");
//   const Translation out = Translate(module, *js);
//   if (out.problems.empty()) { cout << out.text; }
#pragma once

#include "polyir/Std.hpp"
#include "polyir/Export.hpp"
#include "polyir/Hir.hpp"
#include "polyir/translate/Page.hpp"
#include "polyir/translate/Coverage.hpp"

namespace polyir {

  using translate::Schema;
  using translate::Target;
  using translate::Translation;
  using translate::RuleCounts;
  using translate::NewTargetText;
  using translate::UpdatedTargetText;
  using translate::MissingConstructs;
  using translate::TargetProblems;
  using translate::CountRules;
  using translate::TargetExtension;
  using translate::SchemaOf;
  using translate::PageSettings;
  using translate::PageSettingsOf;
  using translate::Node;
  using translate::FormCoverage;
  using translate::SourceSpan;
  using translate::SpanMapping;
  using translate::SourceMapText;
  using translate::SourceMapLink;

  POLYIR_API Vector<Target> BuiltInTargets();  // the targets built into the library, from targets/<lang>/<Lang>.translate

  POLYIR_API const Schema& PolyirSchema();  // polyir's canon (targets/Polyir.canon): what every built-in page writes

  POLYIR_API Optional<Schema> FindSchema(const String& source);  // canons built in (`polyir`), or a .canon file's path

  // A .translate page on disk and the files under its runtime/ folder; it starts from a page beside it, else a built-in one.
  POLYIR_API Optional<Target> ReadTarget(const String& path);

  // The target named `name`. It comes from `rulesPath` when that is given: a page, or a folder holding <lang>/<Lang>.translate.
  // Otherwise it comes from the folder POLYIR_TARGETS names, or else from the built-in targets.
  POLYIR_API Optional<Target> FindTarget(const String& name, const String& rulesPath);

  POLYIR_API Translation Translate(const Module& module, const Target& target);

  // The module as a tree of polyir's forms, made as the target's page asks (its style): what Translate writes.
  POLYIR_API Node TreeOf(const Module& module, const Target& target);

  // How polyir's source stands to each form of its canon: every form of PolyirSchema() named once (Coverage.cpp).
  POLYIR_API const Vector<FormCoverage>& PolyirCoverage();

}  // namespace polyir
