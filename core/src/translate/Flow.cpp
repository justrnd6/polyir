// A function body with labels (`label loop(a: i64):`, `goto`, `branch`, `switch`) as constructs every target can write: a state
// machine. Each block is one `case` of a `machine`; `jump` says which block runs next. A label's parameters are variables, and
// a `goto` sets them before it jumps. The state is the variable `label`, a .poly keyword, so no program names one like it.
#include "translate/Nodes.hpp"

namespace polyir::translate {

  namespace {

    const char* const kState = "label";

    // A block: its number, its label's parameters, and its statements up to the next label.
    struct Block {
      String label;
      Vector<String> params;
      Vector<TypePtr> types;
      StmtList body;
    };

    // The body cut at its labels; the statements before the first label are block 0.
    Vector<Block> blocksOf(const StmtList& body) {
      Vector<Block> blocks(1);

      for (const StmtPtr& s : body) {
        if (s->kind == StmtKind::Label) {
          blocks.push_back(Block{s->name, s->parameterNames, s->parameterTypes, {}});
        } else {
          blocks.back().body.push_back(s);
        }
      }

      return blocks;
    }

    int numberOf(const Vector<Block>& blocks, const String& label) {
      for (size_t i = 1; i < blocks.size(); i++) {
        if (blocks[i].label == label) {
          return static_cast<int>(i);
        }
      }

      return -1;
    }

    Node typedName(const String& name, const TypePtr& type) {
      Node node = MakeNode("name");
      AddText(node, "name", PartKind::Name, name);
      node.note = type;

      return node;
    }

    // A state's number: an `int` of type `i64`.
    Node intNode(int64_t value) {
      Node node = MakeNode("int");
      Node type = MakeNode("i64");
      type.isType = true;
      type.note = MakeInt(64);

      AddText(node, "value", PartKind::Text, to_string(value));
      AddNode(node, "type", PartKind::Type, type);
      node.note = MakeInt(64);

      return node;
    }

    // `var name: type` (with `value` when there is one).
    Node varNode(NodeContext& context, const String& name, const TypePtr& type, Optional<Node> value) {
      Node node = MakeNode("var");
      AddText(node, "name", PartKind::Name, name);
      AddNode(node, "type", PartKind::Type, TypeNode(context, type));

      if (value) {
        AddNode(node, "value", PartKind::Value, move(*value));
      }

      return node;
    }

    Node assignNode(const String& name, const TypePtr& type, Node value) {
      Node node = MakeNode("assign");
      AddNode(node, "target", PartKind::Value, typedName(name, type));
      AddNode(node, "value", PartKind::Value, move(value));

      return node;
    }

    // Whether argument `i` is parameter `i` itself, which is then left as it is.
    bool isSame(const Vector<ExprPtr>& args, const Block& target, size_t i) {
      return Reads(*args[i], target.params[i]);
    }

    // Whether setting the parameters in order would change one that a later argument still reads: then they go through `next_`.
    bool needsNext(const Vector<ExprPtr>& args, const Block& target) {
      for (size_t later = 1; later < args.size() && later < target.params.size(); later++) {
        for (size_t set = 0; set < later; set++) {
          if (!isSame(args, target, set) && !isSame(args, target, later) && CountInExpr(args[later], ReadsName(target.params[set])) > 0) {
            return true;
          }
        }
      }

      return false;
    }

    // The arguments as the label's parameters, in order, or through `next_` variables when an argument reads one set before it.
    void setParams(NodeContext& context, Vector<Node>& nodes, const Vector<ExprPtr>& args, const Block& target) {
      const bool throughNext = needsNext(args, target);
      const size_t count = min(args.size(), target.params.size());

      for (size_t i = 0; i < count; i++) {
        if (isSame(args, target, i)) {
          continue;
        }

        Node value = ValueFor(context, args[i], target.types[i]);
        const String& param = target.params[i];
        nodes.push_back(throughNext ? varNode(context, "next_" + param, target.types[i], move(value)) : assignNode(param, target.types[i], move(value)));
      }

      for (size_t i = 0; throughNext && i < count; i++) {
        if (!isSame(args, target, i)) {
          nodes.push_back(assignNode(target.params[i], target.types[i], typedName("next_" + target.params[i], target.types[i])));
        }
      }
    }

    // `goto label(args)`: the parameters set, then `jump(state, number)`.
    Vector<Node> jumpNodes(NodeContext& context, const Vector<Block>& blocks, const String& label, const Vector<ExprPtr>& args) {
      Vector<Node> nodes;
      const int number = numberOf(blocks, label);

      if (number < 0) {
        Problem(context, "a goto to the unknown label " + label);

        return nodes;
      }

      setParams(context, nodes, args, blocks[static_cast<size_t>(number)]);
      Node jump = MakeNode("jump");
      AddText(jump, "state", PartKind::Name, kState);
      AddText(jump, "number", PartKind::Text, to_string(number));
      nodes.push_back(jump);

      return nodes;
    }

    // `if(cond, then, else)`, or `if(cond, then, elseIf)` when the else is only another `if` (a switch's next case).
    Node ifNode(Node cond, Vector<Node> then, Vector<Node> otherwise) {
      Node node = MakeNode("if");
      AddNode(node, "cond", PartKind::Value, move(cond));
      AddNodes(node, "then", PartKind::Block, move(then));
      Node lowered = node;
      AddNodes(lowered, "else", PartKind::Block, otherwise);

      if (otherwise.size() != 1 || otherwise[0].construct != "if") {
        return lowered;
      }

      AddNode(node, "elseIf", PartKind::Statement, move(otherwise[0]));
      node.lowered.push_back(move(lowered));

      return node;
    }

    // `branch c ? a(x) : b(y)` as `if (c) { jump a } else { jump b }`.
    Node branchNode(NodeContext& context, const Vector<Block>& blocks, const Stmt& s) {
      Vector<Node> then = jumpNodes(context, blocks, s.name, s.arguments);
      Vector<Node> otherwise = jumpNodes(context, blocks, s.elseName, s.elseArguments);

      return ifNode(ExpressionNode(context, *s.value), move(then), move(otherwise));
    }

    // `switch (v) { 1 => a() else => b() }` as an `if` for each case, tested in order, the default last.
    Vector<Node> switchNodes(NodeContext& context, const Vector<Block>& blocks, const Stmt& s, size_t from) {
      if (from == s.caseValues.size()) {
        return jumpNodes(context, blocks, s.name, s.arguments);
      }

      Node test = MakeNode("binary");
      AddNode(test, "left", PartKind::Value, ExpressionNode(context, *s.value));
      AddText(test, "op", PartKind::Text, "==");
      AddNode(test, "right", PartKind::Value, intNode(s.caseValues[from]));
      test.note = MakeBool();

      const Vector<ExprPtr> args = from < s.caseArguments.size() ? s.caseArguments[from] : Vector<ExprPtr>{};
      Vector<Node> jump = jumpNodes(context, blocks, s.caseLabels[from], args);

      return {ifNode(move(test), move(jump), switchNodes(context, blocks, s, from + 1))};
    }

    bool isJump(const Stmt& s) {
      return s.kind == StmtKind::Goto || s.kind == StmtKind::Branch || s.kind == StmtKind::Switch;
    }

    bool endsBlock(const StmtList& body) {
      const bool ends = !body.empty() && (isJump(*body.back()) || body.back()->kind == StmtKind::Return || body.back()->kind == StmtKind::Throw);

      return ends;
    }

    void append(Vector<Node>& nodes, const Vector<Node>& more) {
      nodes.insert(nodes.end(), more.begin(), more.end());
    }

    void addJump(NodeContext& context, Vector<Node>& nodes, const Vector<Block>& blocks, const Stmt& s) {
      if (s.kind == StmtKind::Goto) {
        append(nodes, jumpNodes(context, blocks, s.name, s.arguments));
      } else if (s.kind == StmtKind::Branch) {
        nodes.push_back(branchNode(context, blocks, s));
      } else {
        append(nodes, switchNodes(context, blocks, s, 0));
      }
    }

    // The plain statements as any block's, each jump as `jump`.
    Vector<Node> statementsAndJumps(NodeContext& context, const Vector<Block>& blocks, const StmtList& body) {
      Vector<Node> nodes;
      StmtList plain;

      for (const StmtPtr& s : body) {
        if (!isJump(*s)) {
          plain.push_back(s);
          continue;
        }

        append(nodes, StatementNodes(context, plain));
        plain.clear();
        addJump(context, nodes, blocks, *s);
      }

      append(nodes, StatementNodes(context, plain));

      return nodes;
    }

    // A block's statements; a block that runs off its end goes on to the next, and the last one returns, so every case ends.
    Vector<Node> blockBody(NodeContext& context, const Vector<Block>& blocks, size_t number) {
      Vector<Node> nodes = statementsAndJumps(context, blocks, blocks[number].body);

      if (!endsBlock(blocks[number].body) && number + 1 < blocks.size()) {
        append(nodes, jumpNodes(context, blocks, blocks[number + 1].label, {}));
      } else if (!endsBlock(blocks[number].body)) {
        nodes.push_back(MakeNode("return"));
      }

      return nodes;
    }

    // Each label parameter once, as a variable; the same name with another type is a problem.
    Vector<Node> paramVars(NodeContext& context, const Vector<Block>& blocks) {
      Map<String, TypePtr> declared;
      Vector<Node> nodes;

      for (const Block& block : blocks) {
        for (size_t i = 0; i < block.params.size(); i++) {
          const auto seen = declared.find(block.params[i]);

          if (seen == declared.end()) {
            declared[block.params[i]] = block.types[i];
            nodes.push_back(varNode(context, block.params[i], block.types[i], nullopt));
          } else if (!SameType(seen->second, block.types[i])) {
            Problem(context, "labels give `" + block.params[i] + "` two types");
          }
        }
      }

      return nodes;
    }

    // `case(state, number, body)` for each block.
    Vector<Node> caseNodes(NodeContext& context, const Vector<Block>& blocks) {
      Vector<Node> cases;

      for (size_t i = 0; i < blocks.size(); i++) {
        Node node = MakeNode("case");
        AddText(node, "state", PartKind::Name, kState);
        AddText(node, "number", PartKind::Text, to_string(i));
        AddNodes(node, "body", PartKind::Block, blockBody(context, blocks, i));
        cases.push_back(node);
      }

      return cases;
    }

    // ── variables two blocks share ──

    bool readElsewhere(const Vector<Block>& blocks, size_t own, const String& name) {
      for (size_t i = 0; i < blocks.size(); i++) {
        if (i != own && CountIn(blocks[i].body, ReadsName(name)) > 0) {
          return true;
        }
      }

      return false;
    }

    // `let x = v` as `x = v`, for a variable declared with the label parameters; a `var` with no value is then nothing.
    StmtList setInstead(const Stmt& s) {
      if (!s.value) {
        return {};
      }

      auto target = make_shared<Expr>();
      target->kind = ExprKind::Local;
      target->name = s.name;
      target->type = s.type;

      auto assign = make_shared<Stmt>(s);
      assign->kind = StmtKind::Assign;
      assign->target = target;

      return {assign};
    }

    // The variables a block declares and another block reads, by name. Each case is a block of its own, so these are declared
    // before the machine, with the label parameters, and set where they were declared.
    Map<String, TypePtr> shareVariables(Vector<Block>& blocks) {
      Map<String, TypePtr> shared;

      for (size_t i = 0; i < blocks.size(); i++) {
        StmtList body;

        for (const StmtPtr& s : blocks[i].body) {
          const bool moves = (s->kind == StmtKind::Let || s->kind == StmtKind::Var) && readElsewhere(blocks, i, s->name);
          const StmtList made = moves ? setInstead(*s) : StmtList{s};
          body.insert(body.end(), made.begin(), made.end());

          if (moves) {
            shared[s->name] = s->type;
          }
        }

        blocks[i].body = body;
      }

      return shared;
    }

    // The state, the label parameters and the shared variables are the function's own locals, declared before its blocks.
    void declareFlowNames(NodeContext& context, const Vector<Block>& blocks, const Map<String, TypePtr>& shared) {
      DeclareLocal(context, kState);

      for (const Block& block : blocks) {
        for (const String& param : block.params) {
          DeclareLocal(context, param);
        }
      }

      for (const auto& [name, type] : shared) {
        DeclareLocal(context, name);
      }
    }

    bool isLabelParam(const Vector<Block>& blocks, const String& name) {
      for (const Block& block : blocks) {
        if (find(block.params.begin(), block.params.end(), name) != block.params.end()) {
          return true;
        }
      }

      return false;
    }

    // `var name: type` for each shared variable that is not a label parameter too.
    void addShared(NodeContext& context, Vector<Node>& nodes, const Map<String, TypePtr>& shared, const Vector<Block>& blocks) {
      for (const auto& [name, type] : shared) {
        if (type && !isLabelParam(blocks, name)) {
          nodes.push_back(varNode(context, name, type, nullopt));
        }
      }
    }

  }  // namespace

  bool HasLabels(const StmtList& body) {
    return any_of(body.begin(), body.end(), [](const StmtPtr& s) { return s->kind == StmtKind::Label; });
  }

  Vector<Node> FlowNodes(NodeContext& context, const StmtList& body) {
    Vector<Block> blocks = blocksOf(body);
    const Map<String, TypePtr> shared = shareVariables(blocks);
    declareFlowNames(context, blocks, shared);

    Vector<Node> nodes = paramVars(context, blocks);
    nodes.insert(nodes.begin(), varNode(context, kState, MakeInt(64), intNode(0)));
    addShared(context, nodes, shared, blocks);

    Node machine = MakeNode("machine");
    AddText(machine, "state", PartKind::Name, kState);
    AddNodes(machine, "cases", PartKind::Block, caseNodes(context, blocks));
    nodes.push_back(machine);

    return nodes;
  }

}  // namespace polyir::translate
