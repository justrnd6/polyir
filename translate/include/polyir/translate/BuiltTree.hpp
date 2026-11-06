// A tree of Nodes (Tree.hpp) a source builds itself, as a SourceTree (Source.hpp); and any SourceTree read into the Nodes the
// engine writes. polyir's source builds its tree this way, since it lifts the HIR into .poly-level forms; a source that reads
// its own AST as it is gives a SourceTree of its own instead.
#pragma once

#include "polyir/translate/Tree.hpp"
#include "polyir/translate/Source.hpp"

namespace polyir::translate {

  // A source's tree read through its schema, and what was wrong in it: a node that is none of the schema's constructs.
  struct ReadTree {
    Node root;
    Vector<String> problems;
  };

  // A built tree as a SourceTree: each NodeRef is the address of a node of `root`, which must outlive the tree.
  POLYIR_API SourceTree TreeOver(const Node& root, const Schema& schema);

  POLYIR_API ReadTree ReadSourceTree(const SourceTree& tree);

  // A built tree made, in place, what reading it through its schema makes. Each node's parts take the schema's kinds, and those
  // the schema lacks are left out. A lower form stays only where its construct has one. Parts keep the order they were built in.
  POLYIR_API Vector<String> FitToSchema(Node& root, const Schema& schema);

}  // namespace polyir::translate
