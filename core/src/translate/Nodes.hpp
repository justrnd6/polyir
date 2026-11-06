// polyir as a source of the translate library (../../../translate): its checked HIR made into a tree of polyir's forms
// (polyir/translate/Tree.hpp), by the functions in NodesDecls.cpp, NodesStmts.cpp, NodesExprs.cpp and NodesTypes.cpp.
#pragma once

#include "polyir/Hir.hpp"
#include "polyir/translate/Tree.hpp"
#include "hir/Lift.hpp"

namespace polyir::translate {

  // What the nodes are made with: the module, the generics a target writes once, where the code being made sits (for the names it
  // writes), and the names that stand for something else (a walk's key, an `if` value written in place).
  struct NodeContext {
    const Module* module = nullptr;
    WrittenGenerics generics;
    bool copies = false;     // the target writes each generic as its copies
    bool fullNames = false;  // the target names every declaration by its whole path (`Geometry.Point`, even inside Geometry)
    bool flatNames = false;  // `modules flat`: a declaration is declared by its whole path too, as C has no namespaces
    Vector<String> bindingWords;  // the words of the host bindings the target uses (`js=...`), its own first, then its base page's
    String scope;            // the module or struct the code sits in
    Set<String> keyStructs;  // the structs used as a map's key or a set's element
    Map<String, String> walkKeys;       // inside `for (k in m)`: the slot's name and the key's
    Map<String, Node> walkTables;       // the same slot and the table walked
    Map<String, Node> inlined;          // a hidden variable an `if` value stands in for
    const StmtList* function = nullptr;  // the body of the function being made
    TypePtr result;                      // what its `return` gives (what a coroutine yields)
    Set<String> boxed;                   // its variables a closure gives new values: their names and declarations have the fact `boxed`
    Set<String> throwing;                // the functions that can throw, and the interface methods (`Shape.area`)
    bool inCoroutine = false;            // the body being made is a coroutine's: its `return`s have the fact `inCoroutine`
    bool guarded = false;                // it has a `try`: its variables have the fact `guarded` (C keeps them `volatile`)
    Set<String> capturedHere;            // the variables the closure (or coroutine) being made reads from around it: fact `captured`
    bool uniqueLocals = false;           // `locals unique`: a local that would hide another is written with a name of its own
    Vector<Map<String, String>> scopes;  // the locals of each block being made, and the names they are written with
    Set<String> localNames;              // the names the function's locals are written with
    int counter = 0;                     // for the names the nodes make up (`closure1`)
    Map<String, String> provided;        // each library declaration an imported file writes, and that file's path
    String fileName;                     // the checked file's name as a class (`ContainerTests`), `Main` for one from memory
    Map<String, String> importedFiles;   // each declaration of an imported file, and that file's name (`offInt` -> `Check`)
    bool qualifyImported = false;        // `imported qualified`: such a declaration is named through its file (`Check.offInt`)
    bool spans = false;                  // the target asked for spans: statements and functions say where they came from
    String spanFile;                     // the file the function being made is in: "" for the checked file, else its origin
    Vector<String> problems;
  };

  // ── which file a declaration is written in (Files.cpp) ──

  bool IsImported(const NodeContext& context, const DeclRef& ref);  // another file's translation writes it

  Map<String, String> ProvidedNames(const Module& module);  // the library's declarations an imported file writes, and that file

  String FileName(const String& path);  // the file's stem as a name (`host-types` -> `host_types`)

  String FileClassName(const Module& module, const String& path);  // the file's name as a class: `BiquadFile` beside `struct Biquad`

  Map<String, String> ImportedFiles(const Module& module, const Map<String, String>& provided);  // each one's name, its file's name

  Vector<Node> UseNodes(const NodeContext& context);           // `use(name, path, names)` for each imported file the code names

  Set<String> OwnNames(const NodeContext& context);            // what the checked file's own declarations name

  // ── made from the HIR ──

  Node ProgramNode(NodeContext& context);

  Vector<Node> StatementNodes(NodeContext& context, const StmtList& body);

  Node ExpressionNode(NodeContext& context, const Expr& e);

  Node ValueFor(NodeContext& context, const ExprPtr& value, const TypePtr& slot);  // a value in the type its place holds (`slot`, or null)

  Node TypeNode(NodeContext& context, const TypePtr& type);

  // With spans asked for, where a statement or function came from. That is its line of the `.poly` file, or the place a
  // `//# from` line before it names (core/docs/translate.md).
  void AddSpan(const NodeContext& context, Node& node, const Span& at);

  // `body(statements, result)`: a function's (or a closure's) statements as its block holds them. Its facts are what a body
  // may need written: `globals` and `outer` (the names it gives new values), `throws`, and `expression` (one `return`).
  Node BodyNode(NodeContext& context, Vector<Node> statements, const TypePtr& result, const Vector<String>& globals,
    const Vector<String>& outer, bool throws);

  Vector<Node> OneNode(Node node);  // a part's one node, moved in

  Vector<Node> TypeNodes(NodeContext& context, const Vector<TypePtr>& types);

  Vector<Node> FlowNodes(NodeContext& context, const StmtList& body);  // a body with labels, as a machine (Flow.cpp)

  bool HasLabels(const StmtList& body);

  // ── names ──

  String RelativeName(const NodeContext& context, const String& name);  // `Point` inside Geometry, `Geometry.Point` outside it

  String NameDeclared(const NodeContext& context, const String& name);  // the name a declaration is declared by

  String DeclaredName(const NodeContext& context, const String& name, const Generic& generic);  // a copy written once takes its generic's name

  // The binding the target has for host type `name` (`java.util.ArrayList` from `java=java.util.ArrayList`), or null.
  const HostType* BoundHostType(const NodeContext& context, const String& name, String& binding);

  // The text of the first of `bindings` (`java=Math.abs($0)`) under one of the target's binding words, in their order.
  Optional<String> BindingIn(const NodeContext& context, const Vector<String>& bindings);

  void Problem(NodeContext& context, const String& message);

  // ── what throws (Throws.cpp) ──

  Set<String> ThrowingFunctions(const Module& module);  // the functions that can throw, and the interface methods that can

  bool CallThrows(const NodeContext& context, const Expr& e);  // a call of a function, or through an interface, that throws

  // ── locals ──

  void PushScope(NodeContext& context);

  void PopScope(NodeContext& context);

  String DeclareLocal(NodeContext& context, const String& name);  // the name a local declared here is written with

  String LocalName(const NodeContext& context, const String& name);  // the name a local is written with where it is read

  void StartFunction(NodeContext& context, const Vector<String>& params);  // a function's own scopes, with its parameters

  // ── what a body assigns ──

  // The names `body` assigns, in the order first assigned: its globals, or its locals. Its blocks count, its closures do not.
  Vector<String> AssignedNames(const StmtList& body, bool globals);

  Set<String> DeclaredNames(const StmtList& body);  // the variables, loop counters and caught values `body` declares

  void AddNameFact(Node& node, const String& fact, const Vector<String>& names);  // `globals` as `name` nodes, when there are any

  Vector<String> OuterNames(const Expr& closure);  // the variables around a closure that its body gives new values

  Set<String> BoxedNames(const StmtList& body);  // the variables of a body that a closure in it gives new values

  Vector<String> CapturedNames(const Expr& closure);  // the variables (and `this`) a closure uses from around it, in order

  size_t CountTries(const StmtList& body);  // how many `try`s the body has, in its blocks too

}  // namespace polyir::translate
