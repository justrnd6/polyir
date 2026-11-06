// The checked HIR as one line of text per declaration (polyir/HirPrint.hpp), written exactly as the Nina printer
// (HirPrint.nina) writes it, so the C++ and the Nina front ends can be compared by plain text diff. Each expression or
// statement kind has a small `...Text` function; ExprText and StmtText dispatch by kind through a short chain of
// switches, each handing the kinds it does not know to the next.
#include "polyir/Std.hpp"
#include "polyir/HirPrint.hpp"

namespace polyir {

  namespace {

    String join(const Vector<String>& parts, const String& separator) {
      String out;

      for (size_t i = 0; i < parts.size(); i++) {
        out += (i == 0 ? "" : separator) + parts[i];
      }

      return out;
    }

    // A string as a literal: the characters an escape stands for are written as that escape (HirPrint.nina `quoted`).
    const char* escapeOf(char c) {
      static const Pair<char, const char*> kEscapes[] = {{'\\', "\\\\"}, {'"', "\\\""}, {'$', "\\$"}, {'\n', "\\n"}, {'\t', "\\t"}, {'\r', "\\r"}};

      for (const auto& [letter, escape] : kEscapes) {
        if (letter == c) {
          return escape;
        }
      }

      return nullptr;
    }

    String quoted(const String& text) {
      String out = "\"";

      for (const char c : text) {
        const char* escape = escapeOf(c);
        out += escape != nullptr ? String(escape) : String(1, c);
      }

      return out + "\"";
    }

    String exprsText(const Vector<ExprPtr>& exprs, size_t from = 0) {
      Vector<String> parts;

      for (size_t i = from; i < exprs.size(); i++) {
        parts.push_back(ExprText(*exprs[i]));
      }

      return join(parts, ",");
    }

    String operatorText(const String& name, const Expr& e) {
      Vector<String> parts;

      for (const ExprPtr& x : e.operands) {
        parts.push_back(ExprText(*x));
      }

      return "(" + name + ":" + TypeText(e.type) + " " + join(parts, " ") + ")";
    }

    String resultAndArgs(const Expr& e, size_t from) {
      return TypeText(e.type) + "(" + exprsText(e.operands, from) + ")";
    }

    String fieldInitsText(const Expr& e) {
      Vector<String> parts;

      for (size_t i = 0; i < e.operands.size(); i++) {
        parts.push_back("#" + to_string(e.fieldIndices[i]) + "=" + ExprText(*e.operands[i]));
      }

      return join(parts, ",");
    }

    double magnitude(double value) {
      return value < 0.0 ? 0.0 - value : value;
    }

    // `value` in C's `%.*e` form with `digits` digits after the point, such as `1.5e-07`.
    String scientific(double value, int digits) {
      char text[48];
      snprintf(text, sizeof text, "%.*e", digits, value);

      return text;
    }

    // The fewest digits after the point that `%.*e` needs for the text to read back as `value`. 16 (17 significant
    // digits) is always enough for a double, so the loop stops by then.
    int roundTripDigits(double value) {
      int digits = 0;

      for (; digits < 17; digits++) {
        if (strtod(scientific(value, digits).c_str(), nullptr) == value) {
          break;
        }
      }

      return digits;
    }

    // The power of ten after the `e` (`-7` in `1.5e-07`).
    int exponentOf(const String& eNotation) {
      return atoi(eNotation.substr(eNotation.find('e') + 1).c_str());
    }

    // Plain decimal form. The digits after the point are the significant digits after the first one, less the exponent,
    // and always at least one (`7.5`, `0.0015`).
    String fractionText(double value, int digits, int exponent) {
      char out[48];
      snprintf(out, sizeof out, "%.*f", max(digits - exponent, 1), value);

      return out;
    }

    // `%.*e` text such as `1.5e-07` with the exponent written the Nina way, `1.5E-7` (and `.0` added to a lone digit).
    String scientificText(const String& eNotation, int exponent) {
      const String mantissa = eNotation.substr(0, eNotation.find('e'));

      return mantissa + (mantissa.find('.') != String::npos ? "" : ".0") + "E" + to_string(exponent);
    }

    String digitsText(double value) {
      const int digits = roundTripDigits(value);
      const String shortest = scientific(value, digits);
      const int exponent = exponentOf(shortest);

      if (magnitude(value) >= 1e-3 && magnitude(value) < 1e7) {
        return fractionText(value, digits, exponent);
      }

      return scientificText(shortest, exponent);
    }

    // The Nina printer writes a float as Nina's `Double.toString` does. A whole number prints as an integer, else the fewest significant
    // digits that read back as the same double (`7.5`, `6.2831853`). Outside [1e-3, 1e7) those digits are in E-notation (`1.5E-7`).
    String floatText(double value) {
      if (value != value) {
        return "NaN";  // only NaN is unequal to itself
      }

      if (value - value != 0.0) {
        return value > 0.0 ? "Infinity" : "-Infinity";  // infinity minus itself is NaN, not 0
      }

      // Below 2^53 a whole double prints as an integer, as Nina's Double.toString does; a larger one takes the digits and
      // E-notation path (`1.0E20`), which also keeps the cast to long long away from values above 2^63, where it is undefined.
      const bool whole = magnitude(value) < 9007199254740992.0 && static_cast<double>(static_cast<long long>(value)) == value;

      return whole ? to_string(static_cast<long long>(value)) : digitsText(value);
    }

    String builtinCallText(const Expr& e) {
      return String("(") + BuiltinName(e.builtin) + " " + exprsText(e.operands) + ")";
    }

    // The Nina printer shows a closure literal as `<node>`; so does this one, so the two can be compared as text.
    String closureText(const Expr&) {
      return "<node>";
    }

    String constructedText(const Expr& e) {
      switch (e.kind) {
        case ExprKind::VecLit:
          return "vec[" + exprsText(e.operands) + "]";
        case ExprKind::NewContainer:
          return "new " + TypeText(e.type) + "(" + exprsText(e.operands) + ")";
        case ExprKind::VariantLit:
          return "variant#" + to_string(e.fieldIndex) + "(" + ExprText(*e.operands[0]) + ")";
        case ExprKind::EitherLit:
          return String(e.boolValue ? "ok(" : "err(") + ExprText(*e.operands[0]) + ")";
        default:
          return closureText(e);
      }
    }

    String callText(const Expr& e) {
      if (e.kind == ExprKind::Call) {
        return e.name + ":" + resultAndArgs(e, 0);
      }

      return "(*" + ExprText(*e.operands[0]) + "):" + resultAndArgs(e, 1);
    }

    String builtinOrConstructedText(const Expr& e) {
      return e.kind == ExprKind::CallBuiltin ? builtinCallText(e) : constructedText(e);
    }

    String accessText(const Expr& e) {
      switch (e.kind) {
        case ExprKind::Call: case ExprKind::CallIndirect:
          return callText(e);
        case ExprKind::FieldGet:
          return ExprText(*e.operands[0]) + ".#" + to_string(e.fieldIndex);
        case ExprKind::StructLit:
          return e.name + "{" + fieldInitsText(e) + "}";
        case ExprKind::IndexGet:
          return ExprText(*e.operands[0]) + "[" + exprsText(e.operands, 1) + "]";
        default:
          return builtinOrConstructedText(e);
      }
    }

    String bindings(const Vector<String>& names, const Vector<TypePtr>& types) {
      Vector<String> parts;

      for (size_t i = 0; i < names.size(); i++) {
        parts.push_back(names[i] + ":" + TypeText(types[i]));
      }

      return join(parts, ",");
    }

    String valueBindings(const Vector<String>& names, const Vector<ExprPtr>& values) {
      Vector<String> parts;

      for (size_t i = 0; i < names.size(); i++) {
        parts.push_back(names[i] + "=" + ExprText(*values[i]));
      }

      return join(parts, ",");
    }

    String jumpText(const String& label, const Vector<ExprPtr>& args) {
      return label + "(" + exprsText(args) + ")";
    }

    String switchText(const Stmt& s) {
      Vector<String> arms;

      for (size_t i = 0; i < s.caseValues.size(); i++) {
        arms.push_back(to_string(s.caseValues[i]) + "->" + jumpText(s.caseLabels[i], s.caseArguments[i]));
      }

      arms.push_back("default->" + jumpText(s.name, s.arguments));

      return "switch " + ExprText(*s.value) + " {" + join(arms, ", ") + "}";
    }

    // One arm of a `select`: its keys joined by `|`, the payload it binds in brackets, and its block.
    String selectArmText(const Stmt& s, size_t i) {
      Vector<String> keys;

      for (const ExprPtr& key : s.caseKeys[i]) {
        keys.push_back(ExprText(*key));
      }

      const String bound = i < s.names.size() && !s.names[i].empty() ? "(" + s.names[i] + ")" : "";

      return join(keys, "|") + bound + " => " + BlockText(s.caseBodies[i]);
    }

    String selectText(const Stmt& s) {
      Vector<String> arms;

      for (size_t i = 0; i < s.caseKeys.size(); i++) {
        arms.push_back(selectArmText(s, i));
      }

      arms.push_back("else => " + BlockText(s.elseBody));

      return "select " + ExprText(*s.value) + " {" + join(arms, ", ") + "}";
    }

    String branchText(const Stmt& s) {
      return "branch " + ExprText(*s.value) + " ? " + jumpText(s.name, s.arguments) + " : " + jumpText(s.elseName, s.elseArguments);
    }

    String tryText(const Stmt& s) {
      const String handler = s.name.empty() ? String("cleanup ") : "catch (" + s.name + ") ";

      return "try " + BlockText(s.body) + " " + handler + BlockText(s.elseBody);
    }

    String exitText(const Stmt& s) {
      switch (s.kind) {
        case StmtKind::Try:
          return tryText(s);
        case StmtKind::Throw:
          return "throw " + ExprText(*s.value);
        case StmtKind::Yield:
          return "yield " + ExprText(*s.value);
        default:
          return "<stmt>";
      }
    }

    String flowText(const Stmt& s) {
      switch (s.kind) {
        case StmtKind::Label:
          return "label " + s.name + "(" + bindings(s.parameterNames, s.parameterTypes) + "):";
        case StmtKind::Goto:
          return "goto " + jumpText(s.name, s.arguments);
        case StmtKind::Branch:
          return branchText(s);
        case StmtKind::Switch:
          return switchText(s);
        case StmtKind::Select:
          return selectText(s);
        default:
          return exitText(s);
      }
    }

    String ifText(const Stmt& s) {
      return "if " + ExprText(*s.value) + " " + BlockText(s.body) + " else " + BlockText(s.elseBody);
    }

    String forText(const Stmt& s) {
      return "for (" + valueBindings(s.names, s.initializers) + "; " + ExprText(*s.value) + "; " + exprsText(s.steps) + ") " + BlockText(s.body);
    }

    String initText(const Stmt& s) {
      return s.value ? " = " + ExprText(*s.value) : "";
    }

    String signature(const String& kind, const String& name, const Vector<Parameter>& parameters, const TypePtr& result) {
      Vector<String> names;
      Vector<TypePtr> types;

      for (const Parameter& p : parameters) {
        names.push_back(p.name);
        types.push_back(p.type);
      }

      return kind + " " + name + "(" + bindings(names, types) + ")->" + TypeText(result);
    }

    // `@Doc(text="a point") ` before a declaration, or "" when it has no annotations.
    String annotationsText(const Vector<Annotation>& annotations) {
      String out;

      for (const Annotation& a : annotations) {
        Vector<String> args;

        for (const Pair<String, String>& arg : a.args) {
          args.push_back(arg.first + "=" + arg.second);
        }

        out += "@" + a.name + (args.empty() ? "" : "(" + join(args, ",") + ")") + " ";
      }

      return out;
    }

    String structText(const StructDef& def) {
      Vector<String> parts;

      for (const Field& f : def.fields) {
        parts.push_back((f.isMutable ? "var " : "") + f.name + ":" + TypeText(f.type) + (f.initial ? "=" + ExprText(*f.initial) : ""));
      }

      return annotationsText(def.annotations) + "struct " + def.name + "{" + join(parts, ",") + "}";
    }

    // A kernel's function is followed by the line that says how the GPU runs it: `kernel scale compute 0,1`.
    String kernelLine(const FuncDef& def) {
      Vector<String> roles;

      for (ParamRole role : def.roles) {
        roles.push_back(to_string(static_cast<int>(role)));
      }

      return "kernel " + def.name + " " + StageName(def.stage) + " " + join(roles, ",");
    }

    String funcText(const FuncDef& def) {
      const String head = annotationsText(def.annotations) + signature("func", def.name, def.parameters, def.result);
      const String text = def.body.empty() ? head : head + " " + BlockText(def.body);

      return def.stage == KernelStage::None ? text : text + "\n" + kernelLine(def);
    }

    String interfaceText(const InterfaceDef& def) {
      Vector<String> names;

      for (const InterfaceMethod& m : def.methods) {
        names.push_back(m.name);
      }

      return annotationsText(def.annotations) + "interface " + def.name + "{" + join(names, ",") + "}:" + join(def.implementers, ",");
    }

    String nameText(const Expr& e) {
      switch (e.kind) {
        case ExprKind::Local:
          return e.name;
        case ExprKind::LoadSlot:
          return e.name + "@";
        case ExprKind::Global:
          return "@" + e.name;
        case ExprKind::FuncRef:
          return "&" + e.name;
        default:
          return accessText(e);
      }
    }

    String constantText(const Expr& e) {
      switch (e.kind) {
        case ExprKind::ConstInt:
          return to_string(e.intValue);
        case ExprKind::ConstFloat:
          return floatText(e.floatValue);
        case ExprKind::ConstBool:
          return e.boolValue ? "true" : "false";
        case ExprKind::ConstString:
          return quoted(e.text);
        default:
          return nameText(e);
      }
    }

    // A bare `return` (in a function with no result) carries a void constant and is written `return`, as HirPrint.nina's
    // `returnText` does.
    String returnText(const Stmt& s) {
      if (!s.value) {
        return "return ";
      }

      const bool bare = s.value->kind == ExprKind::ConstInt && s.value->type && IsVoid(*s.value->type);

      return bare ? String("return") : "return " + ExprText(*s.value);
    }

    String structuredText(const Stmt& s) {
      switch (s.kind) {
        case StmtKind::ExprStmt:
          return ExprText(*s.value);
        case StmtKind::If:
          return ifText(s);
        case StmtKind::For:
          return forText(s);
        default:
          return flowText(s);
      }
    }

    // `import js "crc-32" as crc`.
    String importText(const HostImport& d) {
      const String head = "import " + d.target + " \"" + d.module + "\"";

      return d.alias.empty() ? head : head + " as " + d.alias;
    }

    String hostTypeText(const HostType& d) {
      return "host " + d.name + " = " + join(d.bindings, " | ");
    }

    String bridgeText(const Bridge& d) {
      return "bridge " + TypeText(d.polyirType) + " <-> " + d.target + " " + d.hostType;
    }

    String hostMemberText(const HostMember& d) {
      return "host member " + d.function + " = " + join(d.bindings, " | ");
    }

    String externText(const ExternDef& d) {
      return signature("extern", d.name, d.parameters, d.result);
    }

    String globalText(const GlobalDef& d) {
      return "global " + d.name + ":" + TypeText(d.type);
    }

    void addDeclarations(Vector<String>& lines, const Module& module) {
      for (const StructDef& d : module.structs) {
        lines.push_back(structText(d));
      }

      for (const InterfaceDef& d : module.interfaces) {
        lines.push_back(interfaceText(d));
      }

      for (const HostType& d : module.hostTypes) {
        lines.push_back(hostTypeText(d));
      }

      for (const Bridge& d : module.bridges) {
        lines.push_back(bridgeText(d));
      }

      for (const HostImport& d : module.hostImports) {
        lines.push_back(importText(d));
      }
    }

    void addDefinitions(Vector<String>& lines, const Module& module) {
      for (const HostMember& d : module.hostMembers) {
        lines.push_back(hostMemberText(d));
      }

      for (const ExternDef& d : module.externs) {
        lines.push_back(externText(d));
      }

      for (const GlobalDef& d : module.globals) {
        lines.push_back(globalText(d));
      }

      for (const FuncDef& d : module.functions) {
        lines.push_back(funcText(d));
      }
    }

    String definitionLine(const Module& m, const DeclRef& ref) {
      switch (ref.kind) {
        case DeclKind::HostMember:
          return hostMemberText(m.hostMembers[ref.index]);
        case DeclKind::Extern:
          return externText(m.externs[ref.index]);
        case DeclKind::Global:
          return globalText(m.globals[ref.index]);
        default:
          return funcText(m.functions[ref.index]);
      }
    }

  }  // namespace

  String ExprText(const Expr& e) {
    switch (e.kind) {
      case ExprKind::Binary:
        return operatorText(BinOpName(e.binaryOp), e);
      case ExprKind::Unary:
        return operatorText(UnOpName(e.unaryOp), e);
      case ExprKind::Cast: case ExprKind::Coerce:
        return operatorText(e.kind == ExprKind::Cast ? "cast" : "coerce", e);
      case ExprKind::Unsupported:
        return "?" + e.text + "?";
      default:
        return constantText(e);
    }
  }

  String StmtText(const Stmt& s) {
    switch (s.kind) {
      case StmtKind::Let:
        return "let " + s.name + ":" + TypeText(s.type) + " = " + ExprText(*s.value);
      case StmtKind::Var:
        return "var " + s.name + ":" + TypeText(s.type) + initText(s);
      case StmtKind::Assign:
        return ExprText(*s.target) + " = " + ExprText(*s.value);
      case StmtKind::Return:
        return returnText(s);
      default:
        return structuredText(s);
    }
  }

  String BlockText(const StmtList& body) {
    Vector<String> parts;

    for (const StmtPtr& s : body) {
      parts.push_back(StmtText(*s));
    }

    return "{" + join(parts, "; ") + "}";
  }

  namespace {

    // The host-language declarations (host types, bridges, imports); anything else is a definition.
    String hostDeclarationLine(const Module& m, const DeclRef& ref) {
      switch (ref.kind) {
        case DeclKind::HostType:
          return hostTypeText(m.hostTypes[ref.index]);
        case DeclKind::Bridge:
          return bridgeText(m.bridges[ref.index]);
        case DeclKind::HostImport:
          return importText(m.hostImports[ref.index]);
        default:
          return definitionLine(m, ref);
      }
    }

    String declarationLine(const Module& m, const DeclRef& ref) {
      switch (ref.kind) {
        case DeclKind::Struct:
          return structText(m.structs[ref.index]);
        case DeclKind::Interface:
          return interfaceText(m.interfaces[ref.index]);
        default:
          return hostDeclarationLine(m, ref);
      }
    }

    // The declarations in the order the front end wrote them, or (for a module built by hand) grouped by kind.
    void addAll(Vector<String>& lines, const Module& module) {
      if (module.order.size() != DeclarationCount(module)) {
        addDeclarations(lines, module);
        addDefinitions(lines, module);
        return;
      }

      for (const DeclRef& ref : module.order) {
        lines.push_back(declarationLine(module, ref));
      }
    }

  }  // namespace

  String ModuleText(const Module& module) {
    Vector<String> lines;
    addAll(lines, module);

    return join(lines, "\n");
  }

}  // namespace polyir
