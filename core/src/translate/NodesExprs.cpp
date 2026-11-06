// An expression as a construct (Nodes.hpp). Most HIR expressions are one construct each; some are found again as .poly writes them
// (Lift.hpp): a method call `p.sum()`, a list literal `[a, b]`, text with holes `"n = ${x}"`, `freeze()` and `thaw()`. A builtin is
// the construct of its .poly name, its receiver the part `object`.
#include "translate/Nodes.hpp"

namespace polyir::translate {

  namespace {

    // A builtin's construct and the names of its operands (the receiver first); `lanes` takes every operand after the receiver.
    struct BuiltinParts {
      Builtin op;
      const char* construct;
      Vector<String> parts;
    };

    const BuiltinParts kBuiltinParts[] = {{Builtin::Len, "len", {"object"}}, {Builtin::Capacity, "capacity", {"object"}},
      {Builtin::Rows, "rows", {"object"}}, {Builtin::Cols, "cols", {"object"}}, {Builtin::Dim, "dim", {"object", "axis"}},
      {Builtin::Push, "push", {"object", "value"}}, {Builtin::Hash, "hash", {"value"}}, {Builtin::Shuffle, "shuffle", {"object", "lanes"}},
      {Builtin::AtomicLoad, "atomicLoad", {"object", "order"}}, {Builtin::AtomicStore, "atomicStore", {"object", "value", "order"}},
      {Builtin::AtomicRmw, "atomicRmw", {"object", "op", "value", "order"}},
      {Builtin::AtomicCmpxchg, "cmpxchg", {"object", "expected", "value", "success", "failure"}}, {Builtin::AtomicFence, "fence", {"", "order"}},
      {Builtin::VariantTag, "tag", {"object"}}, {Builtin::VariantIsCase, "isCase", {"object", "case"}},
      {Builtin::VariantUnwrap, "unwrapAt", {"object", "case"}}, {Builtin::EitherIsOk, "isOk", {"object"}}, {Builtin::EitherIsErr, "isErr", {"object"}},
      {Builtin::EitherUnwrapOk, "unwrapOk", {"object"}}, {Builtin::EitherUnwrapErr, "unwrapErr", {"object"}},
      {Builtin::OptionIsSome, "isSome", {"object"}}, {Builtin::OptionIsNone, "isNone", {"object"}}, {Builtin::OptionUnwrap, "unwrap", {"object"}},
      {Builtin::CoroutineResume, "resume", {"object"}}, {Builtin::CoroutineDone, "done", {"object"}}, {Builtin::CoroutineClose, "close", {"object"}},
      {Builtin::CoroutineYield, "yielded", {"value"}}, {Builtin::Copy, "copy", {"object"}}, {Builtin::With, "with", {"object", "value"}},
      {Builtin::Has, "has", {"object", "key"}}, {Builtin::Add, "add", {"object", "value"}}, {Builtin::Remove, "remove", {"object", "key"}},
      {Builtin::Truncate, "truncate", {"object", "size"}}, {Builtin::StrOf, "str", {"value"}}, {Builtin::CharAt, "charAt", {"object", "index"}},
      {Builtin::Slice, "slice", {"object", "from", "to"}}, {Builtin::Find, "find", {"object", "part", "from"}}, {Builtin::Print, "print", {"value"}},
      {Builtin::Fail, "fail", {"value"}}};

    const Pair<BinOp, const char*> kOperators[] = {{BinOp::Add, "+"}, {BinOp::Sub, "-"}, {BinOp::Mul, "*"}, {BinOp::Div, "/"}, {BinOp::Rem, "%"},
      {BinOp::And, "&"}, {BinOp::Or, "|"}, {BinOp::Xor, "^"}, {BinOp::Shl, "<<"}, {BinOp::Shr, ">>"}, {BinOp::Eq, "=="}, {BinOp::Ne, "!="},
      {BinOp::Lt, "<"}, {BinOp::Le, "<="}, {BinOp::Gt, ">"}, {BinOp::Ge, ">="}, {BinOp::LogAnd, "&&"}, {BinOp::LogOr, "||"},
      {BinOp::Identical, "==="}, {BinOp::NotIdentical, "!=="}};

    Node literalAs(const Expr& e, const TypePtr& slot);

    Node valueOf(NodeContext& context, const ExprPtr& e) {
      return ExpressionNode(context, *e);
    }

    Vector<Node> valuesFrom(NodeContext& context, const Vector<ExprPtr>& exprs, size_t from) {
      Vector<Node> nodes;

      for (size_t i = from; i < exprs.size(); i++) {
        nodes.push_back(valueOf(context, exprs[i]));
      }

      return nodes;
    }

    Node typed(Node node, const TypePtr& type) {
      node.note = type;

      return node;
    }

    // The values from `from` on, each as the type its place holds (`slots`, a function's parameter types), when that is known.
    Vector<Node> valuesFor(NodeContext& context, const Vector<ExprPtr>& exprs, size_t from, const Vector<TypePtr>& slots) {
      Vector<Node> nodes;

      for (size_t i = from; i < exprs.size(); i++) {
        const size_t at = i - from;
        nodes.push_back(ValueFor(context, exprs[i], at < slots.size() ? slots[at] : nullptr));
      }

      return nodes;
    }

    Vector<TypePtr> typesOf(const Vector<Parameter>& parameters, size_t from) {
      Vector<TypePtr> types;

      for (size_t i = from; i < parameters.size(); i++) {
        types.push_back(parameters[i].type);
      }

      return types;
    }

    // ── constants ──

    const char* escapeOf(char c) {
      static const Pair<char, const char*> kEscapes[] = {{'\\', "\\\\"}, {'"', "\\\""}, {'\n', "\\n"}, {'\t', "\\t"}, {'\r', "\\r"}};

      for (const auto& [letter, escape] : kEscapes) {
        if (letter == c) {
          return escape;
        }
      }

      return nullptr;
    }

    // The text with C's escapes, without the quotes around it.
    String escaped(const String& text) {
      String out;

      for (const char c : text) {
        const char* escape = escapeOf(c);
        out += escape != nullptr ? String(escape) : String(1, c);
      }

      return out;
    }

    // The fewest digits after the point that read back as the same number.
    String floatText(double value) {
      if (value != value || value - value != 0.0) {
        return value != value ? "(0.0 / 0.0)" : value > 0.0 ? "(1.0 / 0.0)" : "(-1.0 / 0.0)";
      }

      char text[400];

      for (int digits = 1; digits <= 40; digits++) {
        snprintf(text, sizeof text, "%.*f", digits, value);

        if (strtod(text, nullptr) == value) {
          break;
        }
      }

      return text;
    }

    Node literal(const String& construct, const String& text, const TypePtr& type) {
      Node node = MakeNode(construct);
      AddText(node, "value", PartKind::Text, text);

      return typed(move(node), type);
    }

    Node constantNode(const Expr& e) {
      switch (e.kind) {
        case ExprKind::ConstInt:
          return e.type && e.type->kind == TypeKind::Void ? typed(MakeNode("unit"), e.type) : literal("int", to_string(e.intValue), e.type);
        case ExprKind::ConstFloat:
          return literal("float", floatText(e.floatValue), e.type);
        case ExprKind::ConstBool:
          return literal("bool", e.boolValue ? "true" : "false", e.type);
        default:
          return literal("string", escaped(e.text), e.type);
      }
    }

    // ── operators ──

    String operatorText(BinOp op) {
      for (const auto& [each, text] : kOperators) {
        if (each == op) {
          return text;
        }
      }

      return "?";
    }

    // What an operand is written as: an arithmetic operation's own type (`Add:i32 acc i` with `i` an i64). In a comparison, a
    // literal takes the other operand's type, and the right operand the left's.
    TypePtr operandType(const ExprPtr& operand, const ExprPtr& other, bool isRight, const TypePtr& type) {
      if (type && IsNumber(*type) && !IsBoolean(*type)) {
        return type;
      }

      const bool literal = operand->kind == ExprKind::ConstInt && other->kind != ExprKind::ConstInt;

      return literal || (isRight && other->kind != ExprKind::ConstInt) ? other->type : nullptr;
    }

    Node binaryNode(NodeContext& context, const ExprPtr& left, const String& op, const ExprPtr& right, const TypePtr& type) {
      Node node = MakeNode("binary");
      AddNode(node, "left", PartKind::Value, ValueFor(context, left, operandType(left, right, false, type)));
      AddText(node, "op", PartKind::Text, op);
      AddNode(node, "right", PartKind::Value, ValueFor(context, right, operandType(right, left, true, type)));

      return typed(move(node), type);
    }

    // `-x`, `!x`, `~x`; the negation of text equality is `a != b`.
    Node unaryNode(NodeContext& context, const Expr& e) {
      const Expr& operand = *e.operands[0];

      if (e.unaryOp == UnOp::LogNot && operand.kind == ExprKind::CallBuiltin && operand.builtin == Builtin::StrEq) {
        return binaryNode(context, operand.operands[0], "!=", operand.operands[1], e.type);
      }

      Node node = MakeNode("unary");
      AddText(node, "op", PartKind::Text, e.unaryOp == UnOp::Neg ? "-" : e.unaryOp == UnOp::LogNot ? "!" : "~");
      AddNode(node, "operand", PartKind::Value, valueOf(context, e.operands[0]));

      return typed(move(node), e.type);
    }

    // `cast(value, type)`; `coerce(value, type)`, a width fix the front end made (an i64 where an i32 is wanted), whose lower form
    // is the cast. A literal coerced is the literal written in its new type.
    Node castNode(NodeContext& context, const Expr& e, const String& construct) {
      const Expr& value = *e.operands[0];
      const bool literal = value.kind == ExprKind::ConstInt || (value.kind == ExprKind::ConstFloat && e.type && IsFloat(*e.type));

      if (construct == "coerce" && literal && e.type && IsNumber(*e.type) && !IsBoolean(*e.type)) {
        return literalAs(value, e.type);
      }

      Node node = MakeNode(construct);
      AddNode(node, "value", PartKind::Value, valueOf(context, e.operands[0]));
      AddNode(node, "type", PartKind::Type, TypeNode(context, e.type));

      if (construct == "coerce") {
        Node cast = node;
        cast.construct = "cast";
        node.lowered.push_back(typed(move(cast), e.type));
      }

      return typed(move(node), e.type);
    }

    // ── reads ──

    // A name's facts: `global` for a global, `boxed` when a closure changes it, `captured` when the closure being made reads it
    // from around it.
    void addNameFacts(const NodeContext& context, Node& node, const Expr& e) {
      if (e.kind == ExprKind::Global) {
        AddFact(node, "global");
      }

      if (context.boxed.count(e.name) > 0) {
        AddFact(node, "boxed");
      }

      if (context.capturedHere.count(e.name) > 0 && e.kind != ExprKind::Global) {
        AddFact(node, "captured");
      }
    }

    Node nameNode(NodeContext& context, const Expr& e) {
      if (const auto inlined = context.inlined.find(e.name); inlined != context.inlined.end()) {
        return inlined->second;
      }

      if (context.walkKeys.count(e.name) > 0) {
        Problem(context, "a map's or set's slot used outside its key or value");
      }

      Node node = MakeNode(e.name == "this" ? "this" : "name");

      if (e.name != "this") {
        AddText(node, "name", PartKind::Name, e.kind == ExprKind::Global ? e.name : LocalName(context, e.name));
      }

      addNameFacts(context, node, e);

      return typed(move(node), e.type);
    }

    // `funcref(name)`, with three facts for a target that writes `Holder::own` (Java, Kotlin). `own` is its own name. `holder` is
    // the module or file it sits in, as the code here names it. `file` is this file's name.
    Node funcRefNode(NodeContext& context, const Expr& e) {
      const FuncDef* def = FindFunction(*context.module, e.name);
      const String written = RelativeName(context, def != nullptr ? DeclaredName(context, e.name, def->generic) : e.name);
      const String holder = ScopeOf(written);
      Node node = MakeNode("funcref");
      AddText(node, "name", PartKind::Name, written);

      AddNameFact(node, "own", {OwnName(written)});
      AddNameFact(node, "holder", holder.empty() ? Vector<String>{} : Vector<String>{holder});
      AddNameFact(node, "file", {context.fileName});

      return typed(move(node), e.type);
    }

    String fieldName(const NodeContext& context, const TypePtr& type, int index) {
      const StructDef* def = type && type->kind == TypeKind::Struct ? AnyStruct(*context.module, type->name) : nullptr;
      const bool known = def != nullptr && index >= 0 && static_cast<size_t>(index) < def->fields.size();

      return known ? def->fields[static_cast<size_t>(index)].name : "field" + to_string(index);
    }

    Node getNode(NodeContext& context, const Expr& e) {
      Node node = MakeNode("get");
      AddNode(node, "object", PartKind::Value, valueOf(context, e.operands[0]));
      AddText(node, "name", PartKind::Name, fieldName(context, e.operands[0]->type, e.fieldIndex));

      return typed(move(node), e.type);
    }

    Node indexNode(NodeContext& context, const Expr& e) {
      Node node = MakeNode("index");
      AddNode(node, "object", PartKind::Value, valueOf(context, e.operands[0]));

      const TypePtr& object = e.operands[0]->type;
      const bool isMap = object && object->kind == TypeKind::Hashmap;

      if (e.operands.size() == 2) {
        AddNode(node, "index", PartKind::Value, ValueFor(context, e.operands[1], isMap ? object->element : nullptr));
      } else {
        AddNodes(node, "indices", PartKind::List, valuesFrom(context, e.operands, 1));
      }

      AddNode(node, "type", PartKind::Type, TypeNode(context, e.type));

      return typed(move(node), e.type);
    }

    // ── calls ──

    const FuncDef* anyFunction(const Module& module, const String& name) {
      if (const FuncDef* def = FindFunction(module, name)) {
        return def;
      }

      for (const FuncDef& def : module.genericFunctions) {
        if (def.name == name) {
          return &def;
        }
      }

      return nullptr;
    }

    // The text of the target's own binding for extern `name` (`Math.abs($0)` from `java=Math.abs($0)`), or "".
    String bindingOf(const NodeContext& context, const String& name) {
      for (const HostMember& member : context.module->hostMembers) {
        const Optional<String> found = member.function == name ? BindingIn(context, member.bindings) : nullopt;

        if (found) {
          return *found;
        }
      }

      return "";
    }

    // The parameter types of the function or extern `name`.
    Vector<TypePtr> parameterTypes(const Module& module, const String& name) {
      if (const FuncDef* def = anyFunction(module, name)) {
        return typesOf(def->parameters, 0);
      }

      for (const ExternDef& def : module.externs) {
        if (def.name == name) {
          return typesOf(def.parameters, 0);
        }
      }

      return {};
    }

    Node callOf(NodeContext& context, const String& construct, const String& callee, const Expr& e) {
      Node node = MakeNode(construct);
      AddText(node, construct == "hostcall" ? "binding" : "callee", construct == "hostcall" ? PartKind::Text : PartKind::Name, callee);
      AddNodes(node, "args", PartKind::List, valuesFor(context, e.operands, 0, parameterTypes(*context.module, e.name)));

      return node;
    }

    // `object.name(args)`. `slots` are the types of the arguments' places. `owner` is the method's struct or interface by its whole
    // path, as C names a method `Point_size`.
    Node methodCallNode(NodeContext& context, const Expr& e, const String& name, size_t argsFrom, const Vector<TypePtr>& slots, const String& owner) {
      Node node = MakeNode("methodcall");
      AddNode(node, "object", PartKind::Value, valueOf(context, e.operands[0]));
      AddText(node, "name", PartKind::Name, name);
      AddNodes(node, "args", PartKind::List, valuesFor(context, e.operands, argsFrom, slots));
      AddNameFact(node, "owner", {owner});

      return typed(move(node), e.type);
    }

    // A call of a function that can throw has the fact `throws`.
    Node marked(NodeContext& context, Node node, const Expr& e) {
      if (CallThrows(context, e)) {
        AddFact(node, "throws");
      }

      return node;
    }

    // `f(a, b)`; a method `p.sum()`; a copy of a generic written once `pick(a, b)` with its type arguments; a host binding.
    Node madeCall(NodeContext& context, const Expr& e);

    Node callNode(NodeContext& context, const Expr& e) {
      return marked(context, madeCall(context, e), e);
    }

    Node madeCall(NodeContext& context, const Expr& e) {
      const FuncDef* def = anyFunction(*context.module, e.name);
      const StructDef* owner = def != nullptr ? OwnerOf(*context.module, *def) : nullptr;

      if (owner != nullptr && !e.operands.empty()) {
        return methodCallNode(context, e, MethodName(*def, *owner), 1, typesOf(def->parameters, 1), owner->name);
      }

      if (const String binding = bindingOf(context, e.name); !binding.empty()) {
        return typed(callOf(context, "hostcall", binding, e), e.type);
      }

      const String name = def != nullptr ? DeclaredName(context, e.name, def->generic) : e.name;
      Node node = callOf(context, "call", RelativeName(context, name), e);

      if (def != nullptr && name != e.name && !def->generic.arguments.empty()) {
        node.parts.insert(node.parts.begin() + 1, NodePart{"typeArgs", PartKind::Type, "", TypeNodes(context, def->generic.arguments)});
      }

      return typed(move(node), e.type);
    }

    Node invokeNode(NodeContext& context, const Expr& e) {
      const TypePtr& callee = e.operands[0]->type;
      Node node = MakeNode("invoke");
      AddNode(node, "callee", PartKind::Value, valueOf(context, e.operands[0]));
      AddNodes(node, "args", PartKind::List, valuesFor(context, e.operands, 1, callee ? callee->parameters : Vector<TypePtr>{}));

      return typed(move(node), e.type);
    }

    // ── values made ──

    // The type of field `index` of struct `name`, or null.
    TypePtr fieldType(const NodeContext& context, const String& name, int index) {
      const StructDef* def = AnyStruct(*context.module, name);
      const bool known = def != nullptr && index >= 0 && static_cast<size_t>(index) < def->fields.size();

      return known ? def->fields[static_cast<size_t>(index)].type : nullptr;
    }

    // Each field's `init(name, value)`, in the order of the struct's fields.
    Vector<Node> initNodes(NodeContext& context, const Expr& e) {
      Map<int, Node> inits;

      for (size_t i = 0; i < e.operands.size(); i++) {
        Node init = MakeNode("init");
        AddText(init, "name", PartKind::Name, fieldName(context, MakeStruct(e.name), e.fieldIndices[i]));
        AddNode(init, "value", PartKind::Value, ValueFor(context, e.operands[i], fieldType(context, e.name, e.fieldIndices[i])));
        inits[e.fieldIndices[i]] = init;
      }

      Vector<Node> values;

      for (auto& [index, init] : inits) {
        values.push_back(move(init));
      }

      return values;
    }

    // `Point(x: 1, y: 2)` as `new(name, values)`; a copy of a generic written once also has its `typeArgs`.
    Node newNode(NodeContext& context, const Expr& e) {
      const StructDef* def = AnyStruct(*context.module, e.name);
      const Vector<Node> values = initNodes(context, e);
      const String name = def != nullptr ? DeclaredName(context, e.name, def->generic) : e.name;
      Node node = MakeNode("new");
      AddText(node, "name", PartKind::Name, RelativeName(context, name));

      if (def != nullptr && name != e.name && !def->generic.arguments.empty()) {
        AddNodes(node, "typeArgs", PartKind::Type, TypeNodes(context, def->generic.arguments));
      }

      AddNodes(node, "values", PartKind::List, values);

      return typed(move(node), e.type);
    }

    // A new collection: `empty(type)`, or `sized(type, sizes)` when a size was chosen. The front end chooses 16 for a collection
    // that changes, and every size of one that cannot: those are left out.
    Node containerNode(NodeContext& context, const Expr& e) {
      const bool isCollection = e.type && IsCollection(*e.type);
      const bool chosen = !e.operands.empty() && (!isCollection || (e.type->isMutable && !IsConstantInt(e.operands[0], 16)));
      Node node = MakeNode(chosen ? "sized" : "empty");
      AddNode(node, "type", PartKind::Type, TypeNode(context, e.type));

      if (chosen) {
        AddNodes(node, "sizes", PartKind::List, valuesFrom(context, e.operands, 0));
      }

      return typed(move(node), e.type);
    }

    Node listNode(NodeContext& context, const String& construct, const Vector<ExprPtr>& values, const TypePtr& type) {
      Node node = MakeNode(construct);
      AddNodes(node, "values", PartKind::List, valuesFor(context, values, 0, Vector<TypePtr>(values.size(), type ? type->element : nullptr)));

      return typed(move(node), type);
    }

    // `list(values, type)`: a list written out, with its type.
    Node typedList(NodeContext& context, const Vector<ExprPtr>& values, const TypePtr& type) {
      Node node = listNode(context, "list", values, type);
      AddNode(node, "type", PartKind::Type, TypeNode(context, type));

      return node;
    }

    Node vecNode(NodeContext& context, const Expr& e) {
      const bool isList = e.type && e.type->kind == TypeKind::DynamicArray;

      if (isList && e.operands.empty()) {
        return containerNode(context, e);
      }

      Node node = listNode(context, isList ? "list" : "vec", e.operands, e.type);
      AddNode(node, "type", PartKind::Type, TypeNode(context, e.type));

      return node;
    }

    Vector<Node> closureParams(NodeContext& context, const Expr& e) {
      Vector<Node> params;

      for (size_t i = 0; i < e.parameterNames.size(); i++) {
        Node param = MakeNode("param");
        AddText(param, "name", PartKind::Name, DeclareLocal(context, e.parameterNames[i]));
        AddNode(param, "type", PartKind::Type, TypeNode(context, e.parameterTypes[i]));
        params.push_back(param);
      }

      return params;
    }

    // The type of the first read of `name` in the closure's body.
    TypePtr capturedType(const Expr& closure, const String& name) {
      TypePtr found;
      CountIn(closure.closureBody, [&](const Expr& e) {
        found = !found && (e.kind == ExprKind::Local || e.kind == ExprKind::LoadSlot) && e.name == name ? e.type : found;

        return false;
      });

      return found;
    }

    // `capture(name, type, value)` for each variable the closure uses from around it, its value as the code around it names it.
    Vector<Node> captureNodes(NodeContext& context, const Expr& e) {
      Vector<Node> nodes;

      for (const String& name : CapturedNames(e)) {
        auto read = make_shared<Expr>();
        read->kind = ExprKind::Local;
        read->name = name;
        read->type = capturedType(e, name);

        Node capture = MakeNode("capture");
        AddText(capture, "name", PartKind::Name, name);
        AddNode(capture, "type", PartKind::Type, TypeNode(context, read->type));
        AddNode(capture, "value", PartKind::Value, ExpressionNode(context, *read));
        nodes.push_back(capture);
      }

      return nodes;
    }

    // `closure(params, result, body)`, with the facts `globals` and `outer` when its body assigns names it does not declare. Its fact
    // `captures` holds the variables it uses from around it, which its body reads with the fact `captured`.
    Node plainClosure(NodeContext& context, const Expr& e, const TypePtr& result) {
      Node node = MakeNode("closure");
      AddNodes(node, "captures", PartKind::Fact, captureNodes(context, e));
      const Set<String> around = context.capturedHere;
      const Vector<String> captured = CapturedNames(e);
      context.capturedHere = Set<String>(captured.begin(), captured.end());

      AddNodes(node, "params", PartKind::List, closureParams(context, e));
      AddNode(node, "result", PartKind::Type, TypeNode(context, result));
      AddNodes(node, "body", PartKind::Block, OneNode(BodyNode(context, StatementNodes(context, e.closureBody), result,
        AssignedNames(e.closureBody, true), OuterNames(e), false)));

      AddText(node, "name", PartKind::Fact, "closure" + to_string(++context.counter));
      AddNameFact(node, "globals", AssignedNames(e.closureBody, true));
      AddNameFact(node, "outer", OuterNames(e));
      AddNode(node, "type", PartKind::Type, TypeNode(context, e.type));
      context.capturedHere = around;

      return typed(move(node), e.type);
    }

    // `lambda(params, value)`, its lower form the closure.
    Node lambdaOf(NodeContext& context, const Expr& e, const TypePtr& result, Node closure) {
      Node lambda = MakeNode("lambda");
      AddNodes(lambda, "params", PartKind::List, closureParams(context, e));
      AddNode(lambda, "value", PartKind::Value, ValueFor(context, e.closureBody[0]->value, result));
      lambda.lowered.push_back(move(closure));

      return typed(move(lambda), e.type);
    }

    // A closure, its `return`s giving its result; one that only returns a value is a `lambda`.
    Node closureNode(NodeContext& context, const Expr& e) {
      const TypePtr result = e.type ? e.type->element : nullptr;
      const TypePtr around = context.result;
      const bool wasCoroutine = context.inCoroutine;
      context.result = result;

      context.inCoroutine = false;
      PushScope(context);

      Node closure = plainClosure(context, e, result);
      const bool onlyReturns = e.closureBody.size() == 1 && e.closureBody[0]->kind == StmtKind::Return && e.closureBody[0]->value;

      Node node = onlyReturns ? lambdaOf(context, e, result, closure) : closure;
      context.result = around;
      context.inCoroutine = wasCoroutine;
      PopScope(context);

      return node;
    }

    // A number literal written as a number of type `slot`: a whole number as a float, or a float of the other width.
    Node literalAs(const Expr& e, const TypePtr& slot) {
      const double value = e.kind == ExprKind::ConstFloat ? e.floatValue : static_cast<double>(e.intValue);

      return IsFloat(*slot) ? literal("float", floatText(value), slot) : literal("int", to_string(e.intValue), slot);
    }

    // `value` as a number of type `slot`: a cast whose lower form it is, so a page may write it as either.
    Node coerced(NodeContext& context, const Expr& value, const TypePtr& slot) {
      Node node = MakeNode("coerce");
      AddNode(node, "value", PartKind::Value, ExpressionNode(context, value));
      AddNode(node, "type", PartKind::Type, TypeNode(context, slot));

      Node cast = node;
      cast.construct = "cast";
      node.lowered.push_back(typed(move(cast), slot));

      return typed(move(node), slot);
    }

    // What an optional or either holds when it is `some`, `ok` or `err`.
    TypePtr heldType(const Expr& e) {
      if (!e.type) {
        return nullptr;
      }

      return e.type->kind == TypeKind::Either && !e.boolValue ? e.type->second : e.type->element;
    }

    // `some(type, value)`, `none(type)`, `ok(type, value)`, `err(type, value)`; `type` is the optional's or either's.
    Node eitherNode(NodeContext& context, const Expr& e) {
      const bool isOptional = e.type && e.type->kind == TypeKind::Optional;
      Node node = MakeNode(isOptional ? (e.boolValue ? "some" : "none") : (e.boolValue ? "ok" : "err"));
      AddNode(node, "type", PartKind::Type, TypeNode(context, e.type));

      if (node.construct != "none") {
        AddNode(node, "value", PartKind::Value, ValueFor(context, e.operands[0], heldType(e)));
      }

      return typed(move(node), e.type);
    }

    Node variantNode(NodeContext& context, const Expr& e) {
      const bool known = e.type && e.fieldIndex >= 0 && static_cast<size_t>(e.fieldIndex) < e.type->parameters.size();
      Node node = MakeNode("variant");
      AddNode(node, "type", PartKind::Type, TypeNode(context, e.type));
      AddText(node, "case", PartKind::Text, to_string(e.fieldIndex));
      AddNode(node, "value", PartKind::Value, ValueFor(context, e.operands[0], known ? e.type->parameters[static_cast<size_t>(e.fieldIndex)] : nullptr));

      return typed(move(node), e.type);
    }

    // ── builtins ──

    const BuiltinParts* partsOf(Builtin op) {
      for (const BuiltinParts& row : kBuiltinParts) {
        if (row.op == op) {
          return &row;
        }
      }

      return nullptr;
    }

    // Whether `type` is a struct with a `var` field: a copy of it is a copy, not the value itself.
    bool hasVarField(const NodeContext& context, const TypePtr& type) {
      const StructDef* def = type && type->kind == TypeKind::Struct ? AnyStruct(*context.module, type->name) : nullptr;

      return def != nullptr && any_of(def->fields.begin(), def->fields.end(), [](const Field& field) { return field.isMutable; });
    }

    // What the place of a builtin's operand holds: a key the collection's key, a value pushed or added its element.
    TypePtr operandSlot(const Expr& e, const String& name) {
      const TypePtr& object = e.operands.empty() ? nullptr : e.operands[0]->type;
      const bool adds = e.builtin == Builtin::Push || e.builtin == Builtin::Add || e.builtin == Builtin::With;

      if (!object || (name != "key" && (name != "value" || !adds))) {
        return nullptr;
      }

      return name == "value" && object->kind == TypeKind::Hashmap ? object->second : object->element;
    }

    // One operand as the part `name`: a variant's case is text; `lanes` takes the operands from here on.
    void addOperand(NodeContext& context, Node& node, const String& name, const Expr& e, size_t i) {
      if (name == "case") {
        return AddText(node, name, PartKind::Text, to_string(e.operands[i]->intValue));
      }

      if (name == "lanes") {
        return AddNodes(node, name, PartKind::List, valuesFrom(context, e.operands, i));
      }

      if (!name.empty() && i < e.operands.size()) {
        AddNode(node, name, PartKind::Value, ValueFor(context, e.operands[i], operandSlot(e, name)));
      }
    }

    // `with(object, key, value)` for a map.
    Node builtinOf(NodeContext& context, const BuiltinParts& row, const Expr& e) {
      Node node = MakeNode(row.construct);
      Vector<String> parts = row.parts;

      if (e.builtin == Builtin::With && e.operands.size() == 3) {
        parts = {"object", "key", "value"};
      }

      for (size_t i = 0; i < parts.size(); i++) {
        addOperand(context, node, parts[i], e, i);
      }

      if (e.builtin == Builtin::VariantUnwrap) {
        AddNode(node, "type", PartKind::Type, TypeNode(context, e.type));
      }

      if (e.builtin == Builtin::Copy && hasVarField(context, e.operands[0]->type)) {
        AddFact(node, "isMutable");
      }

      return typed(move(node), e.type);
    }

    // `freeze(object)` or `thaw(object)`: a copy into the other kind of collection, its lower form the copy.
    Node kindChangeNode(NodeContext& context, const Expr& copy, const TypePtr& type) {
      Node node = MakeNode(type->isMutable ? "thaw" : "freeze");
      AddNode(node, "object", PartKind::Value, valueOf(context, copy.operands[0]));
      node.lowered.push_back(builtinOf(context, *partsOf(Builtin::Copy), copy));

      return typed(move(node), type);
    }

    // `m[k]`: the value at a walk's key.
    Node slotValue(NodeContext& context, const Node& table, const Node& key, const TypePtr& type) {
      Node node = MakeNode("index");
      AddNode(node, "object", PartKind::Value, table);
      AddNode(node, "index", PartKind::Value, key);
      AddNode(node, "type", PartKind::Type, TypeNode(context, type));

      return typed(move(node), type);
    }

    // Inside `for (k in m)`, the slot's key is `k` and its value `m[k]`.
    Node slotNode(NodeContext& context, const Expr& e) {
      const String slot = e.operands.size() == 2 ? e.operands[1]->name : "";
      const auto key = context.walkKeys.find(slot);

      if (key == context.walkKeys.end()) {
        Problem(context, "a map's or set's slot outside a for loop over it");

        return MakeNode("unit");
      }

      Node name = MakeNode("name");
      AddText(name, "name", PartKind::Name, LocalName(context, key->second));
      name.note = e.builtin == Builtin::AssocKey ? e.type : nullptr;

      return e.builtin == Builtin::AssocKey ? name : slotValue(context, context.walkTables.at(slot), name, e.type);
    }

    // `"n = ${x}"`: each piece literal text, `text(value)`, or a value, `hole(value)`; its lower form the join.
    Node interpNode(NodeContext& context, const Expr& e, const Vector<const Expr*>& pieces) {
      Vector<Node> nodes;

      for (const Expr* piece : pieces) {
        if (piece->kind == ExprKind::ConstString) {
          nodes.push_back(literal("text", escaped(piece->text), piece->type));
          continue;
        }

        Node hole = MakeNode("hole");
        AddNode(hole, "value", PartKind::Value, ExpressionNode(context, IsTextOf(*piece) ? *piece->operands[0] : *piece));
        nodes.push_back(hole);
      }

      Node node = MakeNode("interp");
      AddNodes(node, "pieces", PartKind::List, nodes);
      node.lowered.push_back(binaryNode(context, e.operands[0], "+", e.operands[1], e.type));

      return typed(move(node), e.type);
    }

    // `shape.area(2.0)`: the method's name, found from its slot in the interface's table.
    Node interfaceCallNode(NodeContext& context, const Expr& e) {
      const TypePtr& type = e.operands[0]->type;
      const InterfaceDef* def = type ? FindInterface(*context.module, type->name) : nullptr;
      const int64_t slot = e.operands[1]->intValue;

      if (def == nullptr || slot < 0 || static_cast<size_t>(slot) >= def->methods.size()) {
        Problem(context, "an interface call through an unknown slot");

        return MakeNode("unit");
      }

      const InterfaceMethod& method = def->methods[static_cast<size_t>(slot)];

      return marked(context, methodCallNode(context, e, method.name, 2, method.parameters, def->name), e);
    }

    bool isEmptyList(const Expr& e) {
      const bool made = e.kind == ExprKind::NewContainer || (e.kind == ExprKind::VecLit && e.operands.empty());

      return made && e.type && e.type->kind == TypeKind::DynamicArray;
    }

    // `array<i64>().with(1).with(2)`: the values a `with` chain adds to an empty list, in order; false when `e` is not one.
    bool withChain(const Expr& e, Vector<ExprPtr>& values) {
      if (isEmptyList(e)) {
        return true;
      }

      const bool with = e.kind == ExprKind::CallBuiltin && e.builtin == Builtin::With && e.operands.size() == 2;

      if (!with || !withChain(*e.operands[0], values)) {
        return false;
      }

      values.push_back(e.operands[1]);

      return true;
    }

    // The builtins that are not one construct each.
    Optional<Node> specialBuiltin(NodeContext& context, const Expr& e) {
      if (e.builtin == Builtin::Push && !ListValues(e).empty()) {
        return typedList(context, ListValues(e), e.type);
      }

      if (Vector<ExprPtr> values; e.builtin == Builtin::With && withChain(e, values)) {
        return typedList(context, values, e.type);
      }

      if (e.builtin == Builtin::Copy && IsOtherKind(e)) {
        return kindChangeNode(context, e, e.type);
      }

      if (e.builtin == Builtin::Retype && e.operands[0]->kind == ExprKind::CallBuiltin && e.operands[0]->builtin == Builtin::Copy && e.type) {
        return kindChangeNode(context, *e.operands[0], e.type);
      }

      if (e.builtin == Builtin::StrOf && e.operands.size() == 1 && IsTextOf(*e.operands[0])) {
        return ExpressionNode(context, *e.operands[0]);
      }

      return nullopt;
    }

    // A builtin of its own construct, or one that has none (the HIR's own: `assocLive`, `free`).
    Node namedBuiltin(NodeContext& context, const Expr& e) {
      if (const BuiltinParts* row = partsOf(e.builtin)) {
        return builtinOf(context, *row, e);
      }

      Problem(context, String("the builtin ") + BuiltinName(e.builtin) + " has no construct");

      return MakeNode("unit");
    }

    // Text equality is `a == b`; a join is text with holes when .poly's own interpolation made it, else `a + b`.
    Node textOperator(NodeContext& context, const Expr& e) {
      if (e.builtin == Builtin::StrEq) {
        return binaryNode(context, e.operands[0], "==", e.operands[1], e.type);
      }

      const Vector<const Expr*> pieces = JoinedPieces(e);

      return IsInterpolation(pieces) ? interpNode(context, e, pieces) : binaryNode(context, e.operands[0], "+", e.operands[1], e.type);
    }

    // Text operators, interface calls as method calls, a boxed struct as the struct, a walk's slot as its key or value.
    Node builtinNode(NodeContext& context, const Expr& e) {
      if (Optional<Node> special = specialBuiltin(context, e)) {
        return *special;
      }

      switch (e.builtin) {
        case Builtin::StrEq:
        case Builtin::Concat:
          return textOperator(context, e);
        case Builtin::InterfaceCall:
          return interfaceCallNode(context, e);
        case Builtin::InterfaceBox:
          return valueOf(context, e.operands[0]);
        default:
          return e.builtin == Builtin::AssocKey || e.builtin == Builtin::AssocValue ? slotNode(context, e) : namedBuiltin(context, e);
      }
    }

    // An expression the front end could not make (`Unsupported`): its reason is the problem.
    Node unmadeNode(NodeContext& context, const Expr& e) {
      Problem(context, e.text.empty() ? "an expression with no construct" : e.text);

      return MakeNode("unit");
    }

    Node madeNode(NodeContext& context, const Expr& e) {
      switch (e.kind) {
        case ExprKind::StructLit:
          return newNode(context, e);
        case ExprKind::VecLit:
          return vecNode(context, e);
        case ExprKind::NewContainer:
          return containerNode(context, e);
        case ExprKind::ClosureLit:
          return closureNode(context, e);
        case ExprKind::VariantLit:
          return variantNode(context, e);
        default:
          return e.kind == ExprKind::EitherLit ? eitherNode(context, e) : unmadeNode(context, e);
      }
    }

    Node accessNode(NodeContext& context, const Expr& e) {
      switch (e.kind) {
        case ExprKind::Call:
          return callNode(context, e);
        case ExprKind::CallIndirect:
          return invokeNode(context, e);
        case ExprKind::CallBuiltin:
          return builtinNode(context, e);
        case ExprKind::FieldGet:
          return getNode(context, e);
        case ExprKind::IndexGet:
          return indexNode(context, e);
        default:
          return madeNode(context, e);
      }
    }

    Node operatorNode(NodeContext& context, const Expr& e) {
      switch (e.kind) {
        case ExprKind::Binary:
          return binaryNode(context, e.operands[0], operatorText(e.binaryOp), e.operands[1], e.type);
        case ExprKind::Unary:
          return unaryNode(context, e);
        case ExprKind::Cast:
          return castNode(context, e, "cast");
        case ExprKind::Coerce:
          return castNode(context, e, "coerce");
        default:
          return accessNode(context, e);
      }
    }

    Node madeExpression(NodeContext& context, const Expr& e) {
      if (IsLiteral(e)) {
        return constantNode(e);
      }

      switch (e.kind) {
        case ExprKind::Local:
        case ExprKind::LoadSlot:
        case ExprKind::Global:
          return nameNode(context, e);
        case ExprKind::FuncRef:
          return funcRefNode(context, e);
        default:
          return operatorNode(context, e);
      }
    }

    // An expression built with its checked type (kept as the node's note) gets that type as its `type` part.
    Node withType(NodeContext& context, Node node) {
      const TypePtr type = static_pointer_cast<const Type>(node.note);

      if (type && PartNamed(node, "type") == nullptr) {
        AddNode(node, "type", PartKind::Type, TypeNode(context, type));
      }

      return node;
    }

  }  // namespace

  // The expression as a value of type `slot`. A number literal is written in that type. Another number of a different type is
  // coerced to it. Anything else is the expression's own node.
  Node ValueFor(NodeContext& context, const ExprPtr& value, const TypePtr& slot) {
    const bool numbers = slot && value->type && IsNumber(*slot) && IsNumber(*value->type) && !IsBoolean(*slot) && !IsBoolean(*value->type);
    const bool differs = numbers && !SameType(slot, value->type);

    if (!differs) {
      return ExpressionNode(context, *value);
    }

    const bool literal = value->kind == ExprKind::ConstInt || (value->kind == ExprKind::ConstFloat && IsFloat(*slot));

    return withType(context, literal ? literalAs(*value, slot) : coerced(context, *value, slot));
  }

  // Every expression has its type as the part `type`, which a rule may name (`index(object, index, type)`, C's `{type}`).
  Node ExpressionNode(NodeContext& context, const Expr& e) {
    return withType(context, madeExpression(context, e));
  }

}  // namespace polyir::translate
