// `for (x in xs) { body }` over a list (`mutarray<T>`, `array<T>`, a buffer or a fixed array), written as the counting loop it means
// (CheckForInList.nina):
//
//   let forList_L_C = xs
//   var forIndex_L_C: i64 = 0
//   for (; forIndex_L_C < len(forList_L_C); ) {
//     let x: T = forList_L_C[forIndex_L_C]
//     body
//     forIndex_L_C = forIndex_L_C + 1
//   }
//
// Over a map's keys or a set's members, `for (k in m)` walks the table's slots, taking the key of each slot that holds an entry:
//
//   let forTable_L_C = m
//   var forSlot_L_C: i64 = 0
//   for (; forSlot_L_C < capacity(forTable_L_C); ) {
//     if (assocLive(forTable_L_C, forSlot_L_C)) {
//       let k: K = assocKey(forTable_L_C, forSlot_L_C)
//       body
//     }
//     forSlot_L_C = forSlot_L_C + 1
//   }
//
// A table's slots are each target's own, so the order of the keys can differ from one target to another; the keys are the same on all of them.
// The first two statements go before the loop (`check.hoisted`).
#include "front/Checker.hpp"
#include "polyir/HirPrint.hpp"

namespace polyir::front {

  namespace {

    // The names and types one loop works with.
    struct ForInList {
      String name;       // the loop variable, `x`
      TypePtr element;   // its type
      TypePtr listType;
      String list;       // the hidden local that holds the list, `forList_L_C`
      String index;      // the hidden counter, `forIndex_L_C`
      Span at;
    };

    // The element type of a list a `for (x in ...)` can walk, or nothing.
    Optional<TypePtr> listElement(const TypePtr& type) {
      const TypeKind kind = type->kind;

      if (kind == TypeKind::DynamicArray || kind == TypeKind::Buffer || kind == TypeKind::FixedArray) {
        return type->element;
      }

      return nullopt;
    }

    // The key type of a map or set a `for (k in ...)` can walk, or nothing.
    Optional<TypePtr> tableKey(const TypePtr& type) {
      if (type->kind == TypeKind::Hashmap || type->kind == TypeKind::Set) {
        return type->element;
      }

      return nullopt;
    }

    // The hidden names end in the loop variable's `line_column`, so two loops never share them.
    ForInList loopParts(Checker& check, const AstStmt& stmt, TypePtr element, TypePtr listType, const String& held, const String& counter) {
      const String where = to_string(stmt.name.pos.line) + "_" + to_string(stmt.name.pos.column);
      const String name = Str(check, stmt.name.name);

      return ForInList{name, move(element), move(listType), held + where, counter + where, SpanAt(stmt.name.pos)};
    }

    ForInList listParts(Checker& check, const AstStmt& stmt, TypePtr element, TypePtr listType) {
      return loopParts(check, stmt, move(element), move(listType), "forList_", "forIndex_");
    }

    ForInList tableParts(Checker& check, const AstStmt& stmt, TypePtr key, TypePtr tableType) {
      return loopParts(check, stmt, move(key), move(tableType), "forTable_", "forSlot_");
    }

    ExprPtr intConstant(int64_t value, const ForInList& loop) {
      return ConstInt(value, IntType(64), loop.at);
    }

    ExprPtr listOf(const ForInList& loop) {
      return NamedRef(ExprKind::Local, loop.list, loop.listType, loop.at);
    }

    ExprPtr indexOf(const ForInList& loop) {
      return NamedRef(ExprKind::LoadSlot, loop.index, IntType(64), loop.at);
    }

    ExprPtr elementRead(const ForInList& loop) {
      return IndexGetNode(listOf(loop), {indexOf(loop)}, loop.element, loop.at);
    }

    // `forIndex < len(forList)`
    ExprPtr inRange(const ForInList& loop) {
      return BinaryNode(BinOp::Lt, indexOf(loop), BuiltinNode(Builtin::Len, listOf(loop), {}, IntType(64), loop.at), IntType(1), loop.at);
    }

    // A `let` or `var` statement.
    StmtPtr named(StmtKind kind, String name, TypePtr type, ExprPtr value, Span at) {
      StmtPtr made = NewStmt(kind, at);
      made->name = move(name);
      made->type = move(type);
      made->value = move(value);

      return made;
    }

    // `forIndex = forIndex + 1`
    StmtPtr nextIndex(const ForInList& loop) {
      StmtPtr next = NewStmt(StmtKind::Assign, loop.at);
      next->target = indexOf(loop);
      next->value = BinaryNode(BinOp::Add, indexOf(loop), intConstant(1, loop), IntType(64), loop.at);

      return next;
    }

    // The loop body with the list, its index and the element in scope (and only there).
    StmtList listBody(Checker& check, const AstStmt& stmt, const ForInList& loop) {
      const size_t mark = Mark(check.scope);
      BindName(check.scope, SymbolOf(check, loop.list), BindingKind::Local, loop.listType);
      BindName(check.scope, SymbolOf(check, loop.index), BindingKind::Slot, IntType(64));
      BindName(check.scope, stmt.name.name, BindingKind::Local, loop.element);

      StmtList body = CheckBlock(check, stmt.body);
      Reset(check.scope, mark);

      return body;
    }

    // `let forList = xs` and `var forIndex: i64 = 0`, before the loop.
    void hoistSetup(Checker& check, const ForInList& loop, ExprPtr source) {
      check.hoisted.push_back(named(StmtKind::Let, loop.list, loop.listType, move(source), loop.at));
      check.hoisted.push_back(named(StmtKind::Var, loop.index, IntType(64), intConstant(0, loop), loop.at));
    }

    // `let x: T = list[index]`, the body, then `index = index + 1`.
    StmtList listLoopBody(Checker& check, const AstStmt& stmt, const ForInList& loop) {
      StmtList body{named(StmtKind::Let, loop.name, loop.element, elementRead(loop), loop.at)};

      for (StmtPtr& each : listBody(check, stmt, loop)) {
        body.push_back(move(each));
      }

      body.push_back(nextIndex(loop));

      return body;
    }

    // The `for` itself; its setup goes before it (hoisted).
    StmtPtr listLoop(Checker& check, const AstStmt& stmt, const ForInList& parts, ExprPtr source) {
      StmtList body = listLoopBody(check, stmt, parts);
      StmtPtr loop = NewStmt(StmtKind::For, parts.at);
      loop->value = inRange(parts);
      loop->body = move(body);
      hoistSetup(check, parts, move(source));

      return loop;
    }

    // ── a map's keys, a set's members ──

    // `capacity(forTable)`, `assocLive(forTable, forSlot)` or `assocKey(forTable, forSlot)`.
    ExprPtr tableRead(Builtin op, const ForInList& loop, TypePtr type, Vector<ExprPtr> slot) {
      return BuiltinNode(op, listOf(loop), move(slot), move(type), loop.at);
    }

    // `forSlot < capacity(forTable)`
    ExprPtr inTable(const ForInList& loop) {
      return BinaryNode(BinOp::Lt, indexOf(loop), tableRead(Builtin::Capacity, loop, IntType(64), {}), IntType(1), loop.at);
    }

    // `if (assocLive(forTable, forSlot)) { let k: K = assocKey(forTable, forSlot); body }`, then `forSlot = forSlot + 1`.
    StmtList tableLoopBody(Checker& check, const AstStmt& stmt, const ForInList& loop) {
      StmtPtr live = NewStmt(StmtKind::If, loop.at);
      live->value = tableRead(Builtin::AssocLive, loop, IntType(1), {indexOf(loop)});
      live->body.push_back(named(StmtKind::Let, loop.name, loop.element, tableRead(Builtin::AssocKey, loop, loop.element, {indexOf(loop)}), loop.at));

      for (StmtPtr& each : listBody(check, stmt, loop)) {
        live->body.push_back(move(each));
      }

      return StmtList{live, nextIndex(loop)};
    }

    StmtPtr tableLoop(Checker& check, const AstStmt& stmt, const ForInList& parts, ExprPtr source) {
      StmtList body = tableLoopBody(check, stmt, parts);
      StmtPtr loop = NewStmt(StmtKind::For, parts.at);
      loop->value = inTable(parts);
      loop->body = move(body);
      hoistSetup(check, parts, move(source));

      return loop;
    }

  }  // namespace

  // The loop over a list, a map's keys or a set's members, or the problem when `source` is none of those nor a coroutine.
  StmtPtr CheckForInList(Checker& check, const AstStmt& stmt, ExprPtr source) {
    if (const Optional<TypePtr> element = listElement(source->type)) {
      return listLoop(check, stmt, listParts(check, stmt, *element, source->type), move(source));
    }

    if (const Optional<TypePtr> key = tableKey(source->type)) {
      return tableLoop(check, stmt, tableParts(check, stmt, *key, source->type), move(source));
    }

    return UnsupportedStmt("`for (" + Str(check, stmt.name.name) + " in …)` loops over a list, a map, a set or a coroutine (a mutarray<T>, array<T>, " +
      "buffer, map<K, V>, set<T> or coro<T>); this is a " + TypeText(source->type), SpanAt(stmt.name.pos));
  }

}  // namespace polyir::front
