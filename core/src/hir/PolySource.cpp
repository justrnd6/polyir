// The HIR written as .poly source (polyir/PolySource.hpp), each construct in its .poly spelling, so polyir's front end reads the
// program back.
//
// Most of the HIR is written as it stands: `let x: i64 = a + b`, a struct literal `Point(x: 1, y: 2)`, a field by its name, a cast
// `x.toI64()`, an optional `Some<i64>(x)`. Some shapes go back to what the source says. A method (`Point_sum(this: Point)`) goes
// back into its struct's body, and a call of it is `p.sum()`. A walk over a map's or set's slots, or a loop over a list's
// indices that reads only the element, is `for (k in m)`. A new list with values pushed onto it is `[a, b]`, text joined from
// literals and `str(x)` is `"n = ${x}"`, and a `let` of a copy is `val`. A struct boxed into an interface is the plain struct,
// which the front end boxes where the interface is expected. A declaration with a module sits in a `module X { ... }` block,
// named bare there and `X.name` elsewhere, and `internal` is written back. A variable an `if` fills branch by branch, read once,
// is the `if` value itself (`let n: i64 = if (c) a else b`); an `if` whose branches only return is `return if (c) a else b`.
// What has no .poly spelling is written `?what?` and listed. The shapes it looks for are in Lift.hpp, shared with the translate engine.
#include "polyir/Std.hpp"
#include "polyir/PolySource.hpp"
#include "hir/Lift.hpp"

namespace polyir {

  namespace {

    // A walk over a table's slots, written as `for (key in table)`: inside it, the key at the slot is `key`.
    struct Walk {
      String table;
      String slot;
      String key;
    };

    // What is being written: the module, the names declarations are written with, the module whose block is open, the variables
    // `if` values stand in for, the body of the function being written, how deep the next line is, the lines so far, the walks
    // the next line is inside, and what had no spelling.
    struct Writer {
      const Module* module = nullptr;
      Map<String, String> methods;  // a method's function and the name a dot call uses: `Point_sum` -> `sum`
      Map<String, String> modules;     // a struct's, interface's or function's name and its module (`Geometry.Point` -> `Geometry`)
      Map<String, String> shortNames;  // the same names without their module, as their declarations write them (`Point`)
      String inModule;                 // the module whose block is being written ("" at the top level)
      Map<String, String> inlined;     // a variable an `if` value fills, and the `if` written in its place
      Map<String, const StructDef*> genericStructs;  // each generic written once (`struct Box<T>`), by its name (`Box` -> `Box_T`)
      Map<String, const FuncDef*> genericFunctions;  // the same for generic functions (`pick` -> `pick_T`)
      Set<String> writtenGenerics;                   // the generics written so far
      const StmtList* function = nullptr;
      int depth = 0;
      Vector<String> lines;
      Vector<Walk> walks;
      Vector<String> problems;
    };

    struct OperatorForm {
      BinOp op;
      const char* text;
      int precedence;  // .poly's own (ParserExprs.cpp): a higher one binds more tightly
    };

    constexpr OperatorForm kOperators[] = {
      {BinOp::Mul, "*", 9},          {BinOp::Div, "/", 9},          {BinOp::Rem, "%", 9},           {BinOp::Add, "+", 8},
      {BinOp::Sub, "-", 8},          {BinOp::Shl, "<<", 7},         {BinOp::Shr, ">>", 7},          {BinOp::And, "&", 6},
      {BinOp::Xor, "^", 5},          {BinOp::Or, "|", 4},           {BinOp::Eq, "==", 3},           {BinOp::Ne, "!=", 3},
      {BinOp::Lt, "<", 3},           {BinOp::Le, "<=", 3},          {BinOp::Gt, ">", 3},            {BinOp::Ge, ">=", 3},
      {BinOp::Identical, "===", 3},  {BinOp::NotIdentical, "!==", 3}, {BinOp::LogAnd, "&&", 2},     {BinOp::LogOr, "||", 1}};

    // A string join is written `a + b` and string equality `a == b`.
    constexpr OperatorForm kJoin = {BinOp::Add, "+", 8};
    constexpr OperatorForm kStringEquals = {BinOp::Eq, "==", 3};

    // A builtin written as a call: `str(x)` (isMethod false) or `m.has(k)` (isMethod true).
    struct BuiltinForm {
      Builtin op;
      const char* name;
      bool isMethod;
    };

    constexpr BuiltinForm kBuiltins[] = {
      {Builtin::Len, "len", true},              {Builtin::Push, "push", true},               {Builtin::Hash, "hash", false},
      {Builtin::StrOf, "str", false},           {Builtin::Print, "print", false},            {Builtin::Fail, "fail", false},            {Builtin::Copy, "copy", true},
      {Builtin::Has, "has", true},              {Builtin::Add, "add", true},                 {Builtin::Remove, "remove", true},
      {Builtin::Truncate, "truncate", true},    {Builtin::With, "with", true},               {Builtin::Slice, "slice", true},
      {Builtin::Find, "find", true},            {Builtin::VariantTag, "tag", true},          {Builtin::VariantIsCase, "isCase", true},
      {Builtin::VariantUnwrap, "unwrapAt", true}, {Builtin::EitherIsOk, "isOk", true},       {Builtin::EitherIsErr, "isErr", true},
      {Builtin::EitherUnwrapOk, "unwrapOk", true}, {Builtin::EitherUnwrapErr, "unwrapErr", true}, {Builtin::OptionIsSome, "isSome", true},
      {Builtin::OptionIsNone, "isNone", true},  {Builtin::OptionUnwrap, "unwrap", true},     {Builtin::CoroutineResume, "resume", true},
      {Builtin::CoroutineDone, "done", true},   {Builtin::CoroutineClose, "close", true},    {Builtin::Shuffle, "shuffle", true},
      {Builtin::Rows, "rows", true},            {Builtin::Cols, "cols", true},               {Builtin::Dim, "dim", true}};

    // The words of .poly (front/Lexer.hpp): a name that is one of them gets a `_` after it.
    const char* const kKeywords[] = {"func", "extern", "import", "global", "vec", "darray", "array", "mutarray", "buffer", "map", "mutmap",
      "set", "mutset", "grid", "tensor", "sparse", "hashmap", "meta", "struct", "interface", "type", "static", "let", "var", "val", "if",
      "else", "for", "in", "switch", "label", "goto", "branch", "return", "try", "catch", "throw", "closure", "variant", "either", "eitherOk",
      "eitherErr", "optional", "bridge", "Some", "None", "coro", "kernel", "uniform", "index", "writable", "workgroup", "yield", "true",
      "false", "ptr"};

    // The builtins a call can name (`len(xs)`): a function with one of these names would be taken for the builtin.
    const char* const kCallForms[] = {"push", "len", "hash", "copy", "str", "print", "fail", "fence"};

    // The dot calls .poly takes for its own (builtins and casts) whatever the receiver: a method with one of these names keeps
    // its full name (`Point_count(p)`), since `p.count()` would be the builtin.
    const char* const kTakenByDot[] = {"len", "count", "hash", "push", "copy", "with", "has", "add", "remove", "freeze", "thaw",
      "truncate", "capacity", "yielded", "toI8", "toI16", "toI32", "toI64", "toU8", "toU16", "toU32", "toU64", "toFloat", "toDouble",
      "toPtr"};

    String exprText(Writer& writer, const Expr& e);

    void bodyLines(Writer& writer, const StmtList& body);

    String join(const Vector<String>& parts, const String& separator) {
      String out;

      for (size_t i = 0; i < parts.size(); i++) {
        out += (i == 0 ? "" : separator) + parts[i];
      }

      return out;
    }

    // `?what?` in the text, and `what` noted as a problem.
    String unspelled(Writer& writer, const String& what) {
      writer.problems.push_back("no .poly spelling for " + what);

      return "?" + what + "?";
    }

    void addLine(Writer& writer, const String& text) {
      writer.lines.push_back(String(static_cast<size_t>(writer.depth) * 2, ' ') + text);
    }

    // ── names ──

    bool isKeyword(const String& name) {
      for (const char* word : kKeywords) {
        if (name == word) {
          return true;
        }
      }

      return false;
    }

    bool isCallForm(const String& name) {
      for (const char* word : kCallForms) {
        if (name == word) {
          return true;
        }
      }

      return false;
    }

    // A name as .poly can write it: a keyword gets a `_` after it (`index_`).
    String nameOf(const String& name) {
      return isKeyword(name) ? name + "_" : name;
    }

    bool isTakenByDot(const String& name) {
      for (const char* word : kTakenByDot) {
        if (name == word) {
          return true;
        }
      }

      return false;
    }

    // ── modules ──

    // The prefix a module gives its declarations' names: `Geometry.Shapes.` for `Geometry.Shapes`.
    String prefixOf(const String& module) {
      return module + ".";
    }

    // A declaration's own name, without its module's prefix: `Point` for `Geometry.Point`.
    String withoutModule(const String& name, const String& module) {
      const String prefix = prefixOf(module);

      return !module.empty() && name.rfind(prefix, 0) == 0 ? name.substr(prefix.size()) : name;
    }

    // Whether the code of module `here` sees the declarations of `module` bare: it is that module, or one inside it.
    bool isInside(const String& here, const String& module) {
      return here == module || here.rfind(module + ".", 0) == 0;
    }

    // A declaration's name as the code being written names it: bare in its own module (or a module inside it), else with its
    // module, `Geometry.Point`.
    String declName(const Writer& writer, const String& name) {
      const auto module = writer.modules.find(name);
      const auto own = writer.shortNames.find(name);
      const String shortName = own == writer.shortNames.end() ? nameOf(name) : own->second;

      if (module == writer.modules.end() || module->second.empty() || isInside(writer.inModule, module->second)) {
        return shortName;
      }

      const String& path = module->second;
      const bool below = !writer.inModule.empty() && path.rfind(writer.inModule + ".", 0) == 0;

      return (below ? path.substr(writer.inModule.size() + 1) : path) + "." + shortName;
    }

    Vector<String> partsOf(const String& module) {
      Vector<String> parts;
      size_t from = 0;

      while (!module.empty() && from <= module.size()) {
        const size_t dot = module.find('.', from);
        parts.push_back(module.substr(from, dot == String::npos ? String::npos : dot - from));
        from = dot == String::npos ? module.size() + 1 : dot + 1;
      }

      return parts;
    }

    void closeModule(Writer& writer) {
      while (!writer.lines.empty() && writer.lines.back().empty()) {
        writer.lines.pop_back();
      }

      writer.depth -= 1;
      addLine(writer, "}");
      writer.lines.push_back("");
    }

    // The blocks between the module being written and `target`: the ones left are closed, the ones entered opened.
    void enterModule(Writer& writer, const String& target) {
      const Vector<String> here = partsOf(writer.inModule);
      const Vector<String> there = partsOf(target);
      size_t common = 0;

      while (common < here.size() && common < there.size() && here[common] == there[common]) {
        common++;
      }

      for (size_t i = here.size(); i > common; i--) {
        closeModule(writer);
      }

      for (size_t i = common; i < there.size(); i++) {
        addLine(writer, "module " + there[i] + " {");
        writer.depth += 1;
      }

      writer.inModule = target;
    }

    // ── generics ──
    // A generic that works for every type (Hir.hpp `Generic`) is written once, `struct Box<T>`, and its copies are named as it is:
    // `Box<i64>` as a type, `Box(value: 1)` as a literal, `pick(a, b)` as a call. Any other generic is written as its copies.

    // A generic struct is written once when it works for every type and a dot can call each of its methods.
    bool isWrittenStruct(const Module& module, const StructDef& def) {
      if (!WorksForEveryType(def.generic)) {
        return false;
      }

      for (const FuncDef* method : GenericMethodsOf(module, def)) {
        if (isTakenByDot(MethodName(*method, def))) {
          return false;
        }
      }

      return true;
    }

    bool isWrittenGeneric(const Writer& writer, const String& of) {
      return writer.genericStructs.count(of) > 0 || writer.genericFunctions.count(of) > 0;
    }

    // The name a declaration is written with: a copy of a generic written once, and that generic itself, take the generic's name.
    String writtenName(const Writer& writer, const String& name, const Generic& generic) {
      return isWrittenGeneric(writer, generic.of) ? generic.of : name;
    }

    // `<T>` after a generic's name as declared, nothing after any other.
    String paramsText(const Generic& generic) {
      return generic.params.empty() ? "" : "<" + join(generic.params, ", ") + ">";
    }

    // ── types ──

    String typeText(const Writer& writer, const TypePtr& type);

    String typesText(const Writer& writer, const Vector<TypePtr>& types) {
      Vector<String> parts;

      for (const TypePtr& type : types) {
        parts.push_back(typeText(writer, type));
      }

      return join(parts, ", ");
    }

    // `mutarray<i64>`; a closing `>` after another is written `> >`, since .poly reads `>>` as the shift.
    String wrapped(const Writer& writer, const String& head, const Vector<TypePtr>& types) {
      const String inner = typesText(writer, types);

      return head + "<" + inner + (!inner.empty() && inner.back() == '>' ? " >" : ">");
    }

    // A list, map or set, of the kind `isMutable` says.
    String collectionText(const Writer& writer, const Type& t, bool isMutable) {
      switch (t.kind) {
        case TypeKind::DynamicArray:
          return wrapped(writer, isMutable ? "mutarray" : "array", {t.element});
        case TypeKind::Hashmap:
          return wrapped(writer, isMutable ? "mutmap" : "map", {t.element, t.second});
        default:
          return wrapped(writer, isMutable ? "mutset" : "set", {t.element});
      }
    }

    String calledText(const Writer& writer, const Type& t) {
      switch (t.kind) {
        case TypeKind::Closure:
          return "closure(" + typesText(writer, t.parameters) + ") -> " + typeText(writer, t.element);
        case TypeKind::Function:
          return "func(" + typesText(writer, t.parameters) + ") -> " + typeText(writer, t.element);
        case TypeKind::Pointer:
          return wrapped(writer, "ptr", {t.element});
        case TypeKind::Coroutine:
          return wrapped(writer, "coro", {t.element});
        default:
          return TypeText(t);
      }
    }

    String sumText(const Writer& writer, const Type& t) {
      switch (t.kind) {
        case TypeKind::Optional:
          return wrapped(writer, "optional", {t.element});
        case TypeKind::Either:
          return wrapped(writer, "either", {t.element, t.second});
        case TypeKind::Variant:
          return wrapped(writer, "variant", t.parameters);
        default:
          return IsCollection(t) ? collectionText(writer, t, t.isMutable) : calledText(writer, t);
      }
    }

    // A struct type; a copy of a generic written once is `Box<i64>`, and the generic itself `Box<T>`.
    String structTypeText(const Writer& writer, const String& name) {
      const StructDef* def = AnyStruct(*writer.module, name);

      if (def == nullptr || writer.genericStructs.count(def->generic.of) == 0) {
        return declName(writer, name);
      }

      return wrapped(writer, declName(writer, name), def->generic.arguments);
    }

    String typeText(const Writer& writer, const TypePtr& type) {
      if (!type || type->kind == TypeKind::Void || type->kind == TypeKind::Param) {
        return type && type->kind == TypeKind::Param ? type->name : "void";
      }

      switch (type->kind) {
        case TypeKind::Int:
          return type->bits == 1 ? "bool" : "i" + to_string(type->bits);
        case TypeKind::UInt:
          return "u" + to_string(type->bits);
        case TypeKind::Float:
          return type->bits == 32 ? "float" : "double";
        case TypeKind::String:
          return "string";
        default:
          return type->kind == TypeKind::Struct ? structTypeText(writer, type->name) : sumText(writer, *type);
      }
    }

    // ── constants ──

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

    // The cast to a number type: `toI32`, `toU8`, `toDouble`; empty for a type no cast makes.
    String castName(const TypePtr& type) {
      if (!type) {
        return "";
      }

      switch (type->kind) {
        case TypeKind::Int:
          return type->bits == 1 ? "" : "toI" + to_string(type->bits);
        case TypeKind::UInt:
          return "toU" + to_string(type->bits);
        case TypeKind::Float:
          return type->bits == 32 ? "toFloat" : "toDouble";
        default:
          return type->kind == TypeKind::Pointer ? "toPtr" : "";
      }
    }

    // An integer literal takes the width its place wants (`c == 32` with `c: i8`), so its digits are enough.
    String intText(const Expr& e) {
      return e.intValue == INT64_MIN ? String("(-9223372036854775807 - 1)") : to_string(e.intValue);
    }

    // The fewest digits after the point that read back as the same number, never in E-notation, which .poly does not read.
    String floatText(Writer& writer, double value) {
      if (value != value || value - value != 0.0) {
        return value != value ? "(0.0 / 0.0)" : value > 0.0 ? "(1.0 / 0.0)" : "(-1.0 / 0.0)";
      }

      char text[400];

      for (int digits = 1; digits <= 40; digits++) {
        snprintf(text, sizeof text, "%.*f", digits, value);

        if (strtod(text, nullptr) == value) {
          return text;
        }
      }

      return unspelled(writer, "the number " + String(text));
    }

    String constantText(Writer& writer, const Expr& e) {
      switch (e.kind) {
        case ExprKind::ConstInt:
          return intText(e);
        case ExprKind::ConstFloat:
          return e.type && e.type->bits == 32 ? floatText(writer, e.floatValue) + ".toFloat()" : floatText(writer, e.floatValue);
        case ExprKind::ConstBool:
          return e.boolValue ? "true" : "false";
        default:
          return quoted(e.text);
      }
    }

    // ── operators ──

    bool isOperator(const Expr& e) {
      const bool written = e.kind == ExprKind::CallBuiltin && (e.builtin == Builtin::Concat || e.builtin == Builtin::StrEq);

      return e.kind == ExprKind::Binary || written;
    }

    const OperatorForm& formOf(const Expr& e) {
      if (e.kind == ExprKind::CallBuiltin) {
        return e.builtin == Builtin::Concat ? kJoin : kStringEquals;
      }

      for (const OperatorForm& form : kOperators) {
        if (form.op == e.binaryOp) {
          return form;
        }
      }

      return kOperators[0];
    }

    bool isNegative(const Expr& e) {
      return (e.kind == ExprKind::ConstInt && e.intValue < 0) || (e.kind == ExprKind::ConstFloat && e.floatValue < 0.0);
    }

    // A read of a variable an `if` value stands in for. The `if` reaches as far as an expression goes, so inside more it is bracketed.
    bool isIfValue(const Writer& writer, const Expr& e) {
      return (e.kind == ExprKind::Local || e.kind == ExprKind::LoadSlot) && writer.inlined.count(e.name) > 0;
    }

    // A value in front of a `.` or after a prefix operator: anything that is not one piece goes in parentheses.
    String tight(Writer& writer, const Expr& e) {
      const bool loose = isOperator(e) || e.kind == ExprKind::Unary || isNegative(e) || e.kind == ExprKind::ClosureLit || isIfValue(writer, e);
      const String text = exprText(writer, e);

      return loose ? "(" + text + ")" : text;
    }

    // An operand of an operator, in parentheses when it binds more loosely (or as loosely, on the right: they group to the left).
    String operandText(Writer& writer, const Expr& operand, int precedence, bool isRight) {
      const String text = exprText(writer, operand);

      if (!isOperator(operand)) {
        return isIfValue(writer, operand) ? "(" + text + ")" : text;
      }

      const int inner = formOf(operand).precedence;

      return inner < precedence || (isRight && inner == precedence) ? "(" + text + ")" : text;
    }

    String operatorText(Writer& writer, const Expr& e) {
      const OperatorForm& form = formOf(e);
      const String left = operandText(writer, *e.operands[0], form.precedence, false);

      return left + " " + form.text + " " + operandText(writer, *e.operands[1], form.precedence, true);
    }

    // `-x`, `!x`, `~x`; the negation of text equality is written `a != b`.
    String unaryText(Writer& writer, const Expr& e) {
      const Expr& operand = *e.operands[0];
      const bool unequal = e.unaryOp == UnOp::LogNot && operand.kind == ExprKind::CallBuiltin && operand.builtin == Builtin::StrEq;

      if (unequal) {
        return exprText(writer, *operand.operands[0]) + " != " + exprText(writer, *operand.operands[1]);
      }

      const char* sign = e.unaryOp == UnOp::Neg ? "-" : e.unaryOp == UnOp::LogNot ? "!" : "~";

      return sign + tight(writer, operand);
    }

    // `x.toI64()`; a cast to a type no cast makes has no spelling.
    String castText(Writer& writer, const Expr& e) {
      const String name = castName(e.type);

      if (name.empty()) {
        return unspelled(writer, "a cast to " + TypeText(e.type));
      }

      return tight(writer, *e.operands[0]) + "." + name + "()";
    }

    // ── reads and calls ──

    String exprsText(Writer& writer, const Vector<ExprPtr>& exprs, size_t from) {
      Vector<String> parts;

      for (size_t i = from; i < exprs.size(); i++) {
        parts.push_back(exprText(writer, *exprs[i]));
      }

      return join(parts, ", ");
    }

    // A local, a `var` read or a global. Inside a walk the slot it counts is not a name the source has.
    String nameText(Writer& writer, const Expr& e) {
      if (const auto inlined = writer.inlined.find(e.name); inlined != writer.inlined.end()) {
        return inlined->second;
      }

      for (const Walk& walk : writer.walks) {
        if (e.name == walk.slot) {
          return unspelled(writer, "a map's or set's slot used outside assocKey");
        }
      }

      return nameOf(e.name);
    }

    String fieldName(const StructDef* def, int index) {
      const bool known = def != nullptr && index >= 0 && static_cast<size_t>(index) < def->fields.size();

      return known ? nameOf(def->fields[static_cast<size_t>(index)].name) : "#" + to_string(index);
    }

    String fieldText(Writer& writer, const Expr& e) {
      const TypePtr& type = e.operands[0]->type;
      const StructDef* def = type && type->kind == TypeKind::Struct ? AnyStruct(*writer.module, type->name) : nullptr;

      return tight(writer, *e.operands[0]) + "." + fieldName(def, e.fieldIndex);
    }

    // `Point(x: 1, y: 2)`.
    String structText(Writer& writer, const Expr& e) {
      const StructDef* def = AnyStruct(*writer.module, e.name);
      Vector<String> parts;

      for (size_t i = 0; i < e.operands.size(); i++) {
        parts.push_back(fieldName(def, e.fieldIndices[i]) + ": " + exprText(writer, *e.operands[i]));
      }

      return declName(writer, e.name) + "(" + join(parts, ", ") + ")";
    }

    // `f(a, b)`; a method is called with a dot, `p.sum()`, its receiver in front.
    String callText(Writer& writer, const Expr& e) {
      const auto method = writer.methods.find(e.name);

      if (method == writer.methods.end() || e.operands.empty()) {
        return declName(writer, e.name) + "(" + exprsText(writer, e.operands, 0) + ")";
      }

      return tight(writer, *e.operands[0]) + "." + method->second + "(" + exprsText(writer, e.operands, 1) + ")";
    }

    // A closure is called through the name that holds it: .poly has no call of any other expression.
    String indirectText(Writer& writer, const Expr& e) {
      const Expr& callee = *e.operands[0];

      if (callee.kind != ExprKind::Local && callee.kind != ExprKind::LoadSlot) {
        return unspelled(writer, "a call of a closure that is not held in a name");
      }

      return nameText(writer, callee) + "(" + exprsText(writer, e.operands, 1) + ")";
    }

    // ── builtins ──

    const BuiltinForm* builtinForm(Builtin op) {
      for (const BuiltinForm& form : kBuiltins) {
        if (form.op == op) {
          return &form;
        }
      }

      return nullptr;
    }

    // `len(xs)` or `m.has(k)`.
    String formText(Writer& writer, const BuiltinForm& form, const Expr& e) {
      if (e.builtin == Builtin::Push && !ListValues(e).empty()) {
        return "[" + exprsText(writer, ListValues(e), 0) + "]";
      }

      if (e.builtin == Builtin::Copy && IsOtherKind(e)) {
        return tight(writer, *e.operands[0]) + (e.type->isMutable ? ".thaw()" : ".freeze()");
      }

      if (!form.isMethod) {
        return String(form.name) + "(" + exprsText(writer, e.operands, 0) + ")";
      }

      return tight(writer, *e.operands[0]) + "." + form.name + "(" + exprsText(writer, e.operands, 1) + ")";
    }

    // The walk whose table and slot `e`'s two operands read, or null.
    const Walk* walkOf(const Writer& writer, const Expr& e) {
      for (const Walk& walk : writer.walks) {
        const bool isTable = e.operands.size() == 2 && e.operands[0]->name == walk.table;

        if (isTable && e.operands[1]->name == walk.slot) {
          return &walk;
        }
      }

      return nullptr;
    }

    // Inside `for (key in table)`, the key at the slot is `key` and the value at it `table[key]`.
    String slotRead(Writer& writer, const Expr& e) {
      const Walk* walk = walkOf(writer, e);

      if (walk == nullptr) {
        return unspelled(writer, "a map's or set's slot outside a for loop over it");
      }

      return e.builtin == Builtin::AssocKey ? nameOf(walk->key) : nameOf(walk->table) + "[" + nameOf(walk->key) + "]";
    }

    // A collection seen as the other kind: `freeze()` and `thaw()` copy, so only a retype of a copy has a spelling.
    String retypeText(Writer& writer, const Expr& e) {
      const Expr& operand = *e.operands[0];

      if (operand.kind != ExprKind::CallBuiltin || operand.builtin != Builtin::Copy || !e.type) {
        return unspelled(writer, "a collection seen as the other kind without a copy");
      }

      return tight(writer, *operand.operands[0]) + (e.type->isMutable ? ".thaw()" : ".freeze()");
    }

    // `shape.area(2.0)`: the method's name, found from its slot in the interface's table.
    String interfaceCallText(Writer& writer, const Expr& e) {
      const TypePtr& type = e.operands[0]->type;
      const InterfaceDef* def = type ? FindInterface(*writer.module, type->name) : nullptr;
      const int64_t slot = e.operands[1]->intValue;

      if (def == nullptr || slot < 0 || static_cast<size_t>(slot) >= def->methods.size()) {
        return unspelled(writer, "an interface call through an unknown slot");
      }

      return tight(writer, *e.operands[0]) + "." + nameOf(def->methods[static_cast<size_t>(slot)].name) + "(" + exprsText(writer, e.operands, 2) + ")";
    }

    // A map's or set's capacity has no spelling: .poly reads its slots only in `for (k in m)`.
    String capacityText(Writer& writer, const Expr& e) {
      const TypePtr& type = e.operands[0]->type;

      if (type && (type->kind == TypeKind::Hashmap || type->kind == TypeKind::Set)) {
        return unspelled(writer, "a map's or set's capacity outside a for loop over it");
      }

      return tight(writer, *e.operands[0]) + ".capacity()";
    }

    String tableBuiltinText(Writer& writer, const Expr& e) {
      switch (e.builtin) {
        case Builtin::Capacity:
          return capacityText(writer, e);
        case Builtin::AssocKey:
        case Builtin::AssocValue:
          return slotRead(writer, e);
        case Builtin::Retype:
          return retypeText(writer, e);
        default:
          return unspelled(writer, String("the builtin ") + BuiltinName(e.builtin));
      }
    }

    // One piece of `"n = ${x}"`: literal text escaped, `str(x)` as a hole. Empty when the piece is neither, or when its
    // expression has a brace of its own (a hole ends at the first `}`).
    String pieceText(Writer& writer, const Expr& piece) {
      if (piece.kind == ExprKind::ConstString) {
        const String text = quoted(piece.text);

        return text.substr(1, text.size() - 2);
      }

      const String inner = IsTextOf(piece) ? exprText(writer, *piece.operands[0]) : String("{");

      return inner.find_first_of("{}") == String::npos ? "${" + inner + "}" : "";
    }

    // Text joined from literals and `str(x)` is written as .poly's own interpolation makes it: `"sum ${a + b}!"`.
    String interpolated(Writer& writer, const Expr& e) {
      const Vector<const Expr*> pieces = JoinedPieces(e);
      String out = "\"";

      for (const Expr* piece : pieces) {
        const String text = IsInterpolation(pieces) ? pieceText(writer, *piece) : "";

        if (text.empty() && !(piece->kind == ExprKind::ConstString && piece->text.empty())) {
          return operatorText(writer, e);
        }

        out += text;
      }

      return out + "\"";
    }

    String builtinText(Writer& writer, const Expr& e) {
      switch (e.builtin) {
        case Builtin::Concat:
          return interpolated(writer, e);
        case Builtin::StrEq:
          return operatorText(writer, e);
        case Builtin::CharAt:
          return tight(writer, *e.operands[0]) + "[" + exprText(writer, *e.operands[1]) + "]";
        case Builtin::InterfaceCall:
          return interfaceCallText(writer, e);
        case Builtin::InterfaceBox:
          return exprText(writer, *e.operands[0]);  // the front end boxes the struct where the interface is expected
        default:
          return builtinForm(e.builtin) != nullptr ? formText(writer, *builtinForm(e.builtin), e) : tableBuiltinText(writer, e);
      }
    }

    // ── values made ──

    // `mutmap<K, V>(4)`: a new collection, with the size its table starts at. The front end's own choice is left out: 16 for a
    // collection that changes, and none for one that cannot.
    String madeCollection(Writer& writer, const TypePtr& type, const Vector<ExprPtr>& sizes) {
      if (!type || !IsCollection(*type)) {
        return unspelled(writer, "a new " + TypeText(type));
      }

      const bool chosen = type->isMutable && !sizes.empty() && !IsConstantInt(sizes[0], 16);

      return collectionText(writer, *type, type->isMutable) + "(" + (chosen ? exprsText(writer, sizes, 0) : "") + ")";
    }

    // A list literal; one with no elements is a new, empty list.
    String vecText(Writer& writer, const Expr& e) {
      if (!e.type || e.type->kind != TypeKind::DynamicArray) {
        return unspelled(writer, "a vec literal");
      }

      return e.operands.empty() ? madeCollection(writer, e.type, {}) : "[" + exprsText(writer, e.operands, 0) + "]";
    }

    // `Some<T>(x)`, `None<T>()`, `eitherOk<A, B>(x)`, `eitherErr<A, B>(e)`.
    String eitherText(Writer& writer, const Expr& e) {
      const bool isOptional = e.type && e.type->kind == TypeKind::Optional;

      if (isOptional) {
        const String some = wrapped(writer, "Some", {e.type->element}) + "(" + exprText(writer, *e.operands[0]) + ")";

        return e.boolValue ? some : wrapped(writer, "None", {e.type->element}) + "()";
      }

      const String made = wrapped(writer, e.boolValue ? "eitherOk" : "eitherErr", {e.type->element, e.type->second});

      return made + "(" + exprText(writer, *e.operands[0]) + ")";
    }

    // The lines `body` makes, one level deeper than the current line, as one piece of text.
    String nestedText(Writer& writer, const StmtList& body) {
      Vector<String> outer = move(writer.lines);
      writer.lines.clear();
      bodyLines(writer, body);
      const String text = join(writer.lines, "\n");
      writer.lines = move(outer);

      return text;
    }

    // `closure(n: i64) -> i64 { ... }`: it captures what it uses, so the HIR's list of captures is not written.
    String closureText(Writer& writer, const Expr& e) {
      Vector<String> parameters;

      for (size_t i = 0; i < e.parameterNames.size(); i++) {
        parameters.push_back(nameOf(e.parameterNames[i]) + ": " + typeText(writer, e.parameterTypes[i]));
      }

      const String head = "closure(" + join(parameters, ", ") + ") -> " + typeText(writer, e.type ? e.type->element : nullptr) + " {\n";

      return head + nestedText(writer, e.closureBody) + "\n" + String(static_cast<size_t>(writer.depth) * 2, ' ') + "}";
    }

    String madeText(Writer& writer, const Expr& e) {
      switch (e.kind) {
        case ExprKind::VecLit:
          return vecText(writer, e);
        case ExprKind::NewContainer:
          return madeCollection(writer, e.type, e.operands);
        case ExprKind::VariantLit:
          return typeText(writer, e.type) + "(" + to_string(e.fieldIndex) + ", " + exprText(writer, *e.operands[0]) + ")";
        case ExprKind::EitherLit:
          return eitherText(writer, e);
        default:
          return e.kind == ExprKind::ClosureLit ? closureText(writer, e) : unspelled(writer, e.text.empty() ? "an expression" : e.text);
      }
    }

    String accessText(Writer& writer, const Expr& e) {
      switch (e.kind) {
        case ExprKind::Call:
          return callText(writer, e);
        case ExprKind::CallIndirect:
          return indirectText(writer, e);
        case ExprKind::CallBuiltin:
          return builtinText(writer, e);
        case ExprKind::FieldGet:
          return fieldText(writer, e);
        case ExprKind::IndexGet:
          return tight(writer, *e.operands[0]) + "[" + exprsText(writer, e.operands, 1) + "]";
        default:
          return e.kind == ExprKind::StructLit ? structText(writer, e) : madeText(writer, e);
      }
    }

    String readText(Writer& writer, const Expr& e) {
      switch (e.kind) {
        case ExprKind::Local:
        case ExprKind::LoadSlot:
        case ExprKind::Global:
          return nameText(writer, e);
        case ExprKind::FuncRef:
          return declName(writer, e.name);
        default:
          return accessText(writer, e);
      }
    }

    String exprText(Writer& writer, const Expr& e) {
      if (IsLiteral(e)) {
        return constantText(writer, e);
      }

      switch (e.kind) {
        case ExprKind::Binary:
          return operatorText(writer, e);
        case ExprKind::Unary:
          return unaryText(writer, e);
        case ExprKind::Cast:
          return castText(writer, e);
        case ExprKind::Coerce:
          return exprText(writer, *e.operands[0]);  // an integer-width fix the front end makes again
        default:
          return readText(writer, e);
      }
    }

    // ── walks over a table's slots, written `for (k in m)` ──

    // `for (x in source) { ... }`: the element's `let` is the loop's own, and so is the index the statements after it leave out.
    void elementLoopLines(Writer& writer, const Stmt& element, const String& source, const StmtList& body) {
      addLine(writer, "for (" + nameOf(element.name) + " in " + source + ") {");
      bodyLines(writer, body);
      addLine(writer, "}");
    }

    void listLoopLines(Writer& writer, const Stmt& held, const Stmt& loop) {
      const StmtList body(loop.body.begin() + 1, loop.body.end() - 1);
      elementLoopLines(writer, *loop.body[0], exprText(writer, *held.value), body);
    }

    void countedListLines(Writer& writer, const Stmt& loop, const String& source) {
      elementLoopLines(writer, *loop.body[0], source, StmtList(loop.body.begin() + 1, loop.body.end()));
    }

    void walkLines(Writer& writer, const WalkShape& walk) {
      const String key = KeyName(walk);
      const bool named = key != walk.slot + "Key";
      const String source = walk.source ? exprText(writer, *walk.source) : nameOf(walk.table);
      addLine(writer, "for (" + nameOf(key) + " in " + source + ") {");

      writer.walks.push_back(Walk{walk.table, walk.slot, key});
      bodyLines(writer, named ? StmtList(walk.inside->begin() + 1, walk.inside->end()) : *walk.inside);
      writer.walks.pop_back();
      addLine(writer, "}");
    }

    // ── `if` values: a variable an `if` fills branch by branch, and an `if` whose branches only return ──

    // `if (c) a else if (d) b else e`.
    String ifText(Writer& writer, const IfValue& value) {
      String text;

      for (const auto& [test, arm] : value.arms) {
        text += "if (" + exprText(writer, *test) + ") " + exprText(writer, *arm) + " else ";
      }

      return text + exprText(writer, *value.otherwise);
    }

    // `let n: T = if (c) a else b` (or `var`, when n changes later), from a `var` filled branch by branch.
    void declaredLine(Writer& writer, const DeclaredIf& declared) {
      const Stmt& slot = *declared.slot;
      addLine(writer, (declared.changes ? "var " : "let ") + nameOf(slot.name) + ": " + typeText(writer, slot.type) + " = " + ifText(writer, declared.value));
    }

    // `return if (c) a else b`; a hidden variable an `if` fills, written where it is read; then a declared `if` value. 0 when none.
    size_t ifValueAt(Writer& writer, const StmtList& body, size_t i) {
      if (const Optional<IfValue> value = ReturnIfOf(*body[i])) {
        addLine(writer, "return " + ifText(writer, *value));

        return 1;
      }

      if (const Optional<IfValue> value = InlinedAt(body, i)) {
        writer.inlined[body[i]->name] = ifText(writer, *value);

        return 2;
      }

      const Optional<DeclaredIf> declared = body[i]->kind == StmtKind::Var ? DeclaredAt(body, i) : nullopt;

      if (declared) {
        declaredLine(writer, *declared);
      }

      return declared ? 2 : 0;
    }

    // ── statements ──

    void stmtLines(Writer& writer, const Stmt& s);

    // `let x: T = v`, or `val x: T = a` when v is a copy of a.
    String letText(Writer& writer, const Stmt& s) {
      if (IsPlainCopy(s.value)) {
        return "val " + nameOf(s.name) + ": " + typeText(writer, s.type) + " = " + exprText(writer, *s.value->operands[0]);
      }

      return "let " + nameOf(s.name) + ": " + typeText(writer, s.type) + " = " + exprText(writer, *s.value);
    }

    // A `var` the function never gives a new value is a `let`. The front end keeps a `let` of a list, map or set in a `var` slot.
    String varText(Writer& writer, const Stmt& s) {
      if (s.value && writer.function != nullptr && !AssignsIn(*writer.function, s.name)) {
        return letText(writer, s);
      }

      const String head = "var " + nameOf(s.name) + ": " + typeText(writer, s.type);

      return s.value ? head + " = " + exprText(writer, *s.value) : head;
    }

    // `if (c) {`, then `} else if (d) {` for an else that is only another if, and `} else {` for any other else.
    void ifLines(Writer& writer, const Stmt& s, const String& lead) {
      addLine(writer, lead + "if (" + exprText(writer, *s.value) + ") {");
      bodyLines(writer, s.body);

      if (s.elseBody.empty()) {
        return addLine(writer, "}");
      }

      if (s.elseBody.size() == 1 && s.elseBody[0]->kind == StmtKind::If) {
        return ifLines(writer, *s.elseBody[0], "} else ");
      }

      addLine(writer, "} else {");
      bodyLines(writer, s.elseBody);
      addLine(writer, "}");
    }

    // `for (i = 0; i < n; i = i + 1) {`: each counter, the test, and each counter's next value; `for (; c; )` has no counters.
    void forLines(Writer& writer, const Stmt& s) {
      Vector<String> starts;
      Vector<String> steps;

      for (size_t i = 0; i < s.names.size(); i++) {
        starts.push_back(nameOf(s.names[i]) + " = " + exprText(writer, *s.initializers[i]));
        steps.push_back(nameOf(s.names[i]) + " = " + exprText(writer, *s.steps[i]));
      }

      addLine(writer, "for (" + join(starts, ", ") + "; " + exprText(writer, *s.value) + "; " + join(steps, ", ") + ") {");
      bodyLines(writer, s.body);
      addLine(writer, "}");
    }

    // ── switch ──

    // A sum case as a `switch` arm writes it: `Some(n)`, `None`, `Ok(v)`, `Err(e)`, `variant(2, s)` (`_` for no payload).
    String caseText(const Type& sum, int64_t number, const String& name) {
      const String payload = name.empty() ? String("_") : nameOf(name);

      switch (sum.kind) {
        case TypeKind::Optional:
          return number == 0 ? "Some(" + payload + ")" : String("None");
        case TypeKind::Either:
          return (number == 0 ? "Ok(" : "Err(") + payload + ")";
        default:
          return "variant(" + to_string(number) + ", " + payload + ")";
      }
    }

    bool isSumType(const Type& type) {
      return type.kind == TypeKind::Optional || type.kind == TypeKind::Either || type.kind == TypeKind::Variant;
    }

    // One arm's keys, joined by `|`.
    String keysText(Writer& writer, const Stmt& s, size_t arm) {
      Vector<String> keys;

      for (const ExprPtr& key : s.caseKeys[arm]) {
        keys.push_back(isSumType(*s.value->type) ? caseText(*s.value->type, key->intValue, s.names[arm]) : exprText(writer, *key));
      }

      return join(keys, " | ");
    }

    // The arm's block without the `let` that binds its payload, which `Some(n)` says.
    StmtList armBlock(const Stmt& s, size_t arm) {
      const StmtList& body = s.caseBodies[arm];
      const bool binds = !s.names[arm].empty() && !body.empty() && body[0]->name == s.names[arm];

      return binds ? StmtList(body.begin() + 1, body.end()) : body;
    }

    // `head => { ... }`, on one line when the block is one short line.
    void armLines(Writer& writer, const String& head, const StmtList& body) {
      const size_t mark = writer.lines.size();
      addLine(writer, head + " => {");
      bodyLines(writer, body);
      const bool oneLine = writer.lines.size() == mark + 2 && writer.lines.back().size() < 90;

      if (oneLine) {
        const String inner = writer.lines.back().substr(writer.lines.back().find_first_not_of(' '));
        writer.lines.resize(mark);

        return addLine(writer, head + " => { " + inner + " }");
      }

      addLine(writer, "}");
    }

    // `switch (value) { keys => { ... } ... else => { ... } }`: the structured switch, an arm per line.
    void selectLines(Writer& writer, const Stmt& s) {
      addLine(writer, "switch (" + exprText(writer, *s.value) + ") {");
      writer.depth += 1;

      for (size_t arm = 0; arm < s.caseKeys.size(); arm++) {
        armLines(writer, keysText(writer, s, arm), armBlock(s, arm));
      }

      if (!s.elseBody.empty()) {
        armLines(writer, "else", s.elseBody);
      }

      writer.depth -= 1;
      addLine(writer, "}");
    }

    // `try { ... } catch (e) { ... }`, or `cleanup` for the handler that runs on the way out.
    void tryLines(Writer& writer, const Stmt& s) {
      addLine(writer, "try {");
      bodyLines(writer, s.body);
      addLine(writer, s.name.empty() ? String("} cleanup {") : "} catch (" + nameOf(s.name) + ") {");
      bodyLines(writer, s.elseBody);
      addLine(writer, "}");
    }

    // A bare `return` carries a void constant, or nothing.
    String returnText(Writer& writer, const Stmt& s) {
      const bool bare = !s.value || (s.value->kind == ExprKind::ConstInt && s.value->type && s.value->type->kind == TypeKind::Void);

      return bare ? String("return") : "return " + exprText(writer, *s.value);
    }

    String jumpText(Writer& writer, const String& label, const Vector<ExprPtr>& arguments) {
      return nameOf(label) + "(" + exprsText(writer, arguments, 0) + ")";
    }

    String labelText(const Writer& writer, const Stmt& s) {
      Vector<String> parameters;

      for (size_t i = 0; i < s.parameterNames.size(); i++) {
        parameters.push_back(nameOf(s.parameterNames[i]) + ": " + typeText(writer, s.parameterTypes[i]));
      }

      return "label " + nameOf(s.name) + "(" + join(parameters, ", ") + "):";
    }

    String flowText(Writer& writer, const Stmt& s) {
      switch (s.kind) {
        case StmtKind::Label:
          return labelText(writer, s);
        case StmtKind::Goto:
          return "goto " + jumpText(writer, s.name, s.arguments);
        case StmtKind::Branch:
          return "branch " + exprText(writer, *s.value) + " ? " + jumpText(writer, s.name, s.arguments) + " : " + jumpText(writer, s.elseName, s.elseArguments);
        case StmtKind::Throw:
        case StmtKind::Yield:
          return (s.kind == StmtKind::Throw ? "throw " : "yield ") + exprText(writer, *s.value);
        default:
          return unspelled(writer, "a switch");
      }
    }

    String simpleText(Writer& writer, const Stmt& s) {
      switch (s.kind) {
        case StmtKind::Let:
          return letText(writer, s);
        case StmtKind::Var:
          return varText(writer, s);
        case StmtKind::Assign:
          return exprText(writer, *s.target) + " = " + exprText(writer, *s.value);
        case StmtKind::Return:
          return returnText(writer, s);
        default:
          return s.kind == StmtKind::ExprStmt ? exprText(writer, *s.value) : flowText(writer, s);
      }
    }

    void stmtLines(Writer& writer, const Stmt& s) {
      switch (s.kind) {
        case StmtKind::If:
          return ifLines(writer, s, "");
        case StmtKind::For:
          return forLines(writer, s);
        case StmtKind::Try:
          return tryLines(writer, s);
        case StmtKind::Select:
          return selectLines(writer, s);
        default:
          return addLine(writer, simpleText(writer, s));
      }
    }

    // A hidden `let` holding what the loop after it walks, read nowhere else: the two are written `for (x in value)`. The
    // statements taken, or 0.
    size_t heldLoopAt(Writer& writer, const StmtList& body, size_t i) {
      const Optional<HeldLoop> held = HeldLoopAt(body, i);

      if (held && held->walk) {
        walkLines(writer, *held->walk);
      } else if (held) {
        countedListLines(writer, *held->loop, exprText(writer, *held->held->value));
      }

      return held ? 2 : 0;
    }

    // The front end's loop over a list (3 statements), or a hidden `let` and the loop over it (2); 0 when neither is there.
    size_t loopAt(Writer& writer, const StmtList& body, size_t i) {
      if (const Stmt* loop = ListLoopAt(body, i)) {
        listLoopLines(writer, *body[i], *loop);

        return 3;
      }

      return heldLoopAt(writer, body, i);
    }

    // A loop over a list or a walk over a table's slots takes the statements it is made of; any other statement is one.
    size_t stmtsAt(Writer& writer, const StmtList& body, size_t i) {
      if (const size_t taken = loopAt(writer, body, i)) {
        return taken;
      }

      if (const size_t taken = ifValueAt(writer, body, i)) {
        return taken;
      }

      if (const String list = CountedList(*body[i]); !list.empty()) {
        countedListLines(writer, *body[i], nameOf(list));

        return 1;
      }

      if (const Optional<WalkShape> walk = WalkAt(body, i)) {
        walkLines(writer, *walk);

        return walk->count;
      }

      stmtLines(writer, *body[i]);

      return 1;
    }

    void bodyLines(Writer& writer, const StmtList& body) {
      writer.depth += 1;
      size_t i = 0;

      while (i < body.size()) {
        i += stmtsAt(writer, body, i);
      }

      writer.depth -= 1;
    }

    // ── declarations ──

    // `func name(a: i64) -> i64`; a method leaves out its `this` (`from` 1); a coroutine is `coro func`, written with what it yields.
    String signature(const Writer& writer, const FuncDef& def, const String& name, size_t from) {
      Vector<String> parameters;

      for (size_t i = from; i < def.parameters.size(); i++) {
        parameters.push_back(nameOf(def.parameters[i].name) + ": " + typeText(writer, def.parameters[i].type));
      }

      const bool yields = def.isCoroutine && def.result && def.result->kind == TypeKind::Coroutine;
      const String marks = String(def.internal ? "internal " : "") + (def.outer.empty() ? "" : "static ");
      const String head = marks + (def.isCoroutine ? "coro func " : "func ");

      return head + name + "(" + join(parameters, ", ") + ") -> " + typeText(writer, yields ? def.result->element : def.result);
    }

    // `@Doc(text: "a point")`, one line per annotation.
    void annotationLines(Writer& writer, const Vector<Annotation>& annotations) {
      for (const Annotation& annotation : annotations) {
        Vector<String> args;

        for (const Pair<String, String>& arg : annotation.args) {
          args.push_back(arg.first + ": " + arg.second);
        }

        addLine(writer, "@" + annotation.name + (args.empty() ? "" : "(" + join(args, ", ") + ")"));
      }
    }

    void functionLines(Writer& writer, const FuncDef& def, const String& name, size_t from) {
      if (def.stage != KernelStage::None) {
        return addLine(writer, unspelled(writer, "the kernel " + def.name));
      }

      annotationLines(writer, def.annotations);
      addLine(writer, signature(writer, def, name, from) + " {");

      writer.function = &def.body;
      bodyLines(writer, def.body);
      writer.function = nullptr;
      addLine(writer, "}");
    }

    // `: Shape, Named`: the interfaces that list the struct among their implementers.
    String implementsText(const Writer& writer, const Module& module, const StructDef& def) {
      Vector<String> names;

      for (const InterfaceDef& interface : module.interfaces) {
        if (Implements(module, def, interface)) {
          names.push_back(declName(writer, interface.name));
        }
      }

      return names.empty() ? "" : " : " + join(names, ", ");
    }

    // `struct Point(x: i64, var y: i64) : Shape`; a struct with no fields is `struct Empty`.
    String structHead(Writer& writer, const Module& module, const StructDef& def) {
      Vector<String> fields;

      for (const Field& field : def.fields) {
        const String initial = field.initial ? " = " + exprText(writer, *field.initial) : "";
        fields.push_back((field.isMutable ? "var " : "") + nameOf(field.name) + ": " + typeText(writer, field.type) + initial);
      }

      const String head = String(def.internal ? "internal " : "") + "struct " + declName(writer, def.name) + paramsText(def.generic);

      return head + (fields.empty() ? "" : "(" + join(fields, ", ") + ")") + implementsText(writer, module, def);
    }

    void structLines(Writer& writer, const StructDef& def);

    // A struct's body: its static functions, its methods, then the structs declared in it. Names are written as they read from the struct.
    void structBody(Writer& writer, const StructDef& def, const Vector<const FuncDef*>& statics, const Vector<const FuncDef*>& methods) {
      const String around = writer.inModule;
      writer.inModule = def.name;
      writer.depth += 1;

      for (const FuncDef* function : statics) {
        functionLines(writer, *function, declName(writer, function->name), 0);
      }

      for (const FuncDef* method : methods) {
        functionLines(writer, *method, nameOf(MethodName(*method, def)), 1);
      }

      for (const StructDef* nested : NestedIn(*writer.module, def)) {
        structLines(writer, *nested);
      }

      writer.depth -= 1;
      writer.inModule = around;
    }

    // The struct, with a body when it has static functions, methods or structs of its own.
    void structLines(Writer& writer, const StructDef& def) {
      const Vector<const FuncDef*> methods = MethodsOf(*writer.module, def);
      const Vector<const FuncDef*> statics = StaticsOf(*writer.module, def);
      const bool hasBody = !methods.empty() || !statics.empty() || !NestedIn(*writer.module, def).empty();

      annotationLines(writer, def.annotations);
      addLine(writer, structHead(writer, *writer.module, def) + (hasBody ? " {" : ""));
      structBody(writer, def, statics, methods);

      if (hasBody) {
        addLine(writer, "}");
      }
    }

    // `interface Shape { func area() -> double }`: the receiver is left out of each method, and the parameters are numbered.
    void interfaceLines(Writer& writer, const InterfaceDef& def) {
      annotationLines(writer, def.annotations);
      addLine(writer, String(def.internal ? "internal " : "") + "interface " + declName(writer, def.name) + " {");

      for (const InterfaceMethod& method : def.methods) {
        Vector<String> parameters;

        for (size_t i = 0; i < method.parameters.size(); i++) {
          parameters.push_back("p" + to_string(i) + ": " + typeText(writer, method.parameters[i]));
        }

        addLine(writer, "  func " + nameOf(method.name) + "(" + join(parameters, ", ") + ") -> " + typeText(writer, method.result));
      }

      addLine(writer, "}");
    }

    // What a module can hold that .poly writes no other way: host types and bindings, globals and raw LLVM IR.
    void noteUnspelled(Writer& writer, const Module& module) {
      const bool hostParts = !module.hostTypes.empty() || !module.bridges.empty() || !module.hostMembers.empty() || !module.hostImports.empty();

      if (hostParts) {
        unspelled(writer, "host types and their bindings");
      }

      if (!module.globals.empty() || !module.rawIr.empty()) {
        unspelled(writer, module.globals.empty() ? "raw LLVM IR" : "globals");
      }
    }

    void externLines(Writer& writer, const Module& module) {
      for (const ExternDef& def : module.externs) {
        Vector<String> parameters;

        for (const Parameter& parameter : def.parameters) {
          parameters.push_back(nameOf(parameter.name) + ": " + typeText(writer, parameter.type));
        }

        addLine(writer, "extern func " + def.name + "(" + join(parameters, ", ") + ") -> " + typeText(writer, def.result));
      }
    }

    // ── which generics are written once ──

    // The generics that work for every type and have no method a dot takes, less those that use one written as its copies.
    void findWrittenGenerics(Writer& writer) {
      const Module& module = *writer.module;
      WrittenGenerics written = FindWrittenGenerics(module, [&](const StructDef& def) { return isWrittenStruct(module, def); });
      writer.genericStructs = move(written.structs);
      writer.genericFunctions = move(written.functions);
    }

    // A declaration's module, and its own name without the module (`Point` for `Geometry.Point`).
    void nameDeclaration(Writer& writer, const String& name, const String& module, const String& own) {
      writer.modules[name] = module;
      writer.shortNames[name] = own;
    }

    // A function's name as written, its module, and for a method the name its dot calls use.
    void nameFunction(Writer& writer, const FuncDef& function) {
      const StructDef* owner = OwnerOf(*writer.module, function);
      const String own = withoutModule(owner == nullptr ? writtenName(writer, function.name, function.generic) : function.name, function.module);

      if (owner == nullptr) {
        nameDeclaration(writer, function.name, function.module, isKeyword(own) || isCallForm(own) ? own + "_" : own);
      } else if (!isTakenByDot(MethodName(function, *owner))) {
        writer.methods[function.name] = nameOf(MethodName(function, *owner));
      }
    }

    void nameStruct(Writer& writer, const StructDef& def) {
      nameDeclaration(writer, def.name, def.module, nameOf(withoutModule(writtenName(writer, def.name, def.generic), def.module)));
    }

    // The name each declaration is written with, its module, and for a method the name its dot calls use; the generics' too.
    void nameFunctions(Writer& writer) {
      for (const FuncDef& function : writer.module->functions) {
        nameFunction(writer, function);
      }

      for (const FuncDef& function : writer.module->genericFunctions) {
        nameFunction(writer, function);
      }

      for (const StructDef& def : writer.module->structs) {
        nameStruct(writer, def);
      }

      for (const StructDef& def : writer.module->genericStructs) {
        nameStruct(writer, def);
      }

      for (const InterfaceDef& def : writer.module->interfaces) {
        nameDeclaration(writer, def.name, def.module, nameOf(withoutModule(def.name, def.module)));
      }
    }

    // The generic a declaration is a copy of, when that generic is written once ("" for any other).
    String copiedGeneric(const Writer& writer, const Module& module, const DeclRef& ref) {
      if (ref.kind == DeclKind::Struct) {
        const String& of = module.structs[ref.index].generic.of;

        return writer.genericStructs.count(of) > 0 ? of : "";
      }

      const bool isFunction = ref.kind == DeclKind::Function && OwnerOf(module, module.functions[ref.index]) == nullptr;

      return isFunction && writer.genericFunctions.count(module.functions[ref.index].generic.of) > 0 ? module.functions[ref.index].generic.of : "";
    }

    // Generic `of` as declared, in its module's block, the first time it is reached.
    void genericAt(Writer& writer, const String& of) {
      if (!writer.writtenGenerics.insert(of).second) {
        return;
      }

      const auto def = writer.genericStructs.find(of);
      const FuncDef* function = def == writer.genericStructs.end() ? writer.genericFunctions.at(of) : nullptr;
      enterModule(writer, function == nullptr ? def->second->module : function->module);

      if (function == nullptr) {
        structLines(writer, *def->second);
      } else {
        functionLines(writer, *function, declName(writer, function->name) + paramsText(function->generic), 0);
      }

      writer.lines.push_back("");
    }

    // One declaration, in its module's block, with a blank line after it. A copy of a generic written once stands for that generic.
    void declarationAt(Writer& writer, const Module& module, const DeclRef& ref) {
      if (const String of = copiedGeneric(writer, module, ref); !of.empty()) {
        return genericAt(writer, of);
      }

      const Optional<String> place = PlaceOf(module, ref);

      if (!place) {
        return;
      }

      enterModule(writer, *place);

      if (ref.kind == DeclKind::Struct) {
        structLines(writer, module.structs[ref.index]);
      } else if (ref.kind == DeclKind::Interface) {
        interfaceLines(writer, module.interfaces[ref.index]);
      } else {
        functionLines(writer, module.functions[ref.index], declName(writer, module.functions[ref.index].name), 0);
      }

      writer.lines.push_back("");
    }

    // The declarations in the order the module says they were written in (without one, by kind), each module's in its block.
    void allDeclarations(Writer& writer, const Module& module) {
      for (const DeclRef& ref : DeclarationOrder(module)) {
        declarationAt(writer, module, ref);
      }

      for (const auto& [of, def] : writer.genericStructs) {
        genericAt(writer, of);
      }

      for (const auto& [of, def] : writer.genericFunctions) {
        genericAt(writer, of);
      }

      enterModule(writer, "");
    }

  }  // namespace

  PolySource WritePoly(const Module& module) {
    Writer writer;
    writer.module = &module;
    findWrittenGenerics(writer);
    nameFunctions(writer);

    noteUnspelled(writer, module);
    externLines(writer, module);
    allDeclarations(writer, module);

    while (!writer.lines.empty() && writer.lines.back().empty()) {
      writer.lines.pop_back();
    }

    sort(writer.problems.begin(), writer.problems.end());
    writer.problems.erase(unique(writer.problems.begin(), writer.problems.end()), writer.problems.end());

    return PolySource{join(writer.lines, "\n") + "\n", move(writer.problems)};
  }

}  // namespace polyir
