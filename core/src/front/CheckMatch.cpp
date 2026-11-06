// `match` and the switch of block arms (ParserMatch.cpp), checked into the statements they mean ("costumes off").
//
// Each arm's pattern becomes tests on a PLACE, which is the subject or a part of it (`found.unwrap()`, `xs[0]`), and the
// names it binds to places. A guard reads the places, not the names, so it simply joins the arm's test with `&&`. An arm's
// block starts with `let n: T = place` only when it reads `n`. The arms become a `Select` (Hir.hpp) when each is exact keys
// or a sum case, and an `if` chain otherwise. A value `match` assigns a hidden `var matchValue_L_C` in each arm, as an `if`
// value does (CheckIfValue.cpp). A subject that is not a plain name is evaluated once, into a hidden `let match_L_C`.
#include "front/Checker.hpp"
#include "hir/Lift.hpp"

namespace polyir::front {

  namespace {

    // A name a pattern binds, and the place it stands for: `n` in `Some(n)` stands for `found.unwrap()`.
    struct Bound {
      Ident name;
      ExprPtr place;
    };

    // What a pattern asks of a place: tests that must all hold (none for `_` or a name), and the names it binds.
    struct Tested {
      Vector<ExprPtr> tests{};
      Vector<Bound> bound{};
    };

    // One arm, checked. `always` for `else` or a catch-all; `keys` (and the payload's name) when a `Select` can take it.
    struct Arm {
      ExprPtr test{};
      StmtList body{};
      Vector<ExprPtr> keys{};
      String payload{};
      bool switchable = false;
      bool always = false;
      Span at{};
    };

    // The match being checked: its subject (null for none), and for a value its hidden variable and type. Problems are
    // gathered here, and the match is then only its problems.
    struct Matching {
      ExprPtr subject{};
      String valueName{};
      TypePtr type{};
      const TypePtr* expected = nullptr;
      bool knownNonEmpty = false;  // an earlier arm was `[]`, so a list that reaches a later arm has an element
      Vector<ExprPtr> problems{};
    };

    // What testing a pattern needs: the checker, and the match it is in.
    struct Walk {
      Checker& check;
      Matching& match;
    };

    const AstPattern& patternNode(const Checker& check, PatternId id) {
      return check.program->patterns[id];
    }

    String spot(const Pos& pos) {
      return to_string(pos.line) + "_" + to_string(pos.column);
    }

    // `an i64`, `a string`: a type named in a sentence.
    String aType(const TypePtr& type) {
      const String text = TypeText(type);

      return (String("aeiou").find(text[0]) != String::npos ? "an " : "a ") + text;
    }

    void fail(Matching& m, const String& why, Pos pos) {
      m.problems.push_back(Unsupported(why, SpanAt(pos)));
    }

    // ── tests ───────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────

    ExprPtr joined(BinOp op, const ExprPtr& left, const ExprPtr& right) {
      return BinaryNode(op, left, right, IntType(1), left->at);
    }

    // The tests joined by `&&`: null when there are none (anything matches).
    ExprPtr allOf(const Vector<ExprPtr>& tests) {
      ExprPtr all;

      for (const ExprPtr& test : tests) {
        all = all ? joined(BinOp::LogAnd, all, test) : test;
      }

      return all;
    }

    ExprPtr builtin(Builtin op, const ExprPtr& place, Vector<ExprPtr> args, TypePtr type) {
      return BuiltinNode(op, place, move(args), move(type), place->at);
    }

    ExprPtr whole(int64_t value, Span at) {
      return ConstInt(value, IntType(64), at);
    }

    ExprPtr flagTest(Builtin op, const ExprPtr& place) {
      return builtin(op, place, {}, IntType(1));
    }

    void testPattern(Walk& walk, PatternId id, const ExprPtr& place, Tested& out);

    // ── literals ──

    bool sameFamily(const Type& a, const Type& b) {
      const bool numbers = IsInteger(a) && IsInteger(b) && !IsBoolean(a) && !IsBoolean(b);
      const bool texts = a.kind == TypeKind::String && b.kind == TypeKind::String;

      return numbers || texts || (IsFloat(a) && IsFloat(b)) || (IsBoolean(a) && IsBoolean(b));
    }

    // `true` is the place itself and `false` its negation; any other literal is `place == literal`.
    ExprPtr literalTest(const ExprPtr& place, const ExprPtr& literal) {
      if (!IsBoolean(*literal->type)) {
        return joined(BinOp::Eq, place, literal);
      }

      const bool truth = literal->kind == ExprKind::ConstBool ? literal->boolValue : literal->intValue != 0;

      return truth ? place : UnaryNode(UnOp::LogNot, place, IntType(1), place->at);
    }

    // The literal a value pattern holds, checked as the place's type; null when it does not fit.
    ExprPtr literalOf(Walk& walk, const AstPattern& pattern, const ExprPtr& place) {
      const ExprPtr literal = CheckExprExpecting(walk.check, pattern.value, place->type);

      if (literal->kind == ExprKind::Unsupported || !sameFamily(*literal->type, *place->type)) {
        fail(walk.match, "the pattern does not fit the " + TypeText(place->type) + " it is matched against", pattern.pos);

        return nullptr;
      }

      return literal;
    }

    void valuePattern(Walk& walk, const AstPattern& pattern, const ExprPtr& place, Tested& out) {
      if (const ExprPtr literal = literalOf(walk, pattern, place)) {
        out.tests.push_back(literalTest(place, literal));
      }
    }

    // ── sums ──

    bool kindIs(const ExprPtr& place, TypeKind kind) {
      return place->type->kind == kind;
    }

    // `Some(p)`, `Ok(p)`, `Err(p)`, `variant(N, p)`: the case's test, then the payload's pattern on the payload.
    void payloadPattern(Walk& walk, const AstPattern& pattern, ExprPtr test, const ExprPtr& payload, Tested& out) {
      out.tests.push_back(move(test));
      testPattern(walk, pattern.first, payload, out);
    }

    void optionalPattern(Walk& walk, const AstPattern& pattern, const ExprPtr& place, Tested& out) {
      if (!kindIs(place, TypeKind::Optional)) {
        return fail(walk.match, "`Some` and `None` match an optional, not " + aType(place->type), pattern.pos);
      }

      if (pattern.kind == AstPatternKind::None) {
        return out.tests.push_back(flagTest(Builtin::OptionIsNone, place));
      }

      const ExprPtr payload = builtin(Builtin::OptionUnwrap, place, {}, place->type->element);
      payloadPattern(walk, pattern, flagTest(Builtin::OptionIsSome, place), payload, out);
    }

    void eitherPattern(Walk& walk, const AstPattern& pattern, const ExprPtr& place, Tested& out) {
      if (!kindIs(place, TypeKind::Either)) {
        return fail(walk.match, "`Ok` and `Err` match an either, not " + aType(place->type), pattern.pos);
      }

      const bool ok = pattern.kind == AstPatternKind::Ok;
      const ExprPtr payload = ok ? builtin(Builtin::EitherUnwrapOk, place, {}, place->type->element)
                                 : builtin(Builtin::EitherUnwrapErr, place, {}, place->type->second);
      payloadPattern(walk, pattern, flagTest(ok ? Builtin::EitherIsOk : Builtin::EitherIsErr, place), payload, out);
    }

    void casePattern(Walk& walk, const AstPattern& pattern, const ExprPtr& place, Tested& out) {
      const int64_t number = ParseInt(pattern.literal.text);
      const bool fits = kindIs(place, TypeKind::Variant) && number < static_cast<int64_t>(place->type->parameters.size());

      if (!fits) {
        return fail(walk.match, "`variant(" + to_string(number) + ", …)` is not a case of " + TypeText(place->type), pattern.pos);
      }

      const ExprPtr test = builtin(Builtin::VariantIsCase, place, {whole(number, place->at)}, IntType(1));
      const ExprPtr payload = builtin(Builtin::VariantUnwrap, place, {whole(number, place->at)}, place->type->parameters[number]);
      payloadPattern(walk, pattern, test, payload, out);
    }

    // ── lists ──

    // `len(xs) == k` for a list of exactly k elements, `len(xs) >= k` for one with a rest; none when every list that gets
    // here passes it.
    void lengthTest(Walk& walk, const AstPattern& pattern, const ExprPtr& place, Tested& out) {
      const int64_t count = static_cast<int64_t>(pattern.items.count);
      const bool needless = pattern.hasRest && (count == 0 || (count == 1 && walk.match.knownNonEmpty));

      if (needless) {
        return;
      }

      const ExprPtr length = builtin(Builtin::Len, place, {}, IntType(64));
      out.tests.push_back(joined(pattern.hasRest ? BinOp::Ge : BinOp::Eq, length, whole(count, place->at)));
    }

    // `...rest`: a new list of the elements after the first k (Lists.poly's `drop`, which the compiler adds).
    void restOf(Walk& walk, const AstPattern& pattern, const ExprPtr& place, Tested& out) {
      if (!Known(pattern.name.pos)) {
        return;
      }

      const Ident drop{SymbolOf(walk.check, "drop"), pattern.name.pos};
      const ExprPtr after = whole(static_cast<int64_t>(pattern.items.count), place->at);
      out.bound.push_back(Bound{pattern.name, DirectCall(walk.check, drop, {place, after})});
    }

    void listPattern(Walk& walk, const AstPattern& pattern, const ExprPtr& place, Tested& out) {
      if (!kindIs(place, TypeKind::DynamicArray)) {
        return fail(walk.match, "a list pattern matches an array or a mutarray, not " + aType(place->type), pattern.pos);
      }

      lengthTest(walk, pattern, place, out);
      const View<PatternId> items = walk.check.program->patternRefs[pattern.items];

      for (size_t i = 0; i < items.size(); i++) {
        testPattern(walk, items[i], IndexInto(place, {whole(static_cast<int64_t>(i), place->at)}), out);
      }

      restOf(walk, pattern, place, out);
    }

    // ── alternatives ──

    // One alternative of `a | b`, which may bind no name.
    ExprPtr alternative(Walk& walk, PatternId id, const ExprPtr& place) {
      Tested one;
      testPattern(walk, id, place, one);

      if (!one.bound.empty()) {
        fail(walk.match, "an alternative (`|`) binds no names: each one matches without them", patternNode(walk.check, id).pos);
      }

      return allOf(one.tests);
    }

    // `a | b`: any of them. One that matches anything makes the whole pattern match anything.
    void orPattern(Walk& walk, const AstPattern& pattern, const ExprPtr& place, Tested& out) {
      ExprPtr any;
      bool always = false;

      for (PatternId id : walk.check.program->patternRefs[pattern.items]) {
        const ExprPtr test = alternative(walk, id, place);
        always = always || !test;
        any = !test ? any : (any ? joined(BinOp::LogOr, any, test) : test);
      }

      if (!always && any) {
        out.tests.push_back(any);
      }
    }

    // A variant's case, a list, alternatives, or `_` (which tests nothing).
    void shapePattern(Walk& walk, const AstPattern& pattern, const ExprPtr& place, Tested& out) {
      switch (pattern.kind) {
        case AstPatternKind::Case:
          return casePattern(walk, pattern, place, out);
        case AstPatternKind::List:
          return listPattern(walk, pattern, place, out);
        case AstPatternKind::Or:
          return orPattern(walk, pattern, place, out);
        default:
          return;
      }
    }

    void testPattern(Walk& walk, PatternId id, const ExprPtr& place, Tested& out) {
      const AstPattern& pattern = patternNode(walk.check, id);

      switch (pattern.kind) {
        case AstPatternKind::Value:
          return valuePattern(walk, pattern, place, out);
        case AstPatternKind::Name:
          return out.bound.push_back(Bound{pattern.name, place});
        case AstPatternKind::Some: case AstPatternKind::None:
          return optionalPattern(walk, pattern, place, out);
        case AstPatternKind::Ok: case AstPatternKind::Err:
          return eitherPattern(walk, pattern, place, out);
        default:
          return shapePattern(walk, pattern, place, out);
      }
    }

  }  // namespace

  namespace {

    // ── what patterns cover ─────────────────────────────────────────────────────────────────────────────────────────────────────────────────

    bool irrefutable(const Checker& check, PatternId id);

    bool anyIrrefutable(const Checker& check, Range<PatternId> items) {
      for (PatternId each : check.program->patternRefs[items]) {
        if (irrefutable(check, each)) {
          return true;
        }
      }

      return false;
    }

    bool allIrrefutable(const Checker& check, Range<PatternId> items) {
      for (PatternId each : check.program->patternRefs[items]) {
        if (!irrefutable(check, each)) {
          return false;
        }
      }

      return true;
    }

    // Whether a pattern matches anything: `_`, a name, or alternatives one of which does.
    bool irrefutable(const Checker& check, PatternId id) {
      const AstPattern& pattern = patternNode(check, id);

      if (pattern.kind == AstPatternKind::Or) {
        return anyIrrefutable(check, pattern.items);
      }

      return pattern.kind == AstPatternKind::Any || pattern.kind == AstPatternKind::Name;
    }

    // The patterns of the arms that have no guard, each alternative of `a | b` on its own.
    void addFlat(const Checker& check, PatternId id, Vector<PatternId>& out) {
      const AstPattern& pattern = patternNode(check, id);

      if (pattern.kind != AstPatternKind::Or) {
        return out.push_back(id);
      }

      for (PatternId each : check.program->patternRefs[pattern.items]) {
        addFlat(check, each, out);
      }
    }

    // The sum case a pattern stands for when its payload matches anything (`Some(_)`: 0), or -1. `None` is case 1.
    int64_t wholeCase(const Checker& check, const AstPattern& pattern) {
      switch (pattern.kind) {
        case AstPatternKind::None:
          return 1;
        case AstPatternKind::Some: case AstPatternKind::Ok:
          return irrefutable(check, pattern.first) ? 0 : -1;
        case AstPatternKind::Err:
          return irrefutable(check, pattern.first) ? 1 : -1;
        case AstPatternKind::Case:
          return irrefutable(check, pattern.first) ? ParseInt(pattern.literal.text) : -1;
        default:
          return -1;
      }
    }

    bool literalIs(const Checker& check, const AstPattern& pattern, AstExprKind kind) {
      return pattern.kind == AstPatternKind::Value && Node(check, pattern.value).kind == kind;
    }

    // Each case of a sum (or both flags of a bool, `false` as case 0) that some pattern takes whole.
    Set<int64_t> casesTaken(const Checker& check, const Vector<PatternId>& patterns) {
      Set<int64_t> taken;

      for (PatternId id : patterns) {
        const AstPattern& pattern = patternNode(check, id);
        taken.insert(literalIs(check, pattern, AstExprKind::False) ? 0 : (literalIs(check, pattern, AstExprKind::True) ? 1 : wholeCase(check, pattern)));
      }

      return taken;
    }

    // A list pattern whose elements match anything: it takes every list of its length, or (with a rest) of at least it.
    bool wholeList(const Checker& check, const AstPattern& pattern) {
      return pattern.kind == AstPatternKind::List && allIrrefutable(check, pattern.items);
    }

    // The lengths the whole list patterns take exactly, and the shortest one a rest pattern takes (SIZE_MAX for none).
    Set<size_t> exactLengths(const Checker& check, const Vector<PatternId>& patterns, size_t& shortestRest) {
      Set<size_t> exact;
      shortestRest = SIZE_MAX;

      for (PatternId id : patterns) {
        const AstPattern& pattern = patternNode(check, id);
        const size_t count = pattern.items.count;

        if (wholeList(check, pattern) && pattern.hasRest) {
          shortestRest = min(shortestRest, count);
        } else if (wholeList(check, pattern)) {
          exact.insert(count);
        }
      }

      return exact;
    }

    // Lists of every length: the shortest list a rest pattern takes, and every length under it exactly.
    bool coversLists(const Checker& check, const Vector<PatternId>& patterns) {
      size_t shortestRest = SIZE_MAX;
      const Set<size_t> exact = exactLengths(check, patterns, shortestRest);

      for (size_t n = 0; n < shortestRest && shortestRest != SIZE_MAX; n++) {
        if (exact.count(n) == 0) {
          return false;
        }
      }

      return shortestRest != SIZE_MAX;
    }

    size_t caseCount(const Type& type) {
      return type.kind == TypeKind::Variant ? type.parameters.size() : 2;
    }

    // Whether patterns with no guard take every value of the subject's type. That is both flags, both cases of an optional
    // or an either, every case of a variant, or lists of every length.
    bool covers(const Checker& check, const Vector<PatternId>& patterns, const Type& type) {
      for (PatternId id : patterns) {
        if (irrefutable(check, id)) {
          return true;
        }
      }

      if (type.kind == TypeKind::DynamicArray) {
        return coversLists(check, patterns);
      }

      const bool sum = IsBoolean(type) || type.kind == TypeKind::Optional || type.kind == TypeKind::Either || type.kind == TypeKind::Variant;
      Set<int64_t> taken = casesTaken(check, patterns);
      taken.erase(-1);

      return sum && taken.size() == caseCount(type);
    }

    Vector<PatternId> unguarded(const Checker& check, View<AstArm> arms) {
      Vector<PatternId> patterns;

      for (const AstArm& arm : arms) {
        if (Valid(arm.pattern) && !Valid(arm.guard)) {
          addFlat(check, arm.pattern, patterns);
        }
      }

      return patterns;
    }

  }  // namespace

  namespace {

    // ── an arm's names, guard and block ─────────────────────────────────────────────────────────────────────────────────────────────────────

    // A growable value (a list, a map) is kept in a `var` slot, as a `let` of one is (CheckStmt.cpp).
    void bindAll(Checker& check, const Vector<Bound>& bound) {
      for (const Bound& each : bound) {
        BindName(check.scope, each.name.name, IsGrowable(each.place->type) ? BindingKind::Slot : BindingKind::Local, each.place->type);
      }
    }

    // `value` with each read of a bound name replaced by its place: a guard reads `found.unwrap()`, not `n`.
    ExprPtr substituted(Checker& check, const ExprPtr& value, const Vector<Bound>& bound) {
      const bool read = value->kind == ExprKind::Local || value->kind == ExprKind::LoadSlot;

      for (const Bound& each : bound) {
        if (read && value->name == Str(check, each.name.name)) {
          return each.place;
        }
      }

      SharedPtr<Expr> copy = make_shared<Expr>(*value);

      for (ExprPtr& operand : copy->operands) {
        operand = substituted(check, operand, bound);
      }

      return copy;
    }

    // A condition or a guard must be a flag. One with no type has a problem of its own, reported where it is.
    ExprPtr flag(Matching& m, ExprPtr value, const String& what) {
      if (value->kind != ExprKind::Unsupported && !IsVoid(*value->type) && !IsBoolean(*value->type)) {
        m.problems.push_back(Unsupported(what + " is a bool, not " + aType(value->type), value->at));
      }

      return value;
    }

    // The guard after `if`, checked with the pattern's names bound and then reading their places. A guard holds no `if` or
    // `match` value: what one hoists would run before the pattern has bound anything.
    ExprPtr guardOf(Checker& check, Matching& m, const AstArm& arm, const Vector<Bound>& bound) {
      const size_t before = check.hoisted.size();
      ExprPtr guard = flag(m, CheckExpr(check, arm.guard), "a guard (`if …`)");

      if (check.hoisted.size() != before) {
        check.hoisted.resize(before);
        m.problems.push_back(Unsupported("a guard holds no `if` or `match` value: test it in the arm instead", guard->at));
      }

      return substituted(check, guard, bound);
    }

    // `let n: T = place` before the block, for each name the block reads.
    StmtList withLets(Checker& check, const Vector<Bound>& bound, StmtList body) {
      StmtList lets;

      for (const Bound& each : bound) {
        const String name = Str(check, each.name.name);

        if (CountIn(body, ReadsName(name)) == 0) {
          continue;
        }

        StmtPtr let = NewStmt(IsGrowable(each.place->type) ? StmtKind::Var : StmtKind::Let, SpanAt(each.name.pos));
        let->name = name;
        let->type = each.place->type;
        let->value = each.place;
        lets.push_back(move(let));
      }

      lets.insert(lets.end(), body.begin(), body.end());

      return lets;
    }

    // ── an arm's value ──

    // The type every arm's value has: the expected one, else the first arm's.
    ExprPtr typedValue(Matching& m, ExprPtr value) {
      if (m.expected != nullptr) {
        return Expecting(*m.expected, move(value));
      }

      if (!m.type) {
        m.type = value->type;
      } else if (value->kind != ExprKind::Unsupported && TypeText(value->type) != TypeText(m.type)) {
        m.problems.push_back(Unsupported("every arm of a `match` value has one type: " + TypeText(m.type) + " and " + TypeText(value->type), value->at));
      }

      return value;
    }

    // An arm's value, with what checking it hoisted kept in the arm (`body`), not before the whole match.
    ExprPtr armValue(Checker& check, Matching& m, ExprId id, StmtList& body) {
      StmtList outer = move(check.hoisted);
      check.hoisted.clear();
      ExprPtr value = m.expected != nullptr ? CheckExprExpecting(check, id, *m.expected) : CheckExpr(check, id);
      body = move(check.hoisted);
      check.hoisted = move(outer);

      return m.valueName.empty() ? value : typedValue(m, move(value));
    }

    // `matchValue_L_C = value`, or the value as a statement when the match gives none.
    StmtPtr valueStmt(const Matching& m, ExprPtr value) {
      StmtPtr made = NewStmt(m.valueName.empty() ? StmtKind::ExprStmt : StmtKind::Assign, value->at);

      if (!m.valueName.empty()) {
        made->target = NamedRef(ExprKind::LoadSlot, m.valueName, m.expected != nullptr ? *m.expected : m.type, value->at);
      }

      made->value = move(value);

      return made;
    }

    StmtList armBody(Checker& check, Matching& m, const AstArm& arm) {
      if (Known(arm.body.pos)) {
        return CheckBlock(check, arm.body);
      }

      StmtList body;
      ExprPtr value = armValue(check, m, arm.value, body);
      body.push_back(valueStmt(m, move(value)));

      return body;
    }

    // ── the arms ──

    // `n` written alone where a name of that name is already in scope: it would bind the subject anew, not test the name.
    void shadowProblem(Checker& check, Matching& m, const AstArm& arm) {
      const AstPattern& pattern = patternNode(check, arm.pattern);

      if (pattern.kind == AstPatternKind::Name && Lookup(check, pattern.name.name) != nullptr) {
        const String name = Str(check, pattern.name.name);
        fail(m, "`" + name + " =>` binds the subject anew, it does not test `" + name + "`: write a condition (`" + name + " == …`, `_ if " + name + "`) or another name",
             pattern.pos);
      }
    }

    // A `Select` takes exact keys (no float), or a sum case whose payload matches anything, and never a guard.
    bool switchablePattern(const Checker& check, PatternId id) {
      const AstPattern& pattern = patternNode(check, id);

      if (pattern.kind == AstPatternKind::Or) {
        return all_of(check.program->patternRefs[pattern.items].begin(), check.program->patternRefs[pattern.items].end(),
                      [&](PatternId each) { return switchablePattern(check, each); });
      }

      const bool exact = pattern.kind == AstPatternKind::Value && Node(check, pattern.value).kind != AstExprKind::Float;

      return exact || wholeCase(check, pattern) >= 0;
    }

    // `-1` as the constant it is, so a key is always a constant.
    ExprPtr folded(const ExprPtr& key) {
      const bool negated = key->kind == ExprKind::Unary && key->unaryOp == UnOp::Neg && key->operands[0]->kind == ExprKind::ConstInt;

      return negated ? ConstInt(-key->operands[0]->intValue, key->type, key->at) : key;
    }

    // The keys a switchable pattern stands for: its literals, or its sum case's number.
    void keysOf(Walk& walk, PatternId id, Vector<ExprPtr>& keys) {
      const AstPattern& pattern = patternNode(walk.check, id);

      if (pattern.kind == AstPatternKind::Or) {
        for (PatternId each : walk.check.program->patternRefs[pattern.items]) {
          keysOf(walk, each, keys);
        }

        return;
      }

      keys.push_back(pattern.kind == AstPatternKind::Value ? folded(CheckExprExpecting(walk.check, pattern.value, walk.match.subject->type))
                                                           : whole(wholeCase(walk.check, pattern), SpanAt(pattern.pos)));
    }

    // The name a sum case binds its payload to (`n` in `Some(n)`), "" for none.
    String payloadName(const Checker& check, const AstPattern& pattern) {
      const bool sum = pattern.kind != AstPatternKind::Value && pattern.kind != AstPatternKind::Or && Valid(pattern.first);

      return sum && patternNode(check, pattern.first).kind == AstPatternKind::Name ? Str(check, patternNode(check, pattern.first).name.name) : "";
    }

    void keyed(Walk& walk, const AstArm& written, Arm& arm) {
      arm.switchable = !Valid(written.guard) && switchablePattern(walk.check, written.pattern);

      if (arm.switchable) {
        keysOf(walk, written.pattern, arm.keys);
        arm.payload = payloadName(walk.check, patternNode(walk.check, written.pattern));
      }
    }

    // The arm of a pattern that fits. Its names are bound for the guard and the block. Its test is the pattern's and the
    // guard's, and its keys are kept when a `Select` takes it.
    Arm boundArm(Walk& walk, const AstArm& written, Tested& tested) {
      const size_t mark = Mark(walk.check.scope);
      bindAll(walk.check, tested.bound);

      if (Valid(written.guard)) {
        tested.tests.push_back(guardOf(walk.check, walk.match, written, tested.bound));
      }

      StmtList body = withLets(walk.check, tested.bound, armBody(walk.check, walk.match, written));
      Arm arm{.test = allOf(tested.tests), .body = move(body), .at = SpanAt(written.pos)};
      Reset(walk.check.scope, mark);
      arm.always = !arm.test;
      keyed(walk, written, arm);

      return arm;
    }

    // A pattern that does not fit is only its problem: its guard and block are not checked (their names would mean nothing).
    Arm patternArm(Checker& check, Matching& m, const AstArm& written) {
      Walk walk{check, m};
      Tested tested;
      const size_t problems = m.problems.size();
      shadowProblem(check, m, written);
      testPattern(walk, written.pattern, m.subject, tested);

      if (m.problems.size() != problems) {
        return Arm{.test = m.problems.back(), .at = SpanAt(written.pos)};
      }

      return boundArm(walk, written, tested);
    }

    Arm checkArm(Checker& check, Matching& m, const AstArm& written) {
      if (Valid(written.pattern)) {
        return patternArm(check, m, written);
      }

      ExprPtr test = written.isElse ? nullptr : flag(m, CheckExpr(check, written.cond), "a condition arm");

      return Arm{.test = test, .body = armBody(check, m, written), .always = written.isElse, .at = SpanAt(written.pos)};
    }

    // An unguarded `[]` arm: a list that gets past it has an element.
    bool isEmptyList(const Checker& check, const AstArm& written) {
      if (!Valid(written.pattern) || Valid(written.guard)) {
        return false;
      }

      const AstPattern& pattern = patternNode(check, written.pattern);

      return pattern.kind == AstPatternKind::List && pattern.items.count == 0 && !pattern.hasRest;
    }

    Vector<Arm> checkArms(Checker& check, Matching& m, View<AstArm> written) {
      Vector<Arm> arms;

      for (const AstArm& each : written) {
        if (!arms.empty() && arms.back().always) {
          fail(m, "this arm is never reached: an `else` or a pattern that matches anything is before it", each.pos);
        }

        arms.push_back(checkArm(check, m, each));
        m.knownNonEmpty = m.knownNonEmpty || isEmptyList(check, each);
      }

      return arms;
    }

  }  // namespace

  namespace {

    // ── what the arms become ────────────────────────────────────────────────────────────────────────────────────────────────────────────────

    bool switchableType(const Type& type) {
      const bool sum = type.kind == TypeKind::Optional || type.kind == TypeKind::Either || type.kind == TypeKind::Variant;

      return IsInteger(type) || type.kind == TypeKind::String || sum;
    }

    bool allSwitchable(const Matching& m, const Vector<Arm>& arms) {
      if (!m.subject || !switchableType(*m.subject->type)) {
        return false;
      }

      return all_of(arms.begin(), arms.end(), [](const Arm& arm) { return arm.switchable || arm.always; });
    }

    // `switch (subject) { keys => { … } … else => { … } }`.
    StmtPtr selectOf(const Matching& m, const Vector<Arm>& arms, Span at) {
      StmtPtr made = NewStmt(StmtKind::Select, at);
      made->value = m.subject;

      for (const Arm& arm : arms) {
        if (arm.always) {
          made->elseBody = arm.body;
          break;
        }

        made->caseKeys.push_back(arm.keys);
        made->names.push_back(arm.payload);
        made->caseBodies.push_back(arm.body);
      }

      return made;
    }

    // `if (test) { … } else if …`: an arm that matches anything is the last `else`.
    StmtList chainFrom(const Vector<Arm>& arms, size_t first) {
      if (first == arms.size()) {
        return {};
      }

      if (arms[first].always) {
        return arms[first].body;
      }

      StmtPtr made = NewStmt(StmtKind::If, arms[first].at);
      made->value = arms[first].test;
      made->body = arms[first].body;
      made->elseBody = chainFrom(arms, first + 1);

      return StmtList{made};
    }

    // The arms with no `else` but covering every case: the last one needs no test, in an `if` chain.
    void lastCovers(const Checker& check, const Matching& m, View<AstArm> written, Vector<Arm>& arms) {
      const bool open = !arms.empty() && !arms.back().always && m.subject;

      if (open && covers(check, unguarded(check, written), *m.subject->type) && !allSwitchable(m, arms)) {
        arms.back().always = true;
      }
    }

    // The statement the arms become: a `Select`, or an `if` chain. A lone `else` keeps its block, inside an `if (true)`.
    StmtPtr assembled(const Matching& m, const Vector<Arm>& arms, Span at) {
      if (allSwitchable(m, arms)) {
        return selectOf(m, arms, at);
      }

      StmtList chain = chainFrom(arms, 0);

      if (chain.size() == 1 && chain[0]->kind == StmtKind::If) {
        return chain[0];
      }

      StmtPtr made = NewStmt(StmtKind::If, at);
      made->value = ConstBool(true, IntType(1), at);
      made->body = move(chain);

      return made;
    }

    // ── the subject, and the match as a whole ───────────────────────────────────────────────────────────────────────────────────────────────

    bool isPlainName(const Expr& value) {
      return value.kind == ExprKind::Local || value.kind == ExprKind::LoadSlot || value.kind == ExprKind::Global;
    }

    // `let match_L_C = value` before the statement holding the match; the subject is the hidden name.
    ExprPtr heldSubject(Checker& check, const ExprPtr& value, Pos pos) {
      const bool growable = IsGrowable(value->type);
      StmtPtr held = NewStmt(growable ? StmtKind::Var : StmtKind::Let, value->at);
      held->name = "match_" + spot(pos);
      held->type = value->type;
      held->value = value;

      check.hoisted.push_back(held);

      return NamedRef(growable ? ExprKind::LoadSlot : ExprKind::Local, held->name, value->type, value->at);
    }

    // The subject as a place: a name is read where it is; anything else is evaluated once, into a hidden `let`.
    ExprPtr subjectOf(Checker& check, Matching& m, ExprId id, Pos pos) {
      ExprPtr value = CheckExpr(check, id);

      if (value->kind == ExprKind::Unsupported) {
        m.problems.push_back(value);

        return nullptr;
      }

      return isPlainName(*value) ? value : heldSubject(check, value, pos);
    }

    StmtPtr problemStmt(const ExprPtr& problem) {
      StmtPtr made = NewStmt(StmtKind::ExprStmt, problem->at);
      made->value = problem;

      return made;
    }

    // A match with problems is only its problems, in order: the last in its place, the others before it. The arms' tests and
    // blocks go before them, as statements, so the problems inside them are reported (and the libraries they call are added).
    ExprPtr problemsOnly(Checker& check, const Matching& m, const Vector<Arm>& arms) {
      for (const Arm& arm : arms) {
        if (arm.test && arm.test->kind != ExprKind::Unsupported) {
          check.hoisted.push_back(problemStmt(arm.test));
        }

        check.hoisted.insert(check.hoisted.end(), arm.body.begin(), arm.body.end());
      }

      for (size_t i = 0; i + 1 < m.problems.size(); i++) {
        check.hoisted.push_back(problemStmt(m.problems[i]));
      }

      return m.problems.back();
    }

    StmtPtr matchStmt(Checker& check, const AstStmt& stmt) {
      Matching m;
      m.subject = Valid(stmt.value) ? subjectOf(check, m, stmt.value, stmt.pos) : nullptr;
      const View<AstArm> written = check.program->arms[stmt.arms];
      Vector<Arm> arms = m.problems.empty() ? checkArms(check, m, written) : Vector<Arm>{};
      lastCovers(check, m, written, arms);

      return m.problems.empty() ? assembled(m, arms, SpanAt(stmt.pos)) : problemStmt(problemsOnly(check, m, arms));
    }

    // A value `match` has a value whichever arm runs: it has an `else`, or its arms cover every case.
    void coverage(const Checker& check, Matching& m, View<AstArm> written, const Vector<Arm>& arms, Pos pos) {
      if (!m.problems.empty()) {
        return;
      }

      const bool always = any_of(arms.begin(), arms.end(), [](const Arm& arm) { return arm.always; });
      const bool covered = always || (m.subject && covers(check, unguarded(check, written), *m.subject->type));
      const TypePtr type = m.expected != nullptr ? *m.expected : m.type;

      if (arms.empty() || !covered) {
        fail(m, "a `match` value needs an `else` arm: its arms do not cover every case", pos);
      } else if (!type || IsVoid(*type)) {
        fail(m, "a `match` value needs a value in each arm", pos);
      }
    }

    // `var matchValue_L_C: T` and the statement that sets it go before the statement holding the match; the value is the variable.
    ExprPtr hoistedValue(Checker& check, const Matching& m, StmtPtr choice, Span at) {
      const TypePtr type = m.expected != nullptr ? *m.expected : m.type;
      StmtPtr slot = NewStmt(StmtKind::Var, at);
      slot->name = m.valueName;
      slot->type = type;

      check.hoisted.push_back(move(slot));
      check.hoisted.push_back(move(choice));

      return NamedRef(ExprKind::LoadSlot, m.valueName, type, at);
    }

    ExprPtr matchValue(Checker& check, const AstExpr& node, const TypePtr* expected) {
      Matching m{.valueName = "matchValue_" + spot(node.pos), .expected = expected};
      m.subject = Valid(node.first) ? subjectOf(check, m, node.first, node.pos) : nullptr;
      const View<AstArm> written = check.program->arms[node.arms];
      Vector<Arm> arms = m.problems.empty() ? checkArms(check, m, written) : Vector<Arm>{};
      coverage(check, m, written, arms, node.pos);

      if (!m.problems.empty()) {
        return problemsOnly(check, m, arms);
      }

      lastCovers(check, m, written, arms);

      return hoistedValue(check, m, assembled(m, arms, SpanAt(node.pos)), SpanAt(node.pos));
    }

    ExprPtr matchValueAsWritten(Checker& check, const AstExpr& node) {
      return matchValue(check, node, nullptr);
    }

  }  // namespace

  ExprPtr MatchValueExpecting(Checker& check, const AstExpr& node, const TypePtr& expected) {
    return matchValue(check, node, &expected);
  }

  void RegisterMatches(Registry& registry) {
    AddExpr(registry, AstExprKind::Match, matchValueAsWritten);
    AddStmt(registry, AstStmtKind::Match, matchStmt);
  }

}  // namespace polyir::front
