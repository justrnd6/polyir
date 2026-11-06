// What translate asks of a source language's tree (docs/translate.md). translate knows no AST: a source passes in its canon
// (a Schema) and its tree, through these functions, which the source fills in over its own AST or parse tree. A node is a NodeRef,
// whatever the source makes it (an index, an address); translate only hands it back.
#pragma once

#include "polyir/translate/Schema.hpp"
#include "polyir/translate/Tree.hpp"

namespace polyir::translate {

  struct NodeRef {
    uint64_t id = 0;
  };

  struct SourceTree {
    const Schema* schema = nullptr;                          // the source's canon: what its tree can hold
    NodeRef root;                                            // the whole program
    Function<ConstructId(NodeRef)> constructOf;              // which construct a node is (kNoId: none of the schema's)
    Function<bool(NodeRef, PartId)> hasPart;                 // whether the node has the part (a fact: whether it holds)
    Function<String(NodeRef, PartId)> textOf;                // a name, an operator, a literal's text, a fact's text
    Function<size_t(NodeRef, PartId)> countOf;               // how many nodes a part holds
    Function<NodeRef(NodeRef, PartId, size_t)> itemOf;       // the part's node at that place
    Function<SourceSpan(NodeRef)> spanOf{};                  // where a node came from, for a source that knows (optional)
  };

}  // namespace polyir::translate
