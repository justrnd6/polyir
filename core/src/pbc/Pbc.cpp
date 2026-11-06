// `.pbc` in C++: the writer and reader of the bytecode the Nina implementation defines in lang/src/pbc/ (PbcFormat.nina is the byte-level
// contract, PbcWrite.nina/PbcRead.nina the tag maps — the numbers here mirror those files exactly and are frozen at v1). The Nina HDecl stream
// maps onto Module the same way HirJson.cpp maps the JSON: HKernel and HOrigin records attach to the function they name instead of being
// declarations of their own.
#include "polyir/Std.hpp"
#include "polyir/Pbc.hpp"

#include "hir/FloatText.hpp"

namespace polyir {

  namespace {

    constexpr uint8_t kMagic[4] = {80, 66, 67, 1};
    constexpr int64_t kVersion = 1;
    constexpr uint8_t kSectionDecls = 1;
    constexpr uint8_t kSectionPlaces = 2;  // this C++ format's own: the module and `internal` mark of each declaration (other readers skip it)
    constexpr uint8_t kSectionGenerics = 3;  // this C++ format's own too: the generics as declared and what each copy is to its generic
    constexpr int64_t kCountLimit = 100000000;

    // ── the tags: each list is in tag order (tag 1 is the first entry), the order of PbcWrite.nina's `ty`, `expr` and `stmt` (the Nina HType,
    // HExpr and HStmt cases), not the order of the C++ enums. Writer and reader both take their tags from here. ──

    const Vector<TypeKind> kTypeKinds = {TypeKind::Int, TypeKind::UInt, TypeKind::Float, TypeKind::String, TypeKind::Void, TypeKind::Struct,
      TypeKind::Pointer, TypeKind::Vec, TypeKind::FixedArray, TypeKind::Buffer, TypeKind::DynamicArray, TypeKind::Sparse, TypeKind::Hashmap,
      TypeKind::Set, TypeKind::Grid, TypeKind::Tensor, TypeKind::Function, TypeKind::Closure, TypeKind::Variant, TypeKind::Either,
      TypeKind::Optional, TypeKind::Coroutine, TypeKind::Matrix, TypeKind::Param};

    const Vector<ExprKind> kExprKinds = {ExprKind::ConstInt, ExprKind::ConstFloat, ExprKind::ConstBool, ExprKind::ConstString, ExprKind::Local,
      ExprKind::LoadSlot, ExprKind::Global, ExprKind::FuncRef, ExprKind::Binary, ExprKind::Unary, ExprKind::Cast, ExprKind::Coerce, ExprKind::Call,
      ExprKind::CallIndirect, ExprKind::CallBuiltin, ExprKind::StructLit, ExprKind::FieldGet, ExprKind::IndexGet, ExprKind::VecLit,
      ExprKind::NewContainer, ExprKind::ClosureLit, ExprKind::VariantLit, ExprKind::EitherLit, ExprKind::Unsupported};

    const Vector<StmtKind> kStmtKinds = {StmtKind::Let, StmtKind::Var, StmtKind::Assign, StmtKind::If, StmtKind::For, StmtKind::Switch, StmtKind::Label,
      StmtKind::Goto, StmtKind::Branch, StmtKind::Return, StmtKind::Try, StmtKind::Throw, StmtKind::Yield, StmtKind::ExprStmt, StmtKind::Select};

    // The declaration tags, PbcWrite.nina's HDecl cases. Tag 11 is the Nina format's HDisplay (a function's display name): this C++ writer
    // never writes it, and this reader rejects it as an unknown declaration tag.
    constexpr int kDeclStruct = 1;
    constexpr int kDeclFunction = 2;
    constexpr int kDeclExtern = 3;
    constexpr int kDeclGlobal = 4;
    constexpr int kDeclInterface = 5;
    constexpr int kDeclHostType = 6;
    constexpr int kDeclBridge = 7;
    constexpr int kDeclHostMember = 8;
    constexpr int kDeclKernel = 9;
    constexpr int kDeclOrigin = 10;
    constexpr int kDeclHostImport = 12;

    // The tag of a kind: its position in the list, counted from 1. These two are templates only so the three tag lists above can share them.
    template <typename Kind>
    int tagOf(const Vector<Kind>& kinds, Kind kind) {
      for (size_t i = 0; i < kinds.size(); i++) {
        if (kinds[i] == kind) {
          return static_cast<int>(i) + 1;
        }
      }

      return 0;
    }

    // The kind a tag names, or a read error when no kind has that tag.
    template <typename Kind>
    Kind kindOfTag(const Vector<Kind>& kinds, int tag, const char* what) {
      if (tag < 1 || tag > static_cast<int>(kinds.size())) {
        throw HirReadError(String("unknown ") + what + " tag " + to_string(tag));
      }

      return kinds[tag - 1];
    }

    // ── code points: the PBC string is Nina's view of a string, one varint per code point ──

    // A UTF-8 sequence being decoded: the bits read so far and how many continuation bytes follow.
    struct Decoded {
      int64_t point = 0;
      int extra = 0;
    };

    Decoded codePointAt(const String& text, size_t i);

    Decoded leadOf(unsigned char c);

    void appendEncoded(String& out, int lead, int64_t point, int continuations);

    Vector<int64_t> codePointsOf(const String& text) {
      Vector<int64_t> out;

      for (size_t i = 0; i < text.size();) {
        const Decoded decoded = codePointAt(text, i);
        out.push_back(decoded.point);
        i += decoded.extra + 1;
      }

      return out;
    }

    Decoded codePointAt(const String& text, size_t i) {
      Decoded decoded = leadOf(static_cast<unsigned char>(text[i]));

      // A lead byte cut off by the end of the text becomes one code point of its own payload bits (a last 0xC3 reads as U+0003).
      if (decoded.extra > 0 && i + decoded.extra >= text.size()) {
        decoded.extra = 0;
      }

      for (int k = 1; k <= decoded.extra; k++) {
        decoded.point = (decoded.point << 6) | (static_cast<unsigned char>(text[i + k]) & 0x3F);
      }

      return decoded;
    }

    Decoded leadOf(unsigned char c) {
      if (c >= 0xF0) {
        return Decoded{c & 0x07, 3};
      }

      if (c >= 0xE0) {
        return Decoded{c & 0x0F, 2};
      }

      if (c >= 0xC0) {
        return Decoded{c & 0x1F, 1};
      }

      return Decoded{c, 0};
    }

    void appendCodePoint(String& out, int64_t point) {
      if (point < 0x80) {
        out += static_cast<char>(point);
        return;
      }

      if (point < 0x800) {
        return appendEncoded(out, 0xC0, point, 1);
      }

      if (point < 0x10000) {
        return appendEncoded(out, 0xE0, point, 2);
      }

      appendEncoded(out, 0xF0, point, 3);
    }

    // The lead byte carries the top bits, then each continuation byte six more, highest first.
    void appendEncoded(String& out, int lead, int64_t point, int continuations) {
      out += static_cast<char>(lead | (point >> (6 * continuations)));

      for (int k = continuations - 1; k >= 0; k--) {
        out += static_cast<char>(0x80 | ((point >> (6 * k)) & 0x3F));
      }
    }

    // ── the writer ──

    // The bytes being written.
    struct Out {
      Vector<uint8_t> bytes;
    };

    void byte(Out& out, int b);

    void varint(Out& out, int64_t v);

    void boolean(Out& out, bool b);

    void string(Out& out, const String& text);

    void floating(Out& out, double v);

    void span(Out& out, const Span& at);

    void byte(Out& out, int b) {
      out.bytes.push_back(static_cast<uint8_t>(b));
    }

    // Zigzag + unsigned LEB128, as PbcFormat.nina's `PbcOut.int`: small magnitudes stay small either sign.
    void varint(Out& out, int64_t v) {
      uint64_t u = v >= 0 ? static_cast<uint64_t>(v) * 2 : static_cast<uint64_t>(-(v + 1)) * 2 + 1;

      for (; u >= 128; u = u / 128) {
        byte(out, static_cast<int>(u % 128 + 128));
      }

      byte(out, static_cast<int>(u));
    }

    void boolean(Out& out, bool b) {
      byte(out, b ? 1 : 0);
    }

    void string(Out& out, const String& text) {
      const Vector<int64_t> points = codePointsOf(text);
      varint(out, static_cast<int64_t>(points.size()));

      for (const int64_t p : points) {
        varint(out, p);
      }
    }

    void floating(Out& out, double v) {
      string(out, FloatText(v));
    }

    void span(Out& out, const Span& at) {
      varint(out, at.line);
      varint(out, at.column);
    }

    template <typename Number>
    void numbers(Out& out, const Vector<Number>& items) {
      varint(out, static_cast<int64_t>(items.size()));

      for (const Number n : items) {
        varint(out, static_cast<int64_t>(n));
      }
    }

    // The writer of the records: the bytes written so far.
    struct Writer {
      Out out;
    };

    void type(Writer& w, const TypePtr& t);

    void types(Writer& w, const Vector<TypePtr>& items);

    void expr(Writer& w, const ExprPtr& e);

    void exprs(Writer& w, const Vector<ExprPtr>& items, size_t from = 0);

    void stmt(Writer& w, const Stmt& s);

    void stmts(Writer& w, const StmtList& items);

    void strings(Writer& w, const Vector<String>& items);

    void decl(Writer& w, const DeclRef& ref, const Module& m);

    void structDef(Writer& w, const StructDef& d);

    void fields(Writer& w, const Vector<Field>& items);

    void function(Writer& w, const FuncDef& d);

    template <typename Def>
    void signature(Writer& w, const Def& d);

    void kernelAndOrigin(Writer& w, const FuncDef& d);

    void kernel(Writer& w, const FuncDef& d);

    void origin(Writer& w, const FuncDef& d);

    void externDef(Writer& w, const ExternDef& d);

    void globalDef(Writer& w, const GlobalDef& d);

    void interface(Writer& w, const InterfaceDef& d);

    void methods(Writer& w, const Vector<InterfaceMethod>& items);

    void hostType(Writer& w, const HostType& d);

    void bridge(Writer& w, const Bridge& d);

    void hostImport(Writer& w, const HostImport& d);

    void hostMember(Writer& w, const HostMember& d);

    void parameters(Writer& w, const Vector<Parameter>& items);

    void types(Writer& w, const Vector<TypePtr>& items) {
      varint(w.out, static_cast<int64_t>(items.size()));

      for (const TypePtr& t : items) {
        type(w, t);
      }
    }

    void exprs(Writer& w, const Vector<ExprPtr>& items, size_t from) {
      varint(w.out, static_cast<int64_t>(items.size() - from));

      for (size_t i = from; i < items.size(); i++) {
        expr(w, items[i]);
      }
    }

    void stmts(Writer& w, const StmtList& items) {
      varint(w.out, static_cast<int64_t>(items.size()));

      for (const StmtPtr& s : items) {
        stmt(w, *s);
      }
    }

    void strings(Writer& w, const Vector<String>& items) {
      varint(w.out, static_cast<int64_t>(items.size()));

      for (const String& s : items) {
        string(w.out, s);
      }
    }

    // ── types: the tag, then what the kind carries ──

    using TypeWriter = void (*)(Writer&, const Type&);

    void writeBits(Writer& w, const Type& t);

    void writeNothing(Writer& w, const Type& t);

    void writeStructName(Writer& w, const Type& t);

    void writeElement(Writer& w, const Type& t);

    void writeElementAndSize(Writer& w, const Type& t);

    void writeElementAndMutability(Writer& w, const Type& t);

    void writeKeyValueAndMutability(Writer& w, const Type& t);

    void writeGrid(Writer& w, const Type& t);

    void writeTensor(Writer& w, const Type& t);

    void writeSignature(Writer& w, const Type& t);

    void writeCases(Writer& w, const Type& t);

    void writeEither(Writer& w, const Type& t);

    const Map<TypeKind, TypeWriter> kTypeWriters = {
      {TypeKind::Int, &writeBits},
      {TypeKind::UInt, &writeBits},
      {TypeKind::Float, &writeBits},
      {TypeKind::String, &writeNothing},
      {TypeKind::Void, &writeNothing},
      {TypeKind::Struct, &writeStructName},
      {TypeKind::Pointer, &writeElement},
      {TypeKind::Sparse, &writeElement},
      {TypeKind::Optional, &writeElement},
      {TypeKind::Coroutine, &writeElement},
      {TypeKind::Vec, &writeElementAndSize},
      {TypeKind::FixedArray, &writeElementAndSize},
      {TypeKind::Buffer, &writeElementAndSize},
      {TypeKind::Matrix, &writeElementAndSize},
      {TypeKind::Param, &writeStructName},
      {TypeKind::DynamicArray, &writeElementAndMutability},
      {TypeKind::Set, &writeElementAndMutability},
      {TypeKind::Hashmap, &writeKeyValueAndMutability},
      {TypeKind::Grid, &writeGrid},
      {TypeKind::Tensor, &writeTensor},
      {TypeKind::Function, &writeSignature},
      {TypeKind::Closure, &writeSignature},
      {TypeKind::Variant, &writeCases},
      {TypeKind::Either, &writeEither},
    };

    void type(Writer& w, const TypePtr& t) {
      byte(w.out, tagOf(kTypeKinds, t->kind));
      kTypeWriters.at(t->kind)(w, *t);
    }

    void writeBits(Writer& w, const Type& t) {
      varint(w.out, t.bits);
    }

    void writeNothing(Writer&, const Type&) {}  // String and Void carry nothing.

    void writeStructName(Writer& w, const Type& t) {
      string(w.out, t.name);
    }

    void writeElement(Writer& w, const Type& t) {
      type(w, t.element);
    }

    void writeElementAndSize(Writer& w, const Type& t) {
      type(w, t.element);
      varint(w.out, t.size);
    }

    void writeElementAndMutability(Writer& w, const Type& t) {
      type(w, t.element);
      boolean(w.out, t.isMutable);
    }

    void writeKeyValueAndMutability(Writer& w, const Type& t) {
      type(w, t.element);
      type(w, t.second);
      boolean(w.out, t.isMutable);
    }

    void writeGrid(Writer& w, const Type& t) {
      type(w, t.element);
      varint(w.out, t.size);
      varint(w.out, t.cols);
    }

    void writeTensor(Writer& w, const Type& t) {
      type(w, t.element);
      numbers(w.out, t.dims);
    }

    void writeSignature(Writer& w, const Type& t) {
      types(w, t.parameters);
      type(w, t.element);
    }

    void writeCases(Writer& w, const Type& t) {
      types(w, t.parameters);
    }

    void writeEither(Writer& w, const Type& t) {
      type(w, t.element);
      type(w, t.second);
    }

    // ── expressions: the tag, the type and the span, then what the kind carries ──

    using ExprWriter = void (*)(Writer&, const Expr&);

    void absent(Writer& w);

    void writeIntValue(Writer& w, const Expr& e);

    void writeFloatValue(Writer& w, const Expr& e);

    void writeBoolValue(Writer& w, const Expr& e);

    void writeText(Writer& w, const Expr& e);

    void writeName(Writer& w, const Expr& e);

    void writeBinary(Writer& w, const Expr& e);

    void writeUnary(Writer& w, const Expr& e);

    void writeOperand(Writer& w, const Expr& e);

    void writeCall(Writer& w, const Expr& e);

    void writeOperandAndArguments(Writer& w, const Expr& e);

    void writeBuiltinCall(Writer& w, const Expr& e);

    void writeStructLiteral(Writer& w, const Expr& e);

    void writeFieldGet(Writer& w, const Expr& e);

    void writeOperands(Writer& w, const Expr& e);

    void writeClosure(Writer& w, const Expr& e);

    void writeVariant(Writer& w, const Expr& e);

    void writeEitherLiteral(Writer& w, const Expr& e);

    const Map<ExprKind, ExprWriter> kExprWriters = {
      {ExprKind::ConstInt, &writeIntValue},
      {ExprKind::ConstFloat, &writeFloatValue},
      {ExprKind::ConstBool, &writeBoolValue},
      {ExprKind::ConstString, &writeText},
      {ExprKind::Unsupported, &writeText},
      {ExprKind::Local, &writeName},
      {ExprKind::LoadSlot, &writeName},
      {ExprKind::Global, &writeName},
      {ExprKind::FuncRef, &writeName},
      {ExprKind::Binary, &writeBinary},
      {ExprKind::Unary, &writeUnary},
      {ExprKind::Cast, &writeOperand},
      {ExprKind::Coerce, &writeOperand},
      {ExprKind::Call, &writeCall},
      {ExprKind::CallIndirect, &writeOperandAndArguments},
      {ExprKind::IndexGet, &writeOperandAndArguments},
      {ExprKind::CallBuiltin, &writeBuiltinCall},
      {ExprKind::StructLit, &writeStructLiteral},
      {ExprKind::FieldGet, &writeFieldGet},
      {ExprKind::VecLit, &writeOperands},
      {ExprKind::NewContainer, &writeOperands},
      {ExprKind::ClosureLit, &writeClosure},
      {ExprKind::VariantLit, &writeVariant},
      {ExprKind::EitherLit, &writeEitherLiteral},
    };

    void expr(Writer& w, const ExprPtr& e) {
      if (e == nullptr) {
        return absent(w);
      }

      byte(w.out, tagOf(kExprKinds, e->kind));
      type(w, e->type);
      span(w.out, e->at);
      kExprWriters.at(e->kind)(w, *e);
    }

    // An absent expression (a `return` with no value) is one the C++ lowering leaves null and the LLVM emitter takes as `ret void`. It is
    // written as the Nina HIR spells it (HirJsonReadStmt.returned): a ConstInt 0 of type void, which the VM returns from as void.
    void absent(Writer& w) {
      byte(w.out, tagOf(kExprKinds, ExprKind::ConstInt));
      type(w, MakeVoid());
      span(w.out, Span{});
      varint(w.out, 0);
    }

    void writeIntValue(Writer& w, const Expr& e) {
      varint(w.out, e.intValue);
    }

    void writeFloatValue(Writer& w, const Expr& e) {
      floating(w.out, e.floatValue);
    }

    void writeBoolValue(Writer& w, const Expr& e) {
      boolean(w.out, e.boolValue);
    }

    void writeText(Writer& w, const Expr& e) {
      string(w.out, e.text);
    }

    void writeName(Writer& w, const Expr& e) {
      string(w.out, e.name);
    }

    void writeBinary(Writer& w, const Expr& e) {
      varint(w.out, static_cast<int>(e.binaryOp));  // the index in PolyirHirOps.allBinOps(), which is the BinOp enum's own order
      expr(w, e.operands[0]);
      expr(w, e.operands[1]);
    }

    void writeUnary(Writer& w, const Expr& e) {
      varint(w.out, static_cast<int>(e.unaryOp));
      expr(w, e.operands[0]);
    }

    void writeOperand(Writer& w, const Expr& e) {
      expr(w, e.operands[0]);
    }

    void writeCall(Writer& w, const Expr& e) {
      string(w.out, e.name);
      exprs(w, e.operands);
    }

    // The callee, container or builtin argument first, then the rest as a list.
    void writeOperandAndArguments(Writer& w, const Expr& e) {
      expr(w, e.operands[0]);
      exprs(w, e.operands, 1);
    }

    void writeBuiltinCall(Writer& w, const Expr& e) {
      varint(w.out, static_cast<int>(e.builtin));  // the index in PolyirHirOps.allBuiltins(), which is the Builtin enum's own order
      writeOperandAndArguments(w, e);
    }

    void writeStructLiteral(Writer& w, const Expr& e) {
      string(w.out, e.name);
      numbers(w.out, e.fieldIndices);
      exprs(w, e.operands);
    }

    void writeFieldGet(Writer& w, const Expr& e) {
      expr(w, e.operands[0]);
      varint(w.out, e.fieldIndex);
    }

    void writeOperands(Writer& w, const Expr& e) {
      exprs(w, e.operands);
    }

    void writeClosure(Writer& w, const Expr& e) {
      strings(w, e.parameterNames);
      types(w, e.parameterTypes);
      stmts(w, e.closureBody);
      strings(w, e.captures);
    }

    void writeVariant(Writer& w, const Expr& e) {
      varint(w.out, e.fieldIndex);
      expr(w, e.operands[0]);
    }

    void writeEitherLiteral(Writer& w, const Expr& e) {
      boolean(w.out, e.boolValue);
      expr(w, e.operands[0]);
    }

    // ── statements: the tag and the span, then what the kind carries ──

    using StmtWriter = void (*)(Writer&, const Stmt&);

    void writeLet(Writer& w, const Stmt& s);

    void writeVar(Writer& w, const Stmt& s);

    void writeAssign(Writer& w, const Stmt& s);

    void writeIf(Writer& w, const Stmt& s);

    void writeFor(Writer& w, const Stmt& s);

    void writeSwitch(Writer& w, const Stmt& s);

    void writeSwitchCases(Writer& w, const Stmt& s);

    void writeSelect(Writer& w, const Stmt& s);

    void writeLabel(Writer& w, const Stmt& s);

    void writeGoto(Writer& w, const Stmt& s);

    void writeBranch(Writer& w, const Stmt& s);

    void writeTry(Writer& w, const Stmt& s);

    void writeValue(Writer& w, const Stmt& s);

    const Map<StmtKind, StmtWriter> kStmtWriters = {
      {StmtKind::Let, &writeLet},
      {StmtKind::Var, &writeVar},
      {StmtKind::Assign, &writeAssign},
      {StmtKind::If, &writeIf},
      {StmtKind::For, &writeFor},
      {StmtKind::Switch, &writeSwitch},
      {StmtKind::Select, &writeSelect},
      {StmtKind::Label, &writeLabel},
      {StmtKind::Goto, &writeGoto},
      {StmtKind::Branch, &writeBranch},
      {StmtKind::Try, &writeTry},
      {StmtKind::Return, &writeValue},
      {StmtKind::Throw, &writeValue},
      {StmtKind::Yield, &writeValue},
      {StmtKind::ExprStmt, &writeValue},
    };

    void stmt(Writer& w, const Stmt& s) {
      byte(w.out, tagOf(kStmtKinds, s.kind));
      span(w.out, s.at);
      kStmtWriters.at(s.kind)(w, s);
    }

    void writeLet(Writer& w, const Stmt& s) {
      string(w.out, s.name);
      type(w, s.type);
      expr(w, s.value);
    }

    void writeVar(Writer& w, const Stmt& s) {
      string(w.out, s.name);
      type(w, s.type);
      boolean(w.out, s.value != nullptr);

      if (s.value) {
        expr(w, s.value);
      }
    }

    void writeAssign(Writer& w, const Stmt& s) {
      expr(w, s.target);
      expr(w, s.value);
    }

    void writeIf(Writer& w, const Stmt& s) {
      expr(w, s.value);
      stmts(w, s.body);
      stmts(w, s.elseBody);
    }

    void writeFor(Writer& w, const Stmt& s) {
      strings(w, s.names);
      exprs(w, s.initializers);
      expr(w, s.value);
      exprs(w, s.steps);
      stmts(w, s.body);
    }

    void writeSwitch(Writer& w, const Stmt& s) {
      expr(w, s.value);
      writeSwitchCases(w, s);
      string(w.out, s.name);
      exprs(w, s.arguments);
    }

    void writeSwitchCases(Writer& w, const Stmt& s) {
      numbers(w.out, s.caseValues);
      strings(w, s.caseLabels);
      varint(w.out, static_cast<int64_t>(s.caseArguments.size()));

      for (const Vector<ExprPtr>& args : s.caseArguments) {
        exprs(w, args);
      }
    }

    // The subject, then each arm's keys and block (the names they bind beside them), then the default.
    void writeSelect(Writer& w, const Stmt& s) {
      expr(w, s.value);
      varint(w.out, static_cast<int64_t>(s.caseKeys.size()));

      for (size_t i = 0; i < s.caseKeys.size(); i++) {
        exprs(w, s.caseKeys[i]);
        stmts(w, s.caseBodies[i]);
      }

      strings(w, s.names);
      stmts(w, s.elseBody);
    }

    void writeLabel(Writer& w, const Stmt& s) {
      string(w.out, s.name);
      strings(w, s.parameterNames);
      types(w, s.parameterTypes);
    }

    void writeGoto(Writer& w, const Stmt& s) {
      string(w.out, s.name);
      exprs(w, s.arguments);
    }

    void writeBranch(Writer& w, const Stmt& s) {
      expr(w, s.value);
      string(w.out, s.name);
      exprs(w, s.arguments);
      string(w.out, s.elseName);
      exprs(w, s.elseArguments);
    }

    void writeTry(Writer& w, const Stmt& s) {
      stmts(w, s.body);
      string(w.out, s.name);
      stmts(w, s.elseBody);
    }

    void writeValue(Writer& w, const Stmt& s) {
      expr(w, s.value);
    }  // Return, Throw, Yield and ExprStmt carry one value.

    // ── declarations ──

    void parameters(Writer& w, const Vector<Parameter>& items) {
      varint(w.out, static_cast<int64_t>(items.size()));

      for (const Parameter& p : items) {
        string(w.out, p.name);
      }

      varint(w.out, static_cast<int64_t>(items.size()));

      for (const Parameter& p : items) {
        type(w, p.type);
      }
    }

    void structDef(Writer& w, const StructDef& d) {
      byte(w.out, kDeclStruct);
      string(w.out, d.name);
      fields(w, d.fields);
    }

    // The names, then the types, then the mutability flags, each list with its count.
    void fields(Writer& w, const Vector<Field>& items) {
      varint(w.out, static_cast<int64_t>(items.size()));

      for (const Field& f : items) {
        string(w.out, f.name);
      }

      varint(w.out, static_cast<int64_t>(items.size()));

      for (const Field& f : items) {
        type(w, f.type);
      }

      varint(w.out, static_cast<int64_t>(items.size()));

      for (const Field& f : items) {
        boolean(w.out, f.isMutable);
      }
    }

    void function(Writer& w, const FuncDef& d) {
      byte(w.out, kDeclFunction);
      signature(w, d);
      stmts(w, d.body);
      boolean(w.out, d.isCoroutine);
      span(w.out, d.at);
    }

    // The name, parameters and result a function and an extern both start with.
    template <typename Def>
    void signature(Writer& w, const Def& d) {
      string(w.out, d.name);
      parameters(w, d.parameters);
      type(w, d.result);
    }

    // What the Nina HIR keeps as declarations of their own (HKernel and HOrigin), written right after the function they name.
    void kernelAndOrigin(Writer& w, const FuncDef& d) {
      if (d.stage != KernelStage::None) {
        kernel(w, d);
      }

      if (!d.origin.empty()) {
        origin(w, d);
      }
    }

    void kernel(Writer& w, const FuncDef& d) {
      byte(w.out, kDeclKernel);
      string(w.out, d.name);
      string(w.out, StageName(d.stage));
      varint(w.out, static_cast<int64_t>(d.roles.size()));

      for (const ParamRole role : d.roles) {
        varint(w.out, static_cast<int>(role));
      }
    }

    void origin(Writer& w, const FuncDef& d) {
      byte(w.out, kDeclOrigin);
      string(w.out, d.name);
      string(w.out, d.origin);
    }

    void externDef(Writer& w, const ExternDef& d) {
      byte(w.out, kDeclExtern);
      signature(w, d);
    }

    void globalDef(Writer& w, const GlobalDef& d) {
      byte(w.out, kDeclGlobal);
      string(w.out, d.name);
      type(w, d.type);
      expr(w, d.initializer);
    }

    void interface(Writer& w, const InterfaceDef& d) {
      byte(w.out, kDeclInterface);
      string(w.out, d.name);
      methods(w, d.methods);
      strings(w, d.implementers);
    }

    // The names, then the parameter lists, then the results, each list with its count.
    void methods(Writer& w, const Vector<InterfaceMethod>& items) {
      varint(w.out, static_cast<int64_t>(items.size()));

      for (const InterfaceMethod& x : items) {
        string(w.out, x.name);
      }

      varint(w.out, static_cast<int64_t>(items.size()));

      for (const InterfaceMethod& x : items) {
        types(w, x.parameters);
      }

      varint(w.out, static_cast<int64_t>(items.size()));

      for (const InterfaceMethod& x : items) {
        type(w, x.result);
      }
    }

    void hostType(Writer& w, const HostType& d) {
      byte(w.out, kDeclHostType);
      string(w.out, d.name);

      string(w.out, d.base);
      types(w, d.typeArguments);
      boolean(w.out, d.isInterface);
      strings(w, d.bindings);
    }

    void bridge(Writer& w, const Bridge& d) {
      byte(w.out, kDeclBridge);
      type(w, d.polyirType);

      string(w.out, d.target);
      string(w.out, d.hostType);
      string(w.out, d.toHost);
      string(w.out, d.fromHost);
    }

    void hostImport(Writer& w, const HostImport& d) {
      byte(w.out, kDeclHostImport);
      string(w.out, d.target);
      string(w.out, d.module);
      string(w.out, d.alias);
    }

    void hostMember(Writer& w, const HostMember& d) {
      byte(w.out, kDeclHostMember);
      string(w.out, d.function);
      string(w.out, d.owner);
      string(w.out, d.member);

      boolean(w.out, d.isStatic);
      boolean(w.out, d.hasResult);
      strings(w, d.bindings);
    }

    // What writes the declaration a DeclRef names: each takes the module and the index into the list of its kind.
    using DeclWriter = void (*)(Writer&, const Module&, size_t);

    void writeStruct(Writer& w, const Module& m, size_t i) {
      structDef(w, m.structs[i]);
    }

    void writeInterface(Writer& w, const Module& m, size_t i) {
      interface(w, m.interfaces[i]);
    }

    void writeHostType(Writer& w, const Module& m, size_t i) {
      hostType(w, m.hostTypes[i]);
    }

    void writeBridge(Writer& w, const Module& m, size_t i) {
      bridge(w, m.bridges[i]);
    }

    void writeHostMember(Writer& w, const Module& m, size_t i) {
      hostMember(w, m.hostMembers[i]);
    }

    void writeExtern(Writer& w, const Module& m, size_t i) {
      externDef(w, m.externs[i]);
    }

    void writeGlobal(Writer& w, const Module& m, size_t i) {
      globalDef(w, m.globals[i]);
    }

    void writeHostImport(Writer& w, const Module& m, size_t i) {
      hostImport(w, m.hostImports[i]);
    }

    void writeFunction(Writer& w, const Module& m, size_t i) {
      function(w, m.functions[i]);
      kernelAndOrigin(w, m.functions[i]);
    }

    const Map<DeclKind, DeclWriter> kDeclWriters = {
      {DeclKind::Struct, &writeStruct},
      {DeclKind::Interface, &writeInterface},
      {DeclKind::HostType, &writeHostType},
      {DeclKind::Bridge, &writeBridge},
      {DeclKind::HostMember, &writeHostMember},
      {DeclKind::Extern, &writeExtern},
      {DeclKind::Global, &writeGlobal},
      {DeclKind::Function, &writeFunction},
      {DeclKind::HostImport, &writeHostImport},
    };

    void decl(Writer& w, const DeclRef& ref, const Module& m) {
      kDeclWriters.at(ref.kind)(w, m, ref.index);
    }

    // A module built by hand has no recorded order: its declarations go out grouped by kind, as WriteHirJson sends them.
    Module orderedModule(const Module& module) {
      Module copy = module;
      copy.order.clear();
      NoteAdded(copy, Vector<size_t>(Counts(module).size(), 0));

      return copy;
    }

    void header(Out& file);

    void section(Out& file, int id, const Vector<uint8_t>& body);

    Vector<uint8_t> declarations(const Module& m);

    int64_t recordCount(const Module& m);

    Vector<uint8_t> places(const Module& m);

    Vector<uint8_t> generics(const Module& m);

    // A section only when it has something to say: a program with no places and no generics writes the bytes it always did.
    void optionalSection(Out& file, int id, const Vector<uint8_t>& body) {
      if (!body.empty()) {
        section(file, id, body);
      }
    }

    // The declarations, then where they sit, then the generics.
    Vector<uint8_t> writeOrdered(const Module& m) {
      Out file;
      header(file);
      section(file, kSectionDecls, declarations(m));

      optionalSection(file, kSectionPlaces, places(m));
      optionalSection(file, kSectionGenerics, generics(m));

      return move(file.bytes);
    }

    // ── where declarations sit (section 2): a count, then per declaration its kind, name, module and `internal` ──

    constexpr int kPlacedStruct = 1;
    constexpr int kPlacedFunction = 2;
    constexpr int kPlacedInterface = 3;
    constexpr int kPlacedDefault = 4;  // a field's default: the struct's name, the field's index, the default
    constexpr int kPlacedOrigin = 5;   // the imported file a declaration came from: its kind (1-3, 6 a global), its name, the file
    constexpr int kPlacedGlobal = 6;

    String outerOf(const StructDef& d) {
      return d.outer;
    }

    String outerOf(const FuncDef& d) {
      return d.outer;
    }

    String outerOf(const InterfaceDef&) {
      return "";
    }

    // The annotations: a count, then each one's name and its arguments (a count, then each name and value).
    void annotations(Out& records, const Vector<Annotation>& items) {
      varint(records, static_cast<int64_t>(items.size()));

      for (const Annotation& a : items) {
        string(records, a.name);
        varint(records, static_cast<int64_t>(a.args.size()));

        for (const Pair<String, String>& arg : a.args) {
          string(records, arg.first);
          string(records, arg.second);
        }
      }
    }

    template <typename Decl>
    bool isPlaced(const Decl& d) {
      return !d.module.empty() || d.internal || !outerOf(d).empty() || !d.annotations.empty();
    }

    // Per placed declaration: its kind, name, module, `internal`, outer struct and annotations.
    template <typename Decl>
    void placesOf(Out& records, int kind, const Vector<Decl>& decls, int64_t& count) {
      for (const Decl& d : decls) {
        if (isPlaced(d)) {
          byte(records, kind);
          string(records, d.name);
          string(records, d.module);

          boolean(records, d.internal);
          string(records, outerOf(d));
          annotations(records, d.annotations);
          count++;
        }
      }
    }

    // Per declaration from an imported file: its kind, its name and the file.
    template <typename Decl>
    void originsOf(Out& records, int kind, const Vector<Decl>& decls, int64_t& count) {
      for (const Decl& d : decls) {
        if (!d.origin.empty()) {
          byte(records, kPlacedOrigin);
          byte(records, kind);
          string(records, d.name);
          string(records, d.origin);
          count++;
        }
      }
    }

    void defaultsOf(Writer& records, const Vector<StructDef>& structs, int64_t& count) {
      for (const StructDef& d : structs) {
        for (size_t i = 0; i < d.fields.size(); i++) {
          if (d.fields[i].initial) {
            byte(records.out, kPlacedDefault);
            string(records.out, d.name);
            varint(records.out, static_cast<int64_t>(i));
            expr(records, d.fields[i].initial);
            count++;
          }
        }
      }
    }

    void allOrigins(Out& records, const Module& m, int64_t& count) {
      originsOf(records, kPlacedStruct, m.structs, count);
      originsOf(records, kPlacedFunction, m.functions, count);
      originsOf(records, kPlacedInterface, m.interfaces, count);
      originsOf(records, kPlacedGlobal, m.globals, count);
    }

    // Nothing when no declaration is in a module, internal, has a field default, or comes from an imported file.
    Vector<uint8_t> places(const Module& m) {
      Writer records;
      int64_t count = 0;
      placesOf(records.out, kPlacedStruct, m.structs, count);
      placesOf(records.out, kPlacedFunction, m.functions, count);

      placesOf(records.out, kPlacedInterface, m.interfaces, count);
      defaultsOf(records, m.structs, count);
      allOrigins(records.out, m, count);

      Out body;

      if (count > 0) {
        varint(body, count);
        body.bytes.insert(body.bytes.end(), records.out.bytes.begin(), records.out.bytes.end());
      }

      return move(body.bytes);
    }

    // ── the generics (section 3): the generics as declared, written as a module of their own (its declarations, then its places with their
    // size), then a count and each generic record: kind, name, the generic it came from, type arguments, parameters, why not parametric ──

    constexpr int kGenericOfStruct = 1;  // a struct copy (`Box_i64`)
    constexpr int kGenericOfFunction = 2;
    constexpr int kGenericStruct = 3;  // a generic struct as declared (`Box_T`)
    constexpr int kGenericFunction = 4;

    void append(Out& out, const Vector<uint8_t>& bytes) {
      out.bytes.insert(out.bytes.end(), bytes.begin(), bytes.end());
    }

    template <typename Decl>
    void genericRecords(Writer& w, int kind, const Vector<Decl>& decls, int64_t& count) {
      for (const Decl& d : decls) {
        if (HasGeneric(d.generic)) {
          byte(w.out, kind);
          string(w.out, d.name);
          string(w.out, d.generic.of);

          types(w, d.generic.arguments);
          strings(w, d.generic.params);
          string(w.out, d.generic.notParametric);
          count++;
        }
      }
    }

    // The generics as declared, so their declarations and places are written as the program's are.
    Vector<uint8_t> declaredGenerics(const Module& m) {
      Module declared;
      declared.structs = m.genericStructs;
      declared.functions = m.genericFunctions;
      NoteAdded(declared, Vector<size_t>(Counts(declared).size(), 0));

      Out body;
      append(body, declarations(declared));
      const Vector<uint8_t> placed = places(declared);
      varint(body, static_cast<int64_t>(placed.size()));
      append(body, placed);

      return move(body.bytes);
    }

    // Nothing for a program without generics.
    Vector<uint8_t> generics(const Module& m) {
      Writer records;
      int64_t count = 0;
      genericRecords(records, kGenericOfStruct, m.structs, count);
      genericRecords(records, kGenericOfFunction, m.functions, count);

      genericRecords(records, kGenericStruct, m.genericStructs, count);
      genericRecords(records, kGenericFunction, m.genericFunctions, count);

      Out body;

      if (count > 0) {
        append(body, declaredGenerics(m));
        varint(body, count);
        append(body, records.out.bytes);
      }

      return move(body.bytes);
    }

    // The magic, then the version as four bytes, lowest first.
    void header(Out& file) {
      for (const uint8_t b : kMagic) {
        byte(file, b);
      }

      byte(file, static_cast<int>(kVersion));
      byte(file, 0);
      byte(file, 0);
      byte(file, 0);
    }

    void section(Out& file, int id, const Vector<uint8_t>& body) {
      byte(file, id);
      varint(file, static_cast<int64_t>(body.size()));
      file.bytes.insert(file.bytes.end(), body.begin(), body.end());
    }

    Vector<uint8_t> declarations(const Module& m) {
      Writer w;
      varint(w.out, recordCount(m));

      for (const DeclRef& ref : m.order) {
        decl(w, ref, m);
      }

      return move(w.out.bytes);
    }

    // The declarations plus the HKernel and HOrigin records written after their functions.
    int64_t recordCount(const Module& m) {
      int64_t count = static_cast<int64_t>(m.order.size());

      for (const FuncDef& d : m.functions) {
        count += (d.stage != KernelStage::None ? 1 : 0) + (d.origin.empty() ? 0 : 1);
      }

      return count;
    }

    // ── the reader ──

    // The bytes being read, and the position of the next one.
    struct In {
      const Vector<uint8_t>& bytes;
      size_t pos = 0;
    };

    bool atEnd(const In& in);

    uint8_t next(In& in);

    int64_t varint(In& in);

    uint64_t unsignedVarint(In& in);

    int64_t count(In& in);

    bool boolean(In& in);

    String string(In& in);

    double floating(In& in);

    Span span(In& in);

    bool atEnd(const In& in) {
      return in.pos >= in.bytes.size();
    }

    uint8_t next(In& in) {
      if (atEnd(in)) {
        throw HirReadError("the file ends in the middle of a record");
      }

      return in.bytes[in.pos++];
    }

    // The inverse of the writer's zigzag.
    int64_t varint(In& in) {
      const uint64_t u = unsignedVarint(in);

      if (u % 2 == 0) {
        return static_cast<int64_t>(u / 2);
      }

      return -static_cast<int64_t>(u / 2) - 1;
    }

    // Unsigned LEB128: seven bits a byte, lowest first, the high bit set on every byte but the last.
    uint64_t unsignedVarint(In& in) {
      uint64_t u = 0;
      int shift = 0;
      uint64_t b = next(in);

      for (; b >= 128; b = next(in), shift += 7) {
        u += (b - 128) << shift;
      }

      return u + (b << shift);
    }

    int64_t count(In& in) {
      const int64_t n = varint(in);

      if (n < 0 || n > kCountLimit) {
        throw HirReadError("a count of " + to_string(n) + " is not believable");
      }

      return n;
    }

    bool boolean(In& in) {
      return next(in) != 0;
    }

    String string(In& in) {
      const int64_t n = count(in);
      String out;

      for (int64_t i = 0; i < n; i++) {
        appendCodePoint(out, varint(in));
      }

      return out;
    }

    double floating(In& in) {
      const String text = string(in);
      const char* begin = text.c_str();
      char* end = nullptr;
      const double v = strtod(begin, &end);

      if (end == begin || *end != 0) {
        throw HirReadError("'" + text + "' is not a float");
      }

      return v;
    }

    Span span(In& in) {
      Span at;
      at.line = static_cast<int>(varint(in));
      at.column = static_cast<int>(varint(in));

      return at;
    }

    // The reader of the records: the bytes, and how far it has read.
    struct Reader {
      In in;
    };

    TypePtr type(Reader& reader);

    Vector<TypePtr> types(Reader& reader);

    ExprPtr expr(Reader& reader);

    Vector<ExprPtr> exprs(Reader& reader);

    StmtPtr stmt(Reader& reader);

    StmtList stmts(Reader& reader);

    Vector<String> strings(Reader& reader);

    Vector<int> integers(Reader& reader);

    void decl(Reader& reader, Module& m);

    Vector<Parameter> parameters(Reader& reader);

    void structDef(Reader& reader, Module& m);

    Vector<Field> fields(Reader& reader);

    void function(Reader& reader, Module& m);

    template <typename Def>
    void signature(Reader& reader, Def& d);

    void kernel(Reader& reader, Module& m);

    Vector<ParamRole> roles(Reader& reader);

    void origin(Reader& reader, Module& m);

    FuncDef& named(Module& m, const String& name);

    void externDef(Reader& reader, Module& m);

    void globalDef(Reader& reader, Module& m);

    void interface(Reader& reader, Module& m);

    Vector<InterfaceMethod> methods(Reader& reader);

    Vector<Vector<TypePtr>> parameterLists(Reader& reader);

    void hostType(Reader& reader, Module& m);

    void bridge(Reader& reader, Module& m);

    void hostMember(Reader& reader, Module& m);

    void callShape(Reader& reader, HostMember& d);

    void hostImport(Reader& reader, Module& m);

    Vector<TypePtr> types(Reader& reader) {
      const int64_t n = count(reader.in);
      Vector<TypePtr> out;

      for (int64_t i = 0; i < n; i++) {
        out.push_back(type(reader));
      }

      return out;
    }

    Vector<ExprPtr> exprs(Reader& reader) {
      const int64_t n = count(reader.in);
      Vector<ExprPtr> out;

      for (int64_t i = 0; i < n; i++) {
        out.push_back(expr(reader));
      }

      return out;
    }

    StmtList stmts(Reader& reader) {
      const int64_t n = count(reader.in);
      StmtList out;

      for (int64_t i = 0; i < n; i++) {
        out.push_back(stmt(reader));
      }

      return out;
    }

    Vector<String> strings(Reader& reader) {
      const int64_t n = count(reader.in);
      Vector<String> out;

      for (int64_t i = 0; i < n; i++) {
        out.push_back(string(reader.in));
      }

      return out;
    }

    Vector<int> integers(Reader& reader) {
      const int64_t n = count(reader.in);
      Vector<int> out;

      for (int64_t i = 0; i < n; i++) {
        out.push_back(static_cast<int>(varint(reader.in)));
      }

      return out;
    }

    // ── types: each reader reads what its kind carries, in the writer's order, and makes the type ──

    using TypeReader = TypePtr (*)(Reader&);

    int readInt(Reader& reader) {
      return static_cast<int>(varint(reader.in));
    }

    TypePtr readIntType(Reader& reader) {
      return MakeInt(readInt(reader));
    }

    TypePtr readUIntType(Reader& reader) {
      return MakeUInt(readInt(reader));
    }

    TypePtr readFloatType(Reader& reader) {
      return MakeFloat(readInt(reader));
    }

    TypePtr readStringType(Reader&) {
      return MakeString();
    }

    TypePtr readVoidType(Reader&) {
      return MakeVoid();
    }

    TypePtr readStructType(Reader& reader) {
      return MakeStruct(string(reader.in));
    }

    TypePtr readParamType(Reader& reader) {
      return MakeParam(string(reader.in));
    }

    TypePtr readPointerType(Reader& reader) {
      return MakePointer(type(reader));
    }

    TypePtr readSparseType(Reader& reader) {
      return MakeSparse(type(reader));
    }

    TypePtr readOptionalType(Reader& reader) {
      return MakeOptional(type(reader));
    }

    TypePtr readCoroutineType(Reader& reader) {
      return MakeCoroutine(type(reader));
    }

    // Each value is read into a local first. C++ does not fix the order a call's arguments are worked out in, and the
    // reads must follow the order of the bytes.
    TypePtr readVecType(Reader& reader) {
      TypePtr element = type(reader);

      return MakeVec(move(element), readInt(reader));
    }

    TypePtr readFixedArrayType(Reader& reader) {
      TypePtr element = type(reader);

      return MakeFixedArray(move(element), readInt(reader));
    }

    TypePtr readBufferType(Reader& reader) {
      TypePtr element = type(reader);

      return MakeBuffer(move(element), readInt(reader));
    }

    TypePtr readMatrixType(Reader& reader) {
      TypePtr element = type(reader);

      return MakeMatrix(move(element), readInt(reader));
    }

    TypePtr readDynamicArrayType(Reader& reader) {
      TypePtr element = type(reader);

      return MakeDynamicArray(move(element), boolean(reader.in));
    }

    TypePtr readSetType(Reader& reader) {
      TypePtr element = type(reader);

      return MakeSet(move(element), boolean(reader.in));
    }

    TypePtr readHashmapType(Reader& reader) {
      TypePtr key = type(reader);
      TypePtr value = type(reader);

      return MakeHashmap(move(key), move(value), boolean(reader.in));
    }

    TypePtr readGridType(Reader& reader) {
      TypePtr element = type(reader);
      const int rows = readInt(reader);

      return MakeGrid(move(element), rows, readInt(reader));
    }

    TypePtr readTensorType(Reader& reader) {
      TypePtr element = type(reader);

      return MakeTensor(move(element), integers(reader));
    }

    TypePtr readFunctionType(Reader& reader) {
      Vector<TypePtr> parameterTypes = types(reader);

      return MakeFunction(move(parameterTypes), type(reader));
    }

    TypePtr readClosureType(Reader& reader) {
      Vector<TypePtr> parameterTypes = types(reader);

      return MakeClosure(move(parameterTypes), type(reader));
    }

    TypePtr readVariantType(Reader& reader) {
      return MakeVariant(types(reader));
    }

    TypePtr readEitherType(Reader& reader) {
      TypePtr ok = type(reader);

      return MakeEither(move(ok), type(reader));
    }

    const Map<TypeKind, TypeReader> kTypeReaders = {
      {TypeKind::Int, &readIntType},
      {TypeKind::UInt, &readUIntType},
      {TypeKind::Float, &readFloatType},
      {TypeKind::String, &readStringType},
      {TypeKind::Void, &readVoidType},
      {TypeKind::Struct, &readStructType},
      {TypeKind::Pointer, &readPointerType},
      {TypeKind::Sparse, &readSparseType},
      {TypeKind::Optional, &readOptionalType},
      {TypeKind::Coroutine, &readCoroutineType},
      {TypeKind::Vec, &readVecType},
      {TypeKind::FixedArray, &readFixedArrayType},
      {TypeKind::Buffer, &readBufferType},
      {TypeKind::Matrix, &readMatrixType},
      {TypeKind::Param, &readParamType},
      {TypeKind::DynamicArray, &readDynamicArrayType},
      {TypeKind::Set, &readSetType},
      {TypeKind::Hashmap, &readHashmapType},
      {TypeKind::Grid, &readGridType},
      {TypeKind::Tensor, &readTensorType},
      {TypeKind::Function, &readFunctionType},
      {TypeKind::Closure, &readClosureType},
      {TypeKind::Variant, &readVariantType},
      {TypeKind::Either, &readEitherType},
    };

    TypePtr type(Reader& reader) {
      const TypeKind kind = kindOfTag(kTypeKinds, next(reader.in), "type");

      return kTypeReaders.at(kind)(reader);
    }

    // ── expressions: each reader fills in what its kind carries ──

    using ExprReader = void (*)(Reader&, Expr&);

    int64_t opIndex(In& in, int64_t limit, const char* what);

    void readIntValue(Reader& reader, Expr& e);

    void readFloatValue(Reader& reader, Expr& e);

    void readBoolValue(Reader& reader, Expr& e);

    void readText(Reader& reader, Expr& e);

    void readName(Reader& reader, Expr& e);

    void readBinary(Reader& reader, Expr& e);

    void readUnary(Reader& reader, Expr& e);

    void readOperand(Reader& reader, Expr& e);

    void readCall(Reader& reader, Expr& e);

    void readOperandAndArguments(Reader& reader, Expr& e);

    void readBuiltinCall(Reader& reader, Expr& e);

    void readStructLiteral(Reader& reader, Expr& e);

    void readFieldGet(Reader& reader, Expr& e);

    void readOperands(Reader& reader, Expr& e);

    void readClosure(Reader& reader, Expr& e);

    void readVariant(Reader& reader, Expr& e);

    void readEitherLiteral(Reader& reader, Expr& e);

    const Map<ExprKind, ExprReader> kExprReaders = {
      {ExprKind::ConstInt, &readIntValue},
      {ExprKind::ConstFloat, &readFloatValue},
      {ExprKind::ConstBool, &readBoolValue},
      {ExprKind::ConstString, &readText},
      {ExprKind::Unsupported, &readText},
      {ExprKind::Local, &readName},
      {ExprKind::LoadSlot, &readName},
      {ExprKind::Global, &readName},
      {ExprKind::FuncRef, &readName},
      {ExprKind::Binary, &readBinary},
      {ExprKind::Unary, &readUnary},
      {ExprKind::Cast, &readOperand},
      {ExprKind::Coerce, &readOperand},
      {ExprKind::Call, &readCall},
      {ExprKind::CallIndirect, &readOperandAndArguments},
      {ExprKind::IndexGet, &readOperandAndArguments},
      {ExprKind::CallBuiltin, &readBuiltinCall},
      {ExprKind::StructLit, &readStructLiteral},
      {ExprKind::FieldGet, &readFieldGet},
      {ExprKind::VecLit, &readOperands},
      {ExprKind::NewContainer, &readOperands},
      {ExprKind::ClosureLit, &readClosure},
      {ExprKind::VariantLit, &readVariant},
      {ExprKind::EitherLit, &readEitherLiteral},
    };

    ExprPtr expr(Reader& reader) {
      auto e = make_shared<Expr>();
      e->kind = kindOfTag(kExprKinds, next(reader.in), "expression");
      e->type = type(reader);
      e->at = span(reader.in);
      kExprReaders.at(e->kind)(reader, *e);

      return e;
    }

    int64_t opIndex(In& in, int64_t limit, const char* what) {
      const int64_t i = varint(in);

      if (i < 0 || i > limit) {
        throw HirReadError(String("unknown ") + what + " index " + to_string(i));
      }

      return i;
    }

    void readIntValue(Reader& reader, Expr& e) {
      e.intValue = varint(reader.in);
    }

    void readFloatValue(Reader& reader, Expr& e) {
      e.floatValue = floating(reader.in);
    }

    void readBoolValue(Reader& reader, Expr& e) {
      e.boolValue = boolean(reader.in);
    }

    void readText(Reader& reader, Expr& e) {
      e.text = string(reader.in);
    }

    void readName(Reader& reader, Expr& e) {
      e.name = string(reader.in);
    }

    void readBinary(Reader& reader, Expr& e) {
      e.binaryOp = static_cast<BinOp>(opIndex(reader.in, static_cast<int64_t>(BinOp::NotIdentical), "operator"));
      e.operands.push_back(expr(reader));
      e.operands.push_back(expr(reader));
    }

    void readUnary(Reader& reader, Expr& e) {
      e.unaryOp = static_cast<UnOp>(opIndex(reader.in, static_cast<int64_t>(UnOp::BitNot), "operator"));
      e.operands.push_back(expr(reader));
    }

    void readOperand(Reader& reader, Expr& e) {
      e.operands.push_back(expr(reader));
    }

    void readCall(Reader& reader, Expr& e) {
      e.name = string(reader.in);
      e.operands = exprs(reader);
    }

    void readOperandAndArguments(Reader& reader, Expr& e) {
      e.operands.push_back(expr(reader));

      for (ExprPtr& a : exprs(reader)) {
        e.operands.push_back(move(a));
      }
    }

    void readBuiltinCall(Reader& reader, Expr& e) {
      e.builtin = static_cast<Builtin>(opIndex(reader.in, static_cast<int64_t>(Builtin::Fail), "builtin"));
      readOperandAndArguments(reader, e);
    }

    void readStructLiteral(Reader& reader, Expr& e) {
      e.name = string(reader.in);
      e.fieldIndices = integers(reader);
      e.operands = exprs(reader);
    }

    void readFieldGet(Reader& reader, Expr& e) {
      e.operands.push_back(expr(reader));
      e.fieldIndex = static_cast<int>(varint(reader.in));
    }

    void readOperands(Reader& reader, Expr& e) {
      e.operands = exprs(reader);
    }

    void readClosure(Reader& reader, Expr& e) {
      e.parameterNames = strings(reader);
      e.parameterTypes = types(reader);
      e.closureBody = stmts(reader);
      e.captures = strings(reader);
    }

    void readVariant(Reader& reader, Expr& e) {
      e.fieldIndex = static_cast<int>(varint(reader.in));
      e.operands.push_back(expr(reader));
    }

    void readEitherLiteral(Reader& reader, Expr& e) {
      e.boolValue = boolean(reader.in);
      e.operands.push_back(expr(reader));
    }

    // ── statements: each reader fills in what its kind carries ──

    using StmtReader = void (*)(Reader&, Stmt&);

    void readLet(Reader& reader, Stmt& s);

    void readVar(Reader& reader, Stmt& s);

    void readAssign(Reader& reader, Stmt& s);

    void readIf(Reader& reader, Stmt& s);

    void readFor(Reader& reader, Stmt& s);

    void readSwitch(Reader& reader, Stmt& s);

    void readSwitchCases(Reader& reader, Stmt& s);

    void readSelect(Reader& reader, Stmt& s);

    void readLabel(Reader& reader, Stmt& s);

    void readGoto(Reader& reader, Stmt& s);

    void readBranch(Reader& reader, Stmt& s);

    void readTry(Reader& reader, Stmt& s);

    void readValue(Reader& reader, Stmt& s);

    const Map<StmtKind, StmtReader> kStmtReaders = {
      {StmtKind::Let, &readLet},
      {StmtKind::Var, &readVar},
      {StmtKind::Assign, &readAssign},
      {StmtKind::If, &readIf},
      {StmtKind::For, &readFor},
      {StmtKind::Switch, &readSwitch},
      {StmtKind::Select, &readSelect},
      {StmtKind::Label, &readLabel},
      {StmtKind::Goto, &readGoto},
      {StmtKind::Branch, &readBranch},
      {StmtKind::Try, &readTry},
      {StmtKind::Return, &readValue},
      {StmtKind::Throw, &readValue},
      {StmtKind::Yield, &readValue},
      {StmtKind::ExprStmt, &readValue},
    };

    StmtPtr stmt(Reader& reader) {
      auto s = make_shared<Stmt>();
      s->kind = kindOfTag(kStmtKinds, next(reader.in), "statement");
      s->at = span(reader.in);
      kStmtReaders.at(s->kind)(reader, *s);

      return s;
    }

    void readLet(Reader& reader, Stmt& s) {
      s.name = string(reader.in);
      s.type = type(reader);
      s.value = expr(reader);
    }

    void readVar(Reader& reader, Stmt& s) {
      s.name = string(reader.in);
      s.type = type(reader);

      if (boolean(reader.in)) {
        s.value = expr(reader);
      }
    }

    void readAssign(Reader& reader, Stmt& s) {
      s.target = expr(reader);
      s.value = expr(reader);
    }

    void readIf(Reader& reader, Stmt& s) {
      s.value = expr(reader);
      s.body = stmts(reader);
      s.elseBody = stmts(reader);
    }

    void readFor(Reader& reader, Stmt& s) {
      s.names = strings(reader);
      s.initializers = exprs(reader);
      s.value = expr(reader);
      s.steps = exprs(reader);
      s.body = stmts(reader);
    }

    void readSwitch(Reader& reader, Stmt& s) {
      s.value = expr(reader);
      readSwitchCases(reader, s);
      s.name = string(reader.in);
      s.arguments = exprs(reader);
    }

    void readSwitchCases(Reader& reader, Stmt& s) {
      const int64_t values = count(reader.in);

      for (int64_t i = 0; i < values; i++) {
        s.caseValues.push_back(varint(reader.in));
      }

      s.caseLabels = strings(reader);
      const int64_t cases = count(reader.in);

      for (int64_t i = 0; i < cases; i++) {
        s.caseArguments.push_back(exprs(reader));
      }
    }

    void readSelect(Reader& reader, Stmt& s) {
      s.value = expr(reader);
      const int64_t arms = count(reader.in);

      for (int64_t i = 0; i < arms; i++) {
        s.caseKeys.push_back(exprs(reader));
        s.caseBodies.push_back(stmts(reader));
      }

      s.names = strings(reader);
      s.elseBody = stmts(reader);
    }

    void readLabel(Reader& reader, Stmt& s) {
      s.name = string(reader.in);
      s.parameterNames = strings(reader);
      s.parameterTypes = types(reader);
    }

    void readGoto(Reader& reader, Stmt& s) {
      s.name = string(reader.in);
      s.arguments = exprs(reader);
    }

    void readBranch(Reader& reader, Stmt& s) {
      s.value = expr(reader);
      s.name = string(reader.in);
      s.arguments = exprs(reader);
      s.elseName = string(reader.in);
      s.elseArguments = exprs(reader);
    }

    void readTry(Reader& reader, Stmt& s) {
      s.body = stmts(reader);
      s.name = string(reader.in);
      s.elseBody = stmts(reader);
    }

    void readValue(Reader& reader, Stmt& s) {
      s.value = expr(reader);
    }  // Return, Throw, Yield, ExprStmt

    // ── declarations ──

    using DeclReader = void (*)(Reader&, Module&);

    // By declaration tag. HKernel and HOrigin are not here: they attach to a function, not declarations of the Module's own.
    const Map<int, DeclReader> kDeclReaders = {
      {kDeclStruct, &structDef},
      {kDeclFunction, &function},
      {kDeclExtern, &externDef},
      {kDeclGlobal, &globalDef},
      {kDeclInterface, &interface},
      {kDeclHostType, &hostType},
      {kDeclBridge, &bridge},
      {kDeclHostMember, &hostMember},
      {kDeclHostImport, &hostImport},
    };

    DeclReader declReader(int tag);

    void decl(Reader& reader, Module& m) {
      const int tag = next(reader.in);

      if (tag == kDeclKernel) {
        return kernel(reader, m);
      }

      if (tag == kDeclOrigin) {
        return origin(reader, m);
      }

      const Vector<size_t> before = Counts(m);
      declReader(tag)(reader, m);
      NoteAdded(m, before);
    }

    DeclReader declReader(int tag) {
      const auto found = kDeclReaders.find(tag);

      if (found == kDeclReaders.end()) {
        throw HirReadError("unknown declaration tag " + to_string(tag));
      }

      return found->second;
    }

    Vector<Parameter> parameters(Reader& reader) {
      const Vector<String> names = strings(reader);
      const Vector<TypePtr> ptypes = types(reader);

      if (names.size() != ptypes.size()) {
        throw HirReadError("a signature with " + to_string(names.size()) + " names and " + to_string(ptypes.size()) + " types");
      }

      Vector<Parameter> out;

      for (size_t i = 0; i < names.size(); i++) {
        out.push_back(Parameter{names[i], ptypes[i]});
      }

      return out;
    }

    void structDef(Reader& reader, Module& m) {
      StructDef d;
      d.name = string(reader.in);
      d.fields = fields(reader);
      m.structs.push_back(move(d));
    }

    Vector<Field> fields(Reader& reader) {
      const Vector<String> names = strings(reader);
      const Vector<TypePtr> ftypes = types(reader);
      const int64_t flags = count(reader.in);

      if (names.size() != ftypes.size() || static_cast<int64_t>(names.size()) != flags) {
        throw HirReadError("a struct whose field lists disagree");
      }

      Vector<Field> out;

      for (size_t i = 0; i < names.size(); i++) {
        out.push_back(Field{names[i], ftypes[i], boolean(reader.in)});
      }

      return out;
    }

    void function(Reader& reader, Module& m) {
      FuncDef d;
      signature(reader, d);
      d.body = stmts(reader);

      d.isCoroutine = boolean(reader.in);
      d.at = span(reader.in);
      m.functions.push_back(move(d));
    }

    template <typename Def>
    void signature(Reader& reader, Def& d) {
      d.name = string(reader.in);
      d.parameters = parameters(reader);
      d.result = type(reader);
    }

    // HKernel (tag 9): the stage and roles of a function already read.
    void kernel(Reader& reader, Module& m) {
      const String name = string(reader.in);
      const String stage = string(reader.in);
      Vector<ParamRole> kernelRoles = roles(reader);

      FuncDef& d = named(m, name);
      d.stage = StageNamed(stage);
      d.roles = move(kernelRoles);
    }

    Vector<ParamRole> roles(Reader& reader) {
      const int64_t n = count(reader.in);
      Vector<ParamRole> out;

      for (int64_t i = 0; i < n; i++) {
        out.push_back(static_cast<ParamRole>(varint(reader.in)));
      }

      return out;
    }

    // HOrigin (tag 10): the source file of a function already read.
    void origin(Reader& reader, Module& m) {
      const String name = string(reader.in);
      const String file = string(reader.in);
      named(m, name).origin = file;
    }

    FuncDef& named(Module& m, const String& name) {
      for (FuncDef& d : m.functions) {
        if (d.name == name) {
          return d;
        }
      }

      throw HirReadError("'" + name + "' names no function of the module");
    }

    void externDef(Reader& reader, Module& m) {
      ExternDef d;
      signature(reader, d);
      m.externs.push_back(move(d));
    }

    void globalDef(Reader& reader, Module& m) {
      GlobalDef d;
      d.name = string(reader.in);
      d.type = type(reader);
      d.initializer = expr(reader);
      m.globals.push_back(move(d));
    }

    void interface(Reader& reader, Module& m) {
      InterfaceDef d;
      d.name = string(reader.in);
      d.methods = methods(reader);
      d.implementers = strings(reader);
      m.interfaces.push_back(move(d));
    }

    Vector<InterfaceMethod> methods(Reader& reader) {
      const Vector<String> names = strings(reader);
      const Vector<Vector<TypePtr>> params = parameterLists(reader);
      const Vector<TypePtr> rets = types(reader);

      if (names.size() != params.size() || names.size() != rets.size()) {
        throw HirReadError("an interface whose method lists disagree");
      }

      Vector<InterfaceMethod> out;

      for (size_t i = 0; i < names.size(); i++) {
        out.push_back(InterfaceMethod{names[i], params[i], rets[i]});
      }

      return out;
    }

    Vector<Vector<TypePtr>> parameterLists(Reader& reader) {
      const int64_t n = count(reader.in);
      Vector<Vector<TypePtr>> out;

      for (int64_t i = 0; i < n; i++) {
        out.push_back(types(reader));
      }

      return out;
    }

    void hostType(Reader& reader, Module& m) {
      HostType d;
      d.name = string(reader.in);
      d.base = string(reader.in);

      d.typeArguments = types(reader);
      d.isInterface = boolean(reader.in);
      d.bindings = strings(reader);
      m.hostTypes.push_back(move(d));
    }

    void bridge(Reader& reader, Module& m) {
      Bridge d;
      d.polyirType = type(reader);
      d.target = string(reader.in);
      d.hostType = string(reader.in);

      d.toHost = string(reader.in);
      d.fromHost = string(reader.in);
      m.bridges.push_back(move(d));
    }

    void hostMember(Reader& reader, Module& m) {
      HostMember d;
      d.function = string(reader.in);
      d.owner = string(reader.in);
      d.member = string(reader.in);

      callShape(reader, d);
      m.hostMembers.push_back(move(d));
    }

    // How the member is called: static or not, with a result or not, and its bindings.
    void callShape(Reader& reader, HostMember& d) {
      d.isStatic = boolean(reader.in);
      d.hasResult = boolean(reader.in);
      d.bindings = strings(reader);
    }

    void hostImport(Reader& reader, Module& m) {
      HostImport d;
      d.target = string(reader.in);
      d.module = string(reader.in);
      d.alias = string(reader.in);
      m.hostImports.push_back(move(d));
    }

    // ── the file ──

    void readHeader(In& in);

    int64_t littleEndianWord(In& in);

    bool readSections(Reader& reader, Module& m);

    bool readSection(Reader& reader, Module& m);

    void readDeclarations(Reader& reader, Module& m);

    void readPlaces(Reader& reader, Module& m);

    void readGenerics(Reader& reader, Module& m);

    Module readModule(In& in) {
      readHeader(in);
      Reader reader{{in.bytes, in.pos}};
      Module m;

      if (!readSections(reader, m)) {
        throw HirReadError("the file has no declarations section");
      }

      return m;
    }

    void readHeader(In& in) {
      for (const uint8_t b : kMagic) {
        if (next(in) != b) {
          throw HirReadError("not a .pbc file (bad magic)");
        }
      }

      const int64_t version = littleEndianWord(in);

      if (version != kVersion) {
        throw HirReadError("pbc version " + to_string(version) + " is not supported (this reader is v" + to_string(kVersion) + ")");
      }
    }

    // Four bytes, lowest first.
    int64_t littleEndianWord(In& in) {
      int64_t value = 0;

      for (int shift = 0; shift < 32; shift += 8) {
        value += static_cast<int64_t>(next(in)) << shift;
      }

      return value;
    }

    // True when one of the sections was the declarations.
    bool readSections(Reader& reader, Module& m) {
      bool sawDecls = false;

      while (!atEnd(reader.in)) {
        sawDecls = readSection(reader, m) || sawDecls;
      }

      return sawDecls;
    }

    bool readSection(Reader& reader, Module& m) {
      const int id = next(reader.in);
      const int64_t size = count(reader.in);

      if (id == kSectionDecls) {
        readDeclarations(reader, m);
      } else if (id == kSectionPlaces) {
        readPlaces(reader, m);
      } else if (id == kSectionGenerics) {
        readGenerics(reader, m);
      } else {
        reader.in.pos += static_cast<size_t>(size);  // a section this reader does not know
      }

      return id == kSectionDecls;
    }

    // Where one declaration sits, as section 2 records it.
    struct PlaceRecord {
      String name;
      String module;
      bool internal = false;
      String outer;
      Vector<Annotation> annotations;
    };

    void setOuter(StructDef& d, const String& outer) {
      d.outer = outer;
    }

    void setOuter(FuncDef& d, const String& outer) {
      d.outer = outer;
    }

    void setOuter(InterfaceDef&, const String&) {
    }

    // The declaration the record names takes its place; a name the file does not declare is ignored.
    template <typename Decl>
    void place(Vector<Decl>& decls, const PlaceRecord& record) {
      for (Decl& d : decls) {
        if (d.name == record.name) {
          d.module = record.module;
          d.internal = record.internal;
          d.annotations = record.annotations;
          setOuter(d, record.outer);

          return;
        }
      }
    }

    Vector<Annotation> readAnnotations(Reader& reader) {
      Vector<Annotation> out(static_cast<size_t>(count(reader.in)));

      for (Annotation& a : out) {
        a.name = string(reader.in);
        a.args.resize(static_cast<size_t>(count(reader.in)));

        for (Pair<String, String>& arg : a.args) {
          arg.first = string(reader.in);
          arg.second = string(reader.in);
        }
      }

      return out;
    }

    // A field's default: the struct, the field's index, the value.
    void readDefault(Reader& reader, Module& m) {
      const String name = string(reader.in);
      const size_t index = static_cast<size_t>(count(reader.in));
      ExprPtr value = expr(reader);

      for (StructDef& d : m.structs) {
        if (d.name == name && index < d.fields.size()) {
          d.fields[index].initial = value;
        }
      }
    }

    template <typename Decl>
    void setOrigin(Vector<Decl>& decls, const String& name, const String& origin) {
      for (Decl& d : decls) {
        if (d.name == name) {
          d.origin = origin;
        }
      }
    }

    // The imported file of the declaration of that kind and name.
    void readOrigin(Reader& reader, Module& m) {
      const int kind = next(reader.in);
      const String name = string(reader.in);
      const String origin = string(reader.in);

      if (kind == kPlacedStruct) {
        setOrigin(m.structs, name, origin);
      } else if (kind == kPlacedFunction) {
        setOrigin(m.functions, name, origin);
      } else {
        kind == kPlacedInterface ? setOrigin(m.interfaces, name, origin) : setOrigin(m.globals, name, origin);
      }
    }

    void readPlace(Reader& reader, Module& m) {
      const int kind = next(reader.in);

      if (kind == kPlacedDefault || kind == kPlacedOrigin) {
        return kind == kPlacedDefault ? readDefault(reader, m) : readOrigin(reader, m);
      }

      PlaceRecord record;
      record.name = string(reader.in);
      record.module = string(reader.in);

      record.internal = boolean(reader.in);
      record.outer = string(reader.in);
      record.annotations = readAnnotations(reader);

      if (kind == kPlacedStruct) {
        return place(m.structs, record);
      }

      return kind == kPlacedFunction ? place(m.functions, record) : place(m.interfaces, record);
    }

    void readPlaces(Reader& reader, Module& m) {
      const int64_t n = count(reader.in);

      for (int64_t i = 0; i < n; i++) {
        readPlace(reader, m);
      }
    }

    void readDeclarations(Reader& reader, Module& m) {
      const int64_t n = count(reader.in);

      for (int64_t i = 0; i < n; i++) {
        decl(reader, m);
      }
    }

    // The declaration of `decls` the record names takes its generic; a name the file does not declare is ignored.
    template <typename Decl>
    void setGeneric(Vector<Decl>& decls, const String& name, const Generic& generic) {
      for (Decl& d : decls) {
        if (d.name == name) {
          d.generic = generic;

          return;
        }
      }
    }

    void readGenericRecord(Reader& reader, Module& m) {
      const int kind = next(reader.in);
      const String name = string(reader.in);

      Generic g;
      g.of = string(reader.in);
      g.arguments = types(reader);
      g.params = strings(reader);
      g.notParametric = string(reader.in);

      if (kind == kGenericOfStruct || kind == kGenericStruct) {
        return setGeneric(kind == kGenericStruct ? m.genericStructs : m.structs, name, g);
      }

      setGeneric(kind == kGenericFunction ? m.genericFunctions : m.functions, name, g);
    }

    // The generics as declared are read as a module of their own, then go beside the program's code.
    void readGenerics(Reader& reader, Module& m) {
      Module declared;
      readDeclarations(reader, declared);

      if (count(reader.in) > 0) {
        readPlaces(reader, declared);
      }

      m.genericStructs = move(declared.structs);
      m.genericFunctions = move(declared.functions);

      for (int64_t n = count(reader.in), i = 0; i < n; i++) {
        readGenericRecord(reader, m);
      }
    }

  }  // namespace

  Vector<uint8_t> WritePbc(const Module& module) {
    if (module.order.size() == DeclarationCount(module)) {
      return writeOrdered(module);
    }

    return writeOrdered(orderedModule(module));
  }

  Module ReadPbc(const Vector<uint8_t>& bytes) {
    In in{bytes};

    return readModule(in);
  }

}  // namespace polyir
