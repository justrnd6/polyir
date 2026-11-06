// A block's statements as constructs (Nodes.hpp). The shapes the front end makes of `for (x in xs)`, `for (k in m)` and `if`
// values (Lift.hpp) become `foreach` and `choose`, each keeping the statements it was made of as its lower form. Every other
// statement is the construct of its kind.
#include "translate/Nodes.hpp"

namespace polyir::translate {

  namespace {

    Node statementNode(NodeContext& context, const Stmt& s);

    StmtList slice(const StmtList& body, size_t from, size_t to) {
      return StmtList(body.begin() + static_cast<long>(from), body.begin() + static_cast<long>(to));
    }

    // The statements as the HIR has them, for a lower form.
    Vector<Node> plainNodes(NodeContext& context, const StmtList& statements) {
      Vector<Node> nodes;

      for (const StmtPtr& s : statements) {
        nodes.push_back(statementNode(context, *s));
      }

      return nodes;
    }

    Node valueNode(NodeContext& context, const ExprPtr& e) {
      return ExpressionNode(context, *e);
    }

    Vector<Node> plainValues(NodeContext& context, const Vector<ExprPtr>& exprs, size_t from) {
      Vector<Node> nodes;

      for (size_t i = from; i < exprs.size(); i++) {
        nodes.push_back(valueNode(context, exprs[i]));
      }

      return nodes;
    }

    // A read of local `name`, of type `type`.
    Node nameNode(const NodeContext& context, const String& name, const TypePtr& type) {
      Node node = MakeNode("name");
      AddText(node, "name", PartKind::Name, LocalName(context, name));
      node.note = type;

      return node;
    }

    // ── loops ──

    Node foreachNode(NodeContext& context, const String& name, Node source, const StmtList& body) {
      Node node = MakeNode("foreach");
      PushScope(context);
      AddText(node, "name", PartKind::Name, DeclareLocal(context, name));
      AddNode(node, "source", PartKind::Value, move(source));

      AddNodes(node, "body", PartKind::Block, StatementNodes(context, body));
      PopScope(context);

      return node;
    }

    // The type of the first read of `name` in `body`.
    TypePtr typeOfName(const StmtList& body, const String& name) {
      TypePtr found;
      CountIn(body, [&](const Expr& e) {
        if (!found && Reads(e, name)) {
          found = e.type;
        }

        return false;
      });

      return found;
    }

    // `for (k in m)`: inside, the slot's key is `k`, and its value `m[k]`.
    Node walkNode(NodeContext& context, const WalkShape& walk) {
      const String key = KeyName(walk);
      const bool named = key != walk.slot + "Key";
      Node table = walk.source ? valueNode(context, walk.source) : nameNode(context, walk.table, typeOfName(*walk.inside, walk.table));
      context.walkKeys[walk.slot] = key;
      context.walkTables[walk.slot] = table;

      Node node = foreachNode(context, key, table, named ? slice(*walk.inside, 1, walk.inside->size()) : *walk.inside);
      context.walkKeys.erase(walk.slot);
      context.walkTables.erase(walk.slot);

      return node;
    }

    // A loop over the list `source` whose first statement takes the element: `for (x in source)`.
    Node elementNode(NodeContext& context, const Stmt& loop, Node source, size_t dropAtEnd) {
      const StmtList body = slice(loop.body, 1, loop.body.size() - dropAtEnd);

      return foreachNode(context, loop.body[0]->name, move(source), body);
    }

    // The loops that take several statements, and how many each took; none when `i` starts none.
    Optional<Pair<Node, size_t>> loopAt(NodeContext& context, const StmtList& body, size_t i) {
      if (const Stmt* loop = ListLoopAt(body, i)) {
        Node node = elementNode(context, *loop, valueNode(context, body[i]->value), 1);
        node.lowered = plainNodes(context, slice(body, i, i + 3));

        return Pair<Node, size_t>{node, 3};
      }

      const Optional<HeldLoop> held = HeldLoopAt(body, i);

      if (!held) {
        return nullopt;
      }

      Node node = held->walk ? walkNode(context, *held->walk) : elementNode(context, *held->loop, valueNode(context, held->held->value), 0);
      node.lowered = held->walk ? Vector<Node>{} : plainNodes(context, slice(body, i, i + 2));

      return Pair<Node, size_t>{node, 2};
    }

    // ── `if` values ──

    // `choose(cond, then, else)`, an `else if` chain as a `choose` in the `else`; each value as a `slot`.
    Node chooseNode(NodeContext& context, const IfValue& value, size_t arm, const TypePtr& slot) {
      if (arm == value.arms.size()) {
        return ValueFor(context, value.otherwise, slot);
      }

      Node node = MakeNode("choose");
      AddNode(node, "cond", PartKind::Value, valueNode(context, value.arms[arm].first));
      AddNode(node, "then", PartKind::Value, ValueFor(context, value.arms[arm].second, slot));
      AddNode(node, "else", PartKind::Value, chooseNode(context, value, arm + 1, slot));
      node.note = slot ? slot : value.otherwise->type;

      return node;
    }

    // `let n = choose(...)`, or `var` when n changes later.
    Node declaredNode(NodeContext& context, const DeclaredIf& declared) {
      Node value = chooseNode(context, declared.value, 0, declared.slot->type);
      Node node = MakeNode(declared.changes ? "var" : "let");
      AddText(node, "name", PartKind::Name, DeclareLocal(context, declared.slot->name));
      AddNode(node, "type", PartKind::Type, TypeNode(context, declared.slot->type));
      AddNode(node, "value", PartKind::Value, move(value));

      return node;
    }

    Node returnChooseNode(NodeContext& context, const IfValue& value) {
      Node node = MakeNode("return");
      AddNode(node, "value", PartKind::Value, chooseNode(context, value, 0, context.result));

      return node;
    }

    // ── statements ──

    // `let` or `var`; a `var` a closure changes has the fact `boxed`.
    Node letNode(NodeContext& context, const Stmt& s, const String& construct) {
      const Optional<Node> value = s.value ? Optional<Node>(ValueFor(context, s.value, s.type)) : nullopt;
      Node node = MakeNode(construct);
      AddText(node, "name", PartKind::Name, DeclareLocal(context, s.name));
      AddNode(node, "type", PartKind::Type, TypeNode(context, s.type));

      if (value) {
        AddNode(node, "value", PartKind::Value, *value);
      }

      if (construct == "var" && context.boxed.count(s.name) > 0) {
        AddFact(node, "boxed");
      }

      if (context.guarded) {
        AddFact(node, "guarded");
      }

      return node;
    }

    // A `var` the function never gives a new value is a `let`. The front end keeps a `let` of a list, map or set in a `var` slot.
    Node varNode(NodeContext& context, const Stmt& s) {
      const bool changes = context.function == nullptr || AssignsIn(*context.function, s.name);

      return letNode(context, s, s.value && !changes ? "let" : "var");
    }

    // Where a store writes: `index` (a map's key in the map's key type), or the `indices` of a grid's cell.
    void addPlace(NodeContext& context, Node& node, const Vector<ExprPtr>& operands) {
      const TypePtr& object = operands[0]->type;
      const bool isMap = object && object->kind == TypeKind::Hashmap;

      if (operands.size() == 2) {
        AddNode(node, "index", PartKind::Value, ValueFor(context, operands[1], isMap ? object->element : nullptr));
      } else {
        AddNodes(node, "indices", PartKind::List, plainValues(context, operands, 1));
      }
    }

    // `m[k] = v` is `store(object, index, value)`, `g[r, c] = v` `store(object, indices, value)`; any other target an `assign`.
    Node assignNode(NodeContext& context, const Stmt& s) {
      Node assign = MakeNode("assign");
      AddNode(assign, "target", PartKind::Value, valueNode(context, s.target));
      AddNode(assign, "value", PartKind::Value, ValueFor(context, s.value, s.target->type));

      if (s.target->kind != ExprKind::IndexGet) {
        return assign;
      }

      Node node = MakeNode("store");
      AddNode(node, "object", PartKind::Value, valueNode(context, s.target->operands[0]));
      addPlace(context, node, s.target->operands);
      AddNode(node, "value", PartKind::Value, ValueFor(context, s.value, s.target->type));
      node.lowered.push_back(move(assign));

      return node;
    }

    // `if(cond, then)`, `if(cond, then, else)`, or `if(cond, then, elseIf)` when the else is only another `if`.
    Node ifNode(NodeContext& context, const Stmt& s) {
      Node node = MakeNode("if");
      AddNode(node, "cond", PartKind::Value, valueNode(context, s.value));
      AddNodes(node, "then", PartKind::Block, StatementNodes(context, s.body));

      if (s.elseBody.empty()) {
        return node;
      }

      Node lowered = node;
      AddNodes(lowered, "else", PartKind::Block, StatementNodes(context, s.elseBody));

      if (s.elseBody.size() != 1 || s.elseBody[0]->kind != StmtKind::If) {
        return lowered;
      }

      AddNode(node, "elseIf", PartKind::Statement, ifNode(context, *s.elseBody[0]));
      node.lowered.push_back(move(lowered));

      return node;
    }

    // A counter's declaration (`counter`, or `var` in the lower form), or the `step` that gives it its next value.
    Node counterNode(NodeContext& context, const String& construct, const String& name, const ExprPtr& value) {
      Node made = valueNode(context, value);
      Node node = MakeNode(construct);
      AddText(node, "name", PartKind::Name, construct == "step" ? LocalName(context, name) : DeclareLocal(context, name));

      if (construct != "step") {
        AddNode(node, "type", PartKind::Type, TypeNode(context, value->type));
      }

      AddNode(node, "value", PartKind::Value, move(made));

      return node;
    }

    Node whileNode(NodeContext& context, const ExprPtr& cond, Vector<Node> body) {
      Node node = MakeNode("while");
      AddNode(node, "cond", PartKind::Value, valueNode(context, cond));
      AddNodes(node, "body", PartKind::Block, move(body));

      return node;
    }

    // The counters as `var`s, then a `while` whose body ends with each counter's next value.
    Vector<Node> forLowered(NodeContext& context, const Stmt& s) {
      Vector<Node> nodes;
      Vector<Node> body = StatementNodes(context, s.body);

      for (size_t i = 0; i < s.names.size(); i++) {
        Node counter = counterNode(context, "var", s.names[i], s.initializers[i]);
        Node assign = MakeNode("assign");
        AddNode(assign, "target", PartKind::Value, nameNode(context, s.names[i], s.initializers[i]->type));
        AddNode(assign, "value", PartKind::Value, ValueFor(context, s.steps[i], s.initializers[i]->type));

        nodes.insert(nodes.begin() + static_cast<long>(i), counter);
        body.push_back(assign);
      }

      nodes.push_back(whileNode(context, s.value, move(body)));

      return nodes;
    }

    // `for (; c; )` is a `while`; with counters it is `for(counters, cond, steps, body)`.
    Node forNode(NodeContext& context, const Stmt& s) {
      if (s.names.empty()) {
        return whileNode(context, s.value, StatementNodes(context, s.body));
      }

      Vector<Node> counters;
      Vector<Node> steps;

      for (size_t i = 0; i < s.names.size(); i++) {
        counters.push_back(counterNode(context, "counter", s.names[i], s.initializers[i]));
        steps.push_back(counterNode(context, "step", s.names[i], s.steps[i]));
      }

      Node node = MakeNode("for");
      AddNodes(node, "counters", PartKind::List, counters);
      AddNode(node, "cond", PartKind::Value, valueNode(context, s.value));
      AddNodes(node, "steps", PartKind::List, steps);
      AddNodes(node, "body", PartKind::Block, StatementNodes(context, s.body));

      node.lowered = forLowered(context, s);

      return node;
    }

    // A bare `return` carries a void constant, or nothing.
    Node returnNode(NodeContext& context, const Stmt& s) {
      Node node = MakeNode("return");
      const bool bare = !s.value || (s.value->kind == ExprKind::ConstInt && s.value->type && s.value->type->kind == TypeKind::Void);

      if (!bare) {
        AddNode(node, "value", PartKind::Value, ValueFor(context, s.value, context.result));
      }

      if (context.inCoroutine) {
        AddFact(node, "inCoroutine");
      }

      return node;
    }

    // `try(body, name, handler)`, or `try(body, cleanup)` for the handler that runs on the way out and throws again.
    Node tryNode(NodeContext& context, const Stmt& s) {
      Node node = MakeNode("try");
      AddNodes(node, "body", PartKind::Block, StatementNodes(context, s.body));

      if (s.name.empty()) {
        AddNodes(node, "cleanup", PartKind::Block, StatementNodes(context, s.elseBody));
      } else {
        PushScope(context);
        AddText(node, "name", PartKind::Name, DeclareLocal(context, s.name));
        AddNodes(node, "handler", PartKind::Block, StatementNodes(context, s.elseBody));
        PopScope(context);
      }

      return node;
    }

    // ── switch ──

    // A sum's case as a `caseArm` names it: `Some`, `None`, `Ok`, `Err`, or a variant's number.
    String caseName(const Type& sum, int64_t number) {
      switch (sum.kind) {
        case TypeKind::Optional:
          return number == 0 ? "Some" : "None";
        case TypeKind::Either:
          return number == 0 ? "Ok" : "Err";
        default:
          return to_string(number);
      }
    }

    bool isSum(const Type& type) {
      return type.kind == TypeKind::Optional || type.kind == TypeKind::Either || type.kind == TypeKind::Variant;
    }

    // An arm's keys: `arm(keys, body)`; or a sum's case, `caseArm(case, body)`.
    Node armHead(NodeContext& context, const Stmt& s, size_t i) {
      if (!isSum(*s.value->type)) {
        Node node = MakeNode("arm");
        AddNodes(node, "keys", PartKind::List, plainValues(context, s.caseKeys[i], 0));

        return node;
      }

      Node node = MakeNode("caseArm");
      AddText(node, "case", PartKind::Text, caseName(*s.value->type, s.caseKeys[i][0]->intValue));

      return node;
    }

    // One arm, with `bound` for a sum case that binds its payload. The HIR binds it with the block's first statement, which
    // `bound` says instead.
    Node armNode(NodeContext& context, const Stmt& s, size_t i) {
      const StmtList& body = s.caseBodies[i];
      const bool binds = !s.names[i].empty() && !body.empty() && body[0]->name == s.names[i];
      Node node = armHead(context, s, i);
      PushScope(context);

      if (binds) {
        AddText(node, "bound", PartKind::Name, DeclareLocal(context, s.names[i]));
      }

      AddNodes(node, "body", PartKind::Block, StatementNodes(context, binds ? slice(body, 1, body.size()) : body));
      PopScope(context);

      return node;
    }

    // `select(value, arms)` or `select(value, arms, else)`; its lower form is the `if` chain it means (Hir.hpp `SelectAsIfs`).
    Node selectNode(NodeContext& context, const Stmt& s) {
      Node node = MakeNode("select");
      AddNode(node, "value", PartKind::Value, valueNode(context, s.value));
      Vector<Node> arms;

      for (size_t i = 0; i < s.caseKeys.size(); i++) {
        arms.push_back(armNode(context, s, i));
      }

      AddNodes(node, "arms", PartKind::Block, move(arms));

      if (!s.elseBody.empty()) {
        AddNodes(node, "else", PartKind::Block, StatementNodes(context, s.elseBody));
      }

      node.lowered = StatementNodes(context, SelectAsIfs(s));

      return node;
    }

    // `expr(value)`, `throw(value)`, `yield(value)`; a coroutine yields what its result type says.
    Node valueStatement(NodeContext& context, const String& construct, const ExprPtr& value) {
      Node node = MakeNode(construct);
      AddNode(node, "value", PartKind::Value, construct == "yield" ? ValueFor(context, value, context.result) : valueNode(context, value));

      return node;
    }

    Node misplacedJump(NodeContext& context) {
      Problem(context, "a label, goto, branch or switch inside a block: only a function's own body may hold them");

      return MakeNode("expr");
    }

    Node simpleNode(NodeContext& context, const Stmt& s) {
      switch (s.kind) {
        case StmtKind::ExprStmt:
          return valueStatement(context, "expr", s.value);
        case StmtKind::Throw:
          return valueStatement(context, "throw", s.value);
        case StmtKind::Yield:
          return valueStatement(context, "yield", s.value);
        case StmtKind::Try:
          return tryNode(context, s);
        case StmtKind::Select:
          return selectNode(context, s);
        default:
          return misplacedJump(context);
      }
    }

    Node statementNode(NodeContext& context, const Stmt& s) {
      switch (s.kind) {
        case StmtKind::Let:
          return letNode(context, s, "let");
        case StmtKind::Var:
          return varNode(context, s);
        case StmtKind::Assign:
          return assignNode(context, s);
        case StmtKind::If:
          return ifNode(context, s);
        case StmtKind::For:
          return forNode(context, s);
        case StmtKind::Return:
          return returnNode(context, s);
        default:
          return simpleNode(context, s);
      }
    }

    // `let n = if (c) a else b` from a `var` and the `if` that fills it, its lower form the two statements.
    Optional<Pair<Node, size_t>> declaredAt(NodeContext& context, const StmtList& body, size_t i) {
      const Optional<DeclaredIf> declared = body[i]->kind == StmtKind::Var ? DeclaredAt(body, i) : nullopt;

      if (!declared) {
        return nullopt;
      }

      Node node = declaredNode(context, *declared);
      node.lowered = plainNodes(context, slice(body, i, i + 2));

      return Pair<Node, size_t>{node, 2};
    }

    // The `if` value forms at `i`, and how many statements each took. A hidden variable written in place makes no node: the
    // statement that reads it is lowered with it (`pending`).
    Optional<Pair<Node, size_t>> ifValueAt(NodeContext& context, const StmtList& body, size_t i, Vector<Node>& pending) {
      if (const Optional<IfValue> value = ReturnIfOf(*body[i])) {
        Node node = returnChooseNode(context, *value);
        node.lowered.push_back(statementNode(context, *body[i]));

        return Pair<Node, size_t>{node, 1};
      }

      if (const Optional<IfValue> value = InlinedAt(body, i)) {
        context.inlined[body[i]->name] = chooseNode(context, *value, 0, body[i]->type);
        const Vector<Node> plain = plainNodes(context, slice(body, i, i + 2));
        pending.insert(pending.end(), plain.begin(), plain.end());

        return Pair<Node, size_t>{Node{}, 2};
      }

      return declaredAt(context, body, i);
    }

    // A loop over a list or a walk over a table that takes one statement.
    Optional<Node> oneLoopAt(NodeContext& context, const StmtList& body, size_t i) {
      if (const String list = CountedList(*body[i]); !list.empty()) {
        const Stmt& loop = *body[i];
        Node node = elementNode(context, loop, nameNode(context, list, loop.body[0]->value->operands[0]->type), 0);
        node.lowered.push_back(statementNode(context, loop));

        return node;
      }

      return nullopt;
    }

    // The node at `i` and how many statements it took.
    Pair<Node, size_t> nodeAt(NodeContext& context, const StmtList& body, size_t i, Vector<Node>& pending) {
      if (Optional<Pair<Node, size_t>> loop = loopAt(context, body, i)) {
        return *loop;
      }

      if (Optional<Pair<Node, size_t>> value = ifValueAt(context, body, i, pending)) {
        return *value;
      }

      if (Optional<Node> loop = oneLoopAt(context, body, i)) {
        return {*loop, 1};
      }

      if (const Optional<WalkShape> walk = WalkAt(body, i)) {
        return {walkNode(context, *walk), walk->count};
      }

      return {statementNode(context, *body[i]), 1};
    }

    // A statement that reads hidden variables written in place takes, as its lower form, their statements and its own.
    void lowerWith(NodeContext& context, Node& node, const Stmt& s, Vector<Node>& pending) {
      const Map<String, Node> inlined = move(context.inlined);
      context.inlined.clear();
      Vector<Node> own = node.lowered.empty() ? Vector<Node>{statementNode(context, s)} : node.lowered;
      context.inlined = inlined;

      pending.insert(pending.end(), own.begin(), own.end());
      node.lowered = move(pending);
      pending.clear();
    }

    // Whether a statement from `from` on reads counter `name` before another loop sets it again.
    bool readLater(const StmtList& body, size_t from, const String& name) {
      for (size_t i = from; i < body.size(); i++) {
        const Vector<String>& names = body[i]->names;

        if (body[i]->kind == StmtKind::For && find(names.begin(), names.end(), name) != names.end()) {
          return false;
        }

        if (CountIn(slice(body, i, i + 1), ReadsName(name)) > 0) {
          return true;
        }
      }

      return false;
    }

    // A `for` whose counter is read after it has the fact `usedAfter`: .poly keeps a counter after its loop.
    void noteUsedAfter(Node& node, const StmtList& body, size_t next) {
      const Stmt& loop = *body[next - 1];
      const bool used = any_of(loop.names.begin(), loop.names.end(), [&](const String& name) { return readLater(body, next, name); });

      if (node.construct == "for" && used) {
        AddFact(node, "usedAfter");
      }
    }

    // A node made, unless the statements made none (a hidden variable written in place).
    void addMade(Vector<Node>& nodes, Node made) {
      if (!made.construct.empty()) {
        nodes.push_back(move(made));
      }
    }

  }  // namespace

  // Each statement's node; one that reads a hidden variable written in place takes their statements as its lower form.
  Vector<Node> StatementNodes(NodeContext& context, const StmtList& body) {
    Vector<Node> nodes;
    Vector<Node> pending;
    PushScope(context);

    for (size_t i = 0; i < body.size();) {
      const bool hadPending = !pending.empty();
      Pair<Node, size_t> made = nodeAt(context, body, i, pending);
      noteUsedAfter(made.first, body, i + made.second);
      AddSpan(context, made.first, body[i]->at);

      if (hadPending && !made.first.construct.empty()) {
        lowerWith(context, made.first, *body[i], pending);
      }

      addMade(nodes, move(made.first));
      i += made.second;
    }

    PopScope(context);

    return nodes;
  }

}  // namespace polyir::translate
