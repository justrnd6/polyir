// Making nodes and reading their parts (polyir/translate/Tree.hpp).
#include "polyir/translate/Tree.hpp"

namespace polyir::translate {

  Node MakeNode(const String& construct) {
    Node node;
    node.construct = construct;

    return node;
  }

  void AddText(Node& node, const String& name, PartKind kind, const String& text) {
    NodePart part;
    part.name = name;
    part.kind = kind;
    part.text = text;
    node.parts.push_back(move(part));
  }

  // The one node is moved in: an initializer list of it would copy it, and everything it holds.
  void AddNode(Node& node, const String& name, PartKind kind, Node part) {
    Vector<Node> parts;
    parts.push_back(move(part));
    AddNodes(node, name, kind, move(parts));
  }

  void AddNodes(Node& node, const String& name, PartKind kind, Vector<Node> parts) {
    NodePart part;
    part.name = name;
    part.kind = kind;
    part.nodes = move(parts);
    node.parts.push_back(move(part));
  }

  void AddFact(Node& node, const String& name) {
    AddText(node, name, PartKind::Fact, "true");
  }

  const NodePart* PartNamed(const Node& node, const String& name) {
    for (const NodePart& part : node.parts) {
      if (part.name == name) {
        return &part;
      }
    }

    return nullptr;
  }

  String OwnName(const String& name) {
    const size_t dot = name.rfind('.');

    return dot == String::npos ? name : name.substr(dot + 1);
  }

  String ScopeOf(const String& name) {
    const size_t dot = name.rfind('.');

    return dot == String::npos ? "" : name.substr(0, dot);
  }

}  // namespace polyir::translate
