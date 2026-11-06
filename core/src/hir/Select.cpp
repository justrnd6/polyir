// A `Select` (Hir.hpp) as the `if` chain it means: one `if` per arm, its test each key's test joined by `||`, and the
// default as the last `else`. The VM, LLVM and the direct wasm writer run this, and a translation writes it as the lower form.
#include "polyir/Std.hpp"
#include "polyir/Hir.hpp"

namespace polyir {

  namespace {

    SharedPtr<Expr> newExpr(ExprKind kind, TypePtr type, Span at) {
      SharedPtr<Expr> made = make_shared<Expr>();
      made->kind = kind;
      made->type = move(type);
      made->at = at;

      return made;
    }

    ExprPtr builtinTest(Builtin op, const Expr& value, Vector<ExprPtr> args) {
      SharedPtr<Expr> made = newExpr(ExprKind::CallBuiltin, MakeBool(), value.at);
      made->builtin = op;
      made->operands.push_back(make_shared<Expr>(value));
      made->operands.insert(made->operands.end(), args.begin(), args.end());

      return made;
    }

    ExprPtr binary(BinOp op, ExprPtr left, ExprPtr right) {
      SharedPtr<Expr> made = newExpr(ExprKind::Binary, MakeBool(), left->at);
      made->binaryOp = op;
      made->operands = {move(left), move(right)};

      return made;
    }

    ExprPtr caseNumber(int64_t number, Span at) {
      SharedPtr<Expr> made = newExpr(ExprKind::ConstInt, MakeInt(64), at);
      made->intValue = number;

      return made;
    }

    // A sum's case `number` tested on `value`: `isSome`/`isNone`, `isOk`/`isErr`, or `isCase(value, number)`.
    ExprPtr caseTest(const Expr& value, int64_t number) {
      switch (value.type->kind) {
        case TypeKind::Optional:
          return builtinTest(number == 0 ? Builtin::OptionIsSome : Builtin::OptionIsNone, value, {});
        case TypeKind::Either:
          return builtinTest(number == 0 ? Builtin::EitherIsOk : Builtin::EitherIsErr, value, {});
        default:
          return builtinTest(Builtin::VariantIsCase, value, {caseNumber(number, value.at)});
      }
    }

    bool isSum(const Type& type) {
      return type.kind == TypeKind::Optional || type.kind == TypeKind::Either || type.kind == TypeKind::Variant;
    }

    // The arm's test: its keys' tests joined by `||`.
    ExprPtr armTest(const Expr& value, const Vector<ExprPtr>& keys) {
      ExprPtr test = SelectKeyTest(value, *keys[0]);

      for (size_t i = 1; i < keys.size(); i++) {
        test = binary(BinOp::LogOr, move(test), SelectKeyTest(value, *keys[i]));
      }

      return test;
    }

    // The arms from `first` on, each an `if` whose `else` holds the arms after it, and the default last.
    StmtList armsFrom(const Stmt& select, size_t first) {
      if (first == select.caseKeys.size()) {
        return select.elseBody;
      }

      StmtPtr made = make_shared<Stmt>();
      made->kind = StmtKind::If;
      made->at = select.at;

      made->value = armTest(*select.value, select.caseKeys[first]);
      made->body = select.caseBodies[first];
      made->elseBody = armsFrom(select, first + 1);

      return StmtList{made};
    }

    bool holdsSelect(const StmtList& body);

    bool holdsSelect(const ExprPtr& e);

    bool anyHoldsSelect(const Vector<ExprPtr>& items) {
      for (const ExprPtr& e : items) {
        if (holdsSelect(e)) {
          return true;
        }
      }

      return false;
    }

    // An expression holds a `Select` when a closure in it does.
    bool holdsSelect(const ExprPtr& e) {
      if (!e) {
        return false;
      }

      return (e->kind == ExprKind::ClosureLit && holdsSelect(e->closureBody)) || anyHoldsSelect(e->operands);
    }

    bool exprsHoldSelect(const Stmt& s) {
      const bool jumps = anyHoldsSelect(s.arguments) || anyHoldsSelect(s.elseArguments);

      return holdsSelect(s.value) || holdsSelect(s.target) || anyHoldsSelect(s.initializers) || anyHoldsSelect(s.steps) || jumps;
    }

    bool holdsSelect(const Stmt& s) {
      return s.kind == StmtKind::Select || holdsSelect(s.body) || holdsSelect(s.elseBody) || exprsHoldSelect(s);
    }

    bool holdsSelect(const StmtList& body) {
      for (const StmtPtr& s : body) {
        if (s && holdsSelect(*s)) {
          return true;
        }
      }

      return false;
    }

    ExprPtr lowered(const ExprPtr& e);

    Vector<ExprPtr> lowered(const Vector<ExprPtr>& items) {
      Vector<ExprPtr> out;

      for (const ExprPtr& e : items) {
        out.push_back(lowered(e));
      }

      return out;
    }

    // An expression whose closures hold a `Select` is copied with their bodies lowered; any other is shared as it is.
    ExprPtr lowered(const ExprPtr& e) {
      if (!holdsSelect(e)) {
        return e;
      }

      SharedPtr<Expr> copy = make_shared<Expr>(*e);
      copy->operands = lowered(e->operands);
      copy->closureBody = WithoutSelects(e->closureBody);

      return copy;
    }

    void lowerExprs(Stmt& s) {
      s.value = lowered(s.value);
      s.target = lowered(s.target);
      s.initializers = lowered(s.initializers);
      s.steps = lowered(s.steps);

      s.arguments = lowered(s.arguments);
      s.elseArguments = lowered(s.elseArguments);
    }

    // A statement that holds a `Select` is copied with its blocks and expressions lowered; any other is shared as it is.
    StmtPtr lowered(const StmtPtr& s) {
      if (!s || !holdsSelect(*s)) {
        return s;
      }

      StmtPtr copy = make_shared<Stmt>(*s);
      copy->body = WithoutSelects(s->body);
      copy->elseBody = WithoutSelects(s->elseBody);
      lowerExprs(*copy);

      return copy;
    }

  }  // namespace

  Vector<const StmtList*> BlocksOf(const Stmt& s) {
    Vector<const StmtList*> blocks{&s.body};

    for (const StmtList& arm : s.caseBodies) {
      blocks.push_back(&arm);
    }

    blocks.push_back(&s.elseBody);

    return blocks;
  }

  StmtList WithoutSelects(const StmtList& body) {
    StmtList out;

    for (const StmtPtr& s : body) {
      if (s && s->kind == StmtKind::Select) {
        const StmtList ifs = WithoutSelects(SelectAsIfs(*s));
        out.insert(out.end(), ifs.begin(), ifs.end());
      } else {
        out.push_back(lowered(s));
      }
    }

    return out;
  }

  Module WithoutSelects(const Module& module) {
    Module plain = module;

    for (FuncDef& f : plain.functions) {
      f.body = WithoutSelects(f.body);
    }

    for (GlobalDef& g : plain.globals) {
      g.initializer = lowered(g.initializer);
    }

    return plain;
  }

  ExprPtr SelectKeyTest(const Expr& value, const Expr& key) {
    if (isSum(*value.type)) {
      return caseTest(value, key.intValue);
    }

    return binary(BinOp::Eq, make_shared<Expr>(value), make_shared<Expr>(key));
  }

  StmtList SelectAsIfs(const Stmt& select) {
    return armsFrom(select, 0);
  }

}  // namespace polyir
