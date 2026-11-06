// Structural `==` for the types that are more than a number or a string (LowerEquality.nina, HirStructuralEquality.nina). Every such type the program
// compares gets a function `eq_<type>(a, b) -> i1` added to the module before it is compiled: a struct is equal when every field is (or by its own
// `equals` method), a list when it has the same length and equal elements, an either, optional or variant when it is the same case with an equal payload.
// The comparisons inside those functions are `==` again, so the functions of the types inside get written too. Maps and sets compare themselves.
#include "emit/llvm/LlvmEmitter.hpp"
#include "polyir/Builder.hpp"

namespace polyir::llvm {

  String EqualityName(const TypePtr& type);

  bool NeedsEquality(const Module& module, const TypePtr& type);

  namespace {

    String safe(const String& text) {
      String out;

      for (const char c : text) {
        out += isalnum(static_cast<unsigned char>(c)) ? c : '_';
      }

      return out;
    }

    bool isUserStruct(const Module& module, const TypePtr& type) {
      return type->kind == TypeKind::Struct && FindStruct(module, type->name) != nullptr && FindInterface(module, type->name) == nullptr;
    }

    Vector<TypePtr> fieldTypes(const Module& module, const TypePtr& type) {
      Vector<TypePtr> parts;

      for (const Field& field : FindStruct(module, type->name)->fields) {
        parts.push_back(field.type);
      }

      return parts;
    }

    // The types inside a value of the type: its fields, its elements, its payloads.
    Vector<TypePtr> partsOf(const Module& module, const TypePtr& type) {
      if (isUserStruct(module, type)) {
        return fieldTypes(module, type);
      }

      if (type->kind == TypeKind::Either) {
        return {type->element, type->second};
      }

      if (type->kind == TypeKind::Variant) {
        return type->parameters;
      }

      return type->element ? Vector<TypePtr>{type->element} : Vector<TypePtr>{};
    }

    // The expression that says whether two values of one type are equal; the helper of a type is called by `==` itself.
    struct Writer {
      const Module& module_;
      PolyIRBuilder& builder_;
    };

    void write(Writer& writer, const TypePtr& type);

    ExprPtr same(Writer& writer, Block& at, const String& field);

    void body(Writer& writer, Block& at, const TypePtr& type);

    void structBody(Writer& writer, Block& at, const TypePtr& type);

    void listBody(Block& at, const TypePtr& type);

    void elementCheck(Block& loop);

    ExprPtr eitherSame(Block& at);

    ExprPtr optionalSame(Block& at);

    void variantBody(Block& at, const TypePtr& type);

    void caseCheck(Block& at, int index);

    void write(Writer& writer, const TypePtr& type) {
      FunctionBuilder& function = DefineFunction(writer.builder_, EqualityName(type), {{"a", type}, {"b", type}}, MakeBool());
      body(writer, function.body, type);
    }

    ExprPtr same(Writer& writer, Block& at, const String& field) {
      return Eq(FieldAccess(writer.builder_, Local(at, "a"), field), FieldAccess(writer.builder_, Local(at, "b"), field));
    }

    void body(Writer& writer, Block& at, const TypePtr& type) {
      const TypeKind kind = type->kind;

      if (kind == TypeKind::Struct) {
        return structBody(writer, at, type);
      }

      if (kind == TypeKind::DynamicArray || kind == TypeKind::Buffer || kind == TypeKind::FixedArray) {
        return listBody(at, type);
      }

      if (kind == TypeKind::Variant) {
        return variantBody(at, type);
      }

      Return(at, kind == TypeKind::Either ? eitherSame(at) : optionalSame(at));
    }

    void structBody(Writer& writer, Block& at, const TypePtr& type) {
      const String equals = type->name + "_equals";

      if (FindFunction(writer.module_, equals) != nullptr) {
        return Return(at, Call(writer.builder_, equals, {Local(at, "a"), Local(at, "b")}));
      }

      ExprPtr all;

      for (const Field& field : FindStruct(writer.module_, type->name)->fields) {
        all = all ? LogAnd(all, same(writer, at, field.name)) : same(writer, at, field.name);
      }

      Return(at, all ? all : BoolConst(true));
    }

    void listBody(Block& at, const TypePtr& type) {
      // A fixed array's or a buffer's length is part of its type, so only two growable lists can differ in length.
      const bool lengthsCanDiffer = type->kind == TypeKind::DynamicArray;
      const ExprPtr a = Local(at, "a");
      const ExprPtr b = Local(at, "b");

      if (lengthsCanDiffer) {
        IfThen(at, Ne(Len(a), Len(b)), [&](Block& then) { Return(then, BoolConst(false)); });
      }

      ForRange(at, "i", IntConst(0), Len(a), [&](Block& loop) { elementCheck(loop); });
      Return(at, BoolConst(true));
    }

    void elementCheck(Block& loop) {
      const ExprPtr i = Local(loop, "i");
      const ExprPtr differ = Ne(Index(Local(loop, "a"), {i}), Index(Local(loop, "b"), {i}));
      IfThen(loop, differ, [&](Block& then) { Return(then, BoolConst(false)); });
    }

    ExprPtr eitherSame(Block& at) {
      const ExprPtr a = Local(at, "a");
      const ExprPtr b = Local(at, "b");
      IfThen(at, Ne(IsOk(a), IsOk(b)), [&](Block& then) { Return(then, BoolConst(false)); });
      IfThen(at, IsOk(a), [&](Block& then) { Return(then, Eq(UnwrapOk(a), UnwrapOk(b))); });

      return Eq(UnwrapErr(a), UnwrapErr(b));
    }

    ExprPtr optionalSame(Block& at) {
      const ExprPtr a = Local(at, "a");
      const ExprPtr b = Local(at, "b");
      IfThen(at, Ne(IsSome(a), IsSome(b)), [&](Block& then) { Return(then, BoolConst(false)); });
      IfThen(at, IsSome(a), [&](Block& then) { Return(then, Eq(Unwrap(a), Unwrap(b))); });

      return BoolConst(true);
    }

    void variantBody(Block& at, const TypePtr& type) {
      const ExprPtr a = Local(at, "a");
      const ExprPtr b = Local(at, "b");
      IfThen(at, Ne(VariantTag(a), VariantTag(b)), [&](Block& then) { Return(then, BoolConst(false)); });

      for (size_t i = 0; i < type->parameters.size(); i++) {
        caseCheck(at, static_cast<int>(i));
      }

      Return(at, BoolConst(true));
    }

    void caseCheck(Block& at, int index) {
      const ExprPtr a = Local(at, "a");
      const ExprPtr b = Local(at, "b");
      IfThen(at, VariantIsCase(a, index), [&](Block& then) { Return(then, Eq(VariantUnwrap(a, index), VariantUnwrap(b, index))); });
    }


    // ── finding the comparisons ──────────────────────────────────────────────────────────────────────────────────────────────────────────────────

    struct Finder {
      const Module& module_;
      Map<String, TypePtr> found_{};
    };

    Map<String, TypePtr> run(Finder& finder);

    void findInStatements(Finder& finder, const StmtList& list);

    void findInStatement(Finder& finder, const Stmt& statement);

    void findInStatementExpressions(Finder& finder, const Stmt& statement);

    void findInExpressions(Finder& finder, const Vector<ExprPtr>& list);

    void findInExpression(Finder& finder, const ExprPtr& expr);

    void note(Finder& finder, const TypePtr& type);

    Map<String, TypePtr> run(Finder& finder) {
      for (const FuncDef& definition : finder.module_.functions) {
        findInStatements(finder, definition.body);
      }

      for (const GlobalDef& global : finder.module_.globals) {
        findInExpression(finder, global.initializer);
      }

      return move(finder.found_);
    }

    void findInStatements(Finder& finder, const StmtList& list) {
      for (const StmtPtr& statement : list) {
        findInStatement(finder, *statement);
      }
    }

    // The expressions a statement holds, then the statements inside it.
    void findInStatement(Finder& finder, const Stmt& statement) {
      findInStatementExpressions(finder, statement);
      findInStatements(finder, statement.body);
      findInStatements(finder, statement.elseBody);
    }

    void findInStatementExpressions(Finder& finder, const Stmt& statement) {
      findInExpression(finder, statement.value);
      findInExpression(finder, statement.target);

      findInExpressions(finder, statement.initializers);
      findInExpressions(finder, statement.steps);
      findInExpressions(finder, statement.arguments);
      findInExpressions(finder, statement.elseArguments);

      for (const Vector<ExprPtr>& list : statement.caseArguments) {
        findInExpressions(finder, list);
      }
    }

    void findInExpressions(Finder& finder, const Vector<ExprPtr>& list) {
      for (const ExprPtr& expr : list) {
        findInExpression(finder, expr);
      }
    }

    void findInExpression(Finder& finder, const ExprPtr& expr) {
      if (!expr) {
        return;
      }

      if (expr->kind == ExprKind::Binary && (expr->binaryOp == BinOp::Eq || expr->binaryOp == BinOp::Ne)) {
        note(finder, expr->operands[0]->type);
      }

      findInExpressions(finder, expr->operands);
      findInStatements(finder, expr->closureBody);
    }

    void note(Finder& finder, const TypePtr& type) {
      if (!NeedsEquality(finder.module_, type) || finder.found_.count(EqualityName(type)) != 0) {
        return;
      }

      finder.found_[EqualityName(type)] = type;

      for (const TypePtr& part : partsOf(finder.module_, type)) {
        note(finder, part);
      }
    }

  }  // namespace

  String EqualityName(const TypePtr& type) {
    return "eq_" + safe(TypeText(*type));
  }

  bool NeedsEquality(const Module& module, const TypePtr& type) {
    const TypeKind kind = type->kind;

    if (kind == TypeKind::Struct) {
      return isUserStruct(module, type);
    }

    return kind == TypeKind::DynamicArray || kind == TypeKind::Buffer || kind == TypeKind::FixedArray || kind == TypeKind::Either || kind == TypeKind::Optional ||
           kind == TypeKind::Variant;
  }

  namespace {

    // A builder that knows the module's structs and function signatures, so the comparison functions can be written against them.
    PolyIRBuilder builderFor(const Module& module) {
      PolyIRBuilder builder;

      for (const StructDef& structure : module.structs) {
        DefineStruct(builder, structure.name, structure.fields);
      }

      for (const FuncDef& definition : module.functions) {
        DeclareFunction(builder, definition.name, definition.parameters, definition.result);
      }

      return builder;
    }

    // The module and the functions the builder wrote.
    Module joined(const Module& module, const PolyIRBuilder& builder) {
      Module out = module;

      for (const FuncDef& definition : Build(builder).functions) {
        out.functions.push_back(definition);
      }

      return out;
    }

  }  // namespace

  Module WithEqualityHelpers(const Module& module) {
    Finder finder{module};
    const Map<String, TypePtr> types = run(finder);

    if (types.empty()) {
      return module;
    }

    PolyIRBuilder builder = builderFor(module);
    Writer writer{module, builder};

    for (const auto& entry : types) {
      write(writer, entry.second);
    }

    return joined(module, builder);
  }

}  // namespace polyir::llvm
