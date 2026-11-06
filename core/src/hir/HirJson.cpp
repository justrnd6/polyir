// Reads the checked HIR from the JSON the Nina front end writes (`polyir f.poly --emit-hir-json`) into a Module
// (polyir/HirJson.hpp). Every JSON node is an object whose `k` names its case (`HInt`, `HBinary`, `HLet`, ...) and
// whose other keys are that case's fields. Each `read...` group below knows a few cases and answers nullptr for the
// rest, so readType, readExpr and readStmt just try the groups in turn. HirJsonWrite.cpp writes the same format.
#include "polyir/Std.hpp"
#include "polyir/HirJson.hpp"

#include "hir/Json.hpp"

namespace polyir {

  namespace {

    using json::AsArray;
    using json::Parse;
    using json::Value;

    TypePtr readType(const Value& v);

    ExprPtr readExpr(const Value& v);

    StmtPtr readStmt(const Value& v);

    Vector<TypePtr> readTypes(const Value& v) {
      Vector<TypePtr> out;

      for (const Value& t : AsArray(v)) {
        out.push_back(readType(t));
      }

      return out;
    }

    Vector<ExprPtr> readExprs(const Value& v) {
      Vector<ExprPtr> out;

      for (const Value& e : AsArray(v)) {
        out.push_back(readExpr(e));
      }

      return out;
    }

    StmtList readStmts(const Value& v) {
      StmtList out;

      for (const Value& s : AsArray(v)) {
        out.push_back(readStmt(s));
      }

      return out;
    }

    Vector<String> readStrings(const Value& v) {
      Vector<String> out;

      for (const Value& s : AsArray(v)) {
        out.push_back(AsString(s));
      }

      return out;
    }

    Vector<int> readInts(const Value& v) {
      Vector<int> out;

      for (const Value& i : AsArray(v)) {
        out.push_back(static_cast<int>(AsInt(i)));
      }

      return out;
    }

    Span readSpan(const Value& v) {
      Span at;

      if (Has(v, "at")) {
        const auto& pair = AsArray(At(v, "at"));
        at.line = static_cast<int>(AsInt(pair[0]));
        at.column = static_cast<int>(AsInt(pair[1]));
      }

      return at;
    }

    // ── types ──

    int intOf(const Value& v, const String& key) {
      return static_cast<int>(AsInt(At(v, key)));
    }

    TypePtr readNamedType(const String& kindName, const Value& v) {
      if (kindName == "HString") {
        return MakeString();
      }

      if (kindName == "HVoid") {
        return MakeVoid();
      }

      if (kindName == "HStruct") {
        return MakeStruct(AsString(At(v, "name")));
      }

      if (kindName == "HParam") {
        return MakeParam(AsString(At(v, "name")));
      }

      return nullptr;
    }

    TypePtr readScalarType(const String& kindName, const Value& v) {
      if (kindName == "HInt") {
        return MakeInt(intOf(v, "bits"));
      }

      if (kindName == "HUInt") {
        return MakeUInt(intOf(v, "bits"));
      }

      if (kindName == "HFloat") {
        return MakeFloat(intOf(v, "bits"));
      }

      if (kindName == "HPtr") {
        return MakePointer(readType(At(v, "elem")));
      }

      return readNamedType(kindName, v);
    }

    TypePtr readSequenceType(const String& kindName, const Value& v) {
      const int size = Has(v, "size") ? intOf(v, "size") : 0;

      if (kindName == "HFixedArray") {
        return MakeFixedArray(readType(At(v, "elem")), size);
      }

      if (kindName == "HBuffer") {
        return MakeBuffer(readType(At(v, "elem")), size);
      }

      if (kindName == "HDarray") {
        return MakeDynamicArray(readType(At(v, "elem")), AsBool(At(v, "mutable")));
      }

      if (kindName == "HSparse") {
        return MakeSparse(readType(At(v, "elem")));
      }

      return nullptr;
    }

    // A vector, a matrix, a grid or a tensor: an element type and a fixed shape.
    TypePtr readShapedType(const String& kindName, const Value& v) {
      if (kindName == "HVec") {
        return MakeVec(readType(At(v, "elem")), intOf(v, "count"));
      }

      if (kindName == "HMat") {
        return MakeMatrix(readType(At(v, "elem")), intOf(v, "size"));
      }

      if (kindName == "HGrid") {
        return MakeGrid(readType(At(v, "elem")), intOf(v, "rows"), intOf(v, "cols"));
      }

      if (kindName == "HTensor") {
        return MakeTensor(readType(At(v, "elem")), readInts(At(v, "dims")));
      }

      return nullptr;
    }

    // A hashmap or a set: the types it holds and whether it is mutable.
    TypePtr readHashedType(const String& kindName, const Value& v) {
      if (kindName == "HHashmap") {
        return MakeHashmap(readType(At(v, "key")), readType(At(v, "value")), AsBool(At(v, "mutable")));
      }

      if (kindName == "HSet") {
        return MakeSet(readType(At(v, "elem")), AsBool(At(v, "mutable")));
      }

      return nullptr;
    }

    TypePtr readCollectionType(const String& kindName, const Value& v) {
      if (TypePtr t = readShapedType(kindName, v)) {
        return t;
      }

      return readHashedType(kindName, v);
    }

    TypePtr readContainerType(const String& kindName, const Value& v) {
      if (TypePtr t = readSequenceType(kindName, v)) {
        return t;
      }

      return readCollectionType(kindName, v);
    }

    // A function pointer or a closure (its parameter and result types), or a coroutine (the type it yields).
    TypePtr readCallableType(const String& kindName, const Value& v) {
      if (kindName == "HFuncPtr") {
        return MakeFunction(readTypes(At(v, "params")), readType(At(v, "ret")));
      }

      if (kindName == "HClosure") {
        return MakeClosure(readTypes(At(v, "params")), readType(At(v, "ret")));
      }

      if (kindName == "HCoro") {
        return MakeCoroutine(readType(At(v, "yields")));
      }

      return nullptr;
    }

    // A variant, an either or an optional: a value that is one of a few cases.
    TypePtr readChoiceType(const String& kindName, const Value& v) {
      if (kindName == "HVariant") {
        return MakeVariant(readTypes(At(v, "cases")));
      }

      if (kindName == "HEither") {
        return MakeEither(readType(At(v, "ok")), readType(At(v, "err")));
      }

      if (kindName == "HOptional") {
        return MakeOptional(readType(At(v, "elem")));
      }

      return nullptr;
    }

    TypePtr readWrappedType(const String& kindName, const Value& v) {
      if (TypePtr t = readCallableType(kindName, v)) {
        return t;
      }

      return readChoiceType(kindName, v);
    }

    TypePtr readType(const Value& v) {
      const String& kindName = AsString(At(v, "k"));

      if (TypePtr t = readScalarType(kindName, v)) {
        return t;
      }

      if (TypePtr t = readContainerType(kindName, v)) {
        return t;
      }

      if (TypePtr t = readWrappedType(kindName, v)) {
        return t;
      }

      throw HirReadError("unknown type '" + kindName + "'");
    }

    // ── operators ──

    // An operator is written by its name (BinOpName); find the enum value with that name.
    BinOp readBinOp(const String& name) {
      for (int i = 0; i <= static_cast<int>(BinOp::NotIdentical); i++) {
        if (name == BinOpName(static_cast<BinOp>(i))) {
          return static_cast<BinOp>(i);
        }
      }

      throw HirReadError("unknown operator '" + name + "'");
    }

    UnOp readUnOp(const String& name) {
      for (int i = 0; i <= static_cast<int>(UnOp::BitNot); i++) {
        if (name == UnOpName(static_cast<UnOp>(i))) {
          return static_cast<UnOp>(i);
        }
      }

      throw HirReadError("unknown operator '" + name + "'");
    }

    Builtin readBuiltin(const String& name) {
      for (int i = 0; i <= static_cast<int>(Builtin::Fail); i++) {
        if (name == BuiltinName(static_cast<Builtin>(i))) {
          return static_cast<Builtin>(i);
        }
      }

      throw HirReadError("unknown builtin '" + name + "'");
    }

    // ── expressions ──

    SharedPtr<Expr> startExpr(ExprKind kind, const Value& v) {
      auto e = make_shared<Expr>();
      e->kind = kind;
      e->type = readType(At(v, "ty"));
      e->at = readSpan(v);

      return e;
    }

    // Reads one JSON expression and adds it as the next operand.
    void addOperand(Expr& e, const Value& json) {
      e.operands.push_back(readExpr(json));
    }

    // Reads a JSON list of expressions and adds them all as operands, in order.
    void addOperandList(Expr& e, const Value& list) {
      for (ExprPtr& x : readExprs(list)) {
        e.operands.push_back(move(x));
      }
    }

    SharedPtr<Expr> withOperand(ExprKind kind, const Value& v, const String& key) {
      auto e = startExpr(kind, v);
      addOperand(*e, At(v, key));

      return e;
    }

    SharedPtr<Expr> withOperandAndArgs(ExprKind kind, const Value& v, const String& key) {
      auto e = withOperand(kind, v, key);
      addOperandList(*e, At(v, "args"));

      return e;
    }

    SharedPtr<Expr> constInt(const Value& v) {
      auto e = startExpr(ExprKind::ConstInt, v);
      e->intValue = AsInt(At(v, "value"));

      return e;
    }

    SharedPtr<Expr> constFloat(const Value& v) {
      auto e = startExpr(ExprKind::ConstFloat, v);
      e->floatValue = AsDouble(At(v, "value"));

      return e;
    }

    SharedPtr<Expr> constBool(const Value& v) {
      auto e = startExpr(ExprKind::ConstBool, v);
      e->boolValue = AsBool(At(v, "value"));

      return e;
    }

    SharedPtr<Expr> constString(const Value& v) {
      auto e = startExpr(ExprKind::ConstString, v);
      e->text = AsString(At(v, "text"));

      return e;
    }

    ExprPtr readConstant(const String& kindName, const Value& v) {
      if (kindName == "HConstInt") {
        return constInt(v);
      }

      if (kindName == "HConstFloat") {
        return constFloat(v);
      }

      if (kindName == "HConstBool") {
        return constBool(v);
      }

      if (kindName == "HConstString") {
        return constString(v);
      }

      return nullptr;
    }

    SharedPtr<Expr> reference(ExprKind kind, const Value& v) {
      auto e = startExpr(kind, v);

      if (Has(v, "name")) {
        e->name = AsString(At(v, "name"));
      }

      return e;
    }

    ExprPtr readReference(const String& kindName, const Value& v) {
      if (kindName == "HLocal") {
        return reference(ExprKind::Local, v);
      }

      if (kindName == "HLoadSlot") {
        return reference(ExprKind::LoadSlot, v);
      }

      if (kindName == "HGlobalRef") {
        return reference(ExprKind::Global, v);
      }

      if (kindName == "HFuncRef") {
        return reference(ExprKind::FuncRef, v);
      }

      return nullptr;
    }

    ExprPtr readLeaf(const String& kindName, const Value& v) {
      if (ExprPtr e = readConstant(kindName, v)) {
        return e;
      }

      return readReference(kindName, v);
    }

    SharedPtr<Expr> binary(const Value& v) {
      auto e = startExpr(ExprKind::Binary, v);
      e->binaryOp = readBinOp(AsString(At(v, "op")));

      // Both keys are looked up before either side is read, so a missing key is reported first.
      const Value& left = At(v, "left");
      const Value& right = At(v, "right");
      addOperand(*e, left);
      addOperand(*e, right);

      return e;
    }

    SharedPtr<Expr> unary(const Value& v) {
      auto e = withOperand(ExprKind::Unary, v, "operand");
      e->unaryOp = readUnOp(AsString(At(v, "op")));

      return e;
    }

    ExprPtr readOperation(const String& kindName, const Value& v) {
      if (kindName == "HBinary") {
        return binary(v);
      }

      if (kindName == "HUnary") {
        return unary(v);
      }

      if (kindName == "HCast") {
        return withOperand(ExprKind::Cast, v, "operand");
      }

      if (kindName == "HCoerce") {
        return withOperand(ExprKind::Coerce, v, "operand");
      }

      return nullptr;
    }

    SharedPtr<Expr> call(const Value& v) {
      auto e = startExpr(ExprKind::Call, v);
      e->name = AsString(At(v, "callee"));
      addOperandList(*e, At(v, "args"));

      return e;
    }

    SharedPtr<Expr> callBuiltin(const Value& v) {
      auto e = withOperandAndArgs(ExprKind::CallBuiltin, v, "receiver");
      e->builtin = readBuiltin(AsString(At(v, "op")));

      return e;
    }

    ExprPtr readCall(const String& kindName, const Value& v) {
      if (kindName == "HCall") {
        return call(v);
      }

      if (kindName == "HCallIndirect") {
        return withOperandAndArgs(ExprKind::CallIndirect, v, "callee");
      }

      if (kindName == "HCallBuiltin") {
        return callBuiltin(v);
      }

      return nullptr;
    }

    SharedPtr<Expr> structLit(const Value& v) {
      auto e = startExpr(ExprKind::StructLit, v);
      e->name = AsString(At(v, "name"));
      e->fieldIndices = readInts(At(v, "fieldIndices"));
      addOperandList(*e, At(v, "fields"));

      return e;
    }

    SharedPtr<Expr> fieldGet(const Value& v) {
      auto e = withOperand(ExprKind::FieldGet, v, "base");
      e->fieldIndex = intOf(v, "index");

      return e;
    }

    SharedPtr<Expr> operandList(ExprKind kind, const Value& v, const String& key) {
      auto e = startExpr(kind, v);
      addOperandList(*e, At(v, key));

      return e;
    }

    SharedPtr<Expr> indexGet(const Value& v) {
      auto e = withOperand(ExprKind::IndexGet, v, "base");
      addOperandList(*e, At(v, "indices"));

      return e;
    }

    ExprPtr readData(const String& kindName, const Value& v) {
      if (kindName == "HStructLit") {
        return structLit(v);
      }

      if (kindName == "HFieldGet") {
        return fieldGet(v);
      }

      if (kindName == "HIndexGet") {
        return indexGet(v);
      }

      if (kindName == "HVecLit") {
        return operandList(ExprKind::VecLit, v, "elems");
      }

      if (kindName == "HNewContainer") {
        return operandList(ExprKind::NewContainer, v, "sizeArgs");
      }

      return nullptr;
    }

    SharedPtr<Expr> closureLit(const Value& v) {
      auto e = startExpr(ExprKind::ClosureLit, v);
      e->parameterNames = readStrings(At(v, "paramNames"));
      e->parameterTypes = readTypes(At(v, "paramTypes"));
      e->captures = readStrings(At(v, "captures"));
      e->closureBody = readStmts(At(v, "body"));

      return e;
    }

    SharedPtr<Expr> variantLit(const Value& v) {
      auto e = withOperand(ExprKind::VariantLit, v, "value");
      e->fieldIndex = intOf(v, "caseIndex");

      return e;
    }

    SharedPtr<Expr> eitherLit(const Value& v) {
      auto e = withOperand(ExprKind::EitherLit, v, "value");
      e->boolValue = AsBool(At(v, "isOk"));

      return e;
    }

    SharedPtr<Expr> unsupported(const Value& v) {
      auto e = startExpr(ExprKind::Unsupported, v);
      e->text = AsString(At(v, "what"));

      return e;
    }

    ExprPtr readSpecial(const String& kindName, const Value& v) {
      if (kindName == "HClosureLit") {
        return closureLit(v);
      }

      if (kindName == "HVariantLit") {
        return variantLit(v);
      }

      if (kindName == "HEitherLit") {
        return eitherLit(v);
      }

      if (kindName == "HUnsupported") {
        return unsupported(v);
      }

      return nullptr;
    }

    // An operator or a call expression; null when the kind is neither.
    ExprPtr readOperationOrCall(const String& kindName, const Value& v) {
      if (ExprPtr e = readOperation(kindName, v)) {
        return e;
      }

      return readCall(kindName, v);
    }

    ExprPtr readExpr(const Value& v) {
      const String& kindName = AsString(At(v, "k"));

      if (ExprPtr e = readLeaf(kindName, v)) {
        return e;
      }

      if (ExprPtr e = readOperationOrCall(kindName, v)) {
        return e;
      }

      if (ExprPtr e = readData(kindName, v)) {
        return e;
      }

      if (ExprPtr e = readSpecial(kindName, v)) {
        return e;
      }

      throw HirReadError("unknown expression '" + kindName + "'");
    }

    // ── statements ──

    StmtPtr startStmt(StmtKind kind, const Value& v) {
      auto s = make_shared<Stmt>();
      s->kind = kind;
      s->at = readSpan(v);

      return s;
    }

    StmtPtr letStmt(const Value& v) {
      auto s = startStmt(StmtKind::Let, v);
      s->name = AsString(At(v, "name"));
      s->type = readType(At(v, "ty"));
      s->value = readExpr(At(v, "value"));

      return s;
    }

    StmtPtr varStmt(const Value& v) {
      auto s = startStmt(StmtKind::Var, v);
      s->name = AsString(At(v, "name"));
      s->type = readType(At(v, "ty"));

      if (!IsNull(At(v, "init"))) {
        s->value = readExpr(At(v, "init"));
      }

      return s;
    }

    StmtPtr assignStmt(const Value& v) {
      auto s = startStmt(StmtKind::Assign, v);
      s->target = readExpr(At(v, "target"));
      s->value = readExpr(At(v, "value"));

      return s;
    }

    StmtPtr readBinding(const String& kindName, const Value& v) {
      if (kindName == "HLet") {
        return letStmt(v);
      }

      if (kindName == "HVarDecl") {
        return varStmt(v);
      }

      if (kindName == "HAssign") {
        return assignStmt(v);
      }

      return nullptr;
    }

    StmtPtr ifStmt(const Value& v) {
      auto s = startStmt(StmtKind::If, v);
      s->value = readExpr(At(v, "cond"));
      s->body = readStmts(At(v, "then"));
      s->elseBody = readStmts(At(v, "else"));

      return s;
    }

    StmtPtr forStmt(const Value& v) {
      auto s = startStmt(StmtKind::For, v);
      s->names = readStrings(At(v, "names"));
      s->initializers = readExprs(At(v, "inits"));

      s->value = readExpr(At(v, "cond"));
      s->steps = readExprs(At(v, "steps"));
      s->body = readStmts(At(v, "body"));

      return s;
    }

    StmtPtr tryStmt(const Value& v) {
      auto s = startStmt(StmtKind::Try, v);
      s->body = readStmts(At(v, "try"));
      s->name = AsString(At(v, "name"));
      s->elseBody = readStmts(At(v, "catch"));

      return s;
    }

    // A select's arms: a list of keys, the name bound and a block for each.
    StmtPtr selectStmt(const Value& v) {
      auto s = startStmt(StmtKind::Select, v);
      s->value = readExpr(At(v, "value"));

      for (const Value& keys : AsArray(At(v, "caseKeys"))) {
        s->caseKeys.push_back(readExprs(keys));
      }

      s->names = readStrings(At(v, "names"));

      for (const Value& body : AsArray(At(v, "caseBodies"))) {
        s->caseBodies.push_back(readStmts(body));
      }

      s->elseBody = readStmts(At(v, "else"));

      return s;
    }

    StmtPtr readStructured(const String& kindName, const Value& v) {
      if (kindName == "HIf") {
        return ifStmt(v);
      }

      if (kindName == "HFor") {
        return forStmt(v);
      }

      if (kindName == "HTry") {
        return tryStmt(v);
      }

      return kindName == "HSelect" ? selectStmt(v) : nullptr;
    }

    StmtPtr labelStmt(const Value& v) {
      auto s = startStmt(StmtKind::Label, v);
      s->name = AsString(At(v, "name"));
      s->parameterNames = readStrings(At(v, "paramNames"));
      s->parameterTypes = readTypes(At(v, "paramTypes"));

      return s;
    }

    StmtPtr gotoStmt(const Value& v) {
      auto s = startStmt(StmtKind::Goto, v);
      s->name = AsString(At(v, "label"));
      s->arguments = readExprs(At(v, "args"));

      return s;
    }

    StmtPtr branchStmt(const Value& v) {
      auto s = startStmt(StmtKind::Branch, v);
      s->value = readExpr(At(v, "cond"));

      s->name = AsString(At(v, "thenLabel"));
      s->arguments = readExprs(At(v, "thenArgs"));
      s->elseName = AsString(At(v, "elseLabel"));
      s->elseArguments = readExprs(At(v, "elseArgs"));

      return s;
    }

    void readSwitchCases(Stmt& s, const Value& v) {
      for (const int n : readInts(At(v, "caseValues"))) {
        s.caseValues.push_back(n);
      }

      s.caseLabels = readStrings(At(v, "caseLabels"));

      for (const Value& a : AsArray(At(v, "caseArgs"))) {
        s.caseArguments.push_back(readExprs(a));
      }
    }

    StmtPtr switchStmt(const Value& v) {
      auto s = startStmt(StmtKind::Switch, v);
      s->value = readExpr(At(v, "value"));
      readSwitchCases(*s, v);
      s->name = AsString(At(v, "defaultLabel"));
      s->arguments = readExprs(At(v, "defaultArgs"));

      return s;
    }

    StmtPtr readJump(const String& kindName, const Value& v) {
      if (kindName == "HLabel") {
        return labelStmt(v);
      }

      if (kindName == "HGoto") {
        return gotoStmt(v);
      }

      if (kindName == "HBranch") {
        return branchStmt(v);
      }

      if (kindName == "HSwitch") {
        return switchStmt(v);
      }

      return nullptr;
    }

    StmtPtr valueStmt(StmtKind kind, const Value& v) {
      auto s = startStmt(kind, v);
      s->value = readExpr(At(v, "value"));

      return s;
    }

    StmtPtr readValueStmt(const String& kindName, const Value& v) {
      if (kindName == "HReturn") {
        return valueStmt(StmtKind::Return, v);
      }

      if (kindName == "HThrow") {
        return valueStmt(StmtKind::Throw, v);
      }

      if (kindName == "HYield") {
        return valueStmt(StmtKind::Yield, v);
      }

      if (kindName == "HExprStmt") {
        return valueStmt(StmtKind::ExprStmt, v);
      }

      return nullptr;
    }

    StmtPtr readStmt(const Value& v) {
      const String& kindName = AsString(At(v, "k"));

      if (StmtPtr s = readBinding(kindName, v)) {
        return s;
      }

      if (StmtPtr s = readStructured(kindName, v)) {
        return s;
      }

      if (StmtPtr s = readJump(kindName, v)) {
        return s;
      }

      if (StmtPtr s = readValueStmt(kindName, v)) {
        return s;
      }

      throw HirReadError("unknown statement '" + kindName + "'");
    }

    // ── declarations ──

    // A function that reads one kind of declaration and appends it to the module.
    using DeclReader = void (*)(const Value&, Module&);

    Vector<Parameter> readParameters(const Value& v) {
      const auto names = readStrings(At(v, "paramNames"));
      const auto types = readTypes(At(v, "paramTypes"));
      Vector<Parameter> out;

      for (size_t i = 0; i < names.size(); i++) {
        out.push_back(Parameter{names[i], types[i]});
      }

      return out;
    }

    // A declaration's optional `"module"` and `"internal"`.
    // One annotation: `{"name": ..., "args": [[key, value], ...]}`.
    Annotation readAnnotation(const Value& each) {
      Annotation made{AsString(At(each, "name"))};

      for (const Value& arg : AsArray(At(each, "args"))) {
        made.args.push_back({AsString(AsArray(arg)[0]), AsString(AsArray(arg)[1])});
      }

      return made;
    }

    Vector<Annotation> readAnnotations(const Value& v) {
      Vector<Annotation> out;

      if (!Has(v, "annotations")) {
        return out;
      }

      for (const Value& each : AsArray(At(v, "annotations"))) {
        out.push_back(readAnnotation(each));
      }

      return out;
    }

    // The optional `"origin"`, `"module"`, `"internal"` and `"annotations"` of a struct, function or interface.
    template <typename Decl>
    void readPlace(const Value& v, Decl& d) {
      d.origin = Has(v, "origin") ? AsString(At(v, "origin")) : String();
      d.module = Has(v, "module") ? AsString(At(v, "module")) : String();
      d.internal = Has(v, "internal") && AsBool(At(v, "internal"));
      d.annotations = readAnnotations(v);
    }

    String readOuter(const Value& v) {
      return Has(v, "outer") ? AsString(At(v, "outer")) : String();
    }

    // The optional `"generic"` of a struct or function.
    Generic readGeneric(const Value& v) {
      if (!Has(v, "generic")) {
        return Generic{};
      }

      const Value& g = At(v, "generic");

      return Generic{AsString(At(g, "of")), readTypes(At(g, "arguments")), readStrings(At(g, "params")), AsString(At(g, "notParametric"))};
    }

    // A generic as declared is marked `"isGeneric"` and goes beside the code, not in it.
    bool isGeneric(const Value& v) {
      return Has(v, "isGeneric") && AsBool(At(v, "isGeneric"));
    }

    // Field `i`'s default from the optional `"fieldDefaults"` list, or null.
    ExprPtr readDefault(const Value& v, size_t i) {
      if (!Has(v, "fieldDefaults") || IsNull(AsArray(At(v, "fieldDefaults"))[i])) {
        return nullptr;
      }

      return readExpr(AsArray(At(v, "fieldDefaults"))[i]);
    }

    void readStruct(const Value& v, Module& m) {
      StructDef def;
      def.name = AsString(At(v, "name"));
      const auto names = readStrings(At(v, "fieldNames"));
      const auto types = readTypes(At(v, "fieldTypes"));
      const auto& mutability = AsArray(At(v, "fieldMutable"));

      for (size_t i = 0; i < names.size(); i++) {
        def.fields.push_back(Field{names[i], types[i], AsBool(mutability[i]), readDefault(v, i)});
      }

      readPlace(v, def);
      def.outer = readOuter(v);
      def.generic = readGeneric(v);
      (isGeneric(v) ? m.genericStructs : m.structs).push_back(move(def));
    }

    Vector<InterfaceMethod> readMethods(const Value& v) {
      const auto names = readStrings(At(v, "methodNames"));
      const auto& params = AsArray(At(v, "methodParams"));
      const auto rets = readTypes(At(v, "methodRets"));
      Vector<InterfaceMethod> out;

      for (size_t i = 0; i < names.size(); i++) {
        out.push_back(InterfaceMethod{names[i], readTypes(params[i]), rets[i]});
      }

      return out;
    }

    void readInterface(const Value& v, Module& m) {
      InterfaceDef def;
      def.name = AsString(At(v, "name"));
      def.methods = readMethods(v);
      def.implementers = readStrings(At(v, "implementers"));

      readPlace(v, def);
      m.interfaces.push_back(move(def));
    }

    void readHostType(const Value& v, Module& m) {
      m.hostTypes.push_back(HostType{AsString(At(v, "name")), AsString(At(v, "base")), readTypes(At(v, "typeArgs")), AsBool(At(v, "isInterface")), readStrings(At(v, "bindings"))});
    }

    void readHostImport(const Value& v, Module& m) {
      m.hostImports.push_back(HostImport{AsString(At(v, "target")), AsString(At(v, "module")), AsString(At(v, "alias"))});
    }

    void readBridge(const Value& v, Module& m) {
      m.bridges.push_back(Bridge{readType(At(v, "poly")), AsString(At(v, "target")), AsString(At(v, "hostType")), AsString(At(v, "toHost")), AsString(At(v, "fromHost"))});
    }

    void readHostMember(const Value& v, Module& m) {
      m.hostMembers.push_back(HostMember{AsString(At(v, "function")), AsString(At(v, "owner")), AsString(At(v, "member")), AsBool(At(v, "isStatic")), AsBool(At(v, "hasResult")), readStrings(At(v, "bindings"))});
    }

    // The optional `"stage"` and `"roles"` of a kernel.
    void readKernel(const Value& v, FuncDef& def) {
      def.stage = StageNamed(AsString(At(v, "stage")));

      for (int role : readInts(At(v, "roles"))) {
        def.roles.push_back(static_cast<ParamRole>(role));
      }
    }

    void readFunction(const Value& v, Module& m) {
      FuncDef def{AsString(At(v, "name")), readParameters(v), readType(At(v, "ret")), readStmts(At(v, "body")), AsBool(At(v, "isCoroutine")), readSpan(v)};

      readPlace(v, def);
      def.outer = readOuter(v);

      if (Has(v, "stage")) {
        readKernel(v, def);
      }

      def.generic = readGeneric(v);
      (isGeneric(v) ? m.genericFunctions : m.functions).push_back(move(def));
    }

    void readExtern(const Value& v, Module& m) {
      m.externs.push_back(ExternDef{AsString(At(v, "name")), readParameters(v), readType(At(v, "ret"))});
    }

    void readGlobal(const Value& v, Module& m) {
      const String origin = Has(v, "origin") ? AsString(At(v, "origin")) : String();
      m.globals.push_back(GlobalDef{AsString(At(v, "name")), readType(At(v, "ty")), readExpr(At(v, "init")), origin});
    }

    DeclReader codeReader(const String& kindName) {
      if (kindName == "HStructDef") {
        return readStruct;
      }

      if (kindName == "HFuncDef") {
        return readFunction;
      }

      if (kindName == "HExternDef") {
        return readExtern;
      }

      if (kindName == "HGlobalDef") {
        return readGlobal;
      }

      return nullptr;
    }

    DeclReader typeReader(const String& kindName) {
      if (kindName == "HIfaceDef") {
        return readInterface;
      }

      if (kindName == "HHostType") {
        return readHostType;
      }

      if (kindName == "HBridge") {
        return readBridge;
      }

      if (kindName == "HHostMember") {
        return readHostMember;
      }

      if (kindName == "HHostImport") {
        return readHostImport;
      }

      return nullptr;
    }

    // Reads one declaration and records where it came in the file (NoteAdded), so the module keeps the written order.
    void readDecl(const Value& v, Module& m) {
      const String& kindName = AsString(At(v, "k"));
      DeclReader reader = codeReader(kindName);

      if (!reader) {
        reader = typeReader(kindName);
      }

      if (!reader) {
        throw HirReadError("unknown declaration '" + kindName + "'");
      }

      const Vector<size_t> before = Counts(m);
      reader(v, m);
      NoteAdded(m, before);
    }

    Module readModule(const Value& root) {
      Module module;

      for (const Value& d : AsArray(root)) {
        readDecl(d, module);
      }

      return module;
    }

  }  // namespace

  // Every mistake reaches the caller as a HirReadError, whether the JSON itself or the HIR in it was wrong.
  Module ReadHirJson(const String& text) {
    try {
      return readModule(Parse(text));
    } catch (const HirReadError&) {
      throw;
    } catch (const Exception& problem) {
      throw HirReadError(problem.what());
    }
  }

}  // namespace polyir
