// What a native build cannot take (lang/src/ExternTypes.nina `nativeProblems`): a host-only declaration -- an `extern type` or `extern interface`
// with no `native` binding -- that the program uses: named in a signature or a struct field, claimed by a struct, or called. Declaring one and never
// using it is fine, so a library of host declarations can be imported by a program that builds natively.
#include "front/Pipeline.hpp"
#include "front/HirBuild.hpp"

namespace polyir::front {

  namespace {

    // A declaration bound to some host, but not to `native`.
    bool lacksNative(const Program& program, Range<AstHostBinding> binding) {
      if (Empty(binding)) {
        return false;
      }

      for (const AstHostBinding& each : program.hostBindings[binding]) {
        if (TextOf(program, each.target.name) == "native") {
          return false;
        }
      }

      return true;
    }

    void noteIfHostOnly(const Program& program, Ident name, Range<AstHostBinding> binding, Set<Symbol>& names) {
      if (lacksNative(program, binding)) {
        names.insert(name.name);
      }
    }

    void noteHostOnly(const Program& program, const AstItem& item, Set<Symbol>& names) {
      if (item.kind == ItemKind::ExternType) {
        noteIfHostOnly(program, program.externTypes[item.index].name, program.externTypes[item.index].binding, names);
      }

      if (item.kind != ItemKind::ExternInterface) {
        return;
      }

      noteIfHostOnly(program, program.externInterfaces[item.index].name, program.externInterfaces[item.index].binding, names);
    }

    // A unit written in the program (not one made from a generic declaration).
    bool isProgramUnit(const Unit& unit) {
      return unit.instance == kNoIndex;
    }

    // The names of the extern types and interfaces with no native binding.
    Set<Symbol> hostOnlyTypes(const Session& session) {
      Set<Symbol> names;

      for (const Unit& unit : session.units) {
        if (!isProgramUnit(unit)) {
          continue;
        }

        for (uint32_t i = unit.first; i < unit.last; i++) {
          noteHostOnly(*unit.program, unit.program->items[i], names);
        }
      }

      return names;
    }

    // ── the type names a declaration mentions, in the order the type is written ─────────────────────────────────────────────────────────────────────

    void wordsOf(const Program& program, TypeId id, Vector<Ident>& out);

    void wordsOfAll(const Program& program, Range<TypeId> ids, Vector<Ident>& out) {
      for (const TypeId each : program.typeRefs[ids]) {
        wordsOf(program, each, out);
      }
    }

    void wordsOf(const Program& program, TypeId id, Vector<Ident>& out) {
      if (!Valid(id)) {
        return;
      }

      const AstType& type = program.types[id];

      if (type.kind == AstTypeKind::Named || type.kind == AstTypeKind::Generic) {
        out.push_back(type.name);
      }

      if (type.kind != AstTypeKind::Named) {
        wordsOfAll(program, type.types, out);
      }

      wordsOf(program, type.first, out);
      wordsOf(program, type.second, out);  // the value / err type of map, mutmap and either; invalid (so skipped) for every other kind
    }

    // ── where the uses are ──────────────────────────────────────────────────────────────────────────────────────────────────────────────────────

    // What the search for uses needs: the host-only names, and where the messages go.
    struct Uses {
      const Session& session;
      const Set<Symbol>& hostOnly;
      Vector<String>& out;
    };

    void report(const Uses& uses, Symbol what, Ident owner, Pos at, const Program& program);

    // Every host-only name the type mentions is a problem of `owner`, reported at `at`.
    void reportUsesInType(const Uses& uses, const Program& program, TypeId type, Ident at, Ident owner);

    void reportUsesInSignature(const Uses& uses, const Program& program, Ident name, Range<AstParam> params, TypeId returnType);

    void reportUsesInStruct(const Uses& uses, const Program& program, const AstStructDecl& decl);

    void reportUsesInItem(const Uses& uses, const Program& program, const AstItem& item);

    void report(const Uses& uses, Symbol what, Ident owner, Pos at, const Program& program) {
      const String message = String(TextOf(program, what)) + " is a host type with no native binding, so " + String(TextOf(program, owner.name)) +
                             " cannot be built natively (compile for the target it is bound to)";
      const String file = FileOf(uses.session, owner.name);
      const String located = DiagnosticMessage(message, SpanAt(at));
      uses.out.push_back(file.empty() ? located : file + ":" + located);
    }

    // Every host-only name the type mentions is a problem of `owner`, reported at `at`.
    void reportUsesInType(const Uses& uses, const Program& program, TypeId type, Ident at, Ident owner) {
      Vector<Ident> words;
      wordsOf(program, type, words);

      for (const Ident& word : words) {
        if (uses.hostOnly.count(word.name) != 0) {
          report(uses, word.name, owner, at.pos, program);
        }
      }
    }

    void reportUsesInSignature(const Uses& uses, const Program& program, Ident name, Range<AstParam> params, TypeId returnType) {
      reportUsesInType(uses, program, returnType, name, name);

      for (const AstParam& param : program.params[params]) {
        reportUsesInType(uses, program, param.type, param.name, name);
      }
    }

    void reportUsesInStruct(const Uses& uses, const Program& program, const AstStructDecl& decl) {
      for (const Ident& claim : program.idents[decl.impls]) {
        if (uses.hostOnly.count(claim.name) != 0) {
          report(uses, claim.name, decl.name, claim.pos, program);
        }
      }

      for (const AstField& field : program.fields[decl.primary]) {
        reportUsesInType(uses, program, field.type, field.name, decl.name);
      }

      for (const AstMember& member : program.members[decl.members]) {
        if (!member.isMethod) {
          reportUsesInType(uses, program, member.field.type, member.field.name, decl.name);
        }
      }
    }

    void reportUsesInFunction(const Uses& uses, const Program& program, const AstFuncDecl& decl) {
      reportUsesInSignature(uses, program, decl.name, decl.params, decl.returnType);
    }

    void reportUsesInExternFunction(const Uses& uses, const Program& program, const AstExternFunc& decl) {
      reportUsesInSignature(uses, program, decl.name, decl.params, decl.returnType);
    }

    // The signatures and struct fields of one item (the bodies are searched on the checked module, for calls).
    void reportUsesInItem(const Uses& uses, const Program& program, const AstItem& item) {
      if (item.kind == ItemKind::Struct) {
        reportUsesInStruct(uses, program, program.structs[item.index]);
      }

      if (item.kind == ItemKind::Func || item.kind == ItemKind::CoroutineFunc) {
        reportUsesInFunction(uses, program, program.funcs[FuncId{item.index}]);
      }

      if (item.kind == ItemKind::ExternFunc) {
        reportUsesInExternFunction(uses, program, program.externFuncs[item.index]);
      }
    }

    // ── the calls ───────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────

    // The names of the functions the checked module calls anywhere.
    struct Callees {
      Set<String> names;
    };

    void calleesInStmts(Callees& callees, const StmtList& list);

    void calleesInStmt(Callees& callees, const Stmt& stmt);

    void calleesInExprs(Callees& callees, const Vector<ExprPtr>& list);

    void calleesInExpr(Callees& callees, const ExprPtr& expr);

    Callees makeCallees(const Module& module) {
      Callees callees;

      for (const FuncDef& function : module.functions) {
        calleesInStmts(callees, function.body);
      }

      return callees;
    }

    void calleesInStmts(Callees& callees, const StmtList& list) {
      for (const StmtPtr& stmt : list) {
        calleesInStmt(callees, *stmt);
      }
    }

    // Every expression a statement holds.
    void calleesInStmtExprs(Callees& callees, const Stmt& stmt) {
      calleesInExpr(callees, stmt.value);
      calleesInExpr(callees, stmt.target);
      calleesInExprs(callees, stmt.initializers);
      calleesInExprs(callees, stmt.steps);

      calleesInExprs(callees, stmt.arguments);
      calleesInExprs(callees, stmt.elseArguments);

      for (const Vector<ExprPtr>& list : stmt.caseArguments) {
        calleesInExprs(callees, list);
      }
    }

    // Every expression a statement holds, then its blocks.
    void calleesInStmt(Callees& callees, const Stmt& stmt) {
      calleesInStmtExprs(callees, stmt);

      for (const StmtList* block : BlocksOf(stmt)) {
        calleesInStmts(callees, *block);
      }
    }

    void calleesInExprs(Callees& callees, const Vector<ExprPtr>& list) {
      for (const ExprPtr& expr : list) {
        calleesInExpr(callees, expr);
      }
    }

    void calleesInExpr(Callees& callees, const ExprPtr& expr) {
      if (!expr) {
        return;
      }

      if (expr->kind == ExprKind::Call) {
        callees.names.insert(expr->name);
      }

      calleesInExprs(callees, expr->operands);
      calleesInStmts(callees, expr->closureBody);
    }

    bool hasNativeBinding(const HostMember& member) {
      for (const String& binding : member.bindings) {
        if (binding.rfind("native=", 0) == 0) {
          return true;  // the binding text starts with `native=`
        }
      }

      return false;
    }

    // A call of a host member with no native binding.
    void reportHostOnlyMemberCalls(const Module& module, const Set<String>& called, Vector<String>& out) {
      for (const HostMember& member : module.hostMembers) {
        if (member.owner.empty() || called.count(member.function) == 0 || hasNativeBinding(member)) {
          continue;
        }

        out.push_back("a call to " + member.owner + "." + member.member + " cannot be built natively: it has no native binding");
      }
    }

    // An `extern func` bound to a host only that the program calls, at the name it was declared with.
    void reportIfHostOnlyFuncCalled(const Program& program, const AstExternFunc& decl, const Set<String>& called, Vector<String>& out) {
      const String name(TextOf(program, decl.name.name));

      if (!lacksNative(program, decl.binding) || called.count(name) == 0) {
        return;
      }

      const String why = name + " is bound to a host only (no native binding), so a native build cannot call it";
      out.push_back(DiagnosticMessage(why, SpanAt(decl.name.pos)));
    }

    void reportHostOnlyFuncCalls(const Session& session, const Set<String>& called, Vector<String>& out) {
      for (const Unit& unit : session.units) {
        if (!isProgramUnit(unit)) {
          continue;
        }

        for (uint32_t i = unit.first; i < unit.last; i++) {
          const AstItem& item = unit.program->items[i];

          if (item.kind == ItemKind::ExternFunc) {
            reportIfHostOnlyFuncCalled(*unit.program, unit.program->externFuncs[item.index], called, out);
          }
        }
      }
    }

    void reportHostTypeUses(const Session& session, const Uses& uses) {
      for (const Unit& unit : session.units) {
        if (!isProgramUnit(unit)) {
          continue;
        }

        for (uint32_t i = unit.first; i < unit.last; i++) {
          reportUsesInItem(uses, *unit.program, unit.program->items[i]);
        }
      }
    }

  }  // namespace

  Vector<String> NativeProblems(const Session& session, const Module& module) {
    Vector<String> out;
    const Set<Symbol> hostOnly = hostOnlyTypes(session);
    reportHostTypeUses(session, Uses{session, hostOnly, out});

    const Callees callees = makeCallees(module);
    reportHostOnlyMemberCalls(module, callees.names, out);
    reportHostOnlyFuncCalls(session, callees.names, out);

    return out;
  }

}  // namespace polyir::front
