// A source language's canon as a schema (core/docs/translate.md): every construct its trees can hold, in every form it
// comes in, read from the source's `.canon` file (polyir's is targets/Polyir.canon). It also gives each construct's facts, the
// constructs with a lower form, the families a page's patterns may name, and the parts that hold declarations. translate reads
// a source's tree through it.
#pragma once

#include "polyir/translate/Constructs.hpp"

namespace polyir::translate {

  using ConstructId = int32_t;  // a construct of the schema, by its place
  using PartId = int32_t;       // a part's name, by its place

  constexpr int32_t kNoId = -1;

  // A part a construct may have: its name's id and its kind.
  struct PartOf {
    PartId part = kNoId;
    PartKind kind = PartKind::Value;
  };

  struct Schema {
    String name;                            // `source: polyir`: the language whose trees it describes
    String file;                            // where it was read from, for problems
    Vector<FormSpec> forms;                 // every form, in the order a new page lists them
    Vector<FormSpec> factNodes;             // `## fact nodes`: the nodes a fact holds, which a page may write but needs no rule of
    Map<String, Vector<String>> facts;      // each code construct's facts (`isKey`), in their order
    Map<String, Vector<String>> typeFacts;  // each type's facts (`arity`)
    Set<String> lowered;                    // the constructs a source also gives as their lower form
    Map<String, Set<String>> families;      // a pattern word and the types it stands for (`int`: `i8` … `u64`)
    Set<String> typed;                      // `typed:` the sections whose forms also have their value's `type`
    Set<String> declarations;               // `declarations:` the parts whose items are declarations, set apart by blank lines
    Vector<String> constructs;              // each construct's name, by its id
    Vector<bool> isType;                    // whether each construct is a type
    Vector<Vector<PartOf>> partsOf;         // the parts each construct may have
    Vector<String> parts;                   // each part's name, by its id
    Map<String, ConstructId> codeIds;       // each code construct's id, by its name
    Map<String, ConstructId> typeIds;       // each type's id, by its name
    Map<String, PartId> partIds;            // each part's id, by its name
    Vector<String> problems;                // `file:line: what is wrong`
  };

  // The part a construct's lower form is held in: a source that has one for a construct gives it there.
  inline const char* const kLoweredPart = "lowered";

  POLYIR_API Schema ReadSchema(const String& text, const String& file);  // a `.canon` file's text

  POLYIR_API Vector<const FormSpec*> FormsOf(const Schema& schema, const String& construct, bool isType);  // its fact nodes' too

  POLYIR_API Vector<String> FactsOf(const Schema& schema, const String& construct, bool isType);

  POLYIR_API bool HasLowerForm(const Schema& schema, const String& construct);

  POLYIR_API bool IsTyped(const Schema& schema, const FormSpec& form);  // its section is `typed:`: it has its value's `type`

  POLYIR_API const Set<String>* FamilyNamed(const Schema& schema, const String& word);  // null when no family has the name

  // Every place a construct holds a block, as `construct.part` (`func.body`, `if.then`): where a `block` rule may be for.
  POLYIR_API Vector<String> BlockContexts(const Schema& schema);

  POLYIR_API ConstructId ConstructNamed(const Schema& schema, const String& name, bool isType);  // kNoId when the schema has none

  POLYIR_API PartId PartIdOf(const Schema& schema, const String& name);  // kNoId when no construct has a part of that name

}  // namespace polyir::translate
