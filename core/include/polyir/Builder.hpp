// PolyIRBuilder: makes a polyir Module from C++ without writing polyir source. Expressions are made by free functions (the ones that need
// to know the functions, structs and globals declared so far take the builder, so they can give every expression its type); statements are
// added to a Block, which a function body and every nested body (an `if` branch, a loop) are. A mistake (an unknown name, two operands of
// different types) throws BuildError.
//
//   PolyIRBuilder b;
//   FunctionBuilder& gcd = DefineFunction(b, "gcd", {{"a", MakeInt(64)}, {"b", MakeInt(64)}}, MakeInt(64));
//   Block& body = gcd.body;
//   WhileLoop(body, Ne(Local(body, "b"), IntConst(0)), [&](Block& loop) {
//       Let(loop, "rest", Rem(Local(loop, "a"), Local(loop, "b")));
//       SetLocal(loop, "a", Local(loop, "b"));
//       SetLocal(loop, "b", Local(loop, "rest"));
//   });
//   Return(body, Local(body, "a"));
//   Module program = Build(b);
#pragma once

#include "polyir/Std.hpp"
#include "polyir/Export.hpp"
#include "polyir/Hir.hpp"

#include <deque>
#include <functional>
#include <map>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace polyir {

  class POLYIR_API BuildError : public RuntimeError {
    public:
      using RuntimeError::RuntimeError;
  };

  struct PolyIRBuilder;
  struct FunctionBuilder;
  struct Block;

  using BlockBody = Function<void(Block&)>;
  using FieldInit = Pair<String, ExprPtr>;

  // What a call to a declared function needs to know.
  struct Signature {
    Vector<Parameter> parameters;
    TypePtr result;
  };

  // A name in scope while a function is built: a `var` is read as a slot (LoadSlot); anything else as a value (Local).
  struct Binding {
    TypePtr type;
    bool isVariable = false;
  };

  // A closure being built: the scopes below `floor` belong to the code around it, and the names it reads from there are its captures.
  struct ClosureScope {
    size_t floor = 0;
    Vector<String> captures;
  };

  // A list of statements, with the names in scope where they are written.
  struct POLYIR_API Block {
    PolyIRBuilder* owner = nullptr;
    FunctionBuilder* function = nullptr;
    StmtList* list = nullptr;
  };

  // A function being built. It lives in its builder's `functions` (a deque: its address holds), and `body` points back at it.
  struct POLYIR_API FunctionBuilder {
    FuncDef definition;
    Vector<Map<String, Binding>> scopes;
    Vector<ClosureScope> closures;
    Block body;
  };

  struct POLYIR_API PolyIRBuilder {
    Module module;
    Map<String, Signature> signatures;
    Deque<FunctionBuilder> functions;
    Vector<FuncDef> adopted;  // functions that came from sub-builders
    Vector<size_t> inherited;  // how many structs, globals, externs and interfaces a sub-builder started with
  };

  inline PolyIRBuilder MakeBuilder(String moduleName) {
    PolyIRBuilder made;
    made.module.name = move(moduleName);

    return made;
  }

  // ── blocks ──
  POLYIR_API Block Nested(const Block& block, StmtList& list);  // a block for `list` (a body of a statement made here) with a scope of its own

  POLYIR_API void Declare(const Block& block, const String& name, TypePtr type);  // a name in scope from here on (a loop counter, for instance)

  // ── names in scope (a parameter, a variable or a label parameter) ──
  POLYIR_API ExprPtr Local(const Block& block, const String& name);

  POLYIR_API TypePtr TypeOf(const Block& block, const String& name);

  // ── variables ──
  // `let` cannot be assigned to afterwards by a program that follows polyir's rules; the builder does not enforce that.
  POLYIR_API void Let(Block& block, const String& name, ExprPtr value);

  POLYIR_API void Var(Block& block, const String& name, TypePtr type, ExprPtr init = nullptr);

  POLYIR_API void Assign(Block& block, const ExprPtr& target, ExprPtr value);

  POLYIR_API void SetLocal(Block& block, const String& name, ExprPtr value);

  POLYIR_API void SetField(Block& block, const ExprPtr& object, const String& field, ExprPtr value);

  POLYIR_API void StoreIndex(Block& block, const ExprPtr& base, Vector<ExprPtr> indices, ExprPtr value);  // `xs[i] = v`

  // ── exceptions and coroutines ──
  // `tryCatch` runs `body`; a value thrown inside arrives in `catchBody` as `caught` (an i64).
  POLYIR_API void TryCatch(Block& block, const BlockBody& body, const String& caught, const BlockBody& catchBody);

  // `tryCleanup` runs `body`. Whatever leaves it by a throw (a polyir throw or, in a source language, a host exception) runs
  // `cleanup` first and then carries on out. It is a Try whose `name` is empty. A normal exit does not run it.
  POLYIR_API void TryCleanup(Block& block, const BlockBody& body, const BlockBody& cleanup);

  POLYIR_API void ThrowValue(Block& block, ExprPtr value);

  POLYIR_API void YieldValue(Block& block, ExprPtr value);  // only in a function made by `coroutine`

  // ── closures ──
  // A closure: `body` is written with the parameters in scope, and the outer names it reads are recorded as its captures.
  POLYIR_API ExprPtr ClosureLiteral(Block& block, Vector<Parameter> parameters, TypePtr result, const BlockBody& body);

  // ── closures and `try` in pieces, for callers that cannot pass a lambda (the C API) ──
  // Opens a closure, with its parameters in scope until CloseClosure.
  POLYIR_API SharedPtr<Expr> OpenClosure(Block& block, const Vector<Parameter>& parameters, TypePtr result);

  // The block the open closure's statements are written into.
  POLYIR_API Block ClosureBody(const Block& block, const SharedPtr<Expr>& open);

  // Closes it: the outer names its statements read are recorded as its captures.
  POLYIR_API ExprPtr CloseClosure(Block& block, const SharedPtr<Expr>& open);

  POLYIR_API StmtPtr AppendTry(Block& block, const String& caught);  // its `body` and `elseBody` (the catch) are written with nested(); declare `caught` in the second

  // ── returns and expression statements ──
  POLYIR_API void Return(Block& block, ExprPtr value);

  POLYIR_API void ReturnVoid(Block& block);

  POLYIR_API void Eval(Block& block, ExprPtr value);

  // ── structured control flow ──
  POLYIR_API void IfThen(Block& block, ExprPtr condition, const BlockBody& then);

  POLYIR_API void IfElse(Block& block, ExprPtr condition, const BlockBody& then, const BlockBody& otherwise);

  POLYIR_API void WhileLoop(Block& block, ExprPtr condition, const BlockBody& body);

  POLYIR_API void ForRange(Block& block, const String& counter, ExprPtr from, ExprPtr to, const BlockBody& body);  // counter runs from..to-1

  // ── labels, jumps and branches (control flow without nesting) ──
  // A label's parameters are variables from the label on.
  POLYIR_API void Label(Block& block, const String& name, Vector<Parameter> parameters = {});

  POLYIR_API void LabelParameter(Block& block, const Parameter& parameter);  // adds a parameter to the label made last (for callers that learn them one at a time)

  POLYIR_API void Jump(Block& block, const String& label, Vector<ExprPtr> arguments = {});

  POLYIR_API void Branch(Block& block, ExprPtr condition, const String& thenLabel, Vector<ExprPtr> thenArguments, const String& elseLabel,
                         Vector<ExprPtr> elseArguments);

  POLYIR_API void SwitchOn(Block& block, ExprPtr value, Vector<Pair<int64_t, String>> cases, const String& defaultLabel,
                           Vector<Vector<ExprPtr>> caseArguments = {}, Vector<ExprPtr> defaultArguments = {});  // arguments for each case's label, and the default's

  // ── the statements the lambda forms are made of, for callers that cannot pass a lambda (the C API) ──
  POLYIR_API StmtPtr AppendIf(Block& block, ExprPtr condition);

  POLYIR_API StmtPtr AppendWhile(Block& block, ExprPtr condition);

  POLYIR_API StmtPtr AppendFor(Block& block, const String& counter, ExprPtr from, ExprPtr to);

  // ── functions being built ──
  POLYIR_API void Declare(FunctionBuilder& function, const String& name, TypePtr type, bool isVariable = false);

  POLYIR_API bool IsVariable(const FunctionBuilder& function, const String& name);

  POLYIR_API TypePtr Lookup(FunctionBuilder& function, const String& name);  // null when the name is not in scope; remembers it as captured when a closure being built reads it

  // Makes the function a GPU kernel of `stage`, with one role per parameter (BuildError when the counts differ).
  POLYIR_API void MarkKernel(FunctionBuilder& function, KernelStage stage, Vector<ParamRole> roles);

  POLYIR_API TypePtr Yields(const FunctionBuilder& function);  // what the coroutine hands out (null in a plain function)

  // Starts a closure being built. While it is the innermost one, the names read from outside it are its captures.
  POLYIR_API void BeginClosure(FunctionBuilder& function);

  // Ends the innermost closure being built and gives its captures.
  POLYIR_API Vector<String> EndClosure(FunctionBuilder& function);

  inline void PushScope(FunctionBuilder& function) {
    function.scopes.emplace_back();
  }

  inline void PopScope(FunctionBuilder& function) {
    function.scopes.pop_back();
  }

  // ── declarations ──
  POLYIR_API void DefineStruct(PolyIRBuilder& builder, const String& name, Vector<Field> fields, const String& owner = "");

  POLYIR_API void DefineGlobal(PolyIRBuilder& builder, const String& name, TypePtr type, ExprPtr init);

  POLYIR_API void DeclareExtern(PolyIRBuilder& builder, const String& name, Vector<Parameter> parameters, TypePtr result);

  POLYIR_API void DeclareFunction(PolyIRBuilder& builder, const String& name, Vector<Parameter> parameters, TypePtr result);  // so a call can come before the definition

  POLYIR_API FunctionBuilder& DefineFunction(PolyIRBuilder& builder, const String& name, Vector<Parameter> parameters, TypePtr result);

  POLYIR_API FunctionBuilder& DefineCoroutine(PolyIRBuilder& builder, const String& name, Vector<Parameter> parameters, TypePtr yields);  // its result is coro<yields>

  // An interface; `methods` are the signatures without the receiver, in vtable slot order.
  POLYIR_API void DefineInterface(PolyIRBuilder& builder, const String& name, Vector<InterfaceMethod> methods);

  // The struct `structName` joins the interface `interfaceName` (BuildError when either does not exist).
  POLYIR_API void Implement(PolyIRBuilder& builder, const String& interfaceName, const String& structName);

  POLYIR_API void DeclareHostType(PolyIRBuilder& builder, HostType type);

  POLYIR_API void DefineBridge(PolyIRBuilder& builder, Bridge bridge);

  POLYIR_API void DeclareHostMember(PolyIRBuilder& builder, HostMember member);

  // ── expressions that read the declarations ──
  POLYIR_API ExprPtr Global(const PolyIRBuilder& builder, const String& name);

  POLYIR_API ExprPtr Call(const PolyIRBuilder& builder, const String& function, Vector<ExprPtr> arguments);

  POLYIR_API ExprPtr StructLit(const PolyIRBuilder& builder, const String& structName, Vector<FieldInit> fields);

  POLYIR_API ExprPtr FieldAccess(const PolyIRBuilder& builder, ExprPtr object, const String& fieldName);

  POLYIR_API ExprPtr FuncRef(const PolyIRBuilder& builder, const String& function);  // a function used as a value (a func type)

  POLYIR_API ExprPtr InterfaceBox(const PolyIRBuilder& builder, const String& interfaceName, ExprPtr structValue);

  POLYIR_API ExprPtr InterfaceCall(const PolyIRBuilder& builder, ExprPtr object, const String& method, Vector<ExprPtr> arguments);

  // ── expressions ──
  POLYIR_API ExprPtr IntConst(int64_t value, TypePtr type = nullptr);  // an i64 unless a type is given

  POLYIR_API ExprPtr FloatConst(double value, TypePtr type = nullptr);  // an f64 unless a type is given

  POLYIR_API ExprPtr BoolConst(bool value);

  POLYIR_API ExprPtr StringConst(const String& text);

  POLYIR_API ExprPtr Binary(BinOp op, ExprPtr left, ExprPtr right);

  POLYIR_API ExprPtr Unary(UnOp op, ExprPtr operand);

  POLYIR_API ExprPtr Cast(ExprPtr value, TypePtr to);

  POLYIR_API ExprPtr Coerce(ExprPtr value, TypePtr to);  // an integer-width fix, as the checker adds to a call argument

  POLYIR_API ExprPtr CallIndirect(ExprPtr callee, Vector<ExprPtr> arguments);  // through a func or closure value

  POLYIR_API ExprPtr Unsupported(const String& what, TypePtr type);

  POLYIR_API ExprPtr SlotRead(const String& name, TypePtr type);  // a `var` slot read

  // ── containers ──
  POLYIR_API ExprPtr VecLit(TypePtr type, Vector<ExprPtr> elements);  // `[a, b, c]` as the given container type

  POLYIR_API ExprPtr NewContainer(TypePtr type, Vector<ExprPtr> sizes = {});  // a new one from its size arguments

  POLYIR_API ExprPtr Index(ExprPtr base, Vector<ExprPtr> indices);  // an element read

  POLYIR_API ExprPtr Len(ExprPtr container);  // an i64

  POLYIR_API ExprPtr Push(ExprPtr list, ExprPtr value);  // adds to a mutarray (the result is the list)

  POLYIR_API ExprPtr Truncate(ExprPtr list, ExprPtr length);  // shrinks a mutarray to its first `length` elements (a longer length changes nothing); the result is the list

  POLYIR_API ExprPtr Remove(ExprPtr collection, ExprPtr key);  // takes a key out of a mutmap or mutset in place (an i1: it was there)

  POLYIR_API ExprPtr Has(ExprPtr collection, ExprPtr key);  // an i1

  POLYIR_API ExprPtr SetAdd(ExprPtr set, ExprPtr member);  // `s.add(x)` on a mutset (the result is the set)

  POLYIR_API ExprPtr Copy(ExprPtr container);

  POLYIR_API ExprPtr Freeze(ExprPtr container);  // the same array, map or set seen as its immutable sibling

  POLYIR_API ExprPtr Thaw(ExprPtr container);  // ... and as its mutable one

  POLYIR_API ExprPtr With(ExprPtr collection, Vector<ExprPtr> arguments);  // a changed copy of an immutable array, map or set

  // ── sum values ──
  POLYIR_API ExprPtr VariantLit(TypePtr type, int caseIndex, ExprPtr value);

  POLYIR_API ExprPtr VariantTag(ExprPtr variant);  // an i64

  POLYIR_API ExprPtr VariantIsCase(ExprPtr variant, int caseIndex);

  POLYIR_API ExprPtr VariantUnwrap(ExprPtr variant, int caseIndex);

  POLYIR_API ExprPtr Ok(TypePtr either, ExprPtr value);

  POLYIR_API ExprPtr Err(TypePtr either, ExprPtr value);

  POLYIR_API ExprPtr IsOk(ExprPtr either);

  POLYIR_API ExprPtr IsErr(ExprPtr either);

  POLYIR_API ExprPtr UnwrapOk(ExprPtr either);

  POLYIR_API ExprPtr UnwrapErr(ExprPtr either);

  POLYIR_API ExprPtr Some(ExprPtr value);  // an optional holding the value

  POLYIR_API ExprPtr None(TypePtr element);  // an empty optional

  POLYIR_API ExprPtr IsSome(ExprPtr optional);

  POLYIR_API ExprPtr IsNone(ExprPtr optional);

  POLYIR_API ExprPtr Unwrap(ExprPtr optional);

  // ── coroutines ──
  POLYIR_API ExprPtr Resume(ExprPtr coroutine);  // what it yields next

  POLYIR_API ExprPtr Done(ExprPtr coroutine);  // an i1

  POLYIR_API ExprPtr Yielded(ExprPtr value);  // hands `value` out like `yield`; an i1: whether the coroutine was abandoned at this suspend (only in a coroutine)

  POLYIR_API ExprPtr CloseCoroutine(ExprPtr coroutine);  // abandons a started, unfinished coroutine: it resumes once, sees `yielded` give true and finishes

  // ── other builtins, strings and operators ──
  // Any builtin with the result type spelled out, for the ones without a checked form above.
  POLYIR_API ExprPtr BuiltinCall(Builtin op, ExprPtr receiver, Vector<ExprPtr> arguments, TypePtr result);

  POLYIR_API ExprPtr StrEq(ExprPtr a, ExprPtr b);  // whether two strings hold the same text (an i1)

  POLYIR_API ExprPtr Concat(ExprPtr a, ExprPtr b);  // two strings joined

  inline ExprPtr Identical(ExprPtr l, ExprPtr r) {
    return Binary(BinOp::Identical, move(l), move(r));
  }  // `a === b`: the same object

  inline ExprPtr NotIdentical(ExprPtr l, ExprPtr r) {
    return Binary(BinOp::NotIdentical, move(l), move(r));
  }

  inline ExprPtr Add(ExprPtr l, ExprPtr r) {
    return Binary(BinOp::Add, move(l), move(r));
  }

  inline ExprPtr Sub(ExprPtr l, ExprPtr r) {
    return Binary(BinOp::Sub, move(l), move(r));
  }

  inline ExprPtr Mul(ExprPtr l, ExprPtr r) {
    return Binary(BinOp::Mul, move(l), move(r));
  }

  inline ExprPtr Div(ExprPtr l, ExprPtr r) {
    return Binary(BinOp::Div, move(l), move(r));
  }

  inline ExprPtr Rem(ExprPtr l, ExprPtr r) {
    return Binary(BinOp::Rem, move(l), move(r));
  }

  inline ExprPtr BitAnd(ExprPtr l, ExprPtr r) {
    return Binary(BinOp::And, move(l), move(r));
  }

  inline ExprPtr BitOr(ExprPtr l, ExprPtr r) {
    return Binary(BinOp::Or, move(l), move(r));
  }

  inline ExprPtr BitXor(ExprPtr l, ExprPtr r) {
    return Binary(BinOp::Xor, move(l), move(r));
  }

  inline ExprPtr Shl(ExprPtr l, ExprPtr r) {
    return Binary(BinOp::Shl, move(l), move(r));
  }

  inline ExprPtr Shr(ExprPtr l, ExprPtr r) {
    return Binary(BinOp::Shr, move(l), move(r));
  }

  inline ExprPtr Eq(ExprPtr l, ExprPtr r) {
    return Binary(BinOp::Eq, move(l), move(r));
  }

  inline ExprPtr Ne(ExprPtr l, ExprPtr r) {
    return Binary(BinOp::Ne, move(l), move(r));
  }

  inline ExprPtr Lt(ExprPtr l, ExprPtr r) {
    return Binary(BinOp::Lt, move(l), move(r));
  }

  inline ExprPtr Le(ExprPtr l, ExprPtr r) {
    return Binary(BinOp::Le, move(l), move(r));
  }

  inline ExprPtr Gt(ExprPtr l, ExprPtr r) {
    return Binary(BinOp::Gt, move(l), move(r));
  }

  inline ExprPtr Ge(ExprPtr l, ExprPtr r) {
    return Binary(BinOp::Ge, move(l), move(r));
  }

  inline ExprPtr LogAnd(ExprPtr l, ExprPtr r) {
    return Binary(BinOp::LogAnd, move(l), move(r));
  }

  inline ExprPtr LogOr(ExprPtr l, ExprPtr r) {
    return Binary(BinOp::LogOr, move(l), move(r));
  }

  inline ExprPtr Neg(ExprPtr x) {
    return Unary(UnOp::Neg, move(x));
  }

  inline ExprPtr LogNot(ExprPtr x) {
    return Unary(UnOp::LogNot, move(x));
  }

  inline ExprPtr BitNot(ExprPtr x) {
    return Unary(UnOp::BitNot, move(x));
  }

  // ── LLVM written elsewhere ──
  // Adds LLVM IR for functions (from `CompileFunctionToLlvm`, or written by hand) to the module as it is. Calls to those functions need their
  // signature: declare it with `DeclareFunction`.
  POLYIR_API void AddLlvm(PolyIRBuilder& builder, String text, Vector<String> declarations = {}, Vector<String> metadata = {},
                          Vector<String> namedMetadata = {});

  // ── sub-builders ──
  // A builder for part of a program. It starts with everything this one has declared (structs, globals, externs, interfaces, and the
  // signatures of its functions, so it can call them). It builds functions of its own, and several can be made and filled one after
  // another, each keeping its own scope.
  POLYIR_API PolyIRBuilder SubBuilder(const PolyIRBuilder& builder);

  // Brings what `sub` defined back into `builder`. A name defined in two sub-builders is a BuildError when the second is adopted.
  POLYIR_API void Adopt(PolyIRBuilder& builder, const PolyIRBuilder& sub);

  // ── the result ──
  POLYIR_API Module Build(const PolyIRBuilder& builder);  // the module so far, with the functions' statements

  POLYIR_API const Signature* SignatureOf(const PolyIRBuilder& builder, const String& function);

}  // namespace polyir
