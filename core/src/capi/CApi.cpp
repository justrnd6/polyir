// The C ABI (include/polyir/polyir.h) over PolyIRBuilder and the LLVM compiler. A C caller cannot hold C++ objects,
// so everything it makes (types, expressions, blocks, functions, ...) is kept here in a Context and handed back as a
// handle: the item's position in its list, counted from 1, so 0 can mean "failed". Most entry points run their work
// through `guarded`, which turns a thrown error into 0 plus a message the caller reads with polyir_error. The few that
// create or free a context, read text back out (the error, the HIR text, the output) or need no context at all (the
// version, running an executable) do not.
#include "polyir/Std.hpp"
#include "polyir/polyir.h"

#include "polyir/Builder.hpp"
#include "polyir/CApiExtension.hpp"
#include "polyir/Cli.hpp"
#include "polyir/Frontend.hpp"
#include "polyir/HirPrint.hpp"
#include "polyir/LlvmCompiler.hpp"

#include <deque>
#include <map>
#include <memory>

using namespace polyir;
using polyir::front::FrontResult;
using polyir::front::CheckFile;
using polyir::front::CheckSource;

namespace {

  // A list the caller fills one item at a time (arguments, struct fields, types, numbers) and then passes by handle.
  struct ArgList {
    Vector<ExprPtr> values;
    Vector<FieldInit> fields;
    Vector<TypePtr> types;
    Vector<int> ints;
  };

  // A closure being made through the C ABI.
  struct ClosureRecord {
    SharedPtr<Expr> open;
    Vector<Parameter> parameters;
    TypePtr result;
    Block parent;
    bool opened = false;
  };

  // An interface whose methods are still being added; the builder gets it whole (see `flush`).
  struct PendingInterface {
    String name;
    Vector<InterfaceMethod> methods;
    Vector<String> implementers;  // the structs that implement it
    bool defined = false;
  };

  // An `if` made through the C ABI: its statement, and the block it was added to (which knows the scope).
  struct IfRecord {
    StmtPtr statement;
    Block parent;
  };

  // A function or extern whose parameters are still being added; `built` is set once its body is opened.
  struct PendingFunction {
    String name;
    TypePtr result;
    Vector<Parameter> parameters;
    bool isExtern = false;
    bool declared = false;
    bool isCoroutine = false;
    FunctionBuilder* built = nullptr;
    int64_t body = 0;
    KernelStage stage = KernelStage::None;
    Vector<ParamRole> roles{};
  };

  struct PendingStruct {
    String name;
    Vector<Field> fields;
  };

  // Everything one caller has made, addressed by handle (an index plus one).
  struct Context {
    PolyIRBuilder builder;
    Vector<TypePtr> types;
    Vector<ExprPtr> exprs;
    Deque<ArgList> args;
    Deque<Block> blocks;
    Vector<IfRecord> ifs;
    Deque<ClosureRecord> closures;
    Deque<PendingInterface> interfaces;
    String text;
    Vector<PendingFunction> functions;
    Vector<PendingStruct> structs;
    size_t structsDone = 0;
    Vector<HostType> hostTypes;
    Vector<Bridge> bridges;
    Vector<HostMember> hostMembers;
    size_t hostTypesDone = 0;
    size_t bridgesDone = 0;
    size_t hostMembersDone = 0;
    Optional<Module> checked;  // the program polyir_check_source / polyir_check_file made: it replaces what the builder holds
    String checkedFile;         // the file polyir_check_file read: the LLVM module is named after it, as the polyir CLI names it
    String error;
    String output;
  };

  Map<int64_t, UniquePtr<Context>>& registry() {
    static Map<int64_t, UniquePtr<Context>> contexts;

    return contexts;
  }

  Context* find(int64_t id) {
    const auto found = registry().find(id);

    return found == registry().end() ? nullptr : found->second.get();
  }

  // `keep` stores an item and answers its handle; `at` finds the item a handle names. Templates because the Context
  // keeps many lists of different things; the Deque versions are for lists whose items must not move once made.
  template <typename Item>
  int64_t keep(Vector<Item>& items, Item item) {
    items.push_back(move(item));

    return static_cast<int64_t>(items.size());
  }

  template <typename Item>
  int64_t keep(Deque<Item>& items, Item item) {
    items.push_back(move(item));

    return static_cast<int64_t>(items.size());
  }

  template <typename Item>
  Item& at(Vector<Item>& items, int64_t handle, const char* what) {
    if (handle < 1 || handle > static_cast<int64_t>(items.size())) {
      throw BuildError(String("no such ") + what);
    }

    return items[static_cast<size_t>(handle - 1)];
  }

  template <typename Item>
  Item& at(Deque<Item>& items, int64_t handle, const char* what) {
    if (handle < 1 || handle > static_cast<int64_t>(items.size())) {
      throw BuildError(String("no such ") + what);
    }

    return items[static_cast<size_t>(handle - 1)];
  }

  int64_t typeHandle(Context& context, TypePtr type) {
    return keep(context.types, move(type));
  }

  int64_t exprHandle(Context& context, ExprPtr expr) {
    return keep(context.exprs, move(expr));
  }

  const TypePtr& typeAt(Context& context, int64_t handle) {
    return at(context.types, handle, "type");
  }

  const ExprPtr& exprAt(Context& context, int64_t handle) {
    return at(context.exprs, handle, "expression");
  }

  Block& blockAt(Context& context, int64_t handle) {
    return at(context.blocks, handle, "block");
  }

  int64_t blockHandle(Context& context, Block block) {
    return keep(context.blocks, move(block));
  }

  Vector<ExprPtr> valuesOf(Context& context, int64_t list) {
    return list == 0 ? Vector<ExprPtr>{}
    : at(context.args, list, "argument list").values;
  }

  Vector<TypePtr> typesOf(Context& context, int64_t list) {
    return list == 0 ? Vector<TypePtr>{}
    : at(context.args, list, "argument list").types;
  }

  void flushStructs(Context& context) {
    for (; context.structsDone < context.structs.size(); context.structsDone++) {
      DefineStruct(context.builder, context.structs[context.structsDone].name, context.structs[context.structsDone].fields);
    }
  }

  void flushInterfaces(Context& context) {
    for (PendingInterface& pending : context.interfaces) {
      if (!pending.defined) {
        DefineInterface(context.builder, pending.name, pending.methods);
        pending.defined = true;
      }
    }
  }

  void flushImplementers(Context& context) {
    for (PendingInterface& pending : context.interfaces) {
      for (const String& structure : pending.implementers) {
        Implement(context.builder, pending.name, structure);
      }

      pending.implementers.clear();
    }
  }

  void flushExterns(Context& context) {
    for (PendingFunction& pending : context.functions) {
      if (pending.isExtern && !pending.declared) {
        DeclareExtern(context.builder, pending.name, pending.parameters, pending.result);
        pending.declared = true;
      }
    }
  }

  // The host declarations are for the back ends; the builder keeps them in the module and the LLVM compiler ignores them.
  void flushHostDeclarations(Context& context) {
    for (; context.hostTypesDone < context.hostTypes.size(); context.hostTypesDone++) {
      DeclareHostType(context.builder, context.hostTypes[context.hostTypesDone]);
    }

    for (; context.bridgesDone < context.bridges.size(); context.bridgesDone++) {
      DefineBridge(context.builder, context.bridges[context.bridgesDone]);
    }

    for (; context.hostMembersDone < context.hostMembers.size(); context.hostMembersDone++) {
      DeclareHostMember(context.builder, context.hostMembers[context.hostMembersDone]);
    }
  }

  // Struct and extern declarations are collected a piece at a time; the builder gets them whole, before anything needs them.
  void flush(Context& context) {
    flushStructs(context);
    flushInterfaces(context);
    flushImplementers(context);
    flushExterns(context);
    flushHostDeclarations(context);
  }

  // `Work` is the lambda an entry point passes: it takes the Context and answers a handle (or 1 for done).
  template <typename Work>
  int64_t attempt(Context& context, Work work, bool needsDeclarations) {
    context.error.clear();

    if (needsDeclarations) {
      flush(context);
    }

    return work(context);
  }

  int64_t failed(Context& context, const Exception& problem) {
    context.error = problem.what();

    return 0;
  }

  // Runs `work`; a thrown error becomes 0 and the reason.
  template <typename Work>
  int64_t guarded(int64_t id, Work work, bool needsDeclarations = false) {
    Context* context = find(id);

    if (context == nullptr) {
      return 0;
    }

    try {
      return attempt(*context, work, needsDeclarations);
    } catch (const Exception& problem) {
      return failed(*context, problem);
    }
  }

  // Runs `work` like `guarded`, after first handing the builder the declarations collected so far (see `flush`). It is for
  // the entry points whose work may need a struct, interface, extern or host declaration to be in the builder already.
  template <typename Work>
  int64_t guardedAfterFlush(int64_t id, Work work) {
    return guarded(id, work, true);
  }

  int64_t addExternParam(Context& context, int64_t function, const char* name, int64_t type) {
    PendingFunction& pending = at(context.functions, function, "function");

    if (pending.declared) {
      throw BuildError("the parameters of " + pending.name + " come before anything that uses it");
    }

    pending.parameters.push_back(Parameter{name, typeAt(context, type)});

    return int64_t{1};
  }

  int64_t addFunctionParam(Context& context, int64_t function, const char* name, int64_t type) {
    PendingFunction& pending = at(context.functions, function, "function");

    if (pending.built != nullptr) {
      throw BuildError("the parameters of " + pending.name + " come before its body");
    }

    pending.parameters.push_back(Parameter{name, typeAt(context, type)});

    return int64_t{1};
  }

  // The function is defined in the builder the first time its body is asked for; later asks get the same block.
  void defineFunction(Context& context, PendingFunction& pending) {
    pending.built = pending.isCoroutine ? &DefineCoroutine(context.builder, pending.name, pending.parameters, pending.result)
                                        : &DefineFunction(context.builder, pending.name, pending.parameters, pending.result);

    if (pending.stage != KernelStage::None) {
      MarkKernel(*pending.built, pending.stage, pending.roles);
    }

    pending.body = blockHandle(context, pending.built->body);
  }

  int64_t openFunctionBody(Context& context, int64_t function) {
    PendingFunction& pending = at(context.functions, function, "function");

    if (pending.built == nullptr) {
      defineFunction(context, pending);
    }

    return pending.body;
  }

  int64_t addInterfaceMethod(Context& context, int64_t interfaceHandle, const char* name, int64_t params, int64_t result) {
    PendingInterface& pending = at(context.interfaces, interfaceHandle, "interface");

    if (pending.defined) {
      throw BuildError("the methods of " + pending.name + " come before anything that uses it");
    }

    pending.methods.push_back(InterfaceMethod{name, typesOf(context, params), typeAt(context, result)});

    return int64_t{1};
  }

  int64_t addImplementer(Context& context, const char* interfaceName, const char* structure) {
    for (PendingInterface& pending : context.interfaces) {
      if (pending.name == interfaceName) {
        pending.implementers.push_back(structure);
        return int64_t{1};
      }
    }

    throw BuildError(String("no interface called '") + interfaceName + "'");
  }

  int64_t addHostBinding(Vector<String>& bindings, size_t done, int64_t handle, const char* binding) {
    if (done >= static_cast<size_t>(handle)) {
      throw BuildError("the bindings of a host declaration come before anything that uses it");
    }

    bindings.push_back(binding);

    return int64_t{1};
  }

  int64_t addClosureParam(Context& context, int64_t closure, const char* name, int64_t type) {
    ClosureRecord& record = at(context.closures, closure, "closure");

    if (record.opened) {
      throw BuildError("the parameters of a closure come before its body");
    }

    record.parameters.push_back(Parameter{name, typeAt(context, type)});

    return int64_t{1};
  }

  int64_t openClosureBody(Context& context, int64_t closure) {
    ClosureRecord& record = at(context.closures, closure, "closure");

    if (!record.opened) {
      record.open = OpenClosure(record.parent, record.parameters, record.result);
      record.opened = true;
    }

    return blockHandle(context, ClosureBody(record.parent, record.open));
  }

  // The program to compile or print: the checked source when there is one, else what the builder has made.
  Module programOf(Context& context) {
    if (context.checked) {
      return *context.checked;
    }

    flush(context);

    return Build(context.builder);
  }

  // The checked file's name and directory, as the polyir CLI gives them (`compileOptions`); none for a built program.
  CompileOptions optionsOf(const Context& context) {
    CompileOptions options;

    if (context.checkedFile.empty()) {
      return options;
    }

    const Path full = absolute(Path(context.checkedFile));
    options.sourceFile = full.filename().string();
    options.sourceDirectory = full.parent_path().string();

    return options;
  }

  int64_t compileProgram(Context& context) {
    const CompileResult result = CompileToLlvm(programOf(context), optionsOf(context));

    if (!Ok(result)) {
      throw BuildError(result.errors[0]);
    }

    context.output = result.ir;

    return int64_t{1};
  }

  // The module as text; the declarations made so far are in it, a mistake in them gives the reason instead.
  String hirTextOf(Context& context) {
    try {
      return ModuleText(programOf(context));
    } catch (const Exception& problem) {
      return context.error = problem.what();
    }
  }

  // What the command line prints for a program that does not check: the headline and the messages indented under it.
  String failureText(const FrontResult& result) {
    String text = result.headline + ":";

    for (const String& error : result.errors) {
      text += "\n  " + error;
    }

    return text;
  }

  int64_t keepChecked(Context& context, const FrontResult& result) {
    if (!Ok(result)) {
      throw BuildError(failureText(result));
    }

    if (!result.nativeProblems.empty()) {
      throw BuildError("compile failed:\n  " + result.nativeProblems[0]);
    }

    context.checked = result.module;

    return int64_t{1};
  }

}  // namespace

extern "C" {

  int64_t polyir_context_new(void) {
    static int64_t next = 1;
    registry()[next] = make_unique<Context>();

    return next++;
  }

  void polyir_context_free(int64_t contextId) {
    registry().erase(contextId);
  }

  const char* polyir_version(void) {
    return "0.1.0";
  }

  const char* polyir_error(int64_t contextId) {
    Context* context = find(contextId);

    return context == nullptr ? "no such context" : context->error.c_str();
  }

  // ── types ──

  int64_t polyir_type_int(int64_t contextId, int32_t bits) {
    return guarded(contextId, [&](Context& context) { return typeHandle(context, MakeInt(bits)); });
  }

  int64_t polyir_type_uint(int64_t contextId, int32_t bits) {
    return guarded(contextId, [&](Context& context) { return typeHandle(context, MakeUInt(bits)); });
  }

  int64_t polyir_type_float(int64_t contextId, int32_t bits) {
    return guarded(contextId, [&](Context& context) { return typeHandle(context, MakeFloat(bits)); });
  }

  int64_t polyir_type_string(int64_t contextId) {
    return guarded(contextId, [&](Context& context) { return typeHandle(context, MakeString()); });
  }

  int64_t polyir_type_void(int64_t contextId) {
    return guarded(contextId, [&](Context& context) { return typeHandle(context, MakeVoid()); });
  }

  int64_t polyir_type_struct(int64_t contextId, const char* name) {
    return guarded(contextId, [&](Context& context) { return typeHandle(context, MakeStruct(name)); });
  }

  // The container and sum types: each takes its parts and makes the type.
  int64_t polyir_type_pointer(int64_t contextId, int64_t element) {
    return guarded(contextId, [&](Context& context) { return typeHandle(context, MakePointer(typeAt(context, element))); });
  }

  int64_t polyir_type_func(int64_t contextId, int64_t params, int64_t result) {
    return guarded(contextId, [&](Context& context) { return typeHandle(context, MakeFunction(typesOf(context, params), typeAt(context, result))); });
  }

  int64_t polyir_type_closure(int64_t contextId, int64_t params, int64_t result) {
    return guarded(contextId, [&](Context& context) { return typeHandle(context, MakeClosure(typesOf(context, params), typeAt(context, result))); });
  }

  int64_t polyir_type_matrix(int64_t contextId, int64_t element, int32_t size) {
    return guarded(contextId, [&](Context& context) { return typeHandle(context, MakeMatrix(typeAt(context, element), size)); });
  }

  int64_t polyir_type_vec(int64_t contextId, int64_t element, int32_t count) {
    return guarded(contextId, [&](Context& context) { return typeHandle(context, MakeVec(typeAt(context, element), count)); });
  }

  int64_t polyir_type_fixed_array(int64_t contextId, int64_t element, int32_t size) {
    return guarded(contextId, [&](Context& context) { return typeHandle(context, MakeFixedArray(typeAt(context, element), size)); });
  }

  int64_t polyir_type_buffer(int64_t contextId, int64_t element, int32_t size) {
    return guarded(contextId, [&](Context& context) { return typeHandle(context, MakeBuffer(typeAt(context, element), size)); });
  }

  int64_t polyir_type_array(int64_t contextId, int64_t element, int32_t isMutable) {
    return guarded(contextId, [&](Context& context) { return typeHandle(context, MakeDynamicArray(typeAt(context, element), isMutable != 0)); });
  }

  int64_t polyir_type_sparse(int64_t contextId, int64_t element) {
    return guarded(contextId, [&](Context& context) { return typeHandle(context, MakeSparse(typeAt(context, element))); });
  }

  int64_t polyir_type_map(int64_t contextId, int64_t key, int64_t value, int32_t isMutable) {
    return guarded(contextId, [&](Context& context) { return typeHandle(context, MakeHashmap(typeAt(context, key), typeAt(context, value), isMutable != 0)); });
  }

  int64_t polyir_type_set(int64_t contextId, int64_t element, int32_t isMutable) {
    return guarded(contextId, [&](Context& context) { return typeHandle(context, MakeSet(typeAt(context, element), isMutable != 0)); });
  }

  int64_t polyir_type_grid(int64_t contextId, int64_t element, int32_t rows, int32_t cols) {
    return guarded(contextId, [&](Context& context) { return typeHandle(context, MakeGrid(typeAt(context, element), rows, cols)); });
  }

  int64_t polyir_type_tensor(int64_t contextId, int64_t element, int64_t dims) {
    return guarded(contextId, [&](Context& context) { return typeHandle(context, MakeTensor(typeAt(context, element), at(context.args, dims, "argument list").ints)); });
  }

  int64_t polyir_type_variant(int64_t contextId, int64_t cases) {
    return guarded(contextId, [&](Context& context) { return typeHandle(context, MakeVariant(typesOf(context, cases))); });
  }

  int64_t polyir_type_either(int64_t contextId, int64_t ok, int64_t err) {
    return guarded(contextId, [&](Context& context) { return typeHandle(context, MakeEither(typeAt(context, ok), typeAt(context, err))); });
  }

  int64_t polyir_type_optional(int64_t contextId, int64_t element) {
    return guarded(contextId, [&](Context& context) { return typeHandle(context, MakeOptional(typeAt(context, element))); });
  }

  int64_t polyir_type_coro(int64_t contextId, int64_t yields) {
    return guarded(contextId, [&](Context& context) { return typeHandle(context, MakeCoroutine(typeAt(context, yields))); });
  }

  // ── declarations ──

  int64_t polyir_struct_begin(int64_t contextId, const char* name) {
    return guarded(contextId, [&](Context& context) { return keep(context.structs, PendingStruct{.name = name}); });
  }

  int64_t polyir_struct_field(int64_t contextId, int64_t structure, const char* name, int64_t type, int32_t isMutable) {
    return guarded(contextId, [&](Context& context) {
      if (context.structsDone >= static_cast<size_t>(structure)) {
        throw BuildError("the fields of a struct come before anything that uses it");
      }

      at(context.structs, structure, "struct").fields.push_back(Field{name, typeAt(context, type), isMutable != 0});

      return int64_t{1};
    });
  }

  int64_t polyir_global(int64_t contextId, const char* name, int64_t type, int64_t init) {
    return guardedAfterFlush(contextId, [&](Context& context) {
      DefineGlobal(context.builder, name, typeAt(context, type), exprAt(context, init));

      return int64_t{1};
    });
  }

  int64_t polyir_extern_begin(int64_t contextId, const char* name, int64_t result) {
    return guarded(contextId, [&](Context& context) { return keep(context.functions, PendingFunction{.name = name, .result = typeAt(context, result), .isExtern = true}); });
  }

  int64_t polyir_extern_param(int64_t contextId, int64_t function, const char* name, int64_t type) {
    return guarded(contextId, [&](Context& context) { return addExternParam(context, function, name, type); });
  }

  int64_t polyir_function_begin(int64_t contextId, const char* name, int64_t result) {
    return guarded(contextId, [&](Context& context) { return keep(context.functions, PendingFunction{.name = name, .result = typeAt(context, result)}); });
  }

  int64_t polyir_function_param(int64_t contextId, int64_t function, const char* name, int64_t type) {
    return guarded(contextId, [&](Context& context) { return addFunctionParam(context, function, name, type); });
  }

  int64_t polyir_function_body(int64_t contextId, int64_t function) {
    return guardedAfterFlush(contextId, [&](Context& context) { return openFunctionBody(context, function); });
  }

  // ── expressions ──

  int64_t polyir_const_int(int64_t contextId, int64_t type, int64_t value) {
    return guarded(contextId, [&](Context& context) { return exprHandle(context, IntConst(value, typeAt(context, type))); });
  }

  int64_t polyir_const_float(int64_t contextId, int64_t type, double value) {
    return guarded(contextId, [&](Context& context) { return exprHandle(context, FloatConst(value, typeAt(context, type))); });
  }

  int64_t polyir_const_bool(int64_t contextId, int32_t value) {
    return guarded(contextId, [&](Context& context) { return exprHandle(context, BoolConst(value != 0)); });
  }

  int64_t polyir_const_string(int64_t contextId, const char* text) {
    return guarded(contextId, [&](Context& context) { return exprHandle(context, StringConst(text)); });
  }

  int64_t polyir_local(int64_t contextId, int64_t block, const char* name) {
    return guarded(contextId, [&](Context& context) { return exprHandle(context, Local(blockAt(context, block), name)); });
  }

  int64_t polyir_global_ref(int64_t contextId, const char* name) {
    return guarded(contextId, [&](Context& context) { return exprHandle(context, Global(context.builder, name)); });
  }

  int64_t polyir_binary(int64_t contextId, int32_t op, int64_t left, int64_t right) {
    return guarded(contextId, [&](Context& context) { return exprHandle(context, Binary(static_cast<BinOp>(op), exprAt(context, left), exprAt(context, right))); });
  }

  int64_t polyir_unary(int64_t contextId, int32_t op, int64_t operand) {
    return guarded(contextId, [&](Context& context) { return exprHandle(context, Unary(static_cast<UnOp>(op), exprAt(context, operand))); });
  }

  int64_t polyir_cast(int64_t contextId, int64_t value, int64_t type) {
    return guarded(contextId, [&](Context& context) { return exprHandle(context, Cast(exprAt(context, value), typeAt(context, type))); });
  }

  int64_t polyir_field(int64_t contextId, int64_t object, const char* name) {
    return guardedAfterFlush(contextId, [&](Context& context) { return exprHandle(context, FieldAccess(context.builder, exprAt(context, object), name)); });
  }

  int64_t polyir_args_new(int64_t contextId) {
    return guarded(contextId, [&](Context& context) { return keep(context.args, ArgList{}); });
  }

  int64_t polyir_args_push(int64_t contextId, int64_t args, int64_t value) {
    return guarded(contextId, [&](Context& context) {
      at(context.args, args, "argument list").values.push_back(exprAt(context, value));

      return int64_t{1};
    });
  }

  int64_t polyir_args_field(int64_t contextId, int64_t args, const char* name, int64_t value) {
    return guarded(contextId, [&](Context& context) {
      at(context.args, args, "argument list").fields.emplace_back(name, exprAt(context, value));

      return int64_t{1};
    });
  }

  int64_t polyir_call(int64_t contextId, const char* function, int64_t args) {
    return guardedAfterFlush(contextId, [&](Context& context) { return exprHandle(context, Call(context.builder, function, valuesOf(context, args))); });
  }

  int64_t polyir_struct_lit(int64_t contextId, const char* structure, int64_t args) {
    return guardedAfterFlush(contextId, [&](Context& context) { return exprHandle(context, StructLit(context.builder, structure, at(context.args, args, "argument list").fields)); });
  }

  // ── statements ──

  int64_t polyir_let(int64_t contextId, int64_t block, const char* name, int64_t value) {
    return guarded(contextId, [&](Context& context) {
      Let(blockAt(context, block), name, exprAt(context, value));

      return int64_t{1};
    });
  }

  int64_t polyir_var(int64_t contextId, int64_t block, const char* name, int64_t type, int64_t init) {
    return guarded(contextId, [&](Context& context) {
      Var(blockAt(context, block), name, typeAt(context, type), init == 0 ? nullptr : exprAt(context, init));

      return int64_t{1};
    });
  }

  int64_t polyir_assign(int64_t contextId, int64_t block, int64_t target, int64_t value) {
    return guarded(contextId, [&](Context& context) {
      Assign(blockAt(context, block), exprAt(context, target), exprAt(context, value));

      return int64_t{1};
    });
  }

  int64_t polyir_set(int64_t contextId, int64_t block, const char* name, int64_t value) {
    return guarded(contextId, [&](Context& context) {
      SetLocal(blockAt(context, block), name, exprAt(context, value));

      return int64_t{1};
    });
  }

  int64_t polyir_ret(int64_t contextId, int64_t block, int64_t value) {
    return guarded(contextId, [&](Context& context) {
      Return(blockAt(context, block), exprAt(context, value));

      return int64_t{1};
    });
  }

  int64_t polyir_ret_void(int64_t contextId, int64_t block) {
    return guarded(contextId, [&](Context& context) {
      ReturnVoid(blockAt(context, block));

      return int64_t{1};
    });
  }

  int64_t polyir_eval(int64_t contextId, int64_t block, int64_t value) {
    return guarded(contextId, [&](Context& context) {
      Eval(blockAt(context, block), exprAt(context, value));

      return int64_t{1};
    });
  }

  int64_t polyir_if(int64_t contextId, int64_t block, int64_t condition) {
    return guarded(contextId, [&](Context& context) {
      Block& parent = blockAt(context, block);

      return keep(context.ifs, IfRecord{AppendIf(parent, exprAt(context, condition)), parent});
    });
  }

  int64_t polyir_if_then(int64_t contextId, int64_t ifHandle) {
    return guarded(contextId, [&](Context& context) {
      IfRecord& record = at(context.ifs, ifHandle, "if");

      return blockHandle(context, Nested(record.parent, record.statement->body));
    });
  }

  int64_t polyir_if_else(int64_t contextId, int64_t ifHandle) {
    return guarded(contextId, [&](Context& context) {
      IfRecord& record = at(context.ifs, ifHandle, "if");

      return blockHandle(context, Nested(record.parent, record.statement->elseBody));
    });
  }

  int64_t polyir_while(int64_t contextId, int64_t block, int64_t condition) {
    return guarded(contextId, [&](Context& context) {
      Block& parent = blockAt(context, block);
      StmtPtr statement = AppendWhile(parent, exprAt(context, condition));

      return blockHandle(context, Nested(parent, statement->body));
    });
  }

  int64_t polyir_for_range(int64_t contextId, int64_t block, const char* counter, int64_t from, int64_t to) {
    return guarded(contextId, [&](Context& context) {
      Block& parent = blockAt(context, block);
      StmtPtr statement = AppendFor(parent, counter, exprAt(context, from), exprAt(context, to));
      Block inner = Nested(parent, statement->body);
      Declare(inner, counter, exprAt(context, from)->type);

      return blockHandle(context, inner);
    });
  }

  int64_t polyir_label(int64_t contextId, int64_t block, const char* name) {
    return guarded(contextId, [&](Context& context) {
      Label(blockAt(context, block), name);

      return int64_t{1};
    });
  }

  int64_t polyir_label_param(int64_t contextId, int64_t block, const char* label, const char* name, int64_t type) {
    return guarded(contextId, [&](Context& context) {
      // LabelParameter adds to the label made last (callers call this right after polyir_label), so the label's name
      // is not needed; `(void)` tells the compiler it is unused on purpose.
      (void)label;
      LabelParameter(blockAt(context, block), Parameter{name, typeAt(context, type)});

      return int64_t{1};
    });
  }

  int64_t polyir_jump(int64_t contextId, int64_t block, const char* label, int64_t args) {
    return guarded(contextId, [&](Context& context) {
      Jump(blockAt(context, block), label, valuesOf(context, args));

      return int64_t{1};
    });
  }

  int64_t polyir_branch(int64_t contextId, int64_t block, int64_t condition, const char* thenLabel, int64_t thenArgs, const char* elseLabel, int64_t elseArgs) {
    return guarded(contextId, [&](Context& context) {
      Branch(blockAt(context, block), exprAt(context, condition), thenLabel, valuesOf(context, thenArgs), elseLabel, valuesOf(context, elseArgs));

      return int64_t{1};
    });
  }

  // ── more declarations ──

  int64_t polyir_coroutine_begin(int64_t contextId, const char* name, int64_t yields) {
    return guarded(contextId, [&](Context& context) { return keep(context.functions, PendingFunction{.name = name, .result = typeAt(context, yields), .isCoroutine = true}); });
  }

  int64_t polyir_interface_begin(int64_t contextId, const char* name) {
    return guarded(contextId, [&](Context& context) { return keep(context.interfaces, PendingInterface{.name = name}); });
  }

  int64_t polyir_interface_method(int64_t contextId, int64_t interfaceHandle, const char* name, int64_t params, int64_t result) {
    return guarded(contextId, [&](Context& context) { return addInterfaceMethod(context, interfaceHandle, name, params, result); });
  }

  int64_t polyir_implement(int64_t contextId, const char* interfaceName, const char* structure) {
    return guarded(contextId, [&](Context& context) { return addImplementer(context, interfaceName, structure); });
  }

  // ── host declarations (extern type / extern interface, bridges, host members) ──

  int64_t polyir_host_type(int64_t contextId, const char* name, const char* base, int32_t isInterface, int64_t typeArgs) {
    return guarded(contextId, [&](Context& context) { return keep(context.hostTypes, HostType{name, base, typesOf(context, typeArgs), isInterface != 0, {}}); });
  }

  int64_t polyir_host_type_binding(int64_t contextId, int64_t hostType, const char* binding) {
    return guarded(contextId, [&](Context& context) { return addHostBinding(at(context.hostTypes, hostType, "host type").bindings, context.hostTypesDone, hostType, binding); });
  }

  int64_t polyir_bridge(int64_t contextId, int64_t poly, const char* target, const char* hostType, const char* toHost, const char* fromHost) {
    return guarded(contextId, [&](Context& context) { return keep(context.bridges, Bridge{typeAt(context, poly), target, hostType, toHost, fromHost}); });
  }

  int64_t polyir_host_member(int64_t contextId, const char* function, const char* owner, const char* member, int32_t isStatic, int32_t hasResult) {
    return guarded(contextId, [&](Context& context) { return keep(context.hostMembers, HostMember{function, owner, member, isStatic != 0, hasResult != 0, {}}); });
  }

  int64_t polyir_host_member_binding(int64_t contextId, int64_t hostMember, const char* binding) {
    return guarded(contextId, [&](Context& context) { return addHostBinding(at(context.hostMembers, hostMember, "host member").bindings, context.hostMembersDone, hostMember, binding); });
  }

  // ── more expressions ──

  int64_t polyir_args_push_type(int64_t contextId, int64_t args, int64_t type) {
    return guarded(contextId, [&](Context& context) {
      at(context.args, args, "argument list").types.push_back(typeAt(context, type));

      return int64_t{1};
    });
  }

  int64_t polyir_args_push_int(int64_t contextId, int64_t args, int64_t value) {
    return guarded(contextId, [&](Context& context) {
      at(context.args, args, "argument list").ints.push_back(static_cast<int>(value));

      return int64_t{1};
    });
  }

  // The entry points that only make one expression share one shape, so a table macro writes them: POLYIR_EXPR(name,
  // (parameters), the expression to make) defines `int64_t name(parameters)`, which makes the expression inside
  // `guardedAfterFlush` and answers its handle.
#define POLYIR_EXPR(name, params, make) \
    int64_t name params { return guardedAfterFlush(contextId, [&](Context& context) { return exprHandle(context, make); }); }

  POLYIR_EXPR(polyir_func_ref, (int64_t contextId, const char* function), FuncRef(context.builder, function))
  POLYIR_EXPR(polyir_coerce, (int64_t contextId, int64_t value, int64_t type), Coerce(exprAt(context, value), typeAt(context, type)))
  POLYIR_EXPR(polyir_call_indirect, (int64_t contextId, int64_t callee, int64_t args), CallIndirect(exprAt(context, callee), valuesOf(context, args)))
  POLYIR_EXPR(polyir_unsupported, (int64_t contextId, const char* what, int64_t type), Unsupported(what, typeAt(context, type)))
  POLYIR_EXPR(polyir_vec_lit, (int64_t contextId, int64_t type, int64_t args), VecLit(typeAt(context, type), valuesOf(context, args)))
  POLYIR_EXPR(polyir_new_container, (int64_t contextId, int64_t type, int64_t args), NewContainer(typeAt(context, type), valuesOf(context, args)))
  POLYIR_EXPR(polyir_index, (int64_t contextId, int64_t base, int64_t args), Index(exprAt(context, base), valuesOf(context, args)))
  POLYIR_EXPR(polyir_len, (int64_t contextId, int64_t container), Len(exprAt(context, container)))
  POLYIR_EXPR(polyir_push, (int64_t contextId, int64_t list, int64_t value), Push(exprAt(context, list), exprAt(context, value)))
  POLYIR_EXPR(polyir_truncate, (int64_t contextId, int64_t list, int64_t length), Truncate(exprAt(context, list), exprAt(context, length)))
  POLYIR_EXPR(polyir_remove, (int64_t contextId, int64_t collection, int64_t key), Remove(exprAt(context, collection), exprAt(context, key)))
  POLYIR_EXPR(polyir_has, (int64_t contextId, int64_t collection, int64_t key), Has(exprAt(context, collection), exprAt(context, key)))
  POLYIR_EXPR(polyir_set_add, (int64_t contextId, int64_t set, int64_t member), SetAdd(exprAt(context, set), exprAt(context, member)))
  POLYIR_EXPR(polyir_copy, (int64_t contextId, int64_t container), Copy(exprAt(context, container)))
  POLYIR_EXPR(polyir_freeze, (int64_t contextId, int64_t container), Freeze(exprAt(context, container)))
  POLYIR_EXPR(polyir_thaw, (int64_t contextId, int64_t container), Thaw(exprAt(context, container)))
  POLYIR_EXPR(polyir_with, (int64_t contextId, int64_t collection, int64_t args), With(exprAt(context, collection), valuesOf(context, args)))
  POLYIR_EXPR(polyir_str_eq, (int64_t contextId, int64_t a, int64_t b), StrEq(exprAt(context, a), exprAt(context, b)))
  POLYIR_EXPR(polyir_concat, (int64_t contextId, int64_t a, int64_t b), Concat(exprAt(context, a), exprAt(context, b)))
  POLYIR_EXPR(polyir_variant_lit, (int64_t contextId, int64_t type, int32_t index, int64_t value), VariantLit(typeAt(context, type), index, exprAt(context, value)))
  POLYIR_EXPR(polyir_variant_tag, (int64_t contextId, int64_t variant), VariantTag(exprAt(context, variant)))
  POLYIR_EXPR(polyir_variant_is_case, (int64_t contextId, int64_t variant, int32_t index), VariantIsCase(exprAt(context, variant), index))
  POLYIR_EXPR(polyir_variant_unwrap, (int64_t contextId, int64_t variant, int32_t index), VariantUnwrap(exprAt(context, variant), index))
  POLYIR_EXPR(polyir_ok, (int64_t contextId, int64_t either, int64_t value), Ok(typeAt(context, either), exprAt(context, value)))
  POLYIR_EXPR(polyir_err, (int64_t contextId, int64_t either, int64_t value), Err(typeAt(context, either), exprAt(context, value)))
  POLYIR_EXPR(polyir_is_ok, (int64_t contextId, int64_t either), IsOk(exprAt(context, either)))
  POLYIR_EXPR(polyir_is_err, (int64_t contextId, int64_t either), IsErr(exprAt(context, either)))
  POLYIR_EXPR(polyir_unwrap_ok, (int64_t contextId, int64_t either), UnwrapOk(exprAt(context, either)))
  POLYIR_EXPR(polyir_unwrap_err, (int64_t contextId, int64_t either), UnwrapErr(exprAt(context, either)))
  POLYIR_EXPR(polyir_some, (int64_t contextId, int64_t value), Some(exprAt(context, value)))
  POLYIR_EXPR(polyir_none, (int64_t contextId, int64_t element), None(typeAt(context, element)))
  POLYIR_EXPR(polyir_is_some, (int64_t contextId, int64_t optional), IsSome(exprAt(context, optional)))
  POLYIR_EXPR(polyir_is_none, (int64_t contextId, int64_t optional), IsNone(exprAt(context, optional)))
  POLYIR_EXPR(polyir_unwrap, (int64_t contextId, int64_t optional), Unwrap(exprAt(context, optional)))
  POLYIR_EXPR(polyir_iface_box, (int64_t contextId, const char* interfaceName, int64_t value), InterfaceBox(context.builder, interfaceName, exprAt(context, value)))
  POLYIR_EXPR(polyir_iface_call, (int64_t contextId, int64_t object, const char* method, int64_t args), InterfaceCall(context.builder, exprAt(context, object), method, valuesOf(context, args)))
  POLYIR_EXPR(polyir_resume, (int64_t contextId, int64_t coroutine), Resume(exprAt(context, coroutine)))
  POLYIR_EXPR(polyir_done, (int64_t contextId, int64_t coroutine), Done(exprAt(context, coroutine)))

#undef POLYIR_EXPR

  int64_t polyir_closure_begin(int64_t contextId, int64_t block, int64_t result) {
    return guarded(contextId, [&](Context& context) { return keep(context.closures, ClosureRecord{.result = typeAt(context, result), .parent = blockAt(context, block)}); });
  }

  int64_t polyir_closure_param(int64_t contextId, int64_t closure, const char* name, int64_t type) {
    return guarded(contextId, [&](Context& context) { return addClosureParam(context, closure, name, type); });
  }

  int64_t polyir_closure_body(int64_t contextId, int64_t closure) {
    return guarded(contextId, [&](Context& context) { return openClosureBody(context, closure); });
  }

  int64_t polyir_closure_end(int64_t contextId, int64_t closure) {
    return guarded(contextId, [&](Context& context) {
      ClosureRecord& record = at(context.closures, closure, "closure");

      if (!record.opened) {
        throw BuildError("a closure needs its body before it ends");
      }

      return exprHandle(context, CloseClosure(record.parent, record.open));
    });
  }

  // ── more statements ──

  int64_t polyir_set_index(int64_t contextId, int64_t block, int64_t base, int64_t indices, int64_t value) {
    return guarded(contextId, [&](Context& context) {
      StoreIndex(blockAt(context, block), exprAt(context, base), valuesOf(context, indices), exprAt(context, value));

      return int64_t{1};
    });
  }

  int64_t polyir_try(int64_t contextId, int64_t block, const char* caught) {
    return guarded(contextId, [&](Context& context) {
      Block& parent = blockAt(context, block);

      return keep(context.ifs, IfRecord{AppendTry(parent, caught), parent});
    });
  }

  int64_t polyir_try_body(int64_t contextId, int64_t tryHandle) {
    return guarded(contextId, [&](Context& context) {
      IfRecord& record = at(context.ifs, tryHandle, "try");

      return blockHandle(context, Nested(record.parent, record.statement->body));
    });
  }

  int64_t polyir_try_catch(int64_t contextId, int64_t tryHandle) {
    return guarded(contextId, [&](Context& context) {
      IfRecord& record = at(context.ifs, tryHandle, "try");
      Block handler = Nested(record.parent, record.statement->elseBody);
      Declare(handler, record.statement->name, MakeInt(64));

      return blockHandle(context, handler);
    });
  }

  int64_t polyir_throw(int64_t contextId, int64_t block, int64_t value) {
    return guarded(contextId, [&](Context& context) {
      ThrowValue(blockAt(context, block), exprAt(context, value));

      return int64_t{1};
    });
  }

  int64_t polyir_yield(int64_t contextId, int64_t block, int64_t value) {
    return guarded(contextId, [&](Context& context) {
      YieldValue(blockAt(context, block), exprAt(context, value));

      return int64_t{1};
    });
  }

  // ── the program as text ──

  const char* polyir_hir_text(int64_t contextId) {
    Context* context = find(contextId);

    if (context == nullptr) {
      return "";
    }

    context->text = hirTextOf(*context);

    return context->text.c_str();
  }

  // ── source text ──

  int32_t polyir_check_source(int64_t contextId, const char* text) {
    return static_cast<int32_t>(guarded(contextId, [&](Context& context) {
      context.checkedFile.clear();

      return keepChecked(context, CheckSource(text));
    }));
  }

  int32_t polyir_check_file(int64_t contextId, const char* path) {
    return static_cast<int32_t>(guarded(contextId, [&](Context& context) {
      context.checkedFile = path;

      return keepChecked(context, CheckFile(path));
    }));
  }

  // ── compile ──

  int64_t polyir_function_kernel(int64_t contextId, int64_t function, const char* stage) {
    return guarded(contextId, [&](Context& context) {
      PendingFunction& pending = at(context.functions, function, "function");
      pending.stage = StageNamed(stage);

      if (pending.stage == KernelStage::None) {
        throw BuildError(String("unknown kernel stage ") + stage);
      }

      return int64_t{1};
    });
  }

  int64_t polyir_function_param_role(int64_t contextId, int64_t function, int32_t role) {
    return guarded(contextId, [&](Context& context) {
      at(context.functions, function, "function").roles.push_back(static_cast<ParamRole>(role));

      return int64_t{1};
    });
  }

  int32_t polyir_compile(int64_t contextId) {
    return static_cast<int32_t>(guardedAfterFlush(contextId, [&](Context& context) { return compileProgram(context); }));
  }

  int32_t polyir_build_executable(int64_t contextId, const char* path, int32_t release) {
    return static_cast<int32_t>(guardedAfterFlush(contextId, [&](Context& context) {
      if (compileProgram(context) == 0) {
        return int64_t{0};
      }

      String diagnostics;

      if (!BuildExecutable(context.output, path, release != 0, diagnostics)) {
        throw BuildError("build failed:\n  " + diagnostics);
      }

      return int64_t{1};
    }));
  }

  int32_t polyir_run_executable(const char* path) {
    return RunExecutable(path, {});
  }

  const char* polyir_output(int64_t contextId) {
    Context* context = find(contextId);

    return context == nullptr ? "" : context->output.c_str();
  }

  int64_t polyir_output_length(int64_t contextId) {
    Context* context = find(contextId);

    return context == nullptr ? 0 : static_cast<int64_t>(context->output.size());
  }

}  // extern "C"

namespace polyir {

  int32_t WriteContextOutput(int64_t contextId, const ProgramWriter& write) {
    return static_cast<int32_t>(guardedAfterFlush(contextId, [&](Context& context) {
      context.output = write(programOf(context));

      return int64_t{1};
    }));
  }

}  // namespace polyir
