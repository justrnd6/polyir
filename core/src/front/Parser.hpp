// The recursive-descent parser: the grammar of lang/src/Polyir.intent, production by production, building the tree of Ast.hpp.
// It is step 1 of the pipeline (Pipeline.hpp), right after the lexer: tokens in, syntax tree out.
//
// The one idea: each grammar rule is one function that reads tokens from the front of the list -- `Current` looks at the next token,
// `Accept` takes it if it is the expected kind, `Expect` takes it or reports an error -- and calls the functions of the rules inside it.
//
// How it matches the Nina parser (the generated LL(1) parser of the std grammar fence):
//   * A rule's first-token test and its "expected ..." message are the same: a rule called where its first token is not certain checks it
//     first (`RequireAny`), and a failed `Expect` says `expected 'x' (found 'y')`; the lists are sorted by token name as the generator sorts them.
//   * The first error ends the parse. Instead of exceptions the parser LATCHES it: once `hasFailed` is set, `Current()` answers the EOF token, so
//     every loop and every later `Expect` falls through at once and the only message kept is the first -- the one the Nina parser returns.
//   * Binary operators climb by precedence, all left associative: `* / %` > `+ -` > `<< >>` > `&` > `^` > `|` > comparisons > `&&` > `||`.
//   * The grammar's `{ lookaheadKind(n) == ... }?` predicates (extern type / extern interface, a generic type `Name<`, a struct call
//     `name(field:`, a for-in loop) are decided by peeking ahead with `KindAt(parser, n)`. The one exception is the method suffix `.name(`:
//     ParseNameSuffix reads the name first, so testing the current token for `(` decides it the same way.
#pragma once

#include "front/Ast.hpp"
#include "front/Lexer.hpp"

namespace polyir::front {

  // The shape of a `kw<T...>(...)` expression: which of its optional parts it has.
  struct CtorShape {
    bool pair = false;    // a second type argument
    bool sized = false;   // `, INT_LIT` after the types
    bool value = false;   // one expression inside the parentheses
    bool optional = false;  // ... which may be left out (`mutmap<K, V>()`)
  };

  // How the text after a type keyword is shaped: `kw<T>`, `kw<K, V>`, `kw<T, N...>`, `kw(T...) -> R`, `kw<A, B...>`.
  enum class TypeShape : uint8_t { One, Pair, Sized, Func, Variant };

  // Everything the parser works with: the tokens it reads, the tree it builds and the first error.
  struct Parser {
    // Each list in the tree (the statements of a block, the arguments of a call, ...) is first collected on one of these stacks and then
    // moved into its pool in one piece, so a list's items sit side by side in the pool. Nested lists of the same kind share a stack: each
    // remembers where its own items begin (see SeparatedList).
    struct Scratch {
      Vector<ExprId> exprs;
      Vector<StmtId> stmts;
      Vector<TypeId> types;
      Vector<AstParam> params;
      Vector<AstField> fields;
      Vector<AstMember> members;
      Vector<Ident> idents;
      Vector<AstFieldInit> inits;
      Vector<AstSuffix> suffixes;
      Vector<AstForBinding> bindings;
      Vector<AstSwitchCase> cases;
      Vector<AstArm> arms;
      Vector<PatternId> patterns;
      Vector<Literal> literals;
      Vector<AstInterfaceMethod> interfaceMethods;
      Vector<AstHostBinding> hostBindings;
      Vector<AstHostMember> hostMembers;
      Vector<AstBridgeRule> bridgeRules;
      Vector<AstAnnotation> annotations;
    };

    Program& program;              // the tree being built
    const Vector<Token>& tokens;   // the lexer's tokens; the last one is EOF_
    StringView source;             // the text the tokens point into
    size_t position = 0;           // the index of the current token
    bool hasFailed = false;
    String error{};                // the first error, "line:column: expected ..."
    Scratch scratch{};
    Vector<String> modules{};      // the modules the parser is inside, outermost first (`module Geometry { ... }`)
    bool internal = false;         // an `internal` was read, for the declaration that follows
    Range<AstAnnotation> annotations{};  // the annotations read, for the declaration that follows
    String outer{};                // the struct whose body is being read (its path), "" outside any struct
    int typeLists = 0;             // the lists of types open (`array<map<` is two)
    bool halfShift = false;        // a `>>` closed one list of types, and its other `>` closes the list around it
    bool inArm = false;            // the head or value of a `match` arm is being read (see ArmEnds)
  };

  // ── token access and errors (ParserCore.cpp) ────────────────────────────────────────────────────────────────────────────────────────────

  void ParseProgram(Parser& parser);

  const String& Error(const Parser& parser);

  const Token& Current(const Parser& parser);

  TokenKind CurrentKind(const Parser& parser);

  TokenKind KindAt(const Parser& parser, size_t ahead);

  bool At(const Parser& parser, TokenKind kind);

  bool AtAny(const Parser& parser, const KindSet& set);

  Token Take(Parser& parser);

  bool Accept(Parser& parser, TokenKind kind);

  Token Expect(Parser& parser, TokenKind kind);

  void ExpectTypeOpen(Parser& parser);  // the `<` that opens a list of types

  // The `>` that closes a list of types. `array<variant<A, B>>` closes two with its `>>`, one now and the other next: only
  // where two are open.
  void ExpectTypeClose(Parser& parser);

  void Fail(Parser& parser, StringView expected);

  void RequireAny(Parser& parser, const KindSet& set, StringView expected);

  Pos PosOf(const Parser& parser, const Token& token);

  // The source text of the tokens from `from` up to the current one (`mutarray<i64>`).
  StringView TextSince(const Parser& parser, size_t from);

  Pos AcceptPos(Parser& parser, TokenKind kind);

  Ident IdentOf(Parser& parser, const Token& token);

  Ident ExpectIdent(Parser& parser);

  Literal LiteralOf(const Parser& parser, const Token& token);

  Literal ExpectLiteral(Parser& parser, TokenKind kind);

  void AddHint(Parser& parser);

  template <typename T, typename One>
  Range<T> SeparatedList(Parser& parser, TokenKind separator, Vector<T>& scratch, Pool<T>& pool, One parseOne);

  template <typename T, typename One>
  Range<T> CommaList(Parser& parser, Vector<T>& scratch, Pool<T>& pool, One parseOne);

  template <typename T, typename One>
  Range<T> OptionalCommaList(Parser& parser, const KindSet& first, Vector<T>& scratch, Pool<T>& pool, One parseOne);

  // ── declarations (ParserDecls.cpp) ──────────────────────────────────────────────────────────────────────────────────────────────────────
  void ParseTopLevel(Parser& parser);

  void ParseTypeLevel(Parser& parser);

  void ParseFuncLevel(Parser& parser);

  void ParseExternLevel(Parser& parser);

  void ParseSimpleLevel(Parser& parser);

  template <typename T>
  void AddDecl(Parser& parser, Vector<T>& decls, T decl, ItemKind kind);

  AstStructDecl ParseStruct(Parser& parser);

  void ParseStructHead(Parser& parser, AstStructDecl& decl);

  void ParseStructBody(Parser& parser, AstStructDecl& decl);

  // One member of a struct body. A field or a method goes into `scratch.members`; a nested struct or a static function is an item of its own.
  void ParseMember(Parser& parser);

  AstField ParseField(Parser& parser);

  Range<Ident> ParseTypeParams(Parser& parser);

  Range<Ident> ParseImpls(Parser& parser);

  Range<AstField> ParsePrimaryFields(Parser& parser);

  AstInterfaceDecl ParseInterface(Parser& parser);

  Range<AstInterfaceMethod> ParseInterfaceMethods(Parser& parser);

  Range<AstInterfaceMethod> ParseInterfaceBody(Parser& parser);

  AstInterfaceMethod ParseInterfaceMethod(Parser& parser);

  AstFuncDecl ParseFunc(Parser& parser);

  void ParseSignature(Parser& parser, Range<AstParam>& params, TypeId& returnType);

  Pos ExpectPair(Parser& parser, TokenKind first, TokenKind second);

  Ident ExpectFuncName(Parser& parser);

  void ParseFuncTail(Parser& parser, AstFuncDecl& func);

  Range<AstParam> ParseParamList(Parser& parser);

  AstParam ParseParam(Parser& parser);

  AstParam ParseParamHead(Parser& parser);

  void ParseMark(Parser& parser, AstParam& param);

  AstExternFunc ParseExternFunc(Parser& parser);

  AstExternType ParseExternType(Parser& parser);

  AstExternInterface ParseExternInterface(Parser& parser);

  Range<AstHostMember> ParseHostMembers(Parser& parser);

  Range<AstHostMember> ParseHostMemberBody(Parser& parser);

  void ParseHostSignature(Parser& parser, AstHostMember& member);

  AstHostMember ParseHostMember(Parser& parser);

  Range<AstHostBinding> ParseOptionalBinding(Parser& parser);

  AstHostBinding ParseHostBinding(Parser& parser);

  AstBridge ParseBridge(Parser& parser);

  void ParseBridgeTarget(Parser& parser, AstBridge& decl);

  Range<AstBridgeRule> ParseBridgeRules(Parser& parser);

  AstBridgeRule ParseBridgeRule(Parser& parser);

  AstImport ParseImport(Parser& parser);

  void ParseImportAlias(Parser& parser, AstImport& decl);

  AstMeta ParseMeta(Parser& parser);

  AstGlobal ParseGlobal(Parser& parser);

  void ParseGlobalInit(Parser& parser, AstGlobal& global);

  void ParseGlobalType(Parser& parser, AstGlobal& decl);

  // ── types (ParserTypes.cpp) ─────────────────────────────────────────────────────────────────────────────────────────────────────────────
  TypeId ParseType(Parser& parser);

  void FillType(Parser& parser, AstType& type);

  void FillKeywordType(Parser& parser, AstType& type, TypeShape shape);

  void ParsePairType(Parser& parser, AstType& type);

  void ParseSizedType(Parser& parser, AstType& type);

  void ParseFuncType(Parser& parser, AstType& type);

  void ParseNamedType(Parser& parser, AstType& type);

  void ParseTypeArgs(Parser& parser, AstType& type);

  void ParseArraySize(Parser& parser, AstType& type);

  void ParseSizes(Parser& parser, AstType& type, size_t required, bool allowsMore);

  Range<TypeId> ParseTypeList(Parser& parser);

  TypeId CommaType(Parser& parser);

  Literal CommaSize(Parser& parser);

  // ── expressions (ParserExprs.cpp) ───────────────────────────────────────────────────────────────────────────────────────────────────────
  // A `Parse...` function that returns a value builds that piece itself. One that takes a node, and every `Fill...` function, writes into a
  // node the caller made. A `Fill...` function is the one that first decides which alternative the node is.

  ExprId ParseExpr(Parser& parser);

  ExprId ParseBinary(Parser& parser, int minPrecedence);

  ExprId ParseBinaryNode(Parser& parser, ExprId left, int precedence);

  ExprId ParseUnary(Parser& parser);

  ExprId ParsePostfix(Parser& parser);

  Range<AstSuffix> ParseSuffixes(Parser& parser);

  AstSuffix ParseSuffix(Parser& parser);

  void ParseIndexArgs(Parser& parser, AstSuffix& suffix);

  void ParseNameSuffix(Parser& parser, AstSuffix& suffix);

  ExprId ParsePostfixBase(Parser& parser);

  void FillBase(Parser& parser, AstExpr& expr);

  void FillLiteral(Parser& parser, AstExpr& expr);

  void FillIdent(Parser& parser, AstExpr& expr);

  void FillParen(Parser& parser, AstExpr& expr);

  void FillIfValue(Parser& parser, AstExpr& expr);

  // ── match and the switch of blocks (ParserMatch.cpp) ──

  void FillMatch(Parser& parser, AstExpr& expr);  // `match (subject) { arms }` as a value

  void ParseMatchStmt(Parser& parser, AstStmt& stmt);  // the same as a statement, after its `match`

  // A switch whose arms are blocks: `switch { … }`, or `switch (subject) { … }` with its `{` already read.
  void ParseSwitchArms(Parser& parser, AstStmt& stmt, bool subject);

  bool AtJumpArm(const Parser& parser);  // the first arm of a switch of jumps: `0 => name(` or `else => name(`

  PatternId ParsePattern(Parser& parser);  // a pattern, or alternatives joined by `|`

  // Inside an arm, a `[`, `(` or `-` that starts a line starts the next arm: the expression before it ends there.
  bool ArmEnds(const Parser& parser);

  // ── modules (ParserDecls.cpp) ──

  // `module Name {`: the word `module` is a name everywhere else.
  bool AtModule(const Parser& parser);

  // `internal` before `func`, `coro`, `kernel`, `struct` or `interface`.
  bool AtInternal(const Parser& parser);

  void ParseModule(Parser& parser);

  // `@Name` and `@Name(key: value, ...)` before a declaration, kept for it in `parser.annotations`.
  void ParseAnnotations(Parser& parser);

  // Whether the annotations hold one called `name` (`Builder`).
  bool HasAnnotation(const Parser& parser, Range<AstAnnotation> annotations, StringView name);

  // `@Builder` on a struct: its nested `Builder` and static `builder()`, parsed as if its body held them (ParserBuilder.cpp).
  void ExpandBuilder(Parser& parser, const AstStructDecl& decl);

  // A struct as an item of the program, followed by what `@Builder` makes for it.
  void AddStruct(Parser& parser, AstStructDecl decl);

  // Whether a top-level item starts here: a keyword of one, `module`, `internal` or an annotation.
  bool AtItemStart(const Parser& parser);

  // `Outer.Inner` inside `module Outer { module Inner { ... } }`.
  String ModulePath(const Parser& parser);

  // `Point`, or `Geometry.Point` (its path).
  Ident ParseTypeName(Parser& parser);

  // `Geometry.name`.
  Symbol InModule(Parser& parser, const String& module, Symbol name);

  void FillVecLit(Parser& parser, AstExpr& expr);

  void FillListLit(Parser& parser, AstExpr& expr);

  void FillGrouped(Parser& parser, AstExpr& expr);

  void FillCall(Parser& parser, AstExpr& expr);

  void FillStructLit(Parser& parser, AstExpr& expr, TokenKind close);

  void FillClosure(Parser& parser, AstExpr& expr);

  void FillVariant(Parser& parser, AstExpr& expr);

  void ParseVariantArgs(Parser& parser, AstExpr& expr);

  void FillCtor(Parser& parser, AstExpr& expr);

  void ParseCtorTypes(Parser& parser, AstExpr& expr, const CtorShape& shape);

  void ParseCtorArgs(Parser& parser, AstExpr& expr, const CtorShape& shape);

  Range<ExprId> ParseCallArgs(Parser& parser);

  AstFieldInit ParseFieldInit(Parser& parser);

  ExprId AddExpr(Parser& parser, const AstExpr& expr);

  // ── statements (ParserStmts.cpp) ────────────────────────────────────────────────────────────────────────────────────────────────────────
  AstBlock ParseBlock(Parser& parser);

  Range<StmtId> ParseStmtList(Parser& parser);

  StmtId ParseStmt(Parser& parser);

  void FillStmt(Parser& parser, AstStmt& stmt);

  void FillKeywordStmt(Parser& parser, AstStmt& stmt);

  void FillFlowStmt(Parser& parser, AstStmt& stmt);

  void FillValueStmt(Parser& parser, AstStmt& stmt);

  void ParseLetVar(Parser& parser, AstStmt& stmt);

  void ParseIf(Parser& parser, AstStmt& stmt);

  void ParseFor(Parser& parser, AstStmt& stmt);

  void ParseForHeader(Parser& parser, AstStmt& stmt);

  void ParseForIn(Parser& parser, AstStmt& stmt);

  void ParseSwitch(Parser& parser, AstStmt& stmt);

  Range<AstSwitchCase> ParseSwitchCases(Parser& parser);

  AstSwitchCase ParseSwitchCase(Parser& parser);

  void ParseSwitchDefault(Parser& parser, AstStmt& stmt);

  void ParseLabel(Parser& parser, AstStmt& stmt);

  void ParseBranch(Parser& parser, AstStmt& stmt);

  void ParseTry(Parser& parser, AstStmt& stmt);

  void ParseExprStmt(Parser& parser, AstStmt& stmt);

  AstGotoTarget ParseGotoTarget(Parser& parser);

  AstForBinding ParseForBinding(Parser& parser);

  Range<AstForBinding> ParseForBindings(Parser& parser);

  Range<AstForBinding> ParseForInits(Parser& parser);

  inline const String& Error(const Parser& parser) {
    return parser.error;
  }

  inline TokenKind CurrentKind(const Parser& parser) {
    return Current(parser).kind;
  }

  inline bool At(const Parser& parser, TokenKind kind) {
    return CurrentKind(parser) == kind;
  }

  inline bool AtAny(const Parser& parser, const KindSet& set) {
    return Has(set, CurrentKind(parser));
  }

  inline Pos PosOf(const Parser&, const Token& token) {
    return Pos{token.line, token.column};
  }

  inline Ident ExpectIdent(Parser& parser) {
    return IdentOf(parser, Expect(parser, TokenKind::IDENT));
  }

  inline Literal ExpectLiteral(Parser& parser, TokenKind kind) {
    return LiteralOf(parser, Expect(parser, kind));
  }

  // Parses `one (separator one)*` with `parseOne`, and moves the items into `pool` as one range. A template only because every kind of list
  // (expressions, fields, parameters, ...) has its own item type and pool.
  template <typename T, typename One>
  inline Range<T> SeparatedList(Parser& parser, TokenKind separator, Vector<T>& scratch, Pool<T>& pool, One parseOne) {
    const size_t mark = scratch.size();
    scratch.push_back(parseOne());

    while (Accept(parser, separator)) {
      scratch.push_back(parseOne());
    }

    return Take(pool, scratch, mark);
  }

  template <typename T, typename One>
  inline Range<T> CommaList(Parser& parser, Vector<T>& scratch, Pool<T>& pool, One parseOne) {
    return SeparatedList(parser, TokenKind::COMMA, scratch, pool, parseOne);
  }

  // A comma list that may be empty: it is there only when the current token can start an item.
  template <typename T, typename One>
  inline Range<T> OptionalCommaList(Parser& parser, const KindSet& first, Vector<T>& scratch, Pool<T>& pool, One parseOne) {
    return AtAny(parser, first) ? CommaList(parser, scratch, pool, parseOne) : Range<T>{};
  }

  // Stores a top-level declaration in its own list and records its place in the program's item order.
  template <typename T>
  inline void AddDecl(Parser& parser, Vector<T>& decls, T decl, ItemKind kind) {
    decls.push_back(move(decl));
    parser.program.items.push_back(AstItem{kind, static_cast<uint32_t>(decls.size() - 1)});
  }

  // An expression is a binary expression of the lowest precedence (1, `||`).
  inline ExprId ParseExpr(Parser& parser) {
    return ParseBinary(parser, 1);
  }

  inline ExprId AddExpr(Parser& parser, const AstExpr& expr) {
    return Add(parser.program.exprs, expr);
  }

}  // namespace polyir::front
