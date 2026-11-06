// Small lookups over a checked HIR Module (polyir/Hir.hpp): find a declaration by name, a field's position, and the
// printable names of the HIR's enums. Everything else in the pipeline (the builder, the JSON and PBC readers, the
// emitters) asks these questions, so they live once here. The name tables are in the order of their enums.
#include "polyir/Std.hpp"
#include "polyir/Hir.hpp"

namespace polyir {

  namespace {

    // A template because a Module keeps one list per kind of declaration (structs, functions, ...) and each kind
    // has a `name`. The search is the same for every list.
    template <typename Item>
    const Item* findByName(const Vector<Item>& items, const String& name) {
      for (const Item& item : items) {
        if (item.name == name) {
          return &item;
        }
      }

      return nullptr;
    }

  }  // namespace

  const InterfaceDef* FindInterface(const Module& m, const String& wanted) {
    return findByName(m.interfaces, wanted);
  }

  const StructDef* FindStruct(const Module& m, const String& wanted) {
    return findByName(m.structs, wanted);
  }

  const FuncDef* FindFunction(const Module& m, const String& wanted) {
    return findByName(m.functions, wanted);
  }

  const ExternDef* FindExtern(const Module& m, const String& wanted) {
    return findByName(m.externs, wanted);
  }

  const GlobalDef* FindGlobal(const Module& m, const String& wanted) {
    return findByName(m.globals, wanted);
  }

  int FieldIndex(const Module& m, const String& structName, const String& fieldName) {
    const StructDef* def = FindStruct(m, structName);

    for (size_t i = 0; def != nullptr && i < def->fields.size(); i++) {
      if (def->fields[i].name == fieldName) {
        return static_cast<int>(i);
      }
    }

    return -1;
  }

  size_t DeclarationCount(const Module& m) {
    return m.structs.size() + m.interfaces.size() + m.hostTypes.size() + m.bridges.size() + m.hostMembers.size() + m.externs.size() + m.globals.size() +
      m.functions.size() + m.hostImports.size();
  }

  Vector<size_t> Counts(const Module& m) {
    return {m.structs.size(), m.interfaces.size(), m.hostTypes.size(), m.bridges.size(), m.hostMembers.size(), m.externs.size(), m.globals.size(),
      m.functions.size(), m.hostImports.size()};
  }

  // Records, in `m.order`, every declaration added since `before` (a `Counts` taken earlier). So the module remembers
  // the order its declarations were written in even though it keeps them in separate lists.
  void NoteAdded(Module& m, const Vector<size_t>& before) {
    const Vector<size_t> now = Counts(m);

    for (size_t kind = 0; kind < now.size(); kind++) {
      for (size_t i = before[kind]; i < now[kind]; i++) {
        m.order.push_back(DeclRef{static_cast<DeclKind>(kind), i});
      }
    }
  }

  bool HasGeneric(const Generic& generic) {
    return !generic.of.empty() || !generic.params.empty() || !generic.notParametric.empty();
  }

  const char* StageName(KernelStage stage) {
    static const char* const names[] = {"none", "compute", "vertex", "fragment", "reduce"};

    return names[static_cast<int>(stage)];
  }

  KernelStage StageNamed(const String& name) {
    for (KernelStage stage : {KernelStage::Compute, KernelStage::Vertex, KernelStage::Fragment, KernelStage::Reduce}) {
      if (name == StageName(stage)) {
        return stage;
      }
    }

    return KernelStage::None;
  }

  const char* BinOpName(BinOp op) {
    static const char* const names[] = {"Add", "Sub", "Mul", "Div", "Rem", "And", "Or", "Xor", "Shl", "Shr", "Eq", "Ne", "Lt", "Le", "Gt", "Ge", "LogAnd", "LogOr", "Identical", "NotIdentical"};

    return names[static_cast<int>(op)];
  }

  const char* UnOpName(UnOp op) {
    static const char* const names[] = {"Neg", "LogNot", "BitNot"};

    return names[static_cast<int>(op)];
  }

  const char* BuiltinName(Builtin op) {
    static const char* const names[] = {"len", "capacity", "rows", "cols", "dim", "push", "hash", "streq", "concat", "shuffle", "atomicLoad", "atomicStore",
      "atomicRmw", "cmpxchg", "fence", "tag", "isCase", "unwrapAt", "isOk", "isErr", "unwrapOk", "unwrapErr", "resume", "done", "ifaceCall", "ifaceBox",
      "copy", "free", "freeBox", "assocLive", "assocValue", "present", "with", "has", "add", "assocKey", "retype", "loc", "var", "isSome", "isNone", "unwrap", "truncate", "remove", "yielded", "close", "str", "charAt",
      "slice", "find", "print", "fail"};

    return names[static_cast<int>(op)];
  }

  bool IsComparison(BinOp op) {
    return op >= BinOp::Eq && op <= BinOp::Ge;
  }

  bool IsLogical(BinOp op) {
    return op == BinOp::LogAnd || op == BinOp::LogOr;
  }

}  // namespace polyir
