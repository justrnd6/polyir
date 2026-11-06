// The .poly forms found again in the HIR (Lift.hpp): what WritePoly and the translate engine both look for. The front end's own
// shapes are named after the file that makes them (CheckForInList.cpp, CheckIfValue.cpp); nina-lite's hidden names start `__`.
#include "hir/Lift.hpp"

namespace polyir {

  namespace {

    bool assignsInStmt(const Stmt& s, const String& name);

    // Whether a closure inside `e` assigns `name`.
    bool assignsInExpr(const ExprPtr& e, const String& name) {
      if (!e) {
        return false;
      }

      if (e->kind == ExprKind::ClosureLit && AssignsIn(e->closureBody, name)) {
        return true;
      }

      return any_of(e->operands.begin(), e->operands.end(), [&](const ExprPtr& operand) { return assignsInExpr(operand, name); });
    }

    bool assignsInBlocks(const Stmt& s, const String& name) {
      for (const StmtList* block : BlocksOf(s)) {
        if (AssignsIn(*block, name)) {
          return true;
        }
      }

      return false;
    }

    bool assignsInStmt(const Stmt& s, const String& name) {
      const bool assigned = s.kind == StmtKind::Assign && Reads(*s.target, name);

      return assigned || assignsInBlocks(s, name) || assignsInExpr(s.value, name) || assignsInExpr(s.target, name);
    }

    size_t countInAll(const Vector<ExprPtr>& exprs, const ExprTest& test) {
      size_t count = 0;

      for (const ExprPtr& e : exprs) {
        count += CountInExpr(e, test);
      }

      return count;
    }

    size_t countInStmt(const Stmt& s, const ExprTest& test) {
      const size_t own = CountInExpr(s.value, test) + CountInExpr(s.target, test) + countInAll(s.arguments, test);
      size_t parts = countInAll(s.initializers, test) + countInAll(s.steps, test);

      for (const StmtList* block : BlocksOf(s)) {
        parts += CountIn(*block, test);
      }

      return own + parts;
    }

    // ── walks over a table's slots ──

    // The name of the local holding the table `e` (`op(table, ...)`) reads, or empty.
    String tableOf(const Expr& e, Builtin op) {
      const bool isCall = e.kind == ExprKind::CallBuiltin && e.builtin == op && !e.operands.empty();
      const bool held = isCall && (e.operands[0]->kind == ExprKind::Local || e.operands[0]->kind == ExprKind::LoadSlot);

      return held ? e.operands[0]->name : "";
    }

    // `slot < capacity(table)`: the table's name, or empty.
    String walkedTable(const ExprPtr& condition, const String& slot) {
      const bool isTest = condition && condition->kind == ExprKind::Binary && condition->binaryOp == BinOp::Lt;

      return isTest && Reads(*condition->operands[0], slot) ? tableOf(*condition->operands[1], Builtin::Capacity) : "";
    }

    // `slot + 1`.
    bool isNextSlot(const ExprPtr& e, const String& slot) {
      const bool isAdd = e && e->kind == ExprKind::Binary && e->binaryOp == BinOp::Add;

      return isAdd && Reads(*e->operands[0], slot) && IsConstantInt(e->operands[1], 1);
    }

    // `if (assocLive(table, slot)) { ... }` with no else: the statements inside it, or null.
    const StmtList* liveBody(const Stmt& s, const String& table, const String& slot) {
      const bool isTest = s.kind == StmtKind::If && s.elseBody.empty() && tableOf(*s.value, Builtin::AssocLive) == table;

      return isTest && s.value->operands.size() == 2 && Reads(*s.value->operands[1], slot) ? &s.body : nullptr;
    }

    // The front end's walk (CheckForInList.cpp): `var slot: i64 = 0`, then `for (; slot < capacity(table); ) { if (...) { ... }; slot = slot + 1 }`.
    Optional<WalkShape> steppedWalk(const StmtList& body, size_t i) {
      const bool isCounter = i + 1 < body.size() && body[i]->kind == StmtKind::Var && IsConstantInt(body[i]->value, 0);
      const Stmt* loop = isCounter ? body[i + 1].get() : nullptr;

      if (loop == nullptr || loop->kind != StmtKind::For || !loop->names.empty() || loop->body.size() != 2) {
        return nullopt;
      }

      const String& slot = body[i]->name;
      const String table = walkedTable(loop->value, slot);
      const Stmt& step = *loop->body[1];
      const bool steps = step.kind == StmtKind::Assign && Reads(*step.target, slot) && isNextSlot(step.value, slot);
      const StmtList* inside = table.empty() || !steps ? nullptr : liveBody(*loop->body[0], table, slot);

      return inside == nullptr ? Optional<WalkShape>() : WalkShape{2, nullptr, table, slot, inside};
    }

    // The table the front end holds in a hidden `let forTable_L_C = m` just before its walk: written as the `m` it holds.
    Optional<WalkShape> heldWalk(const StmtList& body, size_t i) {
      const bool isHeld = body[i]->kind == StmtKind::Let && body[i]->name.rfind("forTable_", 0) == 0;
      Optional<WalkShape> walk = isHeld ? steppedWalk(body, i + 1) : nullopt;

      if (!walk || walk->table != body[i]->name) {
        return nullopt;
      }

      walk->count = 3;
      walk->source = body[i]->value;

      return walk;
    }

    // A hidden `let` holding a table that the counted walk after it walks, read only through the walk's slots.
    Optional<WalkShape> heldTableWalk(const Stmt& held, const Stmt& loop) {
      Optional<WalkShape> walk = CountedWalk(loop);
      const bool walks = walk && walk->table == held.name;

      if (!walks || CountIn(*walk->inside, ReadsName(held.name)) != CountIn(*walk->inside, ReadsSlotOf(held.name, walk->slot))) {
        return nullopt;
      }

      walk->source = held.value;

      return walk;
    }

    // `let x = list[index]`: the element a loop over a list's indices takes.
    bool takesElement(const Stmt& first, const String& list, const String& index) {
      const ExprPtr& value = first.value;
      const bool isRead = first.kind == StmtKind::Let && value && value->kind == ExprKind::IndexGet && value->operands.size() == 2;

      return isRead && Reads(*value->operands[0], list) && Reads(*value->operands[1], index);
    }

    // ── `if` values ──

    bool assigns(const Stmt& s, const String& target) {
      return s.kind == StmtKind::Assign && Reads(*s.target, target);
    }

    // `if (c) a else b` replacing a plain fallback `var n = b`: an `if` with no else that only gives n a, its test and value not reading n.
    Optional<IfValue> fallbackOf(const Stmt& slot, const Stmt* filled) {
      const bool plain = slot.value && (IsLiteral(*slot.value) || slot.value->kind == ExprKind::Local);
      const bool replaced = filled && plain && filled->kind == StmtKind::If && filled->elseBody.empty() && filled->body.size() == 1 && assigns(*filled->body[0], slot.name);
      const bool fallsBack = replaced && CountInExpr(filled->value, ReadsName(slot.name)) + CountInExpr(filled->body[0]->value, ReadsName(slot.name)) == 0;

      return fallsBack ? Optional<IfValue>(IfValue{{{filled->value, filled->body[0]->value}}, slot.value}) : nullopt;
    }

    // An `if` value whose first test is `cond` giving `value`, and the rest `rest`.
    IfValue withArm(const ExprPtr& cond, const ExprPtr& value, IfValue rest) {
      rest.arms.insert(rest.arms.begin(), {cond, value});

      return rest;
    }

    // ── which generics are written once ──

    // Whether `type` names a generic struct as declared (`Box_T`) that is written as its copies.
    bool mentionsCopied(const Module& module, const WrittenGenerics& written, const TypePtr& type, int depth) {
      if (!type || depth > 8) {
        return false;
      }

      const StructDef* def = type->kind == TypeKind::Struct ? AnyStruct(module, type->name) : nullptr;

      if (def != nullptr && !def->generic.params.empty() && written.structs.count(def->generic.of) == 0) {
        return true;
      }

      const bool inParts = mentionsCopied(module, written, type->element, depth + 1) || mentionsCopied(module, written, type->second, depth + 1);

      return inParts || any_of(type->parameters.begin(), type->parameters.end(), [&](const TypePtr& each) { return mentionsCopied(module, written, each, depth + 1); });
    }

    bool functionUsesCopied(const Module& module, const WrittenGenerics& written, const FuncDef& def) {
      if (mentionsCopied(module, written, def.result, 0)) {
        return true;
      }

      for (size_t i = 1; i < def.parameters.size(); i++) {
        if (mentionsCopied(module, written, def.parameters[i].type, 0)) {
          return true;
        }
      }

      return CountIn(def.body, [&](const Expr& e) { return mentionsCopied(module, written, e.type, 0); }) > 0;
    }

    bool structUsesCopied(const Module& module, const WrittenGenerics& written, const StructDef& def) {
      for (const Field& field : def.fields) {
        if (mentionsCopied(module, written, field.type, 0)) {
          return true;
        }
      }

      const Vector<const FuncDef*> methods = GenericMethodsOf(module, def);

      return any_of(methods.begin(), methods.end(), [&](const FuncDef* method) { return functionUsesCopied(module, written, *method); });
    }

    // Takes back one generic that uses a generic written as its copies; false when there is none.
    bool dropOneUsingCopied(const Module& module, WrittenGenerics& written) {
      for (auto each = written.structs.begin(); each != written.structs.end(); ++each) {
        if (structUsesCopied(module, written, *each->second)) {
          written.structs.erase(each);

          return true;
        }
      }

      for (auto each = written.functions.begin(); each != written.functions.end(); ++each) {
        if (functionUsesCopied(module, written, *each->second)) {
          written.functions.erase(each);

          return true;
        }
      }

      return false;
    }

    // ── declarations ──

    // Whether `implementer` is struct `def`, or for a generic as declared one of its copies.
    bool implementedBy(const Module& module, const StructDef& def, const String& implementer) {
      const StructDef* copy = def.generic.params.empty() ? nullptr : FindStruct(module, implementer);

      return implementer == def.name || (copy != nullptr && copy->generic.of == def.generic.of);
    }

    // What `@Builder` made for struct `def` (its `builder()` and `Builder`): reading `@Builder` makes them again.
    bool madeByBuilder(const StructDef& def, const String& name) {
      return HasAnnotation(def.annotations, "Builder") && (name == def.name + ".builder" || name == def.name + ".Builder");
    }

    // The declarations by kind (structs, interfaces, then functions), for a module that does not say their order.
    Vector<DeclRef> byKind(const Module& module) {
      Vector<DeclRef> refs;

      for (size_t i = 0; i < module.structs.size(); i++) {
        refs.push_back(DeclRef{DeclKind::Struct, i});
      }

      for (size_t i = 0; i < module.interfaces.size(); i++) {
        refs.push_back(DeclRef{DeclKind::Interface, i});
      }

      for (size_t i = 0; i < module.functions.size(); i++) {
        refs.push_back(DeclRef{DeclKind::Function, i});
      }

      return refs;
    }

    // A struct written on its own: not an interface value's struct, not one declared in another struct.
    Optional<String> placeOfStruct(const Module& module, const StructDef& def) {
      return FindInterface(module, def.name) == nullptr && def.outer.empty() ? Optional<String>(def.module) : nullopt;
    }

    // A function written on its own: not a method, not a static function (both are in their struct's body).
    Optional<String> placeOfFunction(const Module& module, const FuncDef& def) {
      return OwnerOf(module, def) == nullptr && def.outer.empty() ? Optional<String>(def.module) : nullopt;
    }

  }  // namespace

  // ── reading the HIR ──

  bool Reads(const Expr& e, const String& name) {
    return (e.kind == ExprKind::Local || e.kind == ExprKind::LoadSlot) && e.name == name;
  }

  bool IsConstantInt(const ExprPtr& e, int64_t value) {
    return e && e->kind == ExprKind::ConstInt && e->intValue == value;
  }

  bool IsLiteral(const Expr& e) {
    return e.kind == ExprKind::ConstInt || e.kind == ExprKind::ConstFloat || e.kind == ExprKind::ConstBool || e.kind == ExprKind::ConstString;
  }

  bool IsCollection(const Type& t) {
    return t.kind == TypeKind::DynamicArray || t.kind == TypeKind::Hashmap || t.kind == TypeKind::Set;
  }

  size_t CountInExpr(const ExprPtr& e, const ExprTest& test) {
    if (!e) {
      return 0;
    }

    size_t count = test(*e) ? 1 : 0;

    for (const ExprPtr& operand : e->operands) {
      count += CountInExpr(operand, test);
    }

    return e->kind == ExprKind::ClosureLit ? count + CountIn(e->closureBody, test) : count;
  }

  size_t CountIn(const StmtList& body, const ExprTest& test) {
    size_t count = 0;

    for (const StmtPtr& s : body) {
      count += countInStmt(*s, test);
    }

    return count;
  }

  ExprTest ReadsName(const String& name) {
    return [name](const Expr& e) { return Reads(e, name); };
  }

  bool AssignsIn(const StmtList& body, const String& name) {
    return any_of(body.begin(), body.end(), [&](const StmtPtr& s) { return assignsInStmt(*s, name); });
  }

  bool IsHiddenName(const String& name) {
    return name.rfind("__", 0) == 0;
  }

  // The front end's is `ifValue_L_C`, nina-lite's `__value3`.
  bool IsIfValueName(const String& name) {
    return name.rfind("ifValue_", 0) == 0 || name.rfind("__value", 0) == 0;
  }

  bool IsMatchValueName(const String& name) {
    return name.rfind("matchValue_", 0) == 0;
  }

  bool IsUnsetVar(const Stmt& s) {
    return s.kind == StmtKind::Var && !s.value;
  }

  // ── declarations ──

  const StructDef* AnyStruct(const Module& module, const String& name) {
    if (const StructDef* def = FindStruct(module, name)) {
      return def;
    }

    for (const StructDef& def : module.genericStructs) {
      if (def.name == name) {
        return &def;
      }
    }

    return nullptr;
  }

  // A method's first parameter is its struct's `this`, and its name starts `Struct_`.
  const StructDef* OwnerOf(const Module& module, const FuncDef& function) {
    if (function.parameters.empty() || function.parameters[0].name != "this") {
      return nullptr;
    }

    const TypePtr& type = function.parameters[0].type;
    const bool isStruct = type && type->kind == TypeKind::Struct;

    return isStruct && function.name.rfind(type->name + "_", 0) == 0 ? AnyStruct(module, type->name) : nullptr;
  }

  String MethodName(const FuncDef& function, const StructDef& owner) {
    return function.name.substr(owner.name.size() + 1);
  }

  Vector<const FuncDef*> GenericMethodsOf(const Module& module, const StructDef& def) {
    Vector<const FuncDef*> methods;

    for (const FuncDef& function : module.genericFunctions) {
      if (OwnerOf(module, function) == &def) {
        methods.push_back(&function);
      }
    }

    return methods;
  }

  bool WorksForEveryType(const Generic& generic) {
    return !generic.params.empty() && generic.notParametric.empty();
  }

  bool HasOrder(const Module& module) {
    return !module.order.empty() && module.order.size() == DeclarationCount(module);
  }

  Vector<DeclRef> DeclarationOrder(const Module& module) {
    return HasOrder(module) ? module.order : byKind(module);
  }

  // The functions in the module's order when it has one, else as they are listed.
  Vector<const FuncDef*> FunctionsInOrder(const Module& module) {
    Vector<const FuncDef*> functions;

    for (const DeclRef& ref : DeclarationOrder(module)) {
      if (ref.kind == DeclKind::Function) {
        functions.push_back(&module.functions[ref.index]);
      }
    }

    return functions;
  }

  // The methods of `def`, in the order of the functions; a generic as declared has its own.
  Vector<const FuncDef*> MethodsOf(const Module& module, const StructDef& def) {
    if (!def.generic.params.empty()) {
      return GenericMethodsOf(module, def);
    }

    Vector<const FuncDef*> methods;

    for (const FuncDef* function : FunctionsInOrder(module)) {
      if (OwnerOf(module, *function) == &def) {
        methods.push_back(function);
      }
    }

    return methods;
  }

  bool HasAnnotation(const Vector<Annotation>& annotations, const String& name) {
    return any_of(annotations.begin(), annotations.end(), [&](const Annotation& a) { return a.name == name; });
  }

  Vector<const FuncDef*> StaticsOf(const Module& module, const StructDef& def, bool withBuilder) {
    Vector<const FuncDef*> statics;

    for (const FuncDef* function : FunctionsInOrder(module)) {
      if (function->outer == def.name && (withBuilder || !madeByBuilder(def, function->name))) {
        statics.push_back(function);
      }
    }

    return statics;
  }

  Vector<const StructDef*> NestedIn(const Module& module, const StructDef& def, bool withBuilder) {
    Vector<const StructDef*> nested;

    for (const StructDef& each : module.structs) {
      if (each.outer == def.name && (withBuilder || !madeByBuilder(def, each.name))) {
        nested.push_back(&each);
      }
    }

    return nested;
  }

  bool Implements(const Module& module, const StructDef& def, const InterfaceDef& interface) {
    return any_of(interface.implementers.begin(), interface.implementers.end(), [&](const String& implementer) {
      return implementedBy(module, def, implementer);
    });
  }

  // A method, a static function and a nested struct sit in their struct's body; an interface value's struct and an extern nowhere.
  Optional<String> PlaceOf(const Module& module, const DeclRef& ref) {
    switch (ref.kind) {
      case DeclKind::Struct:
        return placeOfStruct(module, module.structs[ref.index]);
      case DeclKind::Interface:
        return module.interfaces[ref.index].module;
      case DeclKind::Function:
        return placeOfFunction(module, module.functions[ref.index]);
      default:
        return nullopt;
    }
  }

  // The generics `writtenOnce` takes (and functions that work for every type), less those using one written as its copies.
  WrittenGenerics FindWrittenGenerics(const Module& module, const Function<bool(const StructDef&)>& writtenOnce) {
    WrittenGenerics written;

    for (const StructDef& def : module.genericStructs) {
      if (writtenOnce(def)) {
        written.structs[def.generic.of] = &def;
      }
    }

    for (const FuncDef& def : module.genericFunctions) {
      if (WorksForEveryType(def.generic) && OwnerOf(module, def) == nullptr) {
        written.functions[def.generic.of] = &def;
      }
    }

    while (dropOneUsingCopied(module, written)) {
    }

    return written;
  }

  // ── statement shapes ──

  // The builder's counted walk: `for (slot = 0; slot < capacity(table); slot = slot + 1) { if (assocLive(table, slot)) { ... } }`.
  Optional<WalkShape> CountedWalk(const Stmt& s) {
    if (s.kind != StmtKind::For || s.names.size() != 1 || !IsConstantInt(s.initializers[0], 0) || !isNextSlot(s.steps[0], s.names[0])) {
      return nullopt;
    }

    const String table = walkedTable(s.value, s.names[0]);
    const StmtList* inside = table.empty() || s.body.size() != 1 ? nullptr : liveBody(*s.body[0], table, s.names[0]);

    if (inside == nullptr) {
      return nullopt;
    }

    return WalkShape{1, nullptr, table, s.names[0], inside};
  }

  Optional<WalkShape> WalkAt(const StmtList& body, size_t i) {
    if (Optional<WalkShape> held = heldWalk(body, i)) {
      return held;
    }

    if (Optional<WalkShape> stepped = steppedWalk(body, i)) {
      return stepped;
    }

    return CountedWalk(*body[i]);
  }

  String KeyName(const WalkShape& walk) {
    const StmtList& inside = *walk.inside;
    const bool named = !inside.empty() && inside[0]->kind == StmtKind::Let && tableOf(*inside[0]->value, Builtin::AssocKey) == walk.table;

    return named ? inside[0]->name : walk.slot + "Key";
  }

  // Inside a walk, `assocKey(table, slot)` or `assocValue(table, slot)`.
  ExprTest ReadsSlotOf(const String& table, const String& slot) {
    return [table, slot](const Expr& e) {
      const bool isRead = e.kind == ExprKind::CallBuiltin && (e.builtin == Builtin::AssocKey || e.builtin == Builtin::AssocValue);

      return isRead && e.operands.size() == 2 && Reads(*e.operands[0], table) && Reads(*e.operands[1], slot);
    };
  }

  // CheckForInList.cpp: `let forList = xs`, `var forIndex: i64 = 0`, then `for (; forIndex < len(forList); ) { let x = forList[forIndex]; ...;
  // forIndex = forIndex + 1 }`.
  const Stmt* ListLoopAt(const StmtList& body, size_t i) {
    const bool isHeld = i + 2 < body.size() && body[i]->kind == StmtKind::Let && body[i]->name.rfind("forList_", 0) == 0;
    const bool isCounter = isHeld && body[i + 1]->kind == StmtKind::Var && body[i + 1]->name.rfind("forIndex_", 0) == 0;
    const Stmt* loop = isCounter ? body[i + 2].get() : nullptr;
    const bool shaped = loop != nullptr && loop->kind == StmtKind::For && loop->names.empty() && loop->body.size() >= 2;

    return shaped && loop->body[0]->kind == StmtKind::Let ? loop : nullptr;
  }

  // `for (i = 0; i < len(xs); i = i + 1) { let x = xs[i]; ... }` where nothing after the element's `let` reads `i`.
  String CountedList(const Stmt& s) {
    const bool counted = s.kind == StmtKind::For && s.names.size() == 1 && IsConstantInt(s.initializers[0], 0) && isNextSlot(s.steps[0], s.names[0]);
    const bool isTest = counted && s.value->kind == ExprKind::Binary && s.value->binaryOp == BinOp::Lt && Reads(*s.value->operands[0], s.names[0]);
    const String list = isTest ? tableOf(*s.value->operands[1], Builtin::Len) : "";

    if (list.empty() || s.body.empty() || !takesElement(*s.body[0], list, s.names[0])) {
      return "";
    }

    return CountIn(StmtList(s.body.begin() + 1, s.body.end()), ReadsName(s.names[0])) == 0 ? list : "";
  }

  Optional<HeldLoop> HeldLoopAt(const StmtList& body, size_t i) {
    const bool isHeld = i + 1 < body.size() && body[i]->kind == StmtKind::Let && IsHiddenName(body[i]->name);
    const String held = isHeld ? body[i]->name : "";

    if (!isHeld || CountIn(StmtList(body.begin() + static_cast<long>(i) + 2, body.end()), ReadsName(held)) != 0) {
      return nullopt;
    }

    if (CountedList(*body[i + 1]) == held && CountIn(body[i + 1]->body, ReadsName(held)) == 1) {
      return HeldLoop{body[i].get(), body[i + 1].get(), nullopt};
    }

    const Optional<WalkShape> walk = heldTableWalk(*body[i], *body[i + 1]);

    return walk ? Optional<HeldLoop>(HeldLoop{body[i].get(), body[i + 1].get(), walk}) : nullopt;
  }

  // The `else` gives `target` its value by one assignment, or by an `if` value of its own (an `else if` chain).
  Optional<IfValue> IfValueOf(const Stmt& s, const String& target) {
    const bool shaped = s.kind == StmtKind::If && s.body.size() == 1 && assigns(*s.body[0], target);
    const StmtList* otherwise = shaped && s.elseBody.size() == 1 ? &s.elseBody : nullptr;

    if (otherwise != nullptr && assigns(*(*otherwise)[0], target)) {
      return IfValue{{{s.value, s.body[0]->value}}, (*otherwise)[0]->value};
    }

    const Optional<IfValue> rest = otherwise != nullptr ? IfValueOf(*(*otherwise)[0], target) : nullopt;

    return rest ? Optional<IfValue>(withArm(s.value, s.body[0]->value, *rest)) : nullopt;
  }

  // The `then` branch is one `return`; the `else` one `return` or a nested `if` that only returns.
  Optional<IfValue> ReturnIfOf(const Stmt& s) {
    const bool shaped = s.kind == StmtKind::If && s.body.size() == 1 && s.body[0]->kind == StmtKind::Return && s.body[0]->value;
    const StmtList* otherwise = shaped && s.elseBody.size() == 1 ? &s.elseBody : nullptr;

    if (otherwise != nullptr && (*otherwise)[0]->kind == StmtKind::Return && (*otherwise)[0]->value) {
      return IfValue{{{s.value, s.body[0]->value}}, (*otherwise)[0]->value};
    }

    const Optional<IfValue> rest = otherwise != nullptr ? ReturnIfOf(*(*otherwise)[0]) : nullopt;

    return rest ? Optional<IfValue>(withArm(s.value, s.body[0]->value, *rest)) : nullopt;
  }

  // A hidden `var` and the `if` that fills it, read once by the statement after them (past more such pairs). The `if` stands where
  // the variable is read.
  Optional<IfValue> InlinedAt(const StmtList& body, size_t i) {
    const bool hidden = i + 2 < body.size() && IsUnsetVar(*body[i]) && IsIfValueName(body[i]->name);
    size_t use = i + 2;

    while (hidden && use + 2 < body.size() && IsUnsetVar(*body[use]) && IsIfValueName(body[use]->name)) {
      use += 2;
    }

    const bool readOnce = hidden && CountIn(StmtList{body[use]}, ReadsName(body[i]->name)) == 1;
    const bool readThere = readOnce && CountIn(StmtList(body.begin() + static_cast<long>(use) + 1, body.end()), ReadsName(body[i]->name)) == 0;

    return readThere ? IfValueOf(*body[i + 1], body[i]->name) : nullopt;
  }

  // From a `var` filled branch by branch, or one starting at a plain fallback that an `if` with no else replaces.
  Optional<DeclaredIf> DeclaredAt(const StmtList& body, size_t i) {
    const Stmt& slot = *body[i];
    const Stmt* filled = i + 1 < body.size() ? body[i + 1].get() : nullptr;
    Optional<IfValue> value = filled && IsUnsetVar(slot) ? IfValueOf(*filled, slot.name) : nullopt;

    if (!value) {
      value = fallbackOf(slot, filled);
    }

    if (slot.kind != StmtKind::Var || IsIfValueName(slot.name) || IsMatchValueName(slot.name) || !value) {
      return nullopt;
    }

    const bool changes = AssignsIn(StmtList(body.begin() + static_cast<long>(i) + 2, body.end()), slot.name);

    return DeclaredIf{&slot, *value, changes};
  }

  // ── expression shapes ──

  // Pushed one by one onto a new, empty list.
  Vector<ExprPtr> ListValues(const Expr& e) {
    Vector<ExprPtr> values;
    const Expr* list = &e;

    while (list->kind == ExprKind::CallBuiltin && list->builtin == Builtin::Push && list->operands.size() == 2) {
      values.insert(values.begin(), list->operands[1]);
      list = list->operands[0].get();
    }

    const bool isNew = list->kind == ExprKind::NewContainer && list->operands.empty();

    return isNew ? values : Vector<ExprPtr>{};
  }

  Vector<const Expr*> JoinedPieces(const Expr& e) {
    if (e.kind != ExprKind::CallBuiltin || e.builtin != Builtin::Concat) {
      return {&e};
    }

    Vector<const Expr*> pieces = JoinedPieces(*e.operands[0]);
    const Vector<const Expr*> right = JoinedPieces(*e.operands[1]);
    pieces.insert(pieces.end(), right.begin(), right.end());

    return pieces;
  }

  bool IsTextOf(const Expr& e) {
    return e.kind == ExprKind::CallBuiltin && e.builtin == Builtin::StrOf && e.operands.size() == 1;
  }

  // At least one `str(x)` hole, and never two literals side by side: those come from a text block, which stays a join.
  bool IsInterpolation(const Vector<const Expr*>& pieces) {
    bool holes = false;

    for (size_t i = 0; i < pieces.size(); i++) {
      const bool literal = pieces[i]->kind == ExprKind::ConstString;
      holes = holes || IsTextOf(*pieces[i]);

      if (literal && i > 0 && pieces[i - 1]->kind == ExprKind::ConstString) {
        return false;
      }
    }

    return holes;
  }

  bool IsOtherKind(const Expr& e) {
    const TypePtr& from = e.operands[0]->type;
    const bool collections = e.type && from && IsCollection(*e.type) && IsCollection(*from);

    return collections && e.type->isMutable != from->isMutable;
  }

  bool IsPlainCopy(const ExprPtr& e) {
    return e && e->kind == ExprKind::CallBuiltin && e->builtin == Builtin::Copy && !IsOtherKind(*e);
  }

}  // namespace polyir
