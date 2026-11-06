// Globals that start with more than a constant: a container, a call, a record. Such a global is stored as zero, and a function the compiler writes
// (`polyir_init_globals`) assigns every one its starting value; the program's entry point calls it before `main` runs.
#include "emit/llvm/LlvmEmitter.hpp"
#include "polyir/Builder.hpp"

namespace polyir::llvm {

  namespace {

    const char* const kInitFunction = "polyir_init_globals";

    PolyIRBuilder builderFor(const Module& module) {
      PolyIRBuilder builder;

      for (const StructDef& structure : module.structs) {
        DefineStruct(builder, structure.name, structure.fields);
      }

      for (const GlobalDef& global : module.globals) {
        DefineGlobal(builder, global.name, global.type, global.initializer);
      }

      for (const FuncDef& definition : module.functions) {
        DeclareFunction(builder, definition.name, definition.parameters, definition.result);
      }

      return builder;
    }

    // Does any global start with more than a constant?
    bool hasComputedGlobal(const Module& module) {
      for (const GlobalDef& global : module.globals) {
        if (!IsConstantInit(global.initializer)) {
          return true;
        }
      }

      return false;
    }

    // The function that gives each non-constant global its value, written with a builder that knows the module.
    FuncDef initializerFunction(const Module& module) {
      PolyIRBuilder builder = builderFor(module);
      Block& body = DefineFunction(builder, kInitFunction, {}, MakeVoid()).body;

      for (const GlobalDef& global : module.globals) {
        if (!IsConstantInit(global.initializer)) {
          Assign(body, Global(builder, global.name), global.initializer);
        }
      }

      return *FindFunction(Build(builder), kInitFunction);
    }

  }  // namespace

  bool IsConstantInit(const ExprPtr& init) {
    const ExprKind kind = init->kind;

    return kind == ExprKind::ConstInt || kind == ExprKind::ConstFloat || kind == ExprKind::ConstBool || kind == ExprKind::ConstString;
  }

  bool HasGlobalInitializer(const Module& module) {
    return FindFunction(module, kInitFunction) != nullptr;
  }

  Module WithGlobalInitializers(const Module& module) {
    if (!hasComputedGlobal(module)) {
      return module;
    }

    Module out = module;
    out.functions.push_back(initializerFunction(module));

    return out;
  }

}  // namespace polyir::llvm
