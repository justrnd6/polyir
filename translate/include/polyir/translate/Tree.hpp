// The tree the engine writes: each node is one construct of a source's canon (Schema.hpp) with its parts. A source reads
// its own tree into these nodes (BuiltTree.hpp), or builds them itself with the functions below.
#pragma once

#include "polyir/translate/Constructs.hpp"

namespace polyir::translate {

  struct Node;

  // One part of a node: text (a name, an operator, a literal's spelling) or the nodes that fill it. A fact's text is `true`, or
  // its nodes are the fact (`annotations`).
  struct NodePart {
    String name;
    PartKind kind = PartKind::Value;
    String text;
    Vector<Node> nodes;
  };

  // Where a node came from in its source's own text, when the source knows: the file, the line and the column (from 1), and
  // the line's text when the source gives it (a page's `{code}`). A line of 0 is no span.
  struct SourceSpan {
    String file;
    int line = 0;
    int column = 0;
    String code;
  };

  // A construct and its parts. `note` is the source's own (polyir keeps an expression's checked type there while it builds the
  // tree): translate never reads it.
  struct Node {
    String construct;
    bool isType = false;
    Vector<NodePart> parts;
    SharedPtr<const void> note;
    Vector<Node> lowered;  // the construct's lower form, written by a `lower` rule
    SourceSpan at{};       // where it came from, when its source gives spans and the translation asks for them
  };

  POLYIR_API Node MakeNode(const String& construct);

  POLYIR_API void AddText(Node& node, const String& name, PartKind kind, const String& text);

  POLYIR_API void AddNode(Node& node, const String& name, PartKind kind, Node part);

  POLYIR_API void AddNodes(Node& node, const String& name, PartKind kind, Vector<Node> parts);

  POLYIR_API void AddFact(Node& node, const String& name);

  POLYIR_API const NodePart* PartNamed(const Node& node, const String& name);

  // A name is its whole path (`Geometry.Point`): these give its last step and the path before it.

  POLYIR_API String OwnName(const String& name);  // `Point` for `Geometry.Point`

  POLYIR_API String ScopeOf(const String& name);  // the module or struct a declaration sits in: `Geometry` for `Geometry.Point`

}  // namespace polyir::translate
