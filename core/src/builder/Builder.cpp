// PolyIRBuilder (polyir/Builder.hpp): building a checked HIR Module from C++ calls instead of from source text.
// Three layers, in this order below: a Block appends statements to a function body; a FunctionBuilder tracks the
// names in scope (and what a closure being built captures); a PolyIRBuilder holds the module and the signatures.
// Every typed operation checks its operands first (builder/BuilderSupport.hpp) and throws a BuildError on a mistake.
#include "polyir/Std.hpp"
#include "polyir/Builder.hpp"

#include "builder/BuilderSupport.hpp"

#include <algorithm>

namespace polyir {

  using namespace polyir::checks;

  namespace {

    TypePtr logicalResult(const TypePtr& left, const TypePtr& right, const String& where) {
      RequireBoolean(left, where);
      RequireBoolean(right, where);

      return MakeBool();
    }

    TypePtr shiftResult(const TypePtr& left, const TypePtr& right, const String& where) {
      RequireInteger(left, where);
      RequireInteger(right, where);

      return left;
    }

    TypePtr comparisonResult(BinOp op, const TypePtr& left, const String& where) {
      if (op != BinOp::Eq && op != BinOp::Ne) {
        RequireNumber(left, where);
      }

      return MakeBool();
    }

    TypePtr arithmeticResult(BinOp op, const TypePtr& left, const String& where) {
      if (op == BinOp::And || op == BinOp::Or || op == BinOp::Xor) {
        RequireInteger(left, where);
      } else {
        RequireNumber(left, where);
      }

      return left;
    }

    TypePtr binaryResult(BinOp op, const TypePtr& left, const TypePtr& right) {
      const String where = String("operator ") + BinOpName(op);

      if (IsLogical(op)) {
        return logicalResult(left, right, where);
      }

      if (op == BinOp::Shl || op == BinOp::Shr) {
        return shiftResult(left, right, where);
      }

      RequireSame(left, right, where);

      if (op == BinOp::Identical || op == BinOp::NotIdentical) {
        return MakeBool();
      }

      if (IsComparison(op)) {
        return comparisonResult(op, left, where);
      }

      return arithmeticResult(op, left, where);
    }

    void checkInit(const String& name, const TypePtr& type, const ExprPtr& init) {
      if (init) {
        RequireSame(type, init->type, "variable " + name);
      }
    }

    // Whether `target` names somewhere a value can be stored: a variable, a global, a field or an element.
    bool isPlace(const ExprPtr& target) {
      const ExprKind kind = target->kind;

      return kind == ExprKind::Local || kind == ExprKind::LoadSlot || kind == ExprKind::Global || kind == ExprKind::FieldGet ||
        kind == ExprKind::IndexGet;
    }

    void noteParameter(Expr& expr, const Parameter& p) {
      expr.parameterNames.push_back(p.name);
      expr.parameterTypes.push_back(p.type);
    }

    SharedPtr<Expr> namedLocal(const String& name, const TypePtr& type) {
      auto expr = MakeExpr(ExprKind::Local, type);
      expr->name = name;

      return expr;
    }

    // A counting loop is a `for` statement: start at `from`, go on while counter < `to`, add 1 each time round.
    void loopParts(Stmt& stmt, const ExprPtr& counter, ExprPtr from, ExprPtr to) {
      stmt.value = Lt(counter, move(to));
      stmt.steps = {Add(counter, IntConst(1, from->type))};
      stmt.initializers = {move(from)};
    }

    // Case i jumps with arguments[i]; a case past the end of `arguments` jumps with none.
    void addCases(Stmt& stmt, const Vector<Pair<int64_t, String>>& cases, const Vector<Vector<ExprPtr>>& arguments) {
      for (size_t i = 0; i < cases.size(); i++) {
        stmt.caseValues.push_back(cases[i].first);
        stmt.caseLabels.push_back(cases[i].second);
        stmt.caseArguments.push_back(i < arguments.size() ? arguments[i] : Vector<ExprPtr>{});
      }
    }

    void requireFloat(const TypePtr& type) {
      if (!IsFloat(*type)) {
        Fail("a float constant needs a float type, not " + TypeText(*type));
      }
    }

    void requireUnaryOperand(UnOp op, const TypePtr& type) {
      if (op == UnOp::LogNot) {
        return RequireBoolean(type, "!");
      }

      if (op == UnOp::Neg) {
        return RequireNumber(type, "unary -");
      }

      RequireInteger(type, "~");
    }

    void requireArguments(const String& function, const Signature& known, const Vector<ExprPtr>& arguments) {
      if (known.parameters.size() != arguments.size()) {
        Fail("call to " + function + " has " + to_string(arguments.size()) + " arguments, expected " + to_string(known.parameters.size()));
      }

      for (size_t i = 0; i < arguments.size(); i++) {
        RequireSame(known.parameters[i].type, arguments[i]->type, "argument " + to_string(i + 1) + " of " + function);
      }
    }

    const StructDef& requireStruct(const Module& module, const String& name) {
      const StructDef* def = FindStruct(module, name);

      if (!def) {
        Fail("no struct called '" + name + "'");
      }

      return *def;
    }

    const StructDef& requireStructOf(const Module& module, const String& name, size_t count) {
      const StructDef& def = requireStruct(module, name);

      if (def.fields.size() != count) {
        Fail("struct " + name + " has " + to_string(def.fields.size()) + " fields, " + to_string(count) + " given");
      }

      return def;
    }

    int requireFieldIndex(const Module& module, const String& structName, const String& fieldName) {
      const int index = FieldIndex(module, structName, fieldName);

      if (index < 0) {
        Fail("struct " + structName + " has no field '" + fieldName + "'");
      }

      return index;
    }

    void addFieldInit(const Module& module, const StructDef& def, Expr& expr, FieldInit& init) {
      auto& [fieldName, value] = init;
      const int index = requireFieldIndex(module, def.name, fieldName);
      RequireSame(def.fields[static_cast<size_t>(index)].type, value->type, "field " + fieldName);
      expr.fieldIndices.push_back(index);
      expr.operands.push_back(move(value));
    }

  }  // namespace

  // ── Block ────────────────────────────────────────────────────────────────────────────────────────────────────────────

  Block Nested(const Block& block, StmtList& list) {
    PushScope(*block.function);

    return Block{block.owner, block.function, &list};
  }

  TypePtr TypeOf(const Block& block, const String& name) {
    TypePtr found = Lookup(*block.function, name);

    if (!found) {
      Fail("no variable called '" + name + "' is in scope in " + block.function->definition.name);
    }

    return found;
  }

  ExprPtr Local(const Block& block, const String& name) {
    const TypePtr type = TypeOf(block, name);
    auto expr = MakeExpr(IsVariable(*block.function, name) ? ExprKind::LoadSlot : ExprKind::Local, type);
    expr->name = name;

    return expr;
  }

  void Declare(const Block& block, const String& name, TypePtr type) {
    Declare(*block.function, name, move(type));
  }

  namespace {

    // Runs `body` on a block that writes into `list` with a scope of its own; the scope ends when the body does.
    void writeNestedBody(Block& block, StmtList& list, const BlockBody& body) {
      Block inner = Nested(block, list);
      body(inner);
      PopScope(*block.function);
    }

    // A new statement of `kind`, added at the end of the block.
    StmtPtr appendStatement(Block& block, StmtKind kind) {
      StmtPtr stmt = make_shared<Stmt>();
      stmt->kind = kind;
      block.list->push_back(stmt);

      return stmt;
    }

  }  // namespace

  void Let(Block& block, const String& name, ExprPtr value) {
    StmtPtr stmt = appendStatement(block, StmtKind::Let);
    stmt->name = name;
    stmt->type = value->type;
    stmt->value = move(value);
    Declare(block, name, stmt->type);
  }

  void Var(Block& block, const String& name, TypePtr type, ExprPtr init) {
    checkInit(name, type, init);

    StmtPtr stmt = appendStatement(block, StmtKind::Var);
    stmt->name = name;
    stmt->type = type;
    stmt->value = move(init);
    Declare(*block.function, name, move(type), true);
  }

  void Assign(Block& block, const ExprPtr& target, ExprPtr value) {
    if (!isPlace(target)) {
      Fail("only a variable, a global, a field or an element can be assigned to");
    }

    RequireSame(target->type, value->type, "assignment");
    StmtPtr stmt = appendStatement(block, StmtKind::Assign);
    stmt->target = target;
    stmt->value = move(value);
  }

  void SetLocal(Block& block, const String& name, ExprPtr value) {
    Assign(block, Local(block, name), move(value));
  }

  void SetField(Block& block, const ExprPtr& object, const String& fieldName, ExprPtr value) {
    Assign(block, FieldAccess(*block.owner, object, fieldName), move(value));
  }

  void StoreIndex(Block& block, const ExprPtr& base, Vector<ExprPtr> indices, ExprPtr value) {
    Assign(block, Index(base, move(indices)), move(value));
  }

  void TryCatch(Block& block, const BlockBody& body, const String& caught, const BlockBody& catchBody) {
    StmtPtr stmt = AppendTry(block, caught);
    writeNestedBody(block, stmt->body, body);
    writeNestedBody(block, stmt->elseBody, [&](Block& handler) {
      Declare(handler, caught, MakeInt(64));
      catchBody(handler);
    });
  }

  void TryCleanup(Block& block, const BlockBody& body, const BlockBody& cleanup) {
    StmtPtr stmt = AppendTry(block, "");
    writeNestedBody(block, stmt->body, body);
    writeNestedBody(block, stmt->elseBody, cleanup);
  }

  void ThrowValue(Block& block, ExprPtr value) {
    RequireInteger(value->type, "throw");
    appendStatement(block, StmtKind::Throw)->value = move(value);
  }

  void YieldValue(Block& block, ExprPtr value) {
    if (!block.function->definition.isCoroutine) {
      Fail("yield belongs in a coroutine, and " + block.function->definition.name + " is not one");
    }

    RequireSame(Yields(*block.function), value->type, "yield");
    appendStatement(block, StmtKind::Yield)->value = move(value);
  }

  SharedPtr<Expr> OpenClosure(Block& block, const Vector<Parameter>& parameters, TypePtr result) {
    auto expr = MakeExpr(ExprKind::ClosureLit, MakeClosure({}, move(result)));
    BeginClosure(*block.function);

    for (const Parameter& p : parameters) {
      noteParameter(*expr, p);
      Declare(block, p.name, p.type);
    }

    return expr;
  }

  Block ClosureBody(const Block& block, const SharedPtr<Expr>& open) {
    return Block{block.owner, block.function, &open->closureBody};
  }

  ExprPtr CloseClosure(Block& block, const SharedPtr<Expr>& open) {
    open->captures = EndClosure(*block.function);
    open->type = MakeClosure(open->parameterTypes, open->type->element);

    return open;
  }

  ExprPtr ClosureLiteral(Block& block, Vector<Parameter> parameters, TypePtr result, const BlockBody& body) {
    const auto open = OpenClosure(block, parameters, move(result));
    Block inner = ClosureBody(block, open);
    body(inner);

    return CloseClosure(block, open);
  }

  StmtPtr AppendTry(Block& block, const String& caught) {
    StmtPtr stmt = appendStatement(block, StmtKind::Try);
    stmt->name = caught;

    return stmt;
  }

  void Return(Block& block, ExprPtr value) {
    StmtPtr stmt = appendStatement(block, StmtKind::Return);
    stmt->value = move(value);
  }

  void ReturnVoid(Block& block) {
    appendStatement(block, StmtKind::Return);
  }

  void Eval(Block& block, ExprPtr value) {
    StmtPtr stmt = appendStatement(block, StmtKind::ExprStmt);
    stmt->value = move(value);
  }

  StmtPtr AppendIf(Block& block, ExprPtr condition) {
    RequireBoolean(condition->type, "if");
    StmtPtr stmt = appendStatement(block, StmtKind::If);
    stmt->value = move(condition);

    return stmt;
  }

  StmtPtr AppendWhile(Block& block, ExprPtr condition) {
    RequireBoolean(condition->type, "while");
    StmtPtr stmt = appendStatement(block, StmtKind::For);
    stmt->value = move(condition);

    return stmt;
  }

  StmtPtr AppendFor(Block& block, const String& counter, ExprPtr from, ExprPtr to) {
    RequireInteger(from->type, "for " + counter);
    RequireSame(from->type, to->type, "for " + counter);

    const auto counterRef = namedLocal(counter, from->type);
    StmtPtr stmt = appendStatement(block, StmtKind::For);
    stmt->names = {counter};
    loopParts(*stmt, counterRef, move(from), move(to));

    return stmt;
  }

  void IfThen(Block& block, ExprPtr condition, const BlockBody& then) {
    StmtPtr stmt = AppendIf(block, move(condition));
    writeNestedBody(block, stmt->body, then);
  }

  void IfElse(Block& block, ExprPtr condition, const BlockBody& then, const BlockBody& otherwise) {
    StmtPtr stmt = AppendIf(block, move(condition));
    writeNestedBody(block, stmt->body, then);
    writeNestedBody(block, stmt->elseBody, otherwise);
  }

  void WhileLoop(Block& block, ExprPtr condition, const BlockBody& body) {
    StmtPtr stmt = AppendWhile(block, move(condition));
    writeNestedBody(block, stmt->body, body);
  }

  void ForRange(Block& block, const String& counter, ExprPtr from, ExprPtr to, const BlockBody& body) {
    StmtPtr stmt = AppendFor(block, counter, from, to);
    writeNestedBody(block, stmt->body, [&](Block& inner) {
      Declare(inner, counter, from->type);
      body(inner);
    });
  }

  void Label(Block& block, const String& name, Vector<Parameter> parameters) {
    StmtPtr stmt = appendStatement(block, StmtKind::Label);
    stmt->name = name;

    for (const Parameter& parameter : parameters) {
      stmt->parameterNames.push_back(parameter.name);
      stmt->parameterTypes.push_back(parameter.type);
      Declare(block, parameter.name, parameter.type);
    }
  }

  void LabelParameter(Block& block, const Parameter& parameter) {
    if (block.list->empty() || block.list->back()->kind != StmtKind::Label) {
      Fail("a label parameter belongs right after its label");
    }

    block.list->back()->parameterNames.push_back(parameter.name);
    block.list->back()->parameterTypes.push_back(parameter.type);
    Declare(block, parameter.name, parameter.type);
  }

  void Jump(Block& block, const String& label, Vector<ExprPtr> arguments) {
    StmtPtr stmt = appendStatement(block, StmtKind::Goto);
    stmt->name = label;
    stmt->arguments = move(arguments);
  }

  void Branch(Block& block, ExprPtr condition, const String& thenLabel, Vector<ExprPtr> thenArguments, const String& elseLabel, Vector<ExprPtr> elseArguments) {
    RequireBoolean(condition->type, "branch");
    StmtPtr stmt = appendStatement(block, StmtKind::Branch);
    stmt->value = move(condition);

    stmt->name = thenLabel;
    stmt->arguments = move(thenArguments);
    stmt->elseName = elseLabel;
    stmt->elseArguments = move(elseArguments);
  }

  void SwitchOn(Block& block, ExprPtr value, Vector<Pair<int64_t, String>> cases, const String& defaultLabel, Vector<Vector<ExprPtr>> caseArguments,
                Vector<ExprPtr> defaultArguments) {
    RequireInteger(value->type, "switch");
    StmtPtr stmt = appendStatement(block, StmtKind::Switch);
    stmt->value = move(value);

    addCases(*stmt, cases, caseArguments);
    stmt->name = defaultLabel;
    stmt->arguments = move(defaultArguments);
  }

  // ── FunctionBuilder ───────────────────────────────────────────────────────────────────────────────────────────────────

  namespace {

    // The innermost scope that holds `name` (scopes are searched from the inside out), or -1 when none does.
    int scopeOf(const FunctionBuilder& function, const String& name) {
      for (size_t i = function.scopes.size(); i > 0; i--) {
        if (function.scopes[i - 1].count(name) > 0) {
          return static_cast<int>(i - 1);
        }
      }

      return -1;
    }

    // A closure being built captures a name that lives below its floor (the scope it started in), unless it already
    // holds it. Every open closure is checked, so a name read in a nested closure is captured by each one it crosses.
    void noteCapture(FunctionBuilder& function, const String& name, size_t scopeIndex) {
      for (ClosureScope& closure : function.closures) {
        const bool isOutside = scopeIndex < closure.floor;
        const bool isKnown = find(closure.captures.begin(), closure.captures.end(), name) != closure.captures.end();

        if (isOutside && !isKnown) {
          closure.captures.push_back(name);
        }
      }
    }

    // A function joins its builder's deque (its address holds), and its body points back at it.
    FunctionBuilder& openFunction(PolyIRBuilder& owner, FuncDef definition) {
      FunctionBuilder& made = owner.functions.emplace_back();
      made.definition = move(definition);
      made.body = Block{&owner, &made, &made.definition.body};
      PushScope(made);

      for (const Parameter& parameter : made.definition.parameters) {
        Declare(made, parameter.name, parameter.type);
      }

      return made;
    }

  }  // namespace

  void Declare(FunctionBuilder& function, const String& name, TypePtr type, bool isVariable) {
    function.scopes.back()[name] = Binding{move(type), isVariable};
  }

  bool IsVariable(const FunctionBuilder& function, const String& name) {
    const int at = scopeOf(function, name);

    return at >= 0 && function.scopes[static_cast<size_t>(at)].at(name).isVariable;
  }

  TypePtr Lookup(FunctionBuilder& function, const String& name) {
    const int at = scopeOf(function, name);

    if (at < 0) {
      return nullptr;
    }

    noteCapture(function, name, static_cast<size_t>(at));

    return function.scopes[static_cast<size_t>(at)].at(name).type;
  }

  void BeginClosure(FunctionBuilder& function) {
    function.closures.push_back(ClosureScope{function.scopes.size(), {}});
    PushScope(function);
  }

  Vector<String> EndClosure(FunctionBuilder& function) {
    PopScope(function);
    Vector<String> captures = move(function.closures.back().captures);
    function.closures.pop_back();

    return captures;
  }

  TypePtr Yields(const FunctionBuilder& function) {
    return function.definition.isCoroutine ? function.definition.result->element : nullptr;
  }

  // ── PolyIRBuilder ─────────────────────────────────────────────────────────────────────────────────────────────────────

  void DefineStruct(PolyIRBuilder& builder, const String& name, Vector<Field> fields, const String& owner) {
    if (FindStruct(builder.module, name)) {
      Fail("struct " + name + " is already defined");
    }

    builder.module.structs.push_back(StructDef{name, move(fields), owner});
  }

  void DefineGlobal(PolyIRBuilder& builder, const String& name, TypePtr type, ExprPtr init) {
    if (FindGlobal(builder.module, name)) {
      Fail("global " + name + " is already defined");
    }

    RequireSame(type, init->type, "global " + name);
    builder.module.globals.push_back(GlobalDef{name, move(type), move(init)});
  }

  namespace {

    // What a call to `name` takes and gives, so calls can be checked before (or without) its body.
    void rememberSignature(PolyIRBuilder& builder, const String& name, Vector<Parameter> parameters, TypePtr result) {
      builder.signatures[name] = Signature{move(parameters), move(result)};
    }

  }  // namespace

  void DeclareExtern(PolyIRBuilder& builder, const String& name, Vector<Parameter> parameters, TypePtr result) {
    builder.module.externs.push_back(ExternDef{name, parameters, result});
    rememberSignature(builder, name, move(parameters), move(result));
  }

  void DeclareFunction(PolyIRBuilder& builder, const String& name, Vector<Parameter> parameters, TypePtr result) {
    rememberSignature(builder, name, move(parameters), move(result));
  }

  void MarkKernel(FunctionBuilder& function, KernelStage stage, Vector<ParamRole> roles) {
    const size_t parameterCount = function.definition.parameters.size();

    if (roles.size() != parameterCount) {
      Fail(function.definition.name + " has " + to_string(parameterCount) + " parameters and " + to_string(roles.size()) + " roles");
    }

    function.definition.stage = stage;
    function.definition.roles = move(roles);
  }

  FunctionBuilder& DefineFunction(PolyIRBuilder& builder, const String& name, Vector<Parameter> parameters, TypePtr result) {
    if (FindFunction(builder.module, name)) {
      Fail("function " + name + " is already defined");
    }

    rememberSignature(builder, name, parameters, result);
    FuncDef definition{name, move(parameters), move(result), {}, false, {}};

    return openFunction(builder, move(definition));
  }

  FunctionBuilder& DefineCoroutine(PolyIRBuilder& builder, const String& name, Vector<Parameter> parameters, TypePtr yields) {
    FunctionBuilder& made = DefineFunction(builder, name, move(parameters), MakeCoroutine(move(yields)));
    made.definition.isCoroutine = true;

    return made;
  }

  void DefineInterface(PolyIRBuilder& builder, const String& name, Vector<InterfaceMethod> methods) {
    if (FindInterface(builder.module, name) || FindStruct(builder.module, name)) {
      Fail("interface " + name + " clashes with a type already defined");
    }

    builder.module.interfaces.push_back(InterfaceDef{name, move(methods), {}});
    // An interface value is a pair of pointers: the object's data and the table of its methods for that interface.
    const TypePtr bytePointer = MakePointer(MakeInt(8));
    builder.module.structs.push_back(StructDef{name, {Field{"data", bytePointer, false}, Field{"vtable", bytePointer, false}}});
  }

  void Implement(PolyIRBuilder& builder, const String& interfaceName, const String& structName) {
    // FindInterface answers a const pointer; the builder owns the module, so it may add to the definition.
    InterfaceDef* def = const_cast<InterfaceDef*>(FindInterface(builder.module, interfaceName));

    if (!def || !FindStruct(builder.module, structName)) {
      Fail("implement needs an interface and a struct that exist: " + interfaceName + ", " + structName);
    }

    def->implementers.push_back(structName);
  }

  void DeclareHostType(PolyIRBuilder& builder, HostType type) {
    builder.module.hostTypes.push_back(move(type));
  }

  void DefineBridge(PolyIRBuilder& builder, Bridge bridge) {
    builder.module.bridges.push_back(move(bridge));
  }

  void DeclareHostMember(PolyIRBuilder& builder, HostMember member) {
    builder.module.hostMembers.push_back(move(member));
  }

  const Signature* SignatureOf(const PolyIRBuilder& builder, const String& function) {
    const auto found = builder.signatures.find(function);

    return found == builder.signatures.end() ? nullptr : &found->second;
  }

  ExprPtr IntConst(int64_t value, TypePtr type) {
    if (!type) {
      type = MakeInt(64);
    }

    RequireInteger(type, "an integer constant");
    auto expr = MakeExpr(ExprKind::ConstInt, move(type));
    expr->intValue = value;

    return expr;
  }

  ExprPtr FloatConst(double value, TypePtr type) {
    if (!type) {
      type = MakeFloat(64);
    }

    requireFloat(type);
    auto expr = MakeExpr(ExprKind::ConstFloat, move(type));
    expr->floatValue = value;

    return expr;
  }

  ExprPtr BoolConst(bool value) {
    auto expr = MakeExpr(ExprKind::ConstBool, MakeBool());
    expr->boolValue = value;

    return expr;
  }

  ExprPtr StringConst(const String& text) {
    auto expr = MakeExpr(ExprKind::ConstString, MakeString());
    expr->text = text;

    return expr;
  }

  ExprPtr Global(const PolyIRBuilder& builder, const String& name) {
    const GlobalDef* def = FindGlobal(builder.module, name);

    if (!def) {
      Fail("no global called '" + name + "'");
    }

    auto expr = MakeExpr(ExprKind::Global, def->type);
    expr->name = name;

    return expr;
  }

  ExprPtr Binary(BinOp op, ExprPtr left, ExprPtr right) {
    auto expr = MakeExpr(ExprKind::Binary, binaryResult(op, left->type, right->type));
    expr->binaryOp = op;
    expr->operands = {move(left), move(right)};

    return expr;
  }

  ExprPtr Unary(UnOp op, ExprPtr operand) {
    requireUnaryOperand(op, operand->type);
    auto expr = MakeExpr(ExprKind::Unary, operand->type);
    expr->unaryOp = op;
    expr->operands = {move(operand)};

    return expr;
  }

  ExprPtr Cast(ExprPtr value, TypePtr to) {
    RequireNumber(value->type, "a conversion");
    RequireNumber(to, "a conversion");
    auto expr = MakeExpr(ExprKind::Cast, move(to));
    expr->operands = {move(value)};

    return expr;
  }

  ExprPtr Call(const PolyIRBuilder& builder, const String& function, Vector<ExprPtr> arguments) {
    const Signature& known = RequireSignature(builder, function);
    requireArguments(function, known, arguments);
    auto expr = MakeExpr(ExprKind::Call, known.result);
    expr->name = function;
    expr->operands = move(arguments);

    return expr;
  }

  ExprPtr StructLit(const PolyIRBuilder& builder, const String& structName, Vector<FieldInit> fields) {
    const StructDef& def = requireStructOf(builder.module, structName, fields.size());
    auto expr = MakeExpr(ExprKind::StructLit, MakeStruct(structName));
    expr->name = structName;

    for (FieldInit& init : fields) {
      addFieldInit(builder.module, def, *expr, init);
    }

    return expr;
  }

  ExprPtr FieldAccess(const PolyIRBuilder& builder, ExprPtr object, const String& fieldName) {
    RequireKind(object->type, TypeKind::Struct, "a field is read from a struct");
    const int index = requireFieldIndex(builder.module, object->type->name, fieldName);
    const StructDef* def = FindStruct(builder.module, object->type->name);

    auto expr = MakeExpr(ExprKind::FieldGet, def->fields[static_cast<size_t>(index)].type);
    expr->fieldIndex = index;
    expr->operands = {move(object)};

    return expr;
  }

  Module Build(const PolyIRBuilder& builder) {
    Module result = builder.module;

    for (const FunctionBuilder& function : builder.functions) {
      result.functions.push_back(function.definition);
    }

    result.functions.insert(result.functions.end(), builder.adopted.begin(), builder.adopted.end());

    return result;
  }

}  // namespace polyir
