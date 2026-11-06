// Sub-builders: part of a program built on its own and brought back into the builder it came from. A sub-builder
// starts as a copy of its parent (the module and the signatures so far) and remembers how many of each declaration it
// inherited, so that `Adopt` brings back only what the sub-builder added, and refuses a name the parent already has.
#include "polyir/Std.hpp"
#include "builder/BuilderSupport.hpp"

namespace polyir {

  using namespace polyir::checks;

  namespace {

    // A template because the module keeps one list per kind of declaration (structs, globals, ...), each with a `name`.
    template <typename Item>
    bool hasNamed(const Vector<Item>& items, const String& name) {
      return any_of(items.begin(), items.end(), [&](const Item& item) { return item.name == name; });
    }

    // Appends the items of `from` past the first `inherited` ones (those the sub-builder added) to `into`.
    template <typename Item>
    void addNew(Vector<Item>& into, const Vector<Item>& from, size_t inherited, const String& what) {
      for (size_t i = inherited; i < from.size(); i++) {
        if (hasNamed(into, from[i].name)) {
          Fail(what + " " + from[i].name + " is already defined");
        }

        into.push_back(from[i]);
      }
    }

    bool isFunctionDefined(const PolyIRBuilder& builder, const String& name) {
      const bool isOwn = any_of(builder.functions.begin(), builder.functions.end(),
        [&](const FunctionBuilder& function) { return function.definition.name == name; });

      return isOwn || hasNamed(builder.adopted, name) || FindFunction(builder.module, name) != nullptr;
    }

    void adoptFunctions(PolyIRBuilder& builder, const PolyIRBuilder& sub, const Module& part) {
      for (const FuncDef& function : part.functions) {
        if (isFunctionDefined(builder, function.name)) {
          Fail("function " + function.name + " is already defined");
        }

        builder.adopted.push_back(function);
        builder.signatures[function.name] = sub.signatures.at(function.name);
      }
    }

    // How many of a kind the sub-builder inherited (`inherited` is empty for a builder that is not a sub-builder).
    size_t inheritedCount(const Vector<size_t>& inherited, size_t kind) {
      return inherited.empty() ? 0 : inherited[kind];
    }

  }  // namespace

  void AddLlvm(PolyIRBuilder& builder, String text, Vector<String> declarations, Vector<String> metadata, Vector<String> namedMetadata) {
    builder.module.rawIr.push_back(RawIr{move(text), move(declarations), move(metadata), move(namedMetadata)});
  }

  // `inherited` lists the counts in the order Adopt reads them: structs, globals, externs, interfaces.
  PolyIRBuilder SubBuilder(const PolyIRBuilder& builder) {
    PolyIRBuilder sub;
    sub.module = builder.module;
    sub.signatures = builder.signatures;
    sub.inherited = {builder.module.structs.size(), builder.module.globals.size(), builder.module.externs.size(), builder.module.interfaces.size()};

    return sub;
  }

  void Adopt(PolyIRBuilder& builder, const PolyIRBuilder& sub) {
    const Module part = Build(sub);

    addNew(builder.module.structs, part.structs, inheritedCount(sub.inherited, 0), "struct");
    addNew(builder.module.globals, part.globals, inheritedCount(sub.inherited, 1), "global");
    addNew(builder.module.externs, part.externs, inheritedCount(sub.inherited, 2), "extern");
    addNew(builder.module.interfaces, part.interfaces, inheritedCount(sub.inherited, 3), "interface");
    adoptFunctions(builder, sub, part);
  }

}  // namespace polyir
