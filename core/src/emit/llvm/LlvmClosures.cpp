// Function values and closures.
//
//   a function value   the address of the function; a call through it passes the arguments as they are
//   a closure          a heap block of two cells: the address of its body and its environment (a block with one cell per captured name)
//
// A closure's body is a function of its own whose first parameter is the environment. A name the closure captures lives in a heap cell that the code
// around the closure and the closure share, so an assignment to it on either side is seen on the other.
#include "polyir/Std.hpp"
#include "emit/llvm/LlvmEmitter.hpp"

#include <algorithm>

namespace polyir::llvm {

  Value FuncRef(FunctionEmitter&, const Expr& expr) {
    return Value{"@" + QuotedName(SymbolName(expr.name)), expr.type};
  }

  namespace {

    TypePtr bytePointer() {
      return MakePointer(MakeInt(8));
    }

    void fillSignature(FuncDef& definition, const Expr& expr) {
      definition.parameters.push_back(Parameter{"__env", bytePointer()});

      for (size_t i = 0; i < expr.parameterNames.size(); i++) {
        definition.parameters.push_back(Parameter{expr.parameterNames[i], expr.parameterTypes[i]});
      }

      definition.result = expr.type->element;
    }

    FuncDef bodyOf(const Expr& expr, const String& name) {
      FuncDef definition;
      definition.name = name;
      fillSignature(definition, expr);

      for (const auto& s : expr.closureBody) {
        definition.body.push_back(s);
      }

      return definition;
    }

    bool inScopes(const Vector<Map<String, Slot>>& scopes, const String& name) {
      for (auto scope = scopes.rbegin(); scope != scopes.rend(); ++scope) {
        if (scope->count(name) != 0) {
          return true;
        }
      }

      return false;
    }

    String joinArguments(const String& leading, const String& rest) {
      return leading.empty() ? rest : (rest.empty() ? leading : leading + ", " + rest);
    }

  }  // namespace

  // The front end lists every name a closure mentions. The ones to capture are those that live in the code around it and are not its own parameters.
  bool IsCaptured(const FunctionEmitter& emitter, const Expr& closure, const String& name) {
    const auto& own = closure.parameterNames;

    return find(own.begin(), own.end(), name) == own.end() && inScopes(emitter.scopes_, name);
  }

  Vector<String> CaptureCells(FunctionEmitter& emitter, const Expr& expr, Vector<Capture>& captures) {
    Vector<String> cells;

    for (const String& captured : expr.captures) {
      if (IsCaptured(emitter, expr, captured)) {
        cells.push_back(CaptureCell(emitter, captured, captures));
      }
    }

    return cells;
  }

  String CaptureCell(FunctionEmitter& emitter, const String& name, Vector<Capture>& captures) {
    const Slot slot = Lookup(emitter, name);
    captures.push_back(Capture{name, slot.type});

    return Instruction(emitter, "load ptr, ptr " + slot.pointer);
  }

  Value EnvironmentOf(FunctionEmitter& emitter, const Vector<String>& cells) {
    const Value environment = cells.empty() ? Value{"null", bytePointer()} : Allocate(emitter, to_string(8 * cells.size()), Layout::References);

    for (size_t k = 0; k < cells.size(); k++) {
      Store(emitter, Value{cells[k], bytePointer()}, WordAddress(emitter, environment.text, static_cast<int64_t>(k)));
    }

    return environment;
  }

  // The two-cell closure block: the address of the body and the environment.
  Value ClosureBlock(FunctionEmitter& emitter, const String& body, const Value& environment, const TypePtr& type) {
    const Value closure = Allocate(emitter, "16", Layout::References);
    Store(emitter, Value{"@" + QuotedName(body), bytePointer()}, WordAddress(emitter, closure.text, 0));
    Store(emitter, environment, WordAddress(emitter, closure.text, 1));

    return Value{closure.text, type};
  }

  Value ClosureLit(FunctionEmitter& emitter, const Expr& expr) {
    const String name = SafeName(emitter.function_.name) + ".closure." + to_string(NextId(emitter.context_));
    Vector<Capture> captures;
    const Vector<String> cells = CaptureCells(emitter, expr, captures);
    const FuncDef body = bodyOf(expr, name);
    AddFunction(emitter.context_, EmitFunction(emitter.context_, body, captures));

    return ClosureBlock(emitter, name, EnvironmentOf(emitter, cells), expr.type);
  }

  // The address to call: a closure's body is loaded from its block, and its environment leads the arguments.
  String IndirectTarget(FunctionEmitter& emitter, const Value& callee, String& leading) {
    if (callee.type->kind != TypeKind::Closure) {
      return callee.text;
    }

    const String target = LoadFrom(emitter, WordAddress(emitter, callee.text, 0), bytePointer()).text;
    leading = "ptr " + LoadFrom(emitter, WordAddress(emitter, callee.text, 1), bytePointer()).text;

    return target;
  }

  bool ClosureBuiltin(FunctionEmitter& emitter, const Expr& expr, Value& out) {
    if (expr.builtin != Builtin::Free || expr.operands[0]->type->kind != TypeKind::Closure) {
      return false;
    }

    out = FreedClosure(emitter, expr);

    return true;
  }

  // `Free` of a closure gives back its block and its environment (when it has one). The cells the environment points to are shared with the code that
  // made the closure, so they stay.
  Value FreedClosure(FunctionEmitter& emitter, const Expr& expr) {
    const Value closure = EmitExpr(emitter, *expr.operands[0]);
    FreeEnvironment(emitter, LoadFrom(emitter, WordAddress(emitter, closure.text, 1), bytePointer()));
    FreeHint(emitter, closure.text);

    return Value{"", MakeVoid()};
  }

  namespace {

    // Ends the block with a branch to `whenSet` when the pointer is set, else to `whenNull`.
    void branchOnNull(FunctionEmitter& emitter, const Value& pointer, const String& whenSet, const String& whenNull) {
      const String isSet = Instruction(emitter, "icmp ne ptr " + pointer.text + ", null");
      Terminate(emitter, "br i1 " + isSet + ", label %" + whenSet + ", label %" + whenNull);
    }

  }  // namespace

  void FreeEnvironment(FunctionEmitter& emitter, const Value& environment) {
    const String release = UniqueName(emitter, "free_env");
    const String done = UniqueName(emitter, "freed");
    branchOnNull(emitter, environment, release, done);

    StartBlock(emitter, release);
    FreeHint(emitter, environment.text);
    BranchTo(emitter, done);
    StartBlock(emitter, done);
  }

  String CallInstruction(FunctionEmitter& emitter, const TypePtr& result, const String& target, const String& args) {
    if (IsVoid(*result)) {
      Line(emitter, "call void " + target + "(" + args + ")");
      return "";
    }

    return Instruction(emitter, "call " + LlvmTypeText(result) + " " + target + "(" + args + ")");
  }

  Value CallIndirect(FunctionEmitter& emitter, const Expr& expr) {
    const Value callee = EmitExpr(emitter, *expr.operands[0]);
    NoteCall(emitter);

    String leading;
    const String target = IndirectTarget(emitter, callee, leading);
    const Value result{CallInstruction(emitter, expr.type, target, joinArguments(leading, ArgumentList(emitter, expr, 1))), expr.type};
    PassOnThrownIfAny(emitter);

    return result;
  }

}  // namespace polyir::llvm
