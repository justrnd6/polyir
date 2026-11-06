// The syntax tree of a polyir program: plain data, one node kind for each alternative of the grammar in lang/src/Polyir.intent
// (the Nina front end's generated PolyirProgram / PolyirTopLevel / PolyirExpr / PolyirStmt / PolyirTypeRef). Everything the checker needs to
// reproduce the Nina checker's results is here, including the position of every token the Nina tree keeps.
//
// HOW THE TREE IS STORED
//   * A `Program` owns flat arrays (`Pool<T>`), one per node kind. A node refers to another node by `Id<T>` (a 32-bit index into the pool of T),
//     and to a list of nodes by `Range<T>` (begin + count into the pool of T). Nothing is allocated per node, nothing is shared, and a whole
//     program is freed by freeing its vectors.
//         const AstExpr& expr = program.exprs[id];                                   // one node
//         for (ExprId argId : program.exprRefs[call.args]) { const AstExpr& arg = program.exprs[argId]; }   // a list: pool[range] gives a View
//   * A name is a `Symbol` (an interned 32-bit number; `TextOf(names, symbol)` or `TextOf(program, symbol)` maps it back). Symbols from one
//     `Interner` compare with `==`; the parser uses `GlobalInterner()` unless it is given another, so two programs parsed one after the other
//     (the prelude and a user program) share symbols. The interner is NOT thread safe: parse on one thread, or give each thread its own interner.
//   * A literal keeps its text exactly as written (`"abc"` with the quotes, `'c'` with the quotes) as a `StringView` into `Program::source`, which
//     the program keeps alive (a heap string, so moving a Program never moves the text).
//   * A position is `Pos{line, column}`, 1-based, the same as the Nina tokens' spans. Line 0 means "not there" (an optional token that was absent).
//   * Where the Nina tree keeps a TOKEN (`name`, `op`, `by`, `mark`, `keyword`, a literal), the node here keeps that token's position, because the
//     Nina checker reports its errors at token positions. A node's own `pos` is always the position of the first token of the construct.
//
// WHAT IS FLATTENED, AND HOW IT MAPS BACK TO THE NINA TREE
//   * Nina's `Expr -> Unary -> Postfix{base, suffixes}` chain is one `AstExpr`: the operators are `Binary` / `Neg` / `ValCopy` / `LogNot` /
//     `BitNot` nodes, the `postfixBase` alternatives are the other kinds, and a base with one or more `.m(..)` / `.f` / `[..]` suffixes is a
//     `Postfix` node (`first` = the base, `suffixes` = the suffixes in order). A base with no suffixes is the base node itself.
//   * An identifier base with its tail is three kinds: `Name` (bare tail), `Call` (call tail) and `StructLit` (struct tail).
//   * `coro func` is an `AstFuncDecl` with `isCoroutine` set (its item kind stays `CoroutineFunc`); struct methods and top-level functions share `funcs`.
//   * `statement := target ('=' value)?` is a single `Expr` statement with `isAssign`.
#pragma once

#include "polyir/Std.hpp"
#include "polyir/Export.hpp"

namespace polyir::front {

  constexpr uint32_t kNoIndex = 0xFFFFFFFFu;

  // ── positions, names, literals ────────────────────────────────────────────────────────────────────────────────────────────────────────────

  // A place in the source: 1-based line and column; line 0 means "not there".
  struct Pos {
    uint32_t line = 0;
    uint32_t column = 0;
  };

  // An interned name. Symbol 0 is the empty string. Compare with ==.
  struct Symbol {
    uint32_t id = 0;

    bool operator==(Symbol other) const {
      return id == other.id;
    }

    bool operator!=(Symbol other) const {
      return id != other.id;
    }

    bool operator<(Symbol other) const {
      return id < other.id;
    }
  };

  // Maps names to Symbols and back. Strings are stored once, in stable storage, so the views `TextOf` returns stay valid for the interner's lifetime.
  struct POLYIR_API Interner {
    Deque<String> storage;     // one copy of each name (a deque, so a copy never moves when more are added)
    Vector<StringView> texts;  // the text of each symbol, indexed by its id
    Vector<uint32_t> slots;    // the hash table: a symbol id per slot, or kNoIndex for an empty slot
  };

  // An identifier token (IDENT): its name and where it is.
  struct Ident {
    Symbol name;
    Pos pos;
  };

  // A literal token (INT_LIT, FLOAT_LIT, STRING_LIT, CHAR_LIT) with its text as written, quotes included.
  struct Literal {
    StringView text;
    Pos pos;
  };

  bool Known(const Pos& pos);

  // Whether `wanted` is one of the names in `list`.
  bool IsOneOf(const String& wanted, const Vector<String>& list);

  // Whether `wanted` is one of the symbols in `list`.
  bool IsOneOf(Symbol wanted, const Vector<Symbol>& list);

  Symbol Intern(Interner& names, StringView text);

  StringView TextOf(const Interner& names, Symbol symbol);

  size_t Size(const Interner& names);

  Interner MakeInterner();

  Interner& GlobalInterner();  // one per process, used by ParseSource unless it is given another

  inline bool Known(const Pos& pos) {
    return pos.line != 0;
  }

  inline bool IsOneOf(const String& wanted, const Vector<String>& list) {
    for (const String& item : list) {
      if (item == wanted) {
        return true;
      }
    }

    return false;
  }

  inline bool IsOneOf(Symbol wanted, const Vector<Symbol>& list) {
    for (Symbol item : list) {
      if (item == wanted) {
        return true;
      }
    }

    return false;
  }

  inline StringView TextOf(const Interner& names, Symbol symbol) {
    return names.texts[symbol.id];
  }

  inline size_t Size(const Interner& names) {
    return names.texts.size();
  }

  // ── ids, ranges and pools ───────────────────────────────────────────────────────────────────────────────────────────────────────────────────

  // The index of a node in the pool of its kind. A template so that, say, an expression id cannot be passed where a statement id is wanted.
  template <typename T>
  struct Id {
    uint32_t index = kNoIndex;

    bool operator==(Id other) const {
      return index == other.index;
    }
  };

  // A list of nodes: `count` nodes of one kind, side by side in its pool from `begin`.
  template <typename T>
  struct Range {
    uint32_t begin = 0;
    uint32_t count = 0;
  };

  // All the nodes of one kind. `pool[id]` is a node, `pool[range]` a read-only view of a list of them.
  template <typename T>
  struct Pool {
    Vector<T> items;

    const T& operator[](Id<T> id) const {
      return items[id.index];
    }

    T& operator[](Id<T> id) {
      return items[id.index];
    }

    View<T> operator[](Range<T> range) const {
      return View<T>(items.data() + range.begin, range.count);
    }
  };

  struct AstType;
  struct AstExpr;
  struct AstStmt;
  struct AstFuncDecl;
  using TypeId = Id<AstType>;
  using ExprId = Id<AstExpr>;
  using StmtId = Id<AstStmt>;
  using FuncId = Id<AstFuncDecl>;

  template <typename T> bool Valid(const Id<T>& id);

  template <typename T> bool Empty(const Range<T>& range);

  template <typename T> size_t Size(const Pool<T>& pool);

  template <typename T> Id<T> Add(Pool<T>& pool, const T& value);

  template <typename T> Range<T> Take(Pool<T>& pool, Vector<T>& scratch, size_t mark);

  template <typename T> inline bool Valid(const Id<T>& id) {
    return id.index != kNoIndex;
  }

  template <typename T> inline bool Empty(const Range<T>& range) {
    return range.count == 0;
  }

  template <typename T> inline size_t Size(const Pool<T>& pool) {
    return pool.items.size();
  }

  // Appends a node and answers its id.
  template <typename T> inline Id<T> Add(Pool<T>& pool, const T& value) {
    pool.items.push_back(value);

    return Id<T>{static_cast<uint32_t>(pool.items.size() - 1)};
  }

  // Moves the last `scratch.size() - mark` elements of a scratch stack into the pool as one list. The parser builds a list on a scratch stack
  // because the nodes of nested lists interleave in the pool.
  template <typename T> inline Range<T> Take(Pool<T>& pool, Vector<T>& scratch, size_t mark) {
    Range<T> range{static_cast<uint32_t>(pool.items.size()), static_cast<uint32_t>(scratch.size() - mark)};
    pool.items.insert(pool.items.end(), scratch.begin() + mark, scratch.end());
    scratch.resize(mark);

    return range;
  }

  // ── types (typeRef) ─────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────

  enum class AstTypeKind : uint8_t {
    Ptr,       // ptr<first>
    Vec,       // vec<first, sizes[0]>
    Array,     // array<first>
    Mutarray,  // mutarray<first>
    Buffer,    // buffer<first, sizes[0]>
    Grid,      // grid<first, sizes[0] rows, sizes[1] cols>
    Tensor,    // tensor<first, sizes...>   (one or more dimensions)
    Sparse,    // sparse<first>
    Map,       // map<first key, second value>
    Mutmap,    // mutmap<first key, second value>
    Set,       // set<first>
    Mutset,    // mutset<first>
    Func,      // func(types...) -> first         (a bare function pointer)
    Closure,   // closure(types...) -> first
    Variant,   // variant<types...>
    Either,    // either<first ok, second err>
    Optional,  // optional<first>
    Coroutine, // coro<first>                     (first = the yield type)
    Generic,   // name<types...>                  (an instance of a user generic struct)
    Named      // name or name[sizes[0]]          (a struct, interface, host type, scalar, type parameter; `[N]` is an array)
  };

  struct AstType {
    AstTypeKind kind = AstTypeKind::Named;
    Pos pos;
    Ident name;               // Generic, Named
    TypeId first;             // the element / inner / key / ok / return type (see AstTypeKind)
    TypeId second;            // Map, Mutmap: the value type; Either: the err type
    Range<TypeId> types;      // Func, Closure: parameter types; Variant: the cases; Generic: the type arguments
    Range<Literal> sizes;     // the INT_LITs of Vec, Buffer, Grid, Tensor and Named
  };

  // ── expressions ─────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────

  // Binary operators in the grammar's own spellings. (`Identical` is `===`, `NotIdentical` is `!==`.)
  enum class Operator : uint8_t {
    Mul, Div, Rem, Add, Sub, Shl, Shr, BitAnd, BitXor, BitOr, Eq, Ne, Lt, Le, Gt, Ge, Identical, NotIdentical, LogAnd, LogOr
  };

  enum class AstExprKind : uint8_t {
    Binary,      // first op second                                (op, opPos)
    Neg,         // -first
    ValCopy,     // val first
    LogNot,      // !first
    BitNot,      // ~first
    Postfix,     // first suffixes...                              (first = the base; suffixes = at least one)
    Name,        // name                                           (a bare identifier)
    Call,        // name(args...)
    StructLit,   // name { inits... }
    Paren,       // (first)
    Int,         // literal
    Float,       // literal
    String,      // literal (with the quotes)
    Char,        // literal (with the quotes)
    True,
    False,
    VecLit,      // vec[args...]
    NewMutarray, // mutarray<type>()
    NewArray,    // array<type>()
    NewBuffer,   // buffer<type, literal>()
    NewSparse,   // sparse<type>()
    NewMap,      // map<type, type2>()
    NewMutmap,   // mutmap<type, type2>(first capacity)
    NewSet,      // set<type>()
    NewMutset,   // mutset<type>(first capacity)
    Closure,     // closure(params...) -> type body
    Variant,     // variant<types...>(literal tag, first value)
    EitherOk,    // eitherOk<type, type2>(first value)
    EitherErr,   // eitherErr<type, type2>(first value)
    Some,        // Some<type>(first value)
    None,        // None<type>()
    ListLit,     // [args...]                                      (pos = the `[`)
    IfValue,     // if (first) second else third                   (pos = the `if`)
    Match        // match (first) { arms... } or match { arms... }  (first invalid for no subject; pos = the `match`)
  };

  struct AstFieldInit;
  struct AstParam;
  struct AstSuffix;
  struct AstArm;

  struct AstBlock {
    Pos pos;                // the `{` (line 0 when the block is absent)
    Range<StmtId> stmts;
  };

  // One expression. Which fields mean something depends on `kind`, as listed beside each AstExprKind; every other field is empty.
  struct AstExpr {
    AstExprKind kind = AstExprKind::Int;
    Operator op = Operator::Add;   // Binary
    Pos pos;                       // the first token
    Pos opPos;                     // Binary: the operator token
    ExprId first;                  // an operand, the base of a Postfix, a payload, a capacity (see AstExprKind)
    ExprId second;                 // Binary: the right operand; IfValue: the value when the condition holds
    ExprId third;                  // IfValue: the value when it does not
    TypeId type;                   // the type argument of a New*, Some, None, EitherOk/Err, Closure (its return type)
    TypeId type2;                  // the second type argument (Map, Mutmap, EitherOk/Err)
    Ident name;                    // Name, Call, StructLit
    Literal literal;               // Int, Float, String, Char; NewBuffer: the size; Variant: the tag
    Range<ExprId> args;            // Call, VecLit, ListLit
    Range<AstFieldInit> inits;     // StructLit
    Range<AstSuffix> suffixes;     // Postfix
    Range<TypeId> types;           // Variant: the case types
    Range<AstParam> params;        // Closure
    AstBlock body;                 // Closure
    Range<AstArm> arms;            // Match
  };

  struct AstFieldInit {
    Ident name;
    ExprId value;
  };

  enum class AstSuffixKind : uint8_t {
    Method,  // .name(args...)
    Field,   // .name
    Index    // [args...]          (one index, or several for grid / tensor / sparse)
  };

  struct AstSuffix {
    AstSuffixKind kind = AstSuffixKind::Field;
    Pos pos;                 // the `.` or `[`
    Ident name;              // Method, Field
    Range<ExprId> args;      // Method: the arguments; Index: the indices
  };

  // ── match arms and patterns ─────────────────────────────────────────────────────────────────────────────────────────────────────────────────

  enum class AstPatternKind : uint8_t {
    Value,  // a literal: `0`, `-1`, `"y"`, `'c'`, `true`              (value)
    Any,    // `_`
    Name,   // `n`: matches anything and binds it                     (name)
    Some,   // `Some(first)`
    None,   // `None`
    Ok,     // `Ok(first)`
    Err,    // `Err(first)`
    Case,   // `variant(N, first)`                                    (literal = N)
    List,   // `[items...]`, `[items..., ...]` or `[items..., ...name]`   (hasRest; name = the rest's, pos unknown for none)
    Or      // `a | b | ...`                                          (items = the alternatives)
  };

  struct AstPattern;
  using PatternId = Id<AstPattern>;

  struct AstPattern {
    AstPatternKind kind = AstPatternKind::Any;
    Pos pos;                  // the first token
    ExprId value;             // Value
    Ident name;               // Name; List: the rest's name
    Literal literal;          // Case: the case number
    PatternId first;          // Some, Ok, Err, Case: the payload's pattern
    Range<PatternId> items;   // List: the elements; Or: the alternatives
    bool hasRest = false;     // List: ends with `...`
  };

  // One arm of a `match` or a `switch`: a pattern (with a guard, or not), a condition, or `else`; then what it gives: a value, a
  // block, or (a switch of jumps only) a jump.
  struct AstArm {
    Pos pos;                  // the arm's first token
    bool isElse = false;
    PatternId pattern;        // a pattern arm: its pattern (invalid for a condition arm and for `else`)
    ExprId guard;             // a pattern arm: the condition after `if` (invalid for none)
    ExprId cond;              // a condition arm: its condition
    ExprId value;             // `=> value`: a value, or a statement written as one expression (invalid for a block)
    AstBlock body;            // `=> { ... }` (body.pos unknown when the arm is no block)
  };

  // ── statements ──────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────

  enum class AstStmtKind : uint8_t {
    Let,     // let|val name: type (= value)?                       (isVal for the `val` spelling)
    Var,     // var name: type (= value)?
    If,      // if (cond) body (else other)?                        (other.pos known only when there is an else)
    For,     // for (inits; cond; steps) body, or for (name in value) body   (isForIn)
    Switch,  // switch (value) { cases... else => goTo }
    Label,   // label name(params...):
    Goto,    // goto goTo
    Branch,  // branch cond ? goTo : goElse
    Return,  // return value
    Try,     // try body catch (name) other
    Throw,   // throw value
    Yield,   // yield value
    Expr,    // target (= value)?                                   (isAssign when there is an `=`)
    Match    // match (value) { arms... }, or a switch with block arms   (isSwitch when written `switch`; value invalid for no subject)
  };

  // `name(args...)`: the target of goto, branch and every switch arm.
  struct AstGotoTarget {
    Ident name;
    Range<ExprId> args;
  };

  struct AstForBinding {
    Ident name;
    ExprId value;
  };

  struct AstSwitchCase {
    Literal value;           // an INT_LIT
    AstGotoTarget target;
  };

  struct AstStmt {
    AstStmtKind kind = AstStmtKind::Expr;
    bool isVal = false;      // Let: written `val`
    bool isAssign = false;   // Expr: has `= value`
    bool isForIn = false;    // For: `for (name in value) body`, a loop over what a coroutine yields
    bool isSwitch = false;   // Match: written `switch`
    Pos pos;                 // the first token (the keyword, or the start of the target)
    Ident name;              // Let, Var, Label, Try (the catch variable), For with isForIn (the loop variable)
    TypeId type;             // Let, Var
    // Let, Var: the initializer (invalid when none); Return, Throw, Yield, Switch, Match: the value; Expr: the assigned value;
    // For with isForIn: the coroutine
    ExprId value;
    ExprId cond;             // If, For, Branch
    ExprId target;           // Expr: the thing assigned to, or the expression evaluated
    AstBlock body;           // If: then; For: body; Try: the try block
    AstBlock other;          // If: else; Try: the catch block
    Range<AstForBinding> inits;   // For
    Range<AstForBinding> steps;   // For
    Range<AstParam> params;       // Label
    Range<AstSwitchCase> cases;   // Switch
    Range<AstArm> arms;           // Match
    AstGotoTarget goTo;      // Goto, Branch: the then target, Switch: the default (`else =>`) target
    AstGotoTarget goElse;    // Branch: the else target
  };

  // ── declarations ────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────

  // `uniform`, `index`, `writable` and `workgroup` mark a parameter of a `kernel func` (and, for labels, are accepted but meaningless).
  enum class ParamMark : uint8_t { None, Uniform, Index, Writable, Workgroup };

  struct AstParam {
    ParamMark mark = ParamMark::None;
    Pos markPos;             // the mark token (unknown when None)
    Ident name;
    Pos byPos;               // the `val` after the colon (`p: val T`): unknown when absent
    TypeId type;
  };

  // `@Name(key: value, ...)` before a declaration.
  struct AstAnnotation {
    Ident name;
    Range<AstFieldInit> args;
  };

  struct AstField {
    Pos varPos;              // the `var` token: unknown when the field is immutable
    Ident name;
    TypeId type;
    ExprId initial;          // the default after `=` (`label: string = ""`): invalid when there is none
    StringView typeText{};   // the type and the default as written, for code made from the struct (`@Builder`)
    StringView initialText{};
  };

  struct AstFuncDecl {
    Pos pos;                   // `kernel`, `coro` or `func`, whichever comes first
    bool isKernel = false;
    bool isCoroutine = false;  // `coro func`: `returnType` is the yield type
    bool isInternal = false;     // `internal func`: only its module, struct or file uses it
    bool isStatic = false;       // `static func` in a struct body: called through the struct (`Point.builder()`), with no `this`
    String module{};           // the module it sits in, as its name begins (`Geometry`, `Outer.Inner`); empty at the top level
    String outer{};            // a static function's struct (its path), "" otherwise
    Range<AstAnnotation> annotations;
    Ident name;
    Range<Ident> typeParams;   // func pick<T>(...)
    Range<AstParam> params;
    TypeId returnType;
    AstBlock body;
  };

  // A member of a struct body: a field or a method.
  struct AstMember {
    bool isMethod = false;
    AstField field;          // !isMethod
    FuncId method;           // isMethod: an index into Program::funcs
  };

  struct AstStructDecl {
    Pos pos;
    bool isInternal = false;
    String module{};
    String outer{};              // the struct it is declared in (its path), "" for one outside any struct
    Range<AstAnnotation> annotations;
    Ident name;
    Range<Ident> typeParams;     // struct Box<T, U>
    Range<AstField> primary;     // struct Point(x: double, var y: double)
    Range<Ident> impls;          // struct Circle : Shape, Named
    Range<AstMember> members;
  };

  struct AstInterfaceMethod {
    Ident name;
    Range<AstParam> params;
    TypeId returnType;
  };

  struct AstInterfaceDecl {
    Pos pos;
    bool isInternal = false;
    String module{};
    String outer{};
    Range<AstAnnotation> annotations;
    Ident name;
    Range<AstInterfaceMethod> methods;
  };

  // `java "java.util.ArrayList"`: how one host language spells a declaration.
  struct AstHostBinding {
    Ident target;
    Literal text;
  };

  struct AstExternFunc {
    Pos pos;
    Ident name;
    Range<AstParam> params;
    TypeId returnType;
    Range<AstHostBinding> binding;   // empty when the declaration has no `= ...`
  };

  struct AstHostMember {
    Pos pos;
    bool isStatic = false;
    Ident name;
    Range<AstParam> params;
    TypeId returnType;               // invalid when the member returns nothing
    Range<AstHostBinding> binding;
  };

  struct AstExternType {
    Pos pos;
    Ident name;
    Range<Ident> typeParams;
    Range<AstHostBinding> binding;
    Range<AstHostMember> members;
  };

  struct AstExternInterface {
    Pos pos;
    Ident name;
    Range<AstHostBinding> binding;
    Range<AstInterfaceMethod> methods;
  };

  struct AstBridgeRule {
    Ident direction;                 // toHost, fromHost
    Literal text;
  };

  struct AstBridge {
    Pos pos;
    TypeId poly;
    Ident target;
    Literal host;
    Range<AstBridgeRule> rules;
  };

  struct AstImport {
    Pos pos;
    Ident lang;                      // pos unknown when no language is named (`import "x.poly"`)
    Literal path;
    Ident asWord;                    // `as` before `alias` (a word, not a keyword); pos unknown when the import names nothing
    Ident alias;
  };

  struct AstMeta {
    Pos pos;
    Ident target;                    // pos unknown when the meta describes the module
    Literal key;
    Literal value;
  };

  enum class GlobalInit : uint8_t { Int, Float, True, False };

  struct AstGlobal {
    Pos pos;
    Ident name;
    TypeId type;
    GlobalInit init = GlobalInit::Int;
    Literal value;                   // Int, Float
  };

  enum class ItemKind : uint8_t { Struct, Interface, Func, CoroutineFunc, ExternType, ExternInterface, ExternFunc, Bridge, Import, Global, Meta };

  // One top-level declaration, in source order; `index` is into the Program vector the kind names
  // (Struct: structs, Interface: interfaces, Func / CoroutineFunc: funcs, ExternType: externTypes, ExternInterface: externInterfaces, ExternFunc: externFuncs,
  // Bridge: bridges, Import: imports, Global: globals, Meta: metas).
  struct AstItem {
    ItemKind kind = ItemKind::Func;
    uint32_t index = 0;
  };

  bool IsVar(const AstField& field);

  inline bool IsVar(const AstField& field) {
    return Known(field.varPos);
  }

  // ── the program ─────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────

  struct Program {
    SharedPtr<const String> source;    // keeps every Literal's text alive
    Vector<SharedPtr<const String>> made{};  // the text of declarations made from others (`@Builder`), kept alive the same way
    Interner* names = nullptr;         // the interner the Symbols belong to
    Vector<AstItem> items;
    Vector<AstStructDecl> structs;
    Vector<AstInterfaceDecl> interfaces;
    Pool<AstFuncDecl> funcs;           // top-level functions and struct methods
    Vector<AstExternType> externTypes;
    Vector<AstExternInterface> externInterfaces;
    Vector<AstExternFunc> externFuncs;
    Vector<AstBridge> bridges;
    Vector<AstImport> imports;
    Vector<AstGlobal> globals;
    Vector<AstMeta> metas;
    Pool<AstType> types;
    Pool<AstExpr> exprs;
    Pool<AstStmt> stmts;
    Pool<TypeId> typeRefs;             // lists of types
    Pool<ExprId> exprRefs;             // lists of expressions (call arguments, indices, vector elements, goto arguments)
    Pool<StmtId> stmtRefs;             // the statements of blocks
    Pool<AstParam> params;
    Pool<AstField> fields;
    Pool<AstMember> members;
    Pool<Ident> idents;                // type parameters, implemented interface names
    Pool<AstFieldInit> inits;
    Pool<AstAnnotation> annotations;
    Pool<AstSuffix> suffixes;
    Pool<AstForBinding> forBindings;
    Pool<AstSwitchCase> cases;
    Pool<AstArm> arms;
    Pool<AstPattern> patterns;
    Pool<PatternId> patternRefs;       // lists of patterns (a list pattern's elements, alternatives)
    Pool<Literal> literals;            // the INT_LIT sizes of types
    Pool<AstInterfaceMethod> interfaceMethods;
    Pool<AstHostBinding> hostBindings;
    Pool<AstHostMember> hostMembers;
    Pool<AstBridgeRule> bridgeRules;
  };

  StringView TextOf(const Program& program, Symbol symbol);

  inline StringView TextOf(const Program& program, Symbol symbol) {
    return TextOf(*program.names, symbol);
  }

  // ── parsing ─────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────

  struct ParseResult {
    Program program;
    Vector<String> errors;   // "line:column: message", exactly as the Nina front end words them (its file prefix is the caller's to add)
  };

  bool Ok(const ParseResult& result);

  // Lexes and parses one polyir source. A syntax error stops the parse (as in the Nina front end). Then `errors` has that one message and the
  // program holds what was parsed before it.
  POLYIR_API ParseResult ParseSource(String text);

  // The same, with the source's names interned in `names` instead of the global interner.
  POLYIR_API ParseResult ParseSource(String text, Interner& names);

  // The tree as stable indented text (for tests and debugging). One line per node, children indented two spaces.
  POLYIR_API String DumpProgram(const Program& program);

  const char* OperatorText(Operator op);  // `+`, `<=`, `&&`: the operator as written

  inline bool Ok(const ParseResult& result) {
    return result.errors.empty();
  }

}  // namespace polyir::front
