// The checked HIR as JSON (HirJson.nina `programJson`): the same text, key for key. Every node is an object whose `k` is the name of its case and whose
// other keys are the case's fields; an expression ends with its `ty` and `at`, a statement with its `at`. One small function writes each case, found by
// the kind of the node in a table (the tables are in the order of the enums). The text being written is a `JsonOut`, handed to every function first.
#include "polyir/Std.hpp"
#include "polyir/HirJson.hpp"

#include "hir/FloatText.hpp"  // floatText, shared with the PBC writer

namespace polyir {

  namespace {

    // The escape JSON needs for `c`, or nullptr when `c` is written as itself.
    const char* escapeOf(char c) {
      if (c == '\\') {
        return "\\\\";
      }

      if (c == '"') {
        return "\\\"";
      }

      if (c == '\n') {
        return "\\n";
      }

      return c == '\t' ? "\\t" : nullptr;
    }

    // The JSON being written; every writer below takes it first, as `w`.
    struct JsonOut {
      String out;
    };

    using TypeWriter = void (*)(JsonOut&, const Type&);
    using ExprWriter = void (*)(JsonOut&, const Expr&);
    using StmtWriter = void (*)(JsonOut&, const Stmt&);
    using DeclWriter = void (*)(JsonOut&, const Module&, size_t);

    void type(JsonOut& w, const TypePtr& t);

    void expr(JsonOut& w, const ExprPtr& e);

    void stmt(JsonOut& w, const Stmt& s);

    void exprList(JsonOut& w, const Vector<ExprPtr>& items, size_t from);

    // ── pieces: `open` starts an object, each key function writes `,"key":value`, `finish` closes it ──

    void raw(JsonOut& w, const char* text) {
      w.out += text;
    }

    void quoted(JsonOut& w, const String& text) {
      w.out += '"';

      for (const char c : text) {
        const char* escape = escapeOf(c);

        if (escape != nullptr) {
          w.out += escape;
        } else {
          w.out += c;
        }
      }

      w.out += '"';
    }

    void open(JsonOut& w, const char* kind) {
      raw(w, "{\"k\":");
      quoted(w, kind);
    }

    void finish(JsonOut& w) {
      raw(w, "}");
    }

    void key(JsonOut& w, const char* name) {
      raw(w, ",\"");
      raw(w, name);
      raw(w, "\":");
    }

    void stringKey(JsonOut& w, const char* name, const String& v) {
      key(w, name);
      quoted(w, v);
    }

    void numberKey(JsonOut& w, const char* name, int64_t v) {
      key(w, name);
      w.out += to_string(v);
    }

    void flagKey(JsonOut& w, const char* name, bool v) {
      key(w, name);
      raw(w, v ? "true" : "false");
    }

    void spanKey(JsonOut& w, const char* name, const Span& at) {
      key(w, name);
      w.out += "[" + to_string(at.line) + "," + to_string(at.column) + "]";
    }

    // A JSON array of `items`, each written by `each`. A template so one function serves every kind of list (types,
    // statements, strings, ...); `each` is a small lambda that writes one item.
    template <typename T, typename F>
    void list(JsonOut& w, const Vector<T>& items, F each) {
      raw(w, "[");

      for (size_t i = 0; i < items.size(); i++) {
        if (i > 0) {
          raw(w, ",");
        }

        each(items[i]);
      }

      raw(w, "]");
    }

    void typeKey(JsonOut& w, const char* name, const TypePtr& t) {
      key(w, name);
      type(w, t);
    }

    void exprKey(JsonOut& w, const char* name, const ExprPtr& e) {
      key(w, name);
      e ? expr(w, e) : raw(w, "null");
    }

    void exprListKey(JsonOut& w, const char* name, const Vector<ExprPtr>& items, size_t from = 0) {
      key(w, name);
      exprList(w, items, from);
    }

    void exprList(JsonOut& w, const Vector<ExprPtr>& items, size_t from) {
      raw(w, "[");

      for (size_t i = from; i < items.size(); i++) {
        if (i > from) {
          raw(w, ",");
        }

        expr(w, items[i]);
      }

      raw(w, "]");
    }

    void stmtListKey(JsonOut& w, const char* name, const StmtList& items) {
      key(w, name);
      list(w, items, [&](const StmtPtr& s) { stmt(w, *s); });
    }

    void typeList(JsonOut& w, const Vector<TypePtr>& items) {
      list(w, items, [&](const TypePtr& t) { type(w, t); });
    }

    void typeListKey(JsonOut& w, const char* name, const Vector<TypePtr>& items) {
      key(w, name);
      typeList(w, items);
    }

    void stringListKey(JsonOut& w, const char* name, const Vector<String>& items) {
      key(w, name);
      list(w, items, [&](const String& s) { quoted(w, s); });
    }

    void intListKey(JsonOut& w, const char* name, const Vector<int>& items) {
      key(w, name);
      list(w, items, [&](int i) { w.out += to_string(i); });
    }

    void exprTail(JsonOut& w, const Expr& e) {
      typeKey(w, "ty", e.type);
      spanKey(w, "at", e.at);
      finish(w);
    }

    void stmtTail(JsonOut& w, const Stmt& s) {
      spanKey(w, "at", s.at);
      finish(w);
    }

    void params(JsonOut& w, const Vector<Parameter>& items) {
      key(w, "paramNames");
      list(w, items, [&](const Parameter& p) { quoted(w, p.name); });
      key(w, "paramTypes");
      list(w, items, [&](const Parameter& p) { type(w, p.type); });
    }

    void signature(JsonOut& w, const char* kind, const String& name, const Vector<Parameter>& parameters, const TypePtr& result) {
      open(w, kind);
      stringKey(w, "name", name);
      params(w, parameters);
      typeKey(w, "ret", result);
    }

    // `"fieldDefaults"`, each field's default or null, only when some field has one: a struct without defaults writes as before.
    void fieldDefaults(JsonOut& w, const Vector<Field>& items) {
      const bool any = any_of(items.begin(), items.end(), [](const Field& f) { return f.initial != nullptr; });

      if (any) {
        key(w, "fieldDefaults");
        list(w, items, [&](const Field& f) { f.initial ? expr(w, f.initial) : raw(w, "null"); });
      }
    }

    void fields(JsonOut& w, const Vector<Field>& items) {
      key(w, "fieldNames");
      list(w, items, [&](const Field& f) { quoted(w, f.name); });

      key(w, "fieldTypes");
      list(w, items, [&](const Field& f) { type(w, f.type); });

      key(w, "fieldMutable");
      list(w, items, [&](const Field& f) { raw(w, f.isMutable ? "true" : "false"); });
      fieldDefaults(w, items);
    }

    void methods(JsonOut& w, const InterfaceDef& d) {
      key(w, "methodNames");
      list(w, d.methods, [&](const InterfaceMethod& x) { quoted(w, x.name); });

      key(w, "methodParams");
      list(w, d.methods, [&](const InterfaceMethod& x) { typeList(w, x.parameters); });

      key(w, "methodRets");
      list(w, d.methods, [&](const InterfaceMethod& x) { type(w, x.result); });
    }

    // ── types ──

    void bitsType(JsonOut& w, const char* kind, const Type& t) {
      open(w, kind);
      numberKey(w, "bits", t.bits);
      finish(w);
    }

    void writeIntType(JsonOut& w, const Type& t) {
      bitsType(w, "HInt", t);
    }

    void writeUIntType(JsonOut& w, const Type& t) {
      bitsType(w, "HUInt", t);
    }

    void writeFloatType(JsonOut& w, const Type& t) {
      bitsType(w, "HFloat", t);
    }

    void writeStringType(JsonOut& w, const Type&) {
      open(w, "HString");
      finish(w);
    }

    void writeVoidType(JsonOut& w, const Type&) {
      open(w, "HVoid");
      finish(w);
    }

    void writeStructType(JsonOut& w, const Type& t) {
      open(w, "HStruct");
      stringKey(w, "name", t.name);
      finish(w);
    }

    void writePointerType(JsonOut& w, const Type& t) {
      open(w, "HPtr");
      typeKey(w, "elem", t.element);
      finish(w);
    }

    void writeFunctionType(JsonOut& w, const Type& t) {
      open(w, "HFuncPtr");
      typeListKey(w, "params", t.parameters);
      typeKey(w, "ret", t.element);
      finish(w);
    }

    void writeVecType(JsonOut& w, const Type& t) {
      open(w, "HVec");
      typeKey(w, "elem", t.element);
      numberKey(w, "count", t.size);
      finish(w);
    }

    void writeMatrixType(JsonOut& w, const Type& t) {
      open(w, "HMat");
      typeKey(w, "elem", t.element);
      numberKey(w, "size", t.size);
      finish(w);
    }

    void writeParamType(JsonOut& w, const Type& t) {
      open(w, "HParam");
      stringKey(w, "name", t.name);
      finish(w);
    }

    void writeFixedArrayType(JsonOut& w, const Type& t) {
      open(w, "HFixedArray");
      typeKey(w, "elem", t.element);
      numberKey(w, "size", t.size);
      finish(w);
    }

    void writeBufferType(JsonOut& w, const Type& t) {
      open(w, "HBuffer");
      typeKey(w, "elem", t.element);
      numberKey(w, "size", t.size);
      finish(w);
    }

    void writeDynamicArrayType(JsonOut& w, const Type& t) {
      open(w, "HDarray");
      typeKey(w, "elem", t.element);
      flagKey(w, "mutable", t.isMutable);
      finish(w);
    }

    void writeSparseType(JsonOut& w, const Type& t) {
      open(w, "HSparse");
      typeKey(w, "elem", t.element);
      finish(w);
    }

    void writeSetType(JsonOut& w, const Type& t) {
      open(w, "HSet");
      typeKey(w, "elem", t.element);
      flagKey(w, "mutable", t.isMutable);
      finish(w);
    }

    void writeTensorType(JsonOut& w, const Type& t) {
      open(w, "HTensor");
      typeKey(w, "elem", t.element);
      intListKey(w, "dims", t.dims);
      finish(w);
    }

    void writeClosureType(JsonOut& w, const Type& t) {
      open(w, "HClosure");
      typeListKey(w, "params", t.parameters);
      typeKey(w, "ret", t.element);
      finish(w);
    }

    void writeVariantType(JsonOut& w, const Type& t) {
      open(w, "HVariant");
      typeListKey(w, "cases", t.parameters);
      finish(w);
    }

    void writeEitherType(JsonOut& w, const Type& t) {
      open(w, "HEither");
      typeKey(w, "ok", t.element);
      typeKey(w, "err", t.second);
      finish(w);
    }

    void writeOptionalType(JsonOut& w, const Type& t) {
      open(w, "HOptional");
      typeKey(w, "elem", t.element);
      finish(w);
    }

    void writeCoroutineType(JsonOut& w, const Type& t) {
      open(w, "HCoro");
      typeKey(w, "yields", t.element);
      finish(w);
    }

    void writeHashmapType(JsonOut& w, const Type& t) {
      open(w, "HHashmap");
      typeKey(w, "key", t.element);
      typeKey(w, "value", t.second);
      flagKey(w, "mutable", t.isMutable);
      finish(w);
    }

    void writeGridType(JsonOut& w, const Type& t) {
      open(w, "HGrid");
      typeKey(w, "elem", t.element);
      numberKey(w, "rows", t.size);
      numberKey(w, "cols", t.cols);
      finish(w);
    }

    // Indexed by TypeKind (see `type` below), so the entries must stay in the enum's order.
    const TypeWriter kTypeWriters[] = {
      writeIntType, writeUIntType, writeFloatType, writeStringType, writeVoidType, writeStructType,
      writePointerType, writeFunctionType, writeVecType, writeFixedArrayType, writeBufferType, writeDynamicArrayType,
      writeSparseType, writeHashmapType, writeSetType, writeGridType, writeTensorType, writeClosureType,
      writeVariantType, writeEitherType, writeOptionalType, writeCoroutineType, writeMatrixType, writeParamType
    };

    void type(JsonOut& w, const TypePtr& t) {
      kTypeWriters[static_cast<size_t>(t->kind)](w, *t);
    }

    // ── expressions ──

    void operand(JsonOut& w, const char* name, const Expr& e, size_t index) {
      key(w, name);
      expr(w, e.operands[index]);
    }

    void writeConstIntExpr(JsonOut& w, const Expr& e) {
      open(w, "HConstInt");
      numberKey(w, "value", e.intValue);
      exprTail(w, e);
    }

    void writeConstFloatExpr(JsonOut& w, const Expr& e) {
      open(w, "HConstFloat");
      stringKey(w, "value", FloatText(e.floatValue));
      exprTail(w, e);
    }

    void writeConstBoolExpr(JsonOut& w, const Expr& e) {
      open(w, "HConstBool");
      flagKey(w, "value", e.boolValue);
      exprTail(w, e);
    }

    void writeConstStringExpr(JsonOut& w, const Expr& e) {
      open(w, "HConstString");
      stringKey(w, "text", e.text);
      exprTail(w, e);
    }

    void writeLocalExpr(JsonOut& w, const Expr& e) {
      open(w, "HLocal");
      stringKey(w, "name", e.name);
      exprTail(w, e);
    }

    void writeGlobalExpr(JsonOut& w, const Expr& e) {
      open(w, "HGlobalRef");
      stringKey(w, "name", e.name);
      exprTail(w, e);
    }

    void writeFuncRefExpr(JsonOut& w, const Expr& e) {
      open(w, "HFuncRef");
      stringKey(w, "name", e.name);
      exprTail(w, e);
    }

    void writeUnaryExpr(JsonOut& w, const Expr& e) {
      open(w, "HUnary");
      stringKey(w, "op", UnOpName(e.unaryOp));
      operand(w, "operand", e, 0);
      exprTail(w, e);
    }

    void writeCastExpr(JsonOut& w, const Expr& e) {
      open(w, "HCast");
      operand(w, "operand", e, 0);
      exprTail(w, e);
    }

    void writeCallExpr(JsonOut& w, const Expr& e) {
      open(w, "HCall");
      stringKey(w, "callee", e.name);
      exprListKey(w, "args", e.operands);
      exprTail(w, e);
    }

    void writeFieldGetExpr(JsonOut& w, const Expr& e) {
      open(w, "HFieldGet");
      operand(w, "base", e, 0);
      numberKey(w, "index", e.fieldIndex);
      exprTail(w, e);
    }

    void writeLoadSlotExpr(JsonOut& w, const Expr& e) {
      open(w, "HLoadSlot");
      stringKey(w, "name", e.name);
      exprTail(w, e);
    }

    void writeCoerceExpr(JsonOut& w, const Expr& e) {
      open(w, "HCoerce");
      operand(w, "operand", e, 0);
      exprTail(w, e);
    }

    void writeIndexGetExpr(JsonOut& w, const Expr& e) {
      open(w, "HIndexGet");
      operand(w, "base", e, 0);
      exprListKey(w, "indices", e.operands, 1);
      exprTail(w, e);
    }

    void writeVecLitExpr(JsonOut& w, const Expr& e) {
      open(w, "HVecLit");
      exprListKey(w, "elems", e.operands);
      exprTail(w, e);
    }

    void writeNewContainerExpr(JsonOut& w, const Expr& e) {
      open(w, "HNewContainer");
      exprListKey(w, "sizeArgs", e.operands);
      exprTail(w, e);
    }

    void writeVariantLitExpr(JsonOut& w, const Expr& e) {
      open(w, "HVariantLit");
      numberKey(w, "caseIndex", e.fieldIndex);
      operand(w, "value", e, 0);
      exprTail(w, e);
    }

    void writeEitherLitExpr(JsonOut& w, const Expr& e) {
      open(w, "HEitherLit");
      flagKey(w, "isOk", e.boolValue);
      operand(w, "value", e, 0);
      exprTail(w, e);
    }

    void writeUnsupportedExpr(JsonOut& w, const Expr& e) {
      open(w, "HUnsupported");
      stringKey(w, "what", e.text);
      exprTail(w, e);
    }

    void writeBinaryExpr(JsonOut& w, const Expr& e) {
      open(w, "HBinary");
      stringKey(w, "op", BinOpName(e.binaryOp));
      operand(w, "left", e, 0);
      operand(w, "right", e, 1);
      exprTail(w, e);
    }

    void writeStructLitExpr(JsonOut& w, const Expr& e) {
      open(w, "HStructLit");
      stringKey(w, "name", e.name);
      intListKey(w, "fieldIndices", e.fieldIndices);
      exprListKey(w, "fields", e.operands);
      exprTail(w, e);
    }

    void writeCallIndirectExpr(JsonOut& w, const Expr& e) {
      open(w, "HCallIndirect");
      operand(w, "callee", e, 0);
      exprListKey(w, "args", e.operands, 1);
      exprTail(w, e);
    }

    void writeCallBuiltinExpr(JsonOut& w, const Expr& e) {
      open(w, "HCallBuiltin");
      stringKey(w, "op", BuiltinName(e.builtin));
      operand(w, "receiver", e, 0);
      exprListKey(w, "args", e.operands, 1);
      exprTail(w, e);
    }

    void writeClosureLitExpr(JsonOut& w, const Expr& e) {
      open(w, "HClosureLit");
      stringListKey(w, "paramNames", e.parameterNames);
      typeListKey(w, "paramTypes", e.parameterTypes);

      stmtListKey(w, "body", e.closureBody);
      stringListKey(w, "captures", e.captures);
      exprTail(w, e);
    }

    // Indexed by ExprKind (see `expr` below), so the entries must stay in the enum's order.
    const ExprWriter kExprWriters[] = {
      writeConstIntExpr, writeConstFloatExpr, writeConstBoolExpr, writeConstStringExpr, writeLocalExpr,
      writeGlobalExpr, writeFuncRefExpr, writeBinaryExpr, writeUnaryExpr, writeCastExpr, writeCallExpr,
      writeStructLitExpr, writeFieldGetExpr, writeLoadSlotExpr, writeCoerceExpr, writeCallIndirectExpr,
      writeCallBuiltinExpr, writeIndexGetExpr, writeVecLitExpr, writeNewContainerExpr, writeClosureLitExpr,
      writeVariantLitExpr, writeEitherLitExpr, writeUnsupportedExpr
    };

    void expr(JsonOut& w, const ExprPtr& e) {
      kExprWriters[static_cast<size_t>(e->kind)](w, *e);
    }

    // ── statements ──

    void writeLetStmt(JsonOut& w, const Stmt& s) {
      open(w, "HLet");
      stringKey(w, "name", s.name);
      typeKey(w, "ty", s.type);
      exprKey(w, "value", s.value);
      stmtTail(w, s);
    }

    void writeVarStmt(JsonOut& w, const Stmt& s) {
      open(w, "HVarDecl");
      stringKey(w, "name", s.name);
      typeKey(w, "ty", s.type);
      exprKey(w, "init", s.value);
      stmtTail(w, s);
    }

    void writeAssignStmt(JsonOut& w, const Stmt& s) {
      open(w, "HAssign");
      exprKey(w, "target", s.target);
      exprKey(w, "value", s.value);
      stmtTail(w, s);
    }

    void writeReturnStmt(JsonOut& w, const Stmt& s) {
      open(w, "HReturn");
      exprKey(w, "value", s.value);
      stmtTail(w, s);
    }

    void writeExprStmt(JsonOut& w, const Stmt& s) {
      open(w, "HExprStmt");
      exprKey(w, "value", s.value);
      stmtTail(w, s);
    }

    void writeGotoStmt(JsonOut& w, const Stmt& s) {
      open(w, "HGoto");
      stringKey(w, "label", s.name);
      exprListKey(w, "args", s.arguments);
      stmtTail(w, s);
    }

    void writeTryStmt(JsonOut& w, const Stmt& s) {
      open(w, "HTry");
      stmtListKey(w, "try", s.body);
      stringKey(w, "name", s.name);
      stmtListKey(w, "catch", s.elseBody);
      stmtTail(w, s);
    }

    void writeThrowStmt(JsonOut& w, const Stmt& s) {
      open(w, "HThrow");
      exprKey(w, "value", s.value);
      stmtTail(w, s);
    }

    void writeYieldStmt(JsonOut& w, const Stmt& s) {
      open(w, "HYield");
      exprKey(w, "value", s.value);
      stmtTail(w, s);
    }

    void writeIfStmt(JsonOut& w, const Stmt& s) {
      open(w, "HIf");
      exprKey(w, "cond", s.value);
      stmtListKey(w, "then", s.body);
      stmtListKey(w, "else", s.elseBody);
      stmtTail(w, s);
    }

    void writeForStmt(JsonOut& w, const Stmt& s) {
      open(w, "HFor");
      stringListKey(w, "names", s.names);
      exprListKey(w, "inits", s.initializers);

      exprKey(w, "cond", s.value);
      exprListKey(w, "steps", s.steps);
      stmtListKey(w, "body", s.body);
      stmtTail(w, s);
    }

    void writeLabelStmt(JsonOut& w, const Stmt& s) {
      open(w, "HLabel");
      stringKey(w, "name", s.name);
      stringListKey(w, "paramNames", s.parameterNames);
      typeListKey(w, "paramTypes", s.parameterTypes);
      stmtTail(w, s);
    }

    void writeBranchStmt(JsonOut& w, const Stmt& s) {
      open(w, "HBranch");
      exprKey(w, "cond", s.value);

      stringKey(w, "thenLabel", s.name);
      exprListKey(w, "thenArgs", s.arguments);
      stringKey(w, "elseLabel", s.elseName);
      exprListKey(w, "elseArgs", s.elseArguments);
      stmtTail(w, s);
    }

    void switchCases(JsonOut& w, const Stmt& s) {
      key(w, "caseValues");
      list(w, s.caseValues, [&](int64_t v) { w.out += to_string(v); });
      stringListKey(w, "caseLabels", s.caseLabels);
      key(w, "caseArgs");
      list(w, s.caseArguments, [&](const Vector<ExprPtr>& args) { exprList(w, args, 0); });
    }

    void writeSwitchStmt(JsonOut& w, const Stmt& s) {
      open(w, "HSwitch");
      exprKey(w, "value", s.value);
      switchCases(w, s);

      stringKey(w, "defaultLabel", s.name);
      exprListKey(w, "defaultArgs", s.arguments);
      stmtTail(w, s);
    }

    // `"caseKeys"` and `"caseBodies"`: one list of keys and one block per arm.
    void selectArms(JsonOut& w, const Stmt& s) {
      key(w, "caseKeys");
      list(w, s.caseKeys, [&](const Vector<ExprPtr>& keys) { exprList(w, keys, 0); });
      stringListKey(w, "names", s.names);
      key(w, "caseBodies");
      list(w, s.caseBodies, [&](const StmtList& body) { list(w, body, [&](const StmtPtr& each) { stmt(w, *each); }); });
    }

    void writeSelectStmt(JsonOut& w, const Stmt& s) {
      open(w, "HSelect");
      exprKey(w, "value", s.value);
      selectArms(w, s);
      stmtListKey(w, "else", s.elseBody);
      stmtTail(w, s);
    }

    // Indexed by StmtKind (see `stmt` below), so the entries must stay in the enum's order.
    const StmtWriter kStmtWriters[] = {
      writeLetStmt, writeVarStmt, writeAssignStmt, writeIfStmt, writeForStmt, writeReturnStmt, writeExprStmt,
      writeLabelStmt, writeGotoStmt, writeBranchStmt, writeSwitchStmt, writeTryStmt, writeThrowStmt, writeYieldStmt, writeSelectStmt
    };

    void stmt(JsonOut& w, const Stmt& s) {
      kStmtWriters[static_cast<size_t>(s.kind)](w, s);
    }

    // ── declarations ──

    // `"annotations"`: each one's name and its arguments, name and value as written.
    void annotationsKey(JsonOut& w, const Vector<Annotation>& annotations) {
      key(w, "annotations");
      list(w, annotations, [&](const Annotation& a) {
        raw(w, "{\"name\":");
        quoted(w, a.name);
        raw(w, ",\"args\":");
        list(w, a.args, [&](const Pair<String, String>& arg) { raw(w, "["); quoted(w, arg.first); raw(w, ","); quoted(w, arg.second); raw(w, "]"); });
        raw(w, "}");
      });
    }

    // Where a struct, function or interface sits: optional `"origin"` (the imported file), `"module"`, `"internal"`, `"outer"` and
    // `"annotations"`. A declaration with none of them has none of the keys, so a program without them reads and writes as before.
    template <typename Decl>
    void placeFields(JsonOut& w, const Decl& d, const String& outer) {
      if (!d.origin.empty()) {
        stringKey(w, "origin", d.origin);
      }

      if (!d.module.empty()) {
        stringKey(w, "module", d.module);
      }

      if (d.internal) {
        flagKey(w, "internal", true);
      }

      if (!outer.empty()) {
        stringKey(w, "outer", outer);
      }

      if (!d.annotations.empty()) {
        annotationsKey(w, d.annotations);
      }
    }

    // The optional `"generic"`: the generic a copy was made from and its type arguments, a generic's own parameters, and why it is not
    // parametric. A program without generics has none.
    void genericKey(JsonOut& w, const Generic& g) {
      if (!HasGeneric(g)) {
        return;
      }

      raw(w, ",\"generic\":{\"of\":");
      quoted(w, g.of);
      typeListKey(w, "arguments", g.arguments);
      stringListKey(w, "params", g.params);

      stringKey(w, "notParametric", g.notParametric);
      raw(w, "}");
    }

    void structDecl(JsonOut& w, const StructDef& d) {
      open(w, "HStructDef");
      stringKey(w, "name", d.name);
      fields(w, d.fields);

      if (!d.owner.empty()) {
        stringKey(w, "owner", d.owner);
      }

      placeFields(w, d, d.outer);
      genericKey(w, d.generic);
    }

    void writeStructDecl(JsonOut& w, const Module& m, size_t i) {
      structDecl(w, m.structs[i]);
    }

    // The optional `"display"`: how an overload reads (`greet` for `greet__ov__String_Int`); other functions have none.
    void displayField(JsonOut& w, const FuncDef& d) {
      if (!d.display.empty()) {
        stringKey(w, "display", d.display);
      }
    }

    // A kernel adds `"stage"` and `"roles"`; any other function has neither, so a program without kernels reads and writes as it always did.
    void kernelFields(JsonOut& w, const FuncDef& d) {
      if (d.stage == KernelStage::None) {
        return;
      }

      key(w, "stage");
      quoted(w, StageName(d.stage));
      key(w, "roles");
      list(w, d.roles, [&](ParamRole r) { w.out += to_string(static_cast<int>(r)); });
    }

    // Where a function came from and how it shows: its display name, place and kernel stage.
    void provenanceFields(JsonOut& w, const FuncDef& d) {
      displayField(w, d);
      placeFields(w, d, d.outer);
      kernelFields(w, d);
    }

    void functionDecl(JsonOut& w, const FuncDef& d) {
      signature(w, "HFuncDef", d.name, d.parameters, d.result);
      stmtListKey(w, "body", d.body);

      flagKey(w, "isCoroutine", d.isCoroutine);
      provenanceFields(w, d);
      genericKey(w, d.generic);
      spanKey(w, "at", d.at);
    }

    void writeFunctionDecl(JsonOut& w, const Module& m, size_t i) {
      functionDecl(w, m.functions[i]);
    }

    void writeExternDecl(JsonOut& w, const Module& m, size_t i) {
      const ExternDef& d = m.externs[i];
      signature(w, "HExternDef", d.name, d.parameters, d.result);
    }

    void writeGlobalDecl(JsonOut& w, const Module& m, size_t i) {
      const GlobalDef& d = m.globals[i];
      open(w, "HGlobalDef");
      stringKey(w, "name", d.name);
      typeKey(w, "ty", d.type);
      exprKey(w, "init", d.initializer);

      if (!d.origin.empty()) {
        stringKey(w, "origin", d.origin);
      }
    }

    void writeInterfaceDecl(JsonOut& w, const Module& m, size_t i) {
      const InterfaceDef& d = m.interfaces[i];
      open(w, "HIfaceDef");
      stringKey(w, "name", d.name);
      methods(w, d);
      stringListKey(w, "implementers", d.implementers);

      placeFields(w, d, String());
    }

    void writeHostTypeDecl(JsonOut& w, const Module& m, size_t i) {
      const HostType& d = m.hostTypes[i];
      open(w, "HHostType");
      stringKey(w, "name", d.name);

      stringKey(w, "base", d.base);
      typeListKey(w, "typeArgs", d.typeArguments);
      flagKey(w, "isInterface", d.isInterface);
      stringListKey(w, "bindings", d.bindings);
    }

    void writeBridgeDecl(JsonOut& w, const Module& m, size_t i) {
      const Bridge& d = m.bridges[i];
      open(w, "HBridge");
      typeKey(w, "poly", d.polyirType);

      stringKey(w, "target", d.target);
      stringKey(w, "hostType", d.hostType);
      stringKey(w, "toHost", d.toHost);
      stringKey(w, "fromHost", d.fromHost);
    }

    void hostMember(JsonOut& w, const HostMember& d) {
      open(w, "HHostMember");
      stringKey(w, "function", d.function);
      stringKey(w, "owner", d.owner);
      stringKey(w, "member", d.member);

      flagKey(w, "isStatic", d.isStatic);
      flagKey(w, "hasResult", d.hasResult);
      stringListKey(w, "bindings", d.bindings);
    }

    void writeHostMemberDecl(JsonOut& w, const Module& m, size_t i) {
      hostMember(w, m.hostMembers[i]);
    }

    void writeHostImportDecl(JsonOut& w, const Module& m, size_t i) {
      const HostImport& d = m.hostImports[i];
      open(w, "HHostImport");
      stringKey(w, "target", d.target);
      stringKey(w, "module", d.module);
      stringKey(w, "alias", d.alias);
    }

    // Indexed by DeclKind (see `decl` below), so the entries must stay in the enum's order.
    const DeclWriter kDeclWriters[] = {
      writeStructDecl, writeInterfaceDecl, writeHostTypeDecl, writeBridgeDecl, writeHostMemberDecl, writeExternDecl,
      writeGlobalDecl, writeFunctionDecl, writeHostImportDecl
    };

    void decl(JsonOut& w, const DeclRef& ref, const Module& m) {
      kDeclWriters[static_cast<size_t>(ref.kind)](w, m, ref.index);
      finish(w);
    }

    // One declaration of the array, after the separator from the one before it.
    void separatedDecl(JsonOut& w, const Module& module, size_t i) {
      if (i > 0) {
        raw(w, ",\n");
      }

      decl(w, module.order[i], module);
    }

    // The generics as declared follow the code, each marked `"isGeneric"`, structs first.
    void genericDecls(JsonOut& w, const Module& module) {
      for (const StructDef& d : module.genericStructs) {
        raw(w, ",\n");
        structDecl(w, d);
        flagKey(w, "isGeneric", true);
        finish(w);
      }

      for (const FuncDef& d : module.genericFunctions) {
        raw(w, ",\n");
        functionDecl(w, d);
        flagKey(w, "isGeneric", true);
        finish(w);
      }
    }

    String write(const Module& module) {
      JsonOut w;
      raw(w, "[");

      for (size_t i = 0; i < module.order.size(); i++) {
        separatedDecl(w, module, i);
      }

      genericDecls(w, module);
      raw(w, "]");

      return move(w.out);
    }

    // A module built by hand has no recorded order (or one whose length does not match its declarations). Its declarations
    // then go out grouped by kind, in DeclKind order.
    Module ordered(const Module& module) {
      Module copy = module;
      copy.order.clear();
      NoteAdded(copy, Vector<size_t>(Counts(module).size(), 0));

      return copy;
    }

  }  // namespace

  String WriteHirJson(const Module& module) {
    if (module.order.size() == DeclarationCount(module)) {
      return write(module);
    }

    return write(ordered(module));
  }

}  // namespace polyir
