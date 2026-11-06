// How a source's tree reader stands to each form of its canon (core/docs/translate.md). A source's coverage table names every
// form of its schema once: produced (its trees have it), todo, unimplemented (understood, not made yet) or unsupported (the
// source has nothing it could be). A test of the source reads its trees and checks each node is a produced form.
#pragma once

#include "polyir/translate/Schema.hpp"
#include "polyir/translate/Tree.hpp"

namespace polyir::translate {

  enum class Coverage { Produced, Todo, Unimplemented, Unsupported };

  // One form, by its name (`if(cond, then)`, a type's `struct(name) (a type)`), and how the reader stands to it.
  struct FormCoverage {
    String form;
    Coverage status = Coverage::Produced;
    String why{};  // why it is not produced
  };

  POLYIR_API String CoverageName(const FormSpec& form);  // `if(cond, then)`; a type's `struct(name) (a type)`

  // The forms a coverage table names twice or leaves out, and the names in it that are none of the schema's forms.
  POLYIR_API Vector<String> CoverageProblems(const Schema& schema, const Vector<FormCoverage>& table);

  // The form a node is, or null. The node has each of the form's parts, but a type or a fact it may leave out. It has no part
  // the form lacks, but its construct's facts and a typed form's `type`. Of two forms, it is the one with fewer left out.
  POLYIR_API const FormSpec* FormOfNode(const Schema& schema, const Node& node);

}  // namespace polyir::translate
