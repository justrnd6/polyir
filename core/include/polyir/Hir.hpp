// polyir's typed HIR: the expressions, statements and declarations the checker produces and every back end reads. Each expression knows its type;
// a program is a Module of declarations. The nodes are plain data, made by PolyIRBuilder (Builder.hpp) and read by the compilers.
#pragma once

#include "polyir/Std.hpp"
#include "polyir/Export.hpp"
#include "polyir/Types.hpp"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace polyir {

  enum class BinOp { Add, Sub, Mul, Div, Rem, And, Or, Xor, Shl, Shr, Eq, Ne, Lt, Le, Gt, Ge, LogAnd, LogOr, Identical, NotIdentical };

  enum class UnOp { Neg, LogNot, BitNot };

  // Where a node came from in the source (all zero when it has no position).
  struct POLYIR_API Span {
    int line = 0;
    int column = 0;
    int endLine = 0;
    int endColumn = 0;
    int offset = 0;
    int endOffset = 0;
  };

  // Container and runtime operations the checker identifies by receiver type and method name (the HBuiltin cases of the Nina HIR, in order).
  enum class Builtin {
    Len, Capacity, Rows, Cols, Dim, Push, Hash, StrEq, Concat, Shuffle,
    AtomicLoad, AtomicStore, AtomicRmw, AtomicCmpxchg, AtomicFence,
    VariantTag, VariantIsCase, VariantUnwrap, EitherIsOk, EitherIsErr, EitherUnwrapOk, EitherUnwrapErr,
    CoroutineResume, CoroutineDone, InterfaceCall, InterfaceBox, Copy, Free, FreeBox, AssocLive, AssocValue, Present, With, Has, Add, AssocKey, Retype,
    Loc, Var, OptionIsSome, OptionIsNone, OptionUnwrap,
    Truncate, Remove,  // `truncate(xs, n)` shrinks a mutarray to its first n elements; `remove(m, k)` takes a key out of a mutmap or mutset (an i1: it was there)
    // `CoroutineYield(v)` hands `v` out like a `yield` and says (an i1) whether the coroutine was abandoned while it was suspended there: the body then cleans up
    // and returns without yielding again. `CoroutineClose(co)` abandons a coroutine that has started and not finished by resuming it once with that answer.
    CoroutineYield, CoroutineClose,
    // `StrOf(x)` a whole number, bool, char (`i8`) or string as text; `CharAt(s, i)` the byte at `i` (0 outside the string); `Slice(s, from, to)` the
    // bytes [from, to), both clamped into the string; `Find(s, sub, from)` the first index >= from where `sub` starts, or -1; `Print(s)` writes the
    // string and a newline to standard output (the checker turns any other value into `str(x)`). `Fail(s)` stops the program: it writes
    // `trap: ` and the message to standard error, and the program ends with status 1 (a @Builder's build() of a missing field).
    StrOf, CharAt, Slice, Find, Print, Fail
  };

  enum class ExprKind {
    ConstInt, ConstFloat, ConstBool, ConstString, Local, Global, FuncRef, Binary, Unary, Cast, Call, StructLit, FieldGet,
    LoadSlot, Coerce, CallIndirect, CallBuiltin, IndexGet, VecLit, NewContainer, ClosureLit, VariantLit, EitherLit, Unsupported
  };

  struct Expr;
  struct Stmt;
  using ExprPtr = SharedPtr<const Expr>;

  // One expression. Which fields mean something depends on `kind`:
  //   ConstInt `intValue`; ConstFloat `floatValue`; ConstBool `boolValue`; ConstString `text`; Local/Global/FuncRef `name`;
  //   Binary `binaryOp` + two `operands`; Unary `unaryOp` + one; Cast one operand (to `type`); Call `name` + `operands` (the arguments);
  //   StructLit `name` (the struct), `fieldIndices` (the slot each operand fills) + `operands`; FieldGet one operand (the object) + `fieldIndex`;
  //   LoadSlot `name` (a `var` slot read); Coerce one operand (an integer-width fix the checker added); CallIndirect `operands` (the callee, then the arguments);
  //   CallBuiltin `builtin` + `operands` (the receiver, then the arguments); IndexGet `operands` (the base, then the indices); VecLit `operands`;
  //   NewContainer `operands` (the size arguments); ClosureLit `parameterNames` + `parameterTypes` + `captures` + `closureBody`;
  //   VariantLit `fieldIndex` (the case) + one operand; EitherLit `boolValue` (true for `ok`) + one operand; Unsupported `text` (what it was).
  struct POLYIR_API Expr {
    ExprKind kind = ExprKind::ConstInt;
    TypePtr type;
    int64_t intValue = 0;
    double floatValue = 0.0;
    bool boolValue = false;
    String text;
    String name;
    BinOp binaryOp = BinOp::Add;
    UnOp unaryOp = UnOp::Neg;
    Vector<ExprPtr> operands;
    Vector<int> fieldIndices;
    int fieldIndex = 0;
    Builtin builtin = Builtin::Len;
    Vector<String> parameterNames;
    Vector<TypePtr> parameterTypes;
    Vector<String> captures;
    Vector<SharedPtr<Stmt>> closureBody;
    Span at;
  };

  enum class StmtKind { Let, Var, Assign, If, For, Return, ExprStmt, Label, Goto, Branch, Switch, Try, Throw, Yield, Select };

  using StmtPtr = SharedPtr<Stmt>;
  using StmtList = Vector<StmtPtr>;

  // One statement. Which fields mean something depends on `kind`:
  //   Let/Var `name` + `type` + `value` (a Var may have none); Assign `target` + `value`; If `value` (the condition) + `body` + `elseBody`;
  //   For `names` + `initializers` (the counters), `value` (the condition), `steps` (one per counter) + `body`; Return `value` (or none);
  //   ExprStmt `value`; Label `name` + `parameterNames` + `parameterTypes`; Goto `name` (the label) + `arguments`;
  //   Branch `value` (the condition), `name` + `arguments` (taken when true), `elseName` + `elseArguments`;
  //   Switch `value`, `caseValues` + `caseLabels` + `caseArguments`, and `name` + `arguments` for the default;
  //   Try `body`, `name` (what the caught value is called) + `elseBody` (the catch; with an empty `name` it is a cleanup:
  //   it runs when anything is thrown out of `body` and the throw then continues); Throw and Yield `value`;
  //   Select `value` (a name: the front end evaluates any other subject into a hidden `let` first), and one arm per entry of
  //   `caseKeys` (the arm's keys, constants), `names` (the payload the arm binds, "" for none) and `caseBodies`, then
  //   `elseBody` for the default (empty for none). A key is the value itself for a whole number, char, bool or string, and the
  //   case's number for a sum: optional `Some` 0 and `None` 1, either `Ok` 0 and `Err` 1, a variant's case. An arm that
  //   binds a payload starts its body with `let name: T = unwrap(value)`, so running the body is all an arm needs.
  struct POLYIR_API Stmt {
    StmtKind kind = StmtKind::ExprStmt;
    String name;
    String elseName;
    TypePtr type;
    ExprPtr value;
    ExprPtr target;
    StmtList body;
    StmtList elseBody;
    Vector<String> names;
    Vector<ExprPtr> initializers;
    Vector<ExprPtr> steps;
    Vector<String> parameterNames;
    Vector<TypePtr> parameterTypes;
    Vector<ExprPtr> arguments;
    Vector<ExprPtr> elseArguments;
    Vector<int64_t> caseValues;
    Vector<String> caseLabels;
    Vector<Vector<ExprPtr>> caseArguments;
    Vector<Vector<ExprPtr>> caseKeys;
    Vector<StmtList> caseBodies;
    Span at;
  };

  struct POLYIR_API Field {
    String name;
    TypePtr type;
    bool isMutable = false;
    ExprPtr initial{};  // the default a struct literal that leaves the field out gives it (`label: string = ""`), a constant; null for none
  };

  struct POLYIR_API Parameter {
    String name;
    TypePtr type;
  };

  // `@Name(key: value, ...)` before a declaration: what tools and back ends read about it. `@Builder` also gives a struct its builder.
  struct POLYIR_API Annotation {
    String name;
    Vector<Pair<String, String>> args{};  // each argument's name and value, the value as written (`"text"`, `3`, `true`)
  };

  // What a declaration is to a generic. A copy made for some types (`Box_i64`, `pick_i64`) names its generic and those types: a typed back end
  // writes `Box<i64>`, not the copy. A generic as declared (in Module::genericStructs / genericFunctions, its types `Param`) has its
  // `params` (`T`); `notParametric` says why its body does not work for every type (`value + 1` on a T), "" when it does.
  struct POLYIR_API Generic {
    String of{};                 // the generic's name (`Box`), "" for a declaration that is not from one
    Vector<TypePtr> arguments{};  // what each type parameter stands for (`i64`; `T` itself in the generic as declared)
    Vector<String> params{};     // the type parameters, in the generic as declared only
    String notParametric{};
  };

  struct POLYIR_API StructDef {
    String name;
    Vector<Field> fields;
    String owner{};  // the function that owns the type (`type greet.Args` -> "greet"), "" for an ordinary struct
    String module{};  // the module or struct it is declared in (`Geometry`, `Geometry.Point`), "" at the top level; its name begins with it
    bool internal = false;  // `internal`: only the code around it (its module, its outer struct, or its file) uses it
    String outer{};  // the struct it is declared in (`Point` for `Point.Builder`), "" for one at the top level or in a module
    Vector<Annotation> annotations{};
    Generic generic{};
    String origin{};  // as for a function
  };

  // What a function is to a GPU: not a kernel at all, a compute kernel (one thread per element), a graphics stage, or a reduction. A `Reduce` function is
  // the COMBINE of a reduction, `(a: f32, b: f32) -> f32` and associative (a sum, a minimum, ...): the GPU folds a whole array into one number in a single
  // launch of one workgroup and the combine is called as the pairs are merged.
  enum class KernelStage { None, Compute, Vertex, Fragment, Reduce };

  // How a kernel parameter reaches the GPU (the numbers PolyirKernelRoles uses): an element of the input array, one value every thread sees, a whole
  // array to read anywhere, the thread's own element number, a whole array to read and write, workgroup scratch memory. The graphics stages add the
  // vertex attribute, the value passed on from the vertex stage to the fragment stage (an input of the fragment stage, an `Output` of the vertex stage with
  // the same name), a value the stage writes for the next stage or the screen, and the vertex stage's clip-space position (`gl_Position`).
  enum class ParamRole : int { Element = 0, Uniform = 1, Buffer = 2, Index = 3, Writable = 4, Workgroup = 5, Attribute = 6, Varying = 7, Output = 8, Position = 9 };

  struct POLYIR_API FuncDef {
    String name;
    Vector<Parameter> parameters;
    TypePtr result;
    StmtList body;
    bool isCoroutine = false;
    Span at;
    KernelStage stage = KernelStage::None;
    Vector<ParamRole> roles{};  // one per parameter when `stage` is not None
    String origin{};  // the imported file it was written in, "" for the checked file's own (and the library's): a translation writes its own
    String display{};  // the name the function reads as when it differs from `name` (an overload `greet__ov__…` reads `greet`): a back end whose
    // language overloads may print it; `name` stays the unique key every lookup uses
    String module{};      // the module it is declared in, as for a struct (a method has its struct's)
    bool internal = false;  // `internal`: only the code around it (its module, its struct's methods, or its file) uses it
    String outer{};       // a `static func`'s struct (`Point` for `Point.builder`); "" for every other function
    Vector<Annotation> annotations{};
    Generic generic{};
  };

  // A function the program calls but does not define (a C library function, for example).
  struct POLYIR_API ExternDef {
    String name;
    Vector<Parameter> parameters;
    TypePtr result;
  };

  struct POLYIR_API GlobalDef {
    String name;
    TypePtr type;
    ExprPtr initializer;
    String origin{};  // as for a function
  };

  // An interface: its method signatures (the receiver left out, in vtable slot order) and the structs that declared `: Name`.
  struct POLYIR_API InterfaceMethod {
    String name;
    Vector<TypePtr> parameters;
    TypePtr result;
  };

  struct POLYIR_API InterfaceDef {
    String name;
    Vector<InterfaceMethod> methods;
    Vector<String> implementers;
    String module{};      // as for a struct
    bool internal = false;
    Vector<Annotation> annotations{};
    String origin{};      // as for a function
  };

  // A class or interface a host language provides (`extern type` / `extern interface`). `base` is the declared name and `typeArguments` what this
  // instance was made with when `name` is a specialization. `bindings` are "target=text" strings, e.g. "java=java.util.ArrayList".
  struct POLYIR_API HostType {
    String name;
    String base;
    Vector<TypePtr> typeArguments;
    bool isInterface = false;
    Vector<String> bindings;
  };

  // How a polyir type crosses to and from a target (`$` is the value in each template).
  struct POLYIR_API Bridge {
    TypePtr polyirType;
    String target;
    String hostType;
    String toHost;
    String fromHost;
  };

  // One member of a host type, as the extern function `function` (the receiver first for an instance member).
  struct POLYIR_API HostMember {
    String function;
    String owner{};
    String member{};
    bool isStatic = false;
    bool hasResult = true;
    Vector<String> bindings{};
  };

  // `import c "zlib.h"`, `import js "crc-32" as crc`: a module of the host `target` that the program uses, which only that target writes (an
  // `#include`, a `require`, an `import`). `alias` is empty when the import names none.
  struct POLYIR_API HostImport {
    String target;
    String module;
    String alias;
  };

  // LLVM IR written elsewhere (a function another builder compiled, see CompileFunctionToLlvm) that goes into the module as it is. `declarations` are the
  // lines it needs at module level (type definitions, constants, library declarations); identical lines from several fragments are written once.
  struct POLYIR_API RawIr {
    String text;
    Vector<String> declarations;
    Vector<String> metadata;  // debug-info node bodies numbered from 0 (`!0 = ...`); the module renumbers them, and the `!N` in `text`, after its own
    Vector<String> namedMetadata;  // "name=0,3": a named list of those nodes, e.g. "llvm.dbg.cu=1"; the module merges it with its own
  };

  // Which list of a Module a declaration is in, and where: the order the front end wrote them in.
  enum class DeclKind { Struct, Interface, HostType, Bridge, HostMember, Extern, Global, Function, HostImport };

  struct POLYIR_API DeclRef {
    DeclKind kind = DeclKind::Struct;
    size_t index = 0;
  };

  // A `//# from FILE:LINE[:COLUMN]` line of a program's text: the line after it came from that place (as C's `#line`), and the
  // lines after that from the lines after it. A program another front end wrote (nina-lite's `.poly`) names its own source so.
  struct POLYIR_API FromLine {
    String in;     // the file it is in: "" for the checked file, else the imported file (as a declaration's `origin` names it)
    int line = 0;  // the `//# from` line itself
    String file;
    int fromLine = 0;
    int fromColumn = 0;  // 0 when it names none
    String code;         // that line's text, when the `//# from` line gives it after the place (`//# from a.nina:3 var x = 0`)
  };

  // A whole program.
  struct POLYIR_API Module {
    String name = "program";
    String file{};  // the file that was checked, "" for a program checked from memory or read from HIR
    Vector<StructDef> structs;
    Vector<GlobalDef> globals;
    Vector<ExternDef> externs;
    Vector<FuncDef> functions;
    Vector<InterfaceDef> interfaces;
    Vector<HostType> hostTypes;
    Vector<Bridge> bridges;
    Vector<HostMember> hostMembers;
    Vector<HostImport> hostImports;
    Vector<RawIr> rawIr;  // added after the functions
    Vector<DeclRef> order;  // the declarations in the order they were written, when that is known (the reader and the front end set it)
    Vector<FromLine> fromLines{};  // where the text's lines came from, when it says (`//# from`); a translation's spans map through them
    // The generics as declared, their types `Param` (a struct's methods among the functions). Only a back end that writes generics reads them;
    // the code above is the copies made for each set of types.
    Vector<StructDef> genericStructs{};
    Vector<FuncDef> genericFunctions{};
  };

  // ── a module's declarations by name (null when there is none), and the order they were added in ──
  POLYIR_API const StructDef* FindStruct(const Module& m, const String& name);

  POLYIR_API const InterfaceDef* FindInterface(const Module& m, const String& name);

  POLYIR_API const FuncDef* FindFunction(const Module& m, const String& name);

  POLYIR_API const ExternDef* FindExtern(const Module& m, const String& name);

  POLYIR_API const GlobalDef* FindGlobal(const Module& m, const String& name);

  POLYIR_API int FieldIndex(const Module& m, const String& structName, const String& fieldName);  // -1 when there is none

  POLYIR_API size_t DeclarationCount(const Module& m);

  POLYIR_API Vector<size_t> Counts(const Module& m);  // how many of each kind there are (in DeclKind order)

  POLYIR_API void NoteAdded(Module& m, const Vector<size_t>& before);  // appends to `order` whatever was added since `before`

  POLYIR_API bool HasGeneric(const Generic& generic);  // true for a generic's copy and for a generic as declared

  // ── kernel stages, operators and builtins ──
  POLYIR_API const char* StageName(KernelStage stage);  // `compute`, `vertex`, `fragment` (`none` for a function that is no kernel)

  POLYIR_API KernelStage StageNamed(const String& name);  // the inverse; an unknown name is `None`

  POLYIR_API const char* BinOpName(BinOp op);

  POLYIR_API const char* UnOpName(UnOp op);

  POLYIR_API const char* BuiltinName(Builtin op);  // `len`, `push`, `isSome`: the spelling the HIR printer uses

  POLYIR_API bool IsComparison(BinOp op);

  POLYIR_API bool IsLogical(BinOp op);

  // The blocks a statement holds, in order: `body`, each arm's block (a `Select`), then `elseBody`. A walker over every
  // statement of a function visits these.
  POLYIR_API Vector<const StmtList*> BlocksOf(const Stmt& s);

  // ── a `Select` as the `if` chain it means ──
  // Each arm's test is `value == key` for each key, joined by `||` (`isSome(value)`, `isOk(value)`, `isCase(value, N)` for a
  // sum), and the default is the last `else`. A back end with no switch of its own runs this instead.
  POLYIR_API StmtList SelectAsIfs(const Stmt& select);

  POLYIR_API ExprPtr SelectKeyTest(const Expr& value, const Expr& key);  // the test of one key: `value == key`, or the sum case's test

  // The statements with every `Select` in them, however deep (in a closure too), as its `if` chain. What holds none is shared,
  // not copied. The Module form lowers every function and global: a back end with no switch of its own compiles that.
  POLYIR_API StmtList WithoutSelects(const StmtList& body);

  POLYIR_API Module WithoutSelects(const Module& module);

}  // namespace polyir
