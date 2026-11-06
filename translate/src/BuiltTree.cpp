// A built tree as a SourceTree, and a SourceTree read into the engine's Nodes (BuiltTree.hpp). The reading asks the tree only
// what its Source.hpp functions answer: each node's construct, then each part its schema says the construct may have.
#include "polyir/translate/BuiltTree.hpp"

namespace polyir::translate {

  namespace {

    const Node& nodeAt(NodeRef ref) {
      return *reinterpret_cast<const Node*>(static_cast<uintptr_t>(ref.id));
    }

    NodeRef refOf(const Node& node) {
      return NodeRef{static_cast<uint64_t>(reinterpret_cast<uintptr_t>(&node))};
    }

    bool isLowered(const Schema& schema, PartId part) {
      return schema.parts[static_cast<size_t>(part)] == kLoweredPart;
    }

    const NodePart* partAt(const Schema& schema, NodeRef ref, PartId part) {
      return PartNamed(nodeAt(ref), schema.parts[static_cast<size_t>(part)]);
    }

    // The nodes a part holds; a construct's lower form is held by the node itself.
    const Vector<Node>* itemsAt(const Schema& schema, NodeRef ref, PartId part) {
      const NodePart* found = isLowered(schema, part) ? nullptr : partAt(schema, ref, part);

      return isLowered(schema, part) ? &nodeAt(ref).lowered : found != nullptr ? &found->nodes : nullptr;
    }

    bool hasPartAt(const Schema& schema, NodeRef ref, PartId part) {
      return isLowered(schema, part) ? !nodeAt(ref).lowered.empty() : partAt(schema, ref, part) != nullptr;
    }

    String textAt(const Schema& schema, NodeRef ref, PartId part) {
      const NodePart* found = isLowered(schema, part) ? nullptr : partAt(schema, ref, part);

      return found != nullptr ? found->text : String();
    }

    // ── reading a tree ──

    struct Reader {
      const SourceTree& tree;
      Vector<String> problems;
    };

    Node readNode(Reader& reader, NodeRef ref);

    Vector<Node> readItems(Reader& reader, NodeRef ref, PartId part) {
      Vector<Node> nodes;
      const size_t count = reader.tree.countOf(ref, part);

      for (size_t i = 0; i < count; i++) {
        nodes.push_back(readNode(reader, reader.tree.itemOf(ref, part, i)));
      }

      return nodes;
    }

    // A name or a text is text, a fact text unless it holds nodes, and every other part its nodes.
    void readPart(Reader& reader, NodeRef ref, const PartOf& part, Node& node) {
      const String& name = reader.tree.schema->parts[static_cast<size_t>(part.part)];
      const bool fact = part.kind == PartKind::Fact && reader.tree.countOf(ref, part.part) == 0;
      const bool textual = part.kind == PartKind::Name || part.kind == PartKind::Text || fact;

      if (name == kLoweredPart) {
        node.lowered = readItems(reader, ref, part.part);
      } else if (textual) {
        AddText(node, name, part.kind, reader.tree.textOf(ref, part.part));
      } else {
        AddNodes(node, name, part.kind, readItems(reader, ref, part.part));
      }
    }

    Node readNode(Reader& reader, NodeRef ref) {
      const Schema& schema = *reader.tree.schema;
      const ConstructId construct = reader.tree.constructOf(ref);

      if (construct == kNoId) {
        reader.problems.push_back("the tree holds a node of no form in " + schema.name + "'s canon");
        return MakeNode("?");
      }

      Node node = MakeNode(schema.constructs[static_cast<size_t>(construct)]);
      node.isType = schema.isType[static_cast<size_t>(construct)];
      node.at = reader.tree.spanOf ? reader.tree.spanOf(ref) : SourceSpan{};

      for (const PartOf& part : schema.partsOf[static_cast<size_t>(construct)]) {
        if (reader.tree.hasPart(ref, part.part)) {
          readPart(reader, ref, part, node);
        }
      }

      return node;
    }

  }  // namespace

  namespace {

    // The schema's part of the construct with this name, or null when it has none.
    const PartOf* schemaPart(const Schema& schema, ConstructId construct, const String& name) {
      for (const PartOf& part : schema.partsOf[static_cast<size_t>(construct)]) {
        if (schema.parts[static_cast<size_t>(part.part)] == name) {
          return &part;
        }
      }

      return nullptr;
    }

    void fitNode(const Schema& schema, Node& node, Vector<String>& problems);

    void fitAll(const Schema& schema, Vector<Node>& nodes, Vector<String>& problems) {
      for (Node& node : nodes) {
        fitNode(schema, node, problems);
      }
    }

    // A part as the schema's kind has it. A name or a text is text, a fact text unless it holds nodes, any other part its nodes.
    void fitPart(NodePart& part, PartKind kind) {
      const bool textual = kind == PartKind::Name || kind == PartKind::Text || (kind == PartKind::Fact && part.nodes.empty());

      part.kind = kind;
      (textual ? part.nodes.clear() : part.text.clear());
    }

    // The node's parts the schema has, each of its kind; a part it lacks is left out.
    void fitParts(const Schema& schema, ConstructId construct, Node& node, Vector<String>& problems) {
      for (size_t i = 0; i < node.parts.size();) {
        const PartOf* known = schemaPart(schema, construct, node.parts[i].name);

        if (known == nullptr || schema.parts[static_cast<size_t>(known->part)] == kLoweredPart) {
          node.parts.erase(node.parts.begin() + static_cast<long>(i));
          continue;
        }

        if (known->kind != node.parts[i].kind) {
          fitPart(node.parts[i], known->kind);
        }

        fitAll(schema, node.parts[i++].nodes, problems);
      }
    }

    void fitNode(const Schema& schema, Node& node, Vector<String>& problems) {
      const ConstructId construct = ConstructNamed(schema, node.construct, node.isType);

      if (construct == kNoId) {
        problems.push_back("the tree holds a node of no form in " + schema.name + "'s canon");
        node = MakeNode("?");
        return;
      }

      fitParts(schema, construct, node, problems);

      if (!node.lowered.empty()) {
        (HasLowerForm(schema, node.construct) && !node.isType ? fitAll(schema, node.lowered, problems) : node.lowered.clear());
      }
    }

  }  // namespace

  Vector<String> FitToSchema(Node& root, const Schema& schema) {
    Vector<String> problems;
    fitNode(schema, root, problems);

    return problems;
  }

  SourceTree TreeOver(const Node& root, const Schema& schema) {
    SourceTree tree;
    tree.schema = &schema;
    tree.root = refOf(root);
    tree.constructOf = [&schema](NodeRef ref) { return ConstructNamed(schema, nodeAt(ref).construct, nodeAt(ref).isType); };
    tree.hasPart = [&schema](NodeRef ref, PartId part) { return hasPartAt(schema, ref, part); };

    tree.textOf = [&schema](NodeRef ref, PartId part) { return textAt(schema, ref, part); };
    tree.countOf = [&schema](NodeRef ref, PartId part) { return itemsAt(schema, ref, part) ? itemsAt(schema, ref, part)->size() : 0; };
    tree.itemOf = [&schema](NodeRef ref, PartId part, size_t i) { return refOf((*itemsAt(schema, ref, part))[i]); };

    return tree;
  }

  ReadTree ReadSourceTree(const SourceTree& tree) {
    Reader reader{tree, {}};
    Node root = readNode(reader, tree.root);

    return ReadTree{move(root), move(reader.problems)};
  }

}  // namespace polyir::translate
