// The body pass of the checker (CheckBody.nina): every signature was already resolved by ResolveDeclarations (Resolve.cpp), so a body can call any
// function, forward or mutually recursive. Each function body is checked against those signatures. A parameter shadows a global; the declared return
// type is what a `return` is converted to (an interface return type boxes a struct).
#include "front/Checker.hpp"
#include "front/Pipeline.hpp"

namespace polyir::front {

  namespace {

    // A parameter written `p: val T` is the callee's own copy: the body starts with `let p = val p`, so the caller's object is never touched.
    StmtPtr copyOfParam(Checker& check, const AstParam& param, const TypePtr& type) {
      const Span at = SpanAt(param.byPos);
      StmtPtr made = NewStmt(StmtKind::Let, at);
      made->name = Str(check, param.name.name);
      made->type = type;
      made->value = ValCopy(NamedRef(ExprKind::Local, made->name, type, at));

      return made;
    }

    void bindParam(Checker& check, const AstParam& param, const TypePtr& type, StmtList& copies) {
      BindName(check.scope, param.name.name, BindingKind::Local, type);

      if (Known(param.byPos)) {
        copies.push_back(copyOfParam(check, param, type));
      }
    }

    // Binds the parameters (`this` first for a method), answering the `let p = val p` copies the body starts with.
    StmtList bindParams(Checker& check, const FuncSource& source, const AstFuncDecl& decl, const FuncDef& function) {
      StmtList copies;
      const size_t offset = source.isMethod ? 1 : 0;

      if (source.isMethod) {
        BindName(check.scope, Intern(check.session.names, "this"), BindingKind::Local, function.parameters[0].type);
      }

      const View<AstParam> params = check.program->params[decl.params];

      for (size_t i = 0; i < params.size(); i++) {
        bindParam(check, params[i], function.parameters[i + offset].type, copies);
      }

      return copies;
    }

    StmtList bodyOf(Checker& check, const FuncSource& source, const FuncDef& function) {
      const AstFuncDecl& decl = check.program->funcs[source.decl];
      StmtList body = bindParams(check, source, decl, function);

      for (StmtPtr& statement : CheckBlock(check, decl.body)) {
        body.push_back(move(statement));
      }

      return body;
    }

    // Where a function's names are looked up from: a method's struct (its path), else the function's module (`Outer.Inner`).
    String lookupPathOf(Checker& check, const FuncSource& source, const FuncDef& function) {
      return source.isMethod ? Str(check, source.owner) : function.module;
    }

    // The function's module, its struct (for a method) and its file are where its names are looked up from.
    void enterFunction(Checker& check, const FuncSource& source, const FuncDef& function) {
      Enter(check, check.session.units[source.unit]);
      Clear(check.scope);
      check.session.inModule = lookupPathOf(check, source, function);
      check.session.owner = source.isMethod ? source.owner : Symbol();
      check.session.file = check.session.units[source.unit].file;
    }

    void checkBody(Checker& check, size_t index) {
      const FuncSource& source = check.session.sources[index];
      FuncDef& function = check.session.module.functions[index];

      enterFunction(check, source, function);
      check.returnType = function.result;
      StmtList body = bodyOf(check, source, function);
      check.session.module.functions[index].body = move(body);  // by its place: a generic function's copy made meanwhile may have moved the list
    }

  }  // namespace

  void CheckBodies(Session& session) {
    Checker check{session, SharedRegistry()};

    for (size_t i = 0; i < session.sources.size(); i++) {
      checkBody(check, i);
    }
  }

}  // namespace polyir::front
