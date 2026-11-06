// Making nodes (Nodes.hpp), and the names they hold. A declaration's name is its whole path (`Geometry.Point`); a node holds it as
// the code where it is used would write it, so the target's code reads like the .poly it came from.
#include "translate/Nodes.hpp"

namespace polyir::translate {

  namespace {

    // Whether code in `here` sees the declarations of `scope` bare: it is that scope, or one inside it.
    bool isInside(const String& here, const String& scope) {
      return scope.empty() || here == scope || here.rfind(scope + ".", 0) == 0;
    }

  }  // namespace

  // ── names ──

  // Bare inside its own scope (or one inside it); from a scope around it, the path down to it; from anywhere else, its whole path.
  // With `imported qualified`, a declaration of an imported file is named through the file (`Check.offInt`).
  String RelativeName(const NodeContext& context, const String& name) {
    const auto imported = context.qualifyImported ? context.importedFiles.find(name) : context.importedFiles.end();

    if (imported != context.importedFiles.end()) {
      return imported->second + "." + name;
    }

    const String scope = ScopeOf(name);

    if (context.fullNames || isInside(context.scope, scope)) {
      return context.fullNames ? name : OwnName(name);
    }

    const bool below = !context.scope.empty() && scope.rfind(context.scope + ".", 0) == 0;

    return below ? name.substr(context.scope.size() + 1) : name;
  }

  // With `modules flat` its whole path (`Geometry.Point`, written `Geometry_Point` in C), as a language without namespaces names
  // it; else its own name, written inside its module.
  String NameDeclared(const NodeContext& context, const String& name) {
    return context.flatNames ? name : OwnName(name);
  }

  String DeclaredName(const NodeContext& context, const String& name, const Generic& generic) {
    const bool written = context.generics.structs.count(generic.of) > 0 || context.generics.functions.count(generic.of) > 0;

    return !generic.of.empty() && written ? generic.of : name;
  }

  Optional<String> BindingIn(const NodeContext& context, const Vector<String>& bindings) {
    for (const String& word : context.bindingWords) {
      for (const String& binding : bindings) {
        if (binding.rfind(word + "=", 0) == 0) {
          return binding.substr(word.size() + 1);
        }
      }
    }

    return nullopt;
  }

  const HostType* BoundHostType(const NodeContext& context, const String& name, String& binding) {
    for (const HostType& host : context.module->hostTypes) {
      const Optional<String> found = host.name == name ? BindingIn(context, host.bindings) : nullopt;

      if (found) {
        binding = *found;

        return &host;
      }
    }

    return nullptr;
  }

  void Problem(NodeContext& context, const String& message) {
    context.problems.push_back(message);
  }

  // ── locals ──

  void PushScope(NodeContext& context) {
    context.scopes.emplace_back();
  }

  void PopScope(NodeContext& context) {
    context.scopes.pop_back();
  }

  String LocalName(const NodeContext& context, const String& name) {
    for (auto scope = context.scopes.rbegin(); scope != context.scopes.rend(); ++scope) {
      if (const auto found = scope->find(name); found != scope->end()) {
        return found->second;
      }
    }

    return name;
  }

  namespace {

    bool isInScope(const NodeContext& context, const String& name) {
      for (const Map<String, String>& scope : context.scopes) {
        if (scope.count(name) > 0) {
          return true;
        }
      }

      return false;
    }

  }  // namespace

  // Its own name, or with `locals unique`, its name and a number when a local of that name is in scope (Java forbids hiding one).
  // Declared again in the same block (a lower form made beside its construct), it keeps the name it was given.
  String DeclareLocal(NodeContext& context, const String& name) {
    if (context.scopes.empty()) {
      PushScope(context);
    }

    if (const auto again = context.scopes.back().find(name); again != context.scopes.back().end()) {
      return again->second;
    }

    const bool hides = context.uniqueLocals && isInScope(context, name);
    String written = name;

    for (int n = 2; hides && (written == name || context.localNames.count(written) > 0); n++) {
      written = name + to_string(n);
    }

    context.scopes.back()[name] = written;
    context.localNames.insert(written);

    return written;
  }

  void StartFunction(NodeContext& context, const Vector<String>& params) {
    context.scopes = {{}};
    context.localNames.clear();

    for (const String& param : params) {
      DeclareLocal(context, param);
    }
  }

  // ── what a body assigns ──

  namespace {

    bool isTarget(const Expr& target, bool globals) {
      return globals ? target.kind == ExprKind::Global : target.kind == ExprKind::Local || target.kind == ExprKind::LoadSlot;
    }

    void addAssigned(const StmtList& body, bool globals, Vector<String>& names) {
      for (const StmtPtr& s : body) {
        const bool assigns = s->kind == StmtKind::Assign && s->target && isTarget(*s->target, globals);

        if (assigns && find(names.begin(), names.end(), s->target->name) == names.end()) {
          names.push_back(s->target->name);
        }

        // A loop's counters are given their next values.
        for (const String& counter : globals || s->kind != StmtKind::For ? Vector<String>{} : s->names) {
          if (find(names.begin(), names.end(), counter) == names.end()) {
            names.push_back(counter);
          }
        }

        for (const StmtList* block : BlocksOf(*s)) {
          addAssigned(*block, globals, names);
        }
      }
    }

    void addDeclared(const StmtList& body, Set<String>& names) {
      for (const StmtPtr& s : body) {
        if (s->kind == StmtKind::Let || s->kind == StmtKind::Var || s->kind == StmtKind::Try) {
          names.insert(s->name);
        }

        names.insert(s->names.begin(), s->names.end());

        for (const StmtList* block : BlocksOf(*s)) {
          addDeclared(*block, names);
        }
      }
    }

  }  // namespace

  namespace {

    // The last `//# from` line before line `line` of the file the function being made is in, or null for none.
    const FromLine* fromLineBefore(const NodeContext& context, int line) {
      const FromLine* from = nullptr;

      for (const FromLine& each : context.module->fromLines) {
        from = each.in == context.spanFile && each.line < line ? &each : from;
      }

      return from;
    }

    // The place `at` stands for: its own line of its file, or the place `from` names (with its code), counted on to `at`'s line.
    SourceSpan placeOf(const NodeContext& context, const Span& at, const FromLine* from) {
      if (from == nullptr) {
        return SourceSpan{context.spanFile.empty() ? context.module->file : context.spanFile, at.line, at.column, ""};
      }

      const bool next = at.line == from->line + 1;

      return SourceSpan{from->file, from->fromLine + at.line - from->line - 1, next ? from->fromColumn : 0, next ? from->code : ""};
    }

  }  // namespace

  void AddSpan(const NodeContext& context, Node& node, const Span& at) {
    if (context.spans && at.line > 0) {
      node.at = placeOf(context, at, fromLineBefore(context, at.line));
    }
  }

  Vector<String> AssignedNames(const StmtList& body, bool globals) {
    Vector<String> names;
    addAssigned(body, globals, names);

    return names;
  }

  Set<String> DeclaredNames(const StmtList& body) {
    Set<String> names;
    addDeclared(body, names);

    return names;
  }

  size_t CountTries(const StmtList& body) {
    size_t count = 0;

    for (const StmtPtr& s : body) {
      count += s->kind == StmtKind::Try ? 1 : 0;

      for (const StmtList* block : BlocksOf(*s)) {
        count += CountTries(*block);
      }
    }

    return count;
  }

  Vector<String> CapturedNames(const Expr& closure) {
    const Set<String> declared = DeclaredNames(closure.closureBody);
    const Vector<String>& params = closure.parameterNames;
    Vector<String> names;

    CountIn(closure.closureBody, [&](const Expr& e) {
      const bool local = e.kind == ExprKind::Local || e.kind == ExprKind::LoadSlot;
      const bool outside = local && declared.count(e.name) == 0 && find(params.begin(), params.end(), e.name) == params.end();

      if (outside && find(names.begin(), names.end(), e.name) == names.end()) {
        names.push_back(e.name);
      }

      return false;
    });

    return names;
  }

  Vector<String> OuterNames(const Expr& closure) {
    const Set<String> declared = DeclaredNames(closure.closureBody);
    Vector<String> outer;

    for (const String& name : AssignedNames(closure.closureBody, false)) {
      const bool isParameter = find(closure.parameterNames.begin(), closure.parameterNames.end(), name) != closure.parameterNames.end();

      if (declared.count(name) == 0 && !isParameter) {
        outer.push_back(name);
      }
    }

    return outer;
  }

  namespace {

    // The variables a closure reads that it does not declare: those it shares with the code around it.
    void addCaptured(const Expr& closure, Set<String>& captured) {
      const Set<String> declared = DeclaredNames(closure.closureBody);
      const Vector<String>& params = closure.parameterNames;

      CountIn(closure.closureBody, [&](const Expr& e) {
        const bool local = e.kind == ExprKind::Local || e.kind == ExprKind::LoadSlot;

        if (local && declared.count(e.name) == 0 && find(params.begin(), params.end(), e.name) == params.end()) {
          captured.insert(e.name);
        }

        return false;
      });
    }

    // The variables both shared with a closure and given a new value.
    Set<String> sharedAndChanged(const Set<String>& captured, const Vector<String>& assigned) {
      Set<String> boxed;

      for (const String& name : assigned) {
        if (captured.count(name) > 0) {
          boxed.insert(name);
        }
      }

      return boxed;
    }

  }  // namespace

  // A variable a closure shares that is given a new value anywhere (in the closure or around it) is boxed, so both see one value.
  Set<String> BoxedNames(const StmtList& body) {
    Set<String> captured;
    Vector<String> assigned = AssignedNames(body, false);

    CountIn(body, [&](const Expr& e) {
      if (e.kind == ExprKind::ClosureLit) {
        addCaptured(e, captured);
        const Vector<String> outer = OuterNames(e);
        assigned.insert(assigned.end(), outer.begin(), outer.end());
      }

      return false;
    });

    return sharedAndChanged(captured, assigned);
  }

  void AddNameFact(Node& node, const String& fact, const Vector<String>& names) {
    Vector<Node> nodes;

    for (const String& name : names) {
      nodes.push_back(MakeNode("name"));
      AddText(nodes.back(), "name", PartKind::Name, name);
    }

    if (!nodes.empty()) {
      AddNodes(node, fact, PartKind::Fact, nodes);
    }
  }

}  // namespace polyir::translate
