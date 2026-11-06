// The tree as text: one line per node, children two spaces deeper, `@line:column` after anything that has a position. Stable, so tests can compare it.
//
// The one idea: a node with children is written as `openNode` (its own line, then one level deeper), its children, and `closeNode` (back up a level).
#include "front/Ast.hpp"

namespace polyir::front {

  namespace {

    // How each kind is written, in the order of its enum.
    const char* const kOperatorText[] = {"*", "/", "%", "+", "-", "<<", ">>", "&", "^", "|", "==", "!=", "<", "<=", ">", ">=", "===", "!==", "&&", "||"};
    const char* const kTypeKindText[] = {"ptr", "vec", "array", "mutarray", "buffer", "grid", "tensor", "sparse", "map", "mutmap", "set", "mutset",
                                         "func", "closure", "variant", "either", "optional", "coro", "generic", "named"};
    const char* const kExprKindText[] = {"binary", "neg", "val", "not", "bitnot", "postfix", "name", "call", "struct", "paren", "int", "float", "string",
                                         "char", "true", "false", "vec", "new-mutarray", "new-array", "new-buffer", "new-sparse", "new-map",
                                         "new-mutmap", "new-set", "new-mutset", "closure", "variant", "either-ok", "either-err", "some", "none", "list",
                                         "if-value", "match"};
    const char* const kStmtKindText[] = {"let", "var", "if", "for", "switch", "label", "goto", "branch", "return", "try", "throw", "yield", "expr",
                                         "match"};
    const char* const kMarkText[] = {"", "uniform ", "index ", "writable ", "workgroup "};
    const char* const kSuffixKindText[] = {"method", "field", "index"};

  }  // namespace

  const char* OperatorText(Operator op) {
    return kOperatorText[static_cast<size_t>(op)];
  }

  namespace {

    String at(Pos pos) {
      return " @" + to_string(pos.line) + ":" + to_string(pos.column);
    }

    // The program being written out, the text so far, and how deep the next line goes.
    struct Dumper {
      const Program& program;
      String text{};
      int depth = 0;
    };

    String typeText(const Dumper& dumper, TypeId id);

    void dumpBlock(Dumper& dumper, const AstBlock& block, const String& label);

    void dumpExpr(Dumper& dumper, ExprId id);

    // ── lines and nodes ─────────────────────────────────────────────────────────────────────────────────────────────────────────────────────

    void writeLine(Dumper& dumper, const String& text) {
      dumper.text += String(dumper.depth * 2, ' ') + text + "\n";
    }

    // A node's own line; the lines written until `closeNode` are its children.
    void openNode(Dumper& dumper, const String& text) {
      writeLine(dumper, text);
      dumper.depth++;
    }

    void closeNode(Dumper& dumper) {
      dumper.depth--;
    }

    // A node whose one child is an expression (`init x`, `step i`).
    void dumpLabelledExpr(Dumper& dumper, const String& label, ExprId value) {
      openNode(dumper, label);
      dumpExpr(dumper, value);
      closeNode(dumper);
    }

    String nameText(const Dumper& dumper, Symbol symbol) {
      return String(TextOf(dumper.program, symbol));
    }

    // A name with the place it was written.
    String nameWithPosition(const Dumper& dumper, const Ident& ident) {
      return nameText(dumper, ident.name) + at(ident.pos);
    }

    // ── types ───────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────

    String typeList(const Dumper& dumper, Range<TypeId> range) {
      String text;

      for (TypeId id : dumper.program.typeRefs[range]) {
        text += (text.empty() ? "" : ",") + typeText(dumper, id);
      }

      return text;
    }

    String sizesText(const Dumper& dumper, Range<Literal> range) {
      String text;

      for (const Literal& size : dumper.program.literals[range]) {
        text += "," + String(size.text);
      }

      return text;
    }

    // `ptr<T>`, `map<K,V>`, `vec<T,4>`, `func(A,B)->R`, `variant<A,B>`.
    String keywordTypeText(const Dumper& dumper, const AstType& type) {
      const String head = kTypeKindText[static_cast<size_t>(type.kind)];
      const bool isCallable = type.kind == AstTypeKind::Func || type.kind == AstTypeKind::Closure;

      if (isCallable) {
        return head + "(" + typeList(dumper, type.types) + ")->" + typeText(dumper, type.first);
      }

      if (type.kind == AstTypeKind::Variant) {
        return head + "<" + typeList(dumper, type.types) + ">";
      }

      const String inside = typeText(dumper, type.first) + (Valid(type.second) ? "," + typeText(dumper, type.second) : "");

      return head + "<" + inside + sizesText(dumper, type.sizes) + ">";
    }

    // `[4]` after a fixed array's name; nothing for a plain name.
    String arraySize(const Dumper& dumper, const AstType& type) {
      return Empty(type.sizes) ? "" : "[" + String(dumper.program.literals[type.sizes][0].text) + "]";
    }

    // `Name`, `Name[4]`, `Name<A,B>`, or a keyword type; `-` when there is no type.
    String typeText(const Dumper& dumper, TypeId id) {
      if (!Valid(id)) {
        return "-";
      }

      const AstType& type = dumper.program.types[id];

      if (type.kind == AstTypeKind::Named) {
        return nameText(dumper, type.name.name) + arraySize(dumper, type);
      }

      if (type.kind == AstTypeKind::Generic) {
        return nameText(dumper, type.name.name) + "<" + typeList(dumper, type.types) + ">";
      }

      return keywordTypeText(dumper, type);
    }

    String paramText(const Dumper& dumper, const AstParam& param) {
      const String byValue = Known(param.byPos) ? "val " : "";

      return kMarkText[static_cast<size_t>(param.mark)] + nameWithPosition(dumper, param.name) + ": " + byValue + typeText(dumper, param.type);
    }

    String paramsText(const Dumper& dumper, Range<AstParam> range) {
      String text;

      for (const AstParam& param : dumper.program.params[range]) {
        text += (text.empty() ? "" : ", ") + paramText(dumper, param);
      }

      return text;
    }

    // `(params) -> ret`
    String signatureText(const Dumper& dumper, Range<AstParam> params, TypeId ret) {
      return "(" + paramsText(dumper, params) + ") -> " + typeText(dumper, ret);
    }

    // ` = java "x" | js "y"`, or nothing.
    String bindingText(const Dumper& dumper, Range<AstHostBinding> range) {
      String text;

      for (const AstHostBinding& binding : dumper.program.hostBindings[range]) {
        text += (text.empty() ? " = " : " | ") + nameText(dumper, binding.target.name) + " " + String(binding.text.text);
      }

      return text;
    }

    // ── declarations ────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────

    String fieldText(const Dumper& dumper, const AstField& field) {
      return String(IsVar(field) ? "var " : "") + nameWithPosition(dumper, field.name) + ": " + typeText(dumper, field.type);
    }

    // `struct Name <T> (x: f64) : Shape`
    String structHead(const Dumper& dumper, const AstStructDecl& decl) {
      String head = "struct " + nameWithPosition(dumper, decl.name);

      for (const Ident& param : dumper.program.idents[decl.typeParams]) {
        head += " <" + nameText(dumper, param.name) + ">";
      }

      for (const AstField& field : dumper.program.fields[decl.primary]) {
        head += " (" + fieldText(dumper, field) + ")";
      }

      for (const Ident& claim : dumper.program.idents[decl.impls]) {
        head += " : " + nameText(dumper, claim.name);
      }

      return head;
    }

    // `coro func `, `kernel func ` or `func `.
    String funcKeywords(const AstFuncDecl& decl) {
      if (decl.isCoroutine) {
        return "coro func ";
      }

      if (decl.isKernel) {
        return "kernel func ";
      }

      return "func ";
    }

    void dumpFunc(Dumper& dumper, const AstFuncDecl& decl) {
      const String kind = funcKeywords(decl);
      openNode(dumper, kind + nameWithPosition(dumper, decl.name) + signatureText(dumper, decl.params, decl.returnType) + at(decl.pos));
      dumpBlock(dumper, decl.body, "body");
      closeNode(dumper);
    }

    void dumpMember(Dumper& dumper, const AstMember& member) {
      if (member.isMethod) {
        return dumpFunc(dumper, dumper.program.funcs[member.method]);
      }

      writeLine(dumper, "field " + fieldText(dumper, member.field));
    }

    void dumpStruct(Dumper& dumper, const AstStructDecl& decl) {
      openNode(dumper, structHead(dumper, decl) + at(decl.pos));

      for (const AstMember& member : dumper.program.members[decl.members]) {
        dumpMember(dumper, member);
      }

      closeNode(dumper);
    }

    void dumpInterfaceMethods(Dumper& dumper, Range<AstInterfaceMethod> range) {
      for (const AstInterfaceMethod& method : dumper.program.interfaceMethods[range]) {
        writeLine(dumper, "method " + nameWithPosition(dumper, method.name) + signatureText(dumper, method.params, method.returnType));
      }
    }

    void dumpInterface(Dumper& dumper, const AstInterfaceDecl& decl) {
      openNode(dumper, "interface " + nameWithPosition(dumper, decl.name) + at(decl.pos));
      dumpInterfaceMethods(dumper, decl.methods);
      closeNode(dumper);
    }

    void dumpExternFunc(Dumper& dumper, const AstExternFunc& decl) {
      const String signature = signatureText(dumper, decl.params, decl.returnType);
      writeLine(dumper, "extern func " + nameWithPosition(dumper, decl.name) + signature + bindingText(dumper, decl.binding) + at(decl.pos));
    }

    void dumpExternInterface(Dumper& dumper, const AstExternInterface& decl) {
      openNode(dumper, "extern interface " + nameWithPosition(dumper, decl.name) + bindingText(dumper, decl.binding) + at(decl.pos));
      dumpInterfaceMethods(dumper, decl.methods);
      closeNode(dumper);
    }

    void dumpHostMember(Dumper& dumper, const AstHostMember& member) {
      const String head = String(member.isStatic ? "static " : "") + "member " + nameWithPosition(dumper, member.name);
      writeLine(dumper, head + signatureText(dumper, member.params, member.returnType) + bindingText(dumper, member.binding));
    }

    void dumpExternType(Dumper& dumper, const AstExternType& decl) {
      openNode(dumper, "extern type " + nameWithPosition(dumper, decl.name) + bindingText(dumper, decl.binding) + at(decl.pos));

      for (const AstHostMember& member : dumper.program.hostMembers[decl.members]) {
        dumpHostMember(dumper, member);
      }

      closeNode(dumper);
    }

    void dumpBridge(Dumper& dumper, const AstBridge& decl) {
      const String host = " " + String(decl.host.text) + " rules " + to_string(decl.rules.count);
      writeLine(dumper, "bridge " + typeText(dumper, decl.poly) + " <-> " + nameWithPosition(dumper, decl.target) + host + at(decl.pos));
    }

    void dumpImport(Dumper& dumper, const AstImport& decl) {
      const String lang = Known(decl.lang.pos) ? nameText(dumper, decl.lang.name) + " " : String();
      writeLine(dumper, "import " + lang + String(decl.path.text) + at(decl.pos));
    }

    void dumpGlobal(Dumper& dumper, const AstGlobal& decl) {
      writeLine(dumper, "global " + nameWithPosition(dumper, decl.name) + ": " + typeText(dumper, decl.type) + " = " + String(decl.value.text) + at(decl.pos));
    }

    void dumpMeta(Dumper& dumper, const AstMeta& decl) {
      const String target = Known(decl.target.pos) ? nameText(dumper, decl.target.name) + " " : String();
      writeLine(dumper, "meta " + target + String(decl.key.text) + " = " + String(decl.value.text) + at(decl.pos));
    }

    void dumpExternDecl(Dumper& dumper, const AstItem& item) {
      if (item.kind == ItemKind::ExternFunc) {
        return dumpExternFunc(dumper, dumper.program.externFuncs[item.index]);
      }

      if (item.kind == ItemKind::ExternInterface) {
        return dumpExternInterface(dumper, dumper.program.externInterfaces[item.index]);
      }

      dumpExternType(dumper, dumper.program.externTypes[item.index]);
    }

    void dumpOtherDecl(Dumper& dumper, const AstItem& item) {
      if (item.kind == ItemKind::Bridge) {
        return dumpBridge(dumper, dumper.program.bridges[item.index]);
      }

      if (item.kind == ItemKind::Import) {
        return dumpImport(dumper, dumper.program.imports[item.index]);
      }

      if (item.kind == ItemKind::Global) {
        return dumpGlobal(dumper, dumper.program.globals[item.index]);
      }

      dumpMeta(dumper, dumper.program.metas[item.index]);
    }

    bool isExtern(ItemKind kind) {
      return kind == ItemKind::ExternType || kind == ItemKind::ExternInterface || kind == ItemKind::ExternFunc;
    }

    void dumpItem(Dumper& dumper, const AstItem& item) {
      if (item.kind == ItemKind::Struct) {
        return dumpStruct(dumper, dumper.program.structs[item.index]);
      }

      if (item.kind == ItemKind::Func || item.kind == ItemKind::CoroutineFunc) {
        return dumpFunc(dumper, dumper.program.funcs.items[item.index]);
      }

      if (item.kind == ItemKind::Interface) {
        return dumpInterface(dumper, dumper.program.interfaces[item.index]);
      }

      if (isExtern(item.kind)) {
        return dumpExternDecl(dumper, item);
      }

      dumpOtherDecl(dumper, item);
    }

    // ── match arms and patterns ─────────────────────────────────────────────────────────────────────────────────────────────────────────────

    String patternText(const Dumper& dumper, PatternId id);

    String join(const Vector<String>& parts, const char* separator) {
      String text;

      for (size_t i = 0; i < parts.size(); i++) {
        text += (i > 0 ? separator : "") + parts[i];
      }

      return text;
    }

    String patternsText(const Dumper& dumper, Range<PatternId> range, const char* separator) {
      Vector<String> parts;

      for (PatternId id : dumper.program.patternRefs[range]) {
        parts.push_back(patternText(dumper, id));
      }

      return join(parts, separator);
    }

    // A literal pattern as written: `0`, `-1`, `"y"`.
    String valueText(const Dumper& dumper, ExprId id) {
      const AstExpr& expr = dumper.program.exprs[id];
      const bool negative = expr.kind == AstExprKind::Neg;

      return (negative ? "-" : "") + String((negative ? dumper.program.exprs[expr.first] : expr).literal.text);
    }

    String listText(const Dumper& dumper, const AstPattern& pattern) {
      Vector<String> parts{patternsText(dumper, pattern.items, ", ")};
      const String rest = Known(pattern.name.pos) ? "..." + nameText(dumper, pattern.name.name) : String("...");

      if (pattern.hasRest) {
        parts.push_back(rest);
      }

      return "[" + join(parts, pattern.items.count > 0 ? ", " : "") + "]";
    }

    String payloadText(const Dumper& dumper, const AstPattern& pattern) {
      const String inner = patternText(dumper, pattern.first);

      switch (pattern.kind) {
        case AstPatternKind::Some:
          return "Some(" + inner + ")";
        case AstPatternKind::Ok:
          return "Ok(" + inner + ")";
        case AstPatternKind::Err:
          return "Err(" + inner + ")";
        default:
          return "variant(" + String(pattern.literal.text) + ", " + inner + ")";
      }
    }

    // A pattern on one line, as it would be written.
    String patternText(const Dumper& dumper, PatternId id) {
      const AstPattern& pattern = dumper.program.patterns[id];

      switch (pattern.kind) {
        case AstPatternKind::Value:
          return valueText(dumper, pattern.value);
        case AstPatternKind::Any: case AstPatternKind::None:
          return pattern.kind == AstPatternKind::Any ? "_" : "None";
        case AstPatternKind::Name:
          return nameText(dumper, pattern.name.name);
        case AstPatternKind::List:
          return listText(dumper, pattern);
        case AstPatternKind::Or:
          return patternsText(dumper, pattern.items, " | ");
        default:
          return payloadText(dumper, pattern);
      }
    }

    // `arm else`, `arm pattern Some(n)` or `arm when` (a condition), then its guard or condition, and its value or block.
    void dumpArm(Dumper& dumper, const AstArm& arm) {
      const String head = arm.isElse ? "else" : (Valid(arm.pattern) ? "pattern " + patternText(dumper, arm.pattern) : String("when"));
      openNode(dumper, "arm " + head + at(arm.pos));

      for (ExprId part : {arm.guard, arm.cond, arm.value}) {
        if (Valid(part)) {
          dumpExpr(dumper, part);
        }
      }

      if (Known(arm.body.pos)) {
        dumpBlock(dumper, arm.body, "body");
      }

      closeNode(dumper);
    }

    void dumpArms(Dumper& dumper, Range<AstArm> arms) {
      for (const AstArm& arm : dumper.program.arms[arms]) {
        dumpArm(dumper, arm);
      }
    }

    // ── statements ──────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────

    void dumpStmt(Dumper& dumper, StmtId id);

    void dumpBlock(Dumper& dumper, const AstBlock& block, const String& label) {
      openNode(dumper, label + at(block.pos));

      for (StmtId id : dumper.program.stmtRefs[block.stmts]) {
        dumpStmt(dumper, id);
      }

      closeNode(dumper);
    }

    void dumpExprs(Dumper& dumper, Range<ExprId> range) {
      for (ExprId id : dumper.program.exprRefs[range]) {
        dumpExpr(dumper, id);
      }
    }

    void dumpGotoTarget(Dumper& dumper, const AstGotoTarget& target, const String& label) {
      openNode(dumper, label + " " + nameWithPosition(dumper, target.name));
      dumpExprs(dumper, target.args);
      closeNode(dumper);
    }

    // `init i @1:9` or `step i @1:20` of a `for`, with its value as the child.
    void dumpForBinding(Dumper& dumper, const char* label, const AstForBinding& binding) {
      dumpLabelledExpr(dumper, String(label) + " " + nameWithPosition(dumper, binding.name), binding.value);
    }

    // The loop bindings, the switch arms and the jump targets of a statement.
    void dumpStmtTargets(Dumper& dumper, const AstStmt& stmt) {
      for (const AstForBinding& init : dumper.program.forBindings[stmt.inits]) {
        dumpForBinding(dumper, "init", init);
      }

      for (const AstForBinding& step : dumper.program.forBindings[stmt.steps]) {
        dumpForBinding(dumper, "step", step);
      }

      for (const AstSwitchCase& arm : dumper.program.cases[stmt.cases]) {
        dumpGotoTarget(dumper, arm.target, "case " + String(arm.value.text) + " =>");
      }

      if (Known(stmt.goTo.name.pos)) {
        dumpGotoTarget(dumper, stmt.goTo, stmt.kind == AstStmtKind::Switch ? "default =>" : "to");
      }

      if (Known(stmt.goElse.name.pos)) {
        dumpGotoTarget(dumper, stmt.goElse, "else");
      }
    }

    // The target, the condition and the value of a statement, each one it has.
    void dumpStmtExprs(Dumper& dumper, const AstStmt& stmt) {
      if (Valid(stmt.target)) {
        dumpExpr(dumper, stmt.target);
      }

      if (Valid(stmt.cond)) {
        dumpExpr(dumper, stmt.cond);
      }

      if (Valid(stmt.value)) {
        dumpExpr(dumper, stmt.value);
      }

      dumpArms(dumper, stmt.arms);
    }

    void dumpStmtChildren(Dumper& dumper, const AstStmt& stmt) {
      dumpStmtExprs(dumper, stmt);

      if (stmt.params.count > 0) {
        writeLine(dumper, "params " + paramsText(dumper, stmt.params));
      }

      if (Known(stmt.body.pos)) {
        dumpBlock(dumper, stmt.body, "body");
      }

      if (Known(stmt.other.pos)) {
        dumpBlock(dumper, stmt.other, "other");
      }

      dumpStmtTargets(dumper, stmt);
    }

    // `let x @1:5: i64`, `if`, `expr =` (an assignment), ...: the kind (or `val`), the name and type it binds, and ` =` when an expression
    // statement assigns.
    String stmtHead(const Dumper& dumper, const AstStmt& stmt) {
      String head = stmt.isVal ? "val" : (stmt.isSwitch ? "switch" : kStmtKindText[static_cast<size_t>(stmt.kind)]);
      const bool hasName = Known(stmt.name.pos);

      if (hasName) {
        head += " " + nameWithPosition(dumper, stmt.name);
      }

      if (hasName && Valid(stmt.type)) {
        head += ": " + typeText(dumper, stmt.type);
      }

      if (stmt.isAssign) {
        head += " =";
      }

      return head;
    }

    void dumpStmt(Dumper& dumper, StmtId id) {
      const AstStmt& stmt = dumper.program.stmts[id];
      openNode(dumper, stmtHead(dumper, stmt) + at(stmt.pos));
      dumpStmtChildren(dumper, stmt);
      closeNode(dumper);
    }

    // ── expressions ─────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────

    // A name written where the node starts needs no position of its own.
    String exprName(const Dumper& dumper, const AstExpr& expr) {
      const bool startsTheNode = expr.name.pos.line == expr.pos.line && expr.name.pos.column == expr.pos.column;

      return startsTheNode ? nameText(dumper, expr.name.name) : nameWithPosition(dumper, expr.name);
    }

    // `<T>` or `<K,V>` of a constructor, and the `<A,B>` of a variant.
    String exprTypes(const Dumper& dumper, const AstExpr& expr) {
      String text;

      if (Valid(expr.type)) {
        text += " <" + typeText(dumper, expr.type) + (Valid(expr.type2) ? "," + typeText(dumper, expr.type2) : "") + ">";
      }

      if (expr.types.count > 0) {
        text += " <" + typeList(dumper, expr.types) + ">";
      }

      return text;
    }

    String exprHead(const Dumper& dumper, const AstExpr& expr) {
      String head = kExprKindText[static_cast<size_t>(expr.kind)];

      if (expr.kind == AstExprKind::Binary) {
        head += String(" ") + kOperatorText[static_cast<size_t>(expr.op)] + at(expr.opPos);
      }

      if (Known(expr.name.pos)) {
        head += " " + exprName(dumper, expr);
      }

      if (Known(expr.literal.pos)) {
        head += " " + String(expr.literal.text);
      }

      return head + exprTypes(dumper, expr) + at(expr.pos);
    }

    void dumpSuffix(Dumper& dumper, const AstSuffix& suffix) {
      const String name = Known(suffix.name.pos) ? " " + nameWithPosition(dumper, suffix.name) : String();
      openNode(dumper, String(kSuffixKindText[static_cast<size_t>(suffix.kind)]) + name + at(suffix.pos));
      dumpExprs(dumper, suffix.args);
      closeNode(dumper);
    }

    // The first and the second operand of an expression (each one it has), then its arguments.
    void dumpExprOperands(Dumper& dumper, const AstExpr& expr) {
      if (Valid(expr.first)) {
        dumpExpr(dumper, expr.first);
      }

      if (Valid(expr.second)) {
        dumpExpr(dumper, expr.second);
      }

      dumpExprs(dumper, expr.args);
      dumpArms(dumper, expr.arms);
    }

    void dumpExprChildren(Dumper& dumper, const AstExpr& expr) {
      dumpExprOperands(dumper, expr);

      for (const AstFieldInit& init : dumper.program.inits[expr.inits]) {
        dumpLabelledExpr(dumper, "init " + nameWithPosition(dumper, init.name), init.value);
      }

      for (const AstSuffix& suffix : dumper.program.suffixes[expr.suffixes]) {
        dumpSuffix(dumper, suffix);
      }

      if (expr.params.count > 0) {
        writeLine(dumper, "params " + paramsText(dumper, expr.params));
      }

      if (Known(expr.body.pos)) {
        dumpBlock(dumper, expr.body, "body");
      }
    }

    void dumpExpr(Dumper& dumper, ExprId id) {
      const AstExpr& expr = dumper.program.exprs[id];
      openNode(dumper, exprHead(dumper, expr));
      dumpExprChildren(dumper, expr);
      closeNode(dumper);
    }

  }  // namespace

  String DumpProgram(const Program& program) {
    Dumper dumper{program};
    openNode(dumper, "program");

    for (const AstItem& item : program.items) {
      dumpItem(dumper, item);
    }

    closeNode(dumper);

    return dumper.text;
  }

}  // namespace polyir::front
