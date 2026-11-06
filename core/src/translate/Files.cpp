// Which file each declaration is written in (docs/translate.md). One .poly file is one file of the target: a translation writes
// the checked file's own declarations, and `use(name, path, names)` for each imported file whose declarations its code names. A
// library declaration (one with no file of its own) that an imported file's code uses is written by that file, and taken from
// it. A file with no `main` of its own is a library (NodesDecls.cpp gives it the facts `library` and `exports`).
//
//   ContainerTests.poly: import "Check.poly" ... offInt(...)   ->   use(Check, Check, [offInt]); Check's declarations not written
#include "translate/Nodes.hpp"

namespace polyir::translate {

  // A file's name as a name the code can write: its stem, each character a name cannot hold made `_` (`host-types` -> `host_types`).
  String FileName(const String& path) {
    String name = Path(path).stem().string();

    for (char& c : name) {
      c = isalnum(static_cast<unsigned char>(c)) || c == '_' ? c : '_';
    }

    return name;
  }

  namespace {

    // Whether a declaration at the top of the file at `path` is named `name` (`struct Biquad` in Biquad.poly).
    bool declaresAtTop(const Module& module, const String& path, const String& name) {
      const String origin = path == module.file ? "" : path;
      const FuncDef* function = FindFunction(module, name);
      const StructDef* def = FindStruct(module, name);
      const InterfaceDef* face = FindInterface(module, name);

      const bool isFunction = function != nullptr && function->origin == origin;

      return isFunction || (def != nullptr && def->origin == origin) || (face != nullptr && face->origin == origin);
    }

  }  // namespace

  // The name of the file as a class, as Java makes each file one. It is the file's name, with `File` after it when the file
  // declares that name at its top (`BiquadFile` for Biquad.poly, which holds `struct Biquad`).
  String FileClassName(const Module& module, const String& path) {
    const String name = path.empty() ? "Main" : FileName(path);

    return declaresAtTop(module, path, name) ? name + "File" : name;
  }

  namespace {

    // The name a file shows a declaration by: the first part of its path (`Geometry` for `Geometry.Point`).
    String shownName(const String& name) {
      return name.substr(0, name.find('.'));
    }

    // ── what a file's code names ──

    // The structs and interfaces a type holds, its element and parameter types included.
    void addTypeNames(const TypePtr& type, Set<String>& names) {
      if (!type) {
        return;
      }

      if (type->kind == TypeKind::Struct) {
        names.insert(type->name);
      }

      for (const TypePtr& parameter : type->parameters) {
        addTypeNames(parameter, names);
      }

      addTypeNames(type->element, names);
      addTypeNames(type->second, names);
    }

    // What a body names: the functions it calls or takes, the structs it makes, the globals it reads, and the types it holds.
    void addBodyNames(const StmtList& body, Set<String>& names) {
      CountIn(body, [&](const Expr& e) {
        const bool named = e.kind == ExprKind::Call || e.kind == ExprKind::StructLit || e.kind == ExprKind::Global || e.kind == ExprKind::FuncRef;

        if (named) {
          names.insert(e.name);
        }

        addTypeNames(e.type, names);

        return false;
      });
    }

    // The declarations a file writes: those of its origin ("" for the checked file), and `also`, less those taken from another file.
    struct Written {
      String origin;
      const Set<String>* also = nullptr;
      const Map<String, String>* taken = nullptr;
    };

    bool writes(const Written& written, const String& origin, const String& name) {
      if (written.taken != nullptr && written.taken->count(name) > 0) {
        return false;
      }

      return origin == written.origin || (written.also != nullptr && written.also->count(name) > 0);
    }

    void addFunctionNames(const Vector<FuncDef>& functions, const Written& written, Set<String>& names) {
      for (const FuncDef& function : functions) {
        if (!writes(written, function.origin, function.name)) {
          continue;
        }

        for (const Parameter& parameter : function.parameters) {
          addTypeNames(parameter.type, names);
        }

        addTypeNames(function.result, names);
        addBodyNames(function.body, names);
      }
    }

    void addStructNames(const Vector<StructDef>& structs, const Written& written, Set<String>& names) {
      for (const StructDef& def : structs) {
        for (const Field& field : writes(written, def.origin, def.name) ? def.fields : Vector<Field>{}) {
          addTypeNames(field.type, names);
        }
      }
    }

    Set<String> namesOf(const Module& module, const Written& written) {
      Set<String> names;
      addFunctionNames(module.functions, written, names);
      addFunctionNames(module.genericFunctions, written, names);
      addStructNames(module.structs, written, names);
      addStructNames(module.genericStructs, written, names);

      return names;
    }

    // A copy of a generic written once names its generic (`Box_i64` names `Box`), which an imported file may declare.
    void addCopiedGenerics(const NodeContext& context, Set<String>& names) {
      for (const StructDef& def : context.module->structs) {
        if (names.count(def.name) > 0 && context.generics.structs.count(def.generic.of) > 0) {
          names.insert(def.generic.of);
        }
      }

      for (const FuncDef& function : context.module->functions) {
        if (names.count(function.name) > 0 && context.generics.functions.count(function.generic.of) > 0) {
          names.insert(function.generic.of);
        }
      }
    }

    // ── the library's declarations an imported file writes ──

    // A declaration no file declares as it is: the library's, or a copy of a generic made for the types code uses. An imported
    // file whose code uses one writes it, as it does when it is translated alone, so the checked file takes it from there.
    template <typename Decl>
    bool unowned(const Decl* d) {
      return d != nullptr && ((d->origin.empty() && d->generic.of.empty()) || !d->generic.of.empty());
    }

    bool isLibrary(const Module& module, const String& name) {
      return unowned(FindFunction(module, name)) || unowned(FindStruct(module, name));
    }

    // The library declarations `file` writes: those its code uses, and those they use in turn. A file before it may have them.
    void addProvided(const Module& module, const String& file, Map<String, String>& provided) {
      Set<String> mine;

      for (bool grew = true; grew;) {
        const size_t before = mine.size();

        for (const String& name : namesOf(module, Written{file, &mine, nullptr})) {
          if (provided.count(name) == 0 && isLibrary(module, name)) {
            mine.insert(name);
          }
        }

        grew = mine.size() > before;
      }

      for (const String& name : mine) {
        provided[name] = file;
      }
    }

    // The imported files, by their paths.
    Set<String> importedPaths(const Module& module) {
      Set<String> files;

      for (const FuncDef& function : module.functions) {
        files.insert(function.origin);
      }

      for (const StructDef& def : module.structs) {
        files.insert(def.origin);
      }

      files.erase("");

      return files;
    }

    // ── where each imported declaration is ──

    // Where an imported declaration is (its file) and the name its file shows it by.
    struct Shown {
      String file;
      String name;
    };

    using ShownNames = Map<String, Shown>;

    // The declarations of imported files; a copy of a generic is the checked file's own, made for the types it uses.
    template <typename Decl>
    void addShown(ShownNames& shown, const Vector<Decl>& decls) {
      for (const Decl& d : decls) {
        if (!d.origin.empty() && d.generic.of.empty()) {
          shown[d.name] = Shown{d.origin, shownName(d.name)};
        }
      }
    }

    // A generic as declared is named by the generic (`Box`), as the code that uses it writes it.
    template <typename Decl>
    void addShownGenerics(ShownNames& shown, const Vector<Decl>& decls) {
      for (const Decl& d : decls) {
        if (!d.origin.empty()) {
          shown[d.generic.of] = Shown{d.origin, shownName(d.generic.of)};
        }
      }
    }

    // Interfaces and globals, which have no generics.
    template <typename Decl>
    void addShownPlain(ShownNames& shown, const Vector<Decl>& decls) {
      for (const Decl& d : decls) {
        if (!d.origin.empty()) {
          shown[d.name] = Shown{d.origin, shownName(d.name)};
        }
      }
    }

    // A method is shown by its struct's name.
    void addShownMethods(ShownNames& shown, const Module& module) {
      for (const FuncDef& function : module.functions) {
        const StructDef* owner = function.origin.empty() ? nullptr : OwnerOf(module, function);

        if (owner != nullptr) {
          shown[function.name] = Shown{function.origin, shownName(owner->name)};
        }
      }
    }

    // Every declaration another file writes, by its name.
    ShownNames shownNames(const Module& module, const Map<String, String>& provided) {
      ShownNames shown;
      addShown(shown, module.structs);
      addShown(shown, module.functions);
      addShownPlain(shown, module.interfaces);
      addShownPlain(shown, module.globals);

      addShownGenerics(shown, module.genericStructs);
      addShownGenerics(shown, module.genericFunctions);
      addShownMethods(shown, module);

      for (const auto& [name, file] : provided) {
        shown[name] = Shown{file, shownName(name)};
      }

      return shown;
    }

    // ── the uses ──

    // The path from the checked file's folder to `file`, without `.poly`: `Check`, `../Wave`.
    String pathFrom(const String& checked, const String& file) {
      const Path to = Path(file).replace_extension("");
      const Path folder = Path(checked).parent_path();

      return folder.empty() ? to.generic_string() : to.lexically_relative(folder).generic_string();
    }

    Node nameNode(const String& name) {
      Node node = MakeNode("name");
      AddText(node, "name", PartKind::Name, name);

      return node;
    }

    // `use(name, path, names)`: the file's own name, the path to it, and the names the checked file's code takes from it. A file
    // with its own `main` has the fact `program` (C++ writes it as a .cpp, not a header).
    Node useNode(const Module& module, const String& file, const Vector<String>& names) {
      Node node = MakeNode("use");
      AddText(node, "name", PartKind::Name, FileName(file));
      AddText(node, "path", PartKind::Text, pathFrom(module.file, file));
      Vector<Node> shown;

      for (const String& name : names) {
        shown.push_back(nameNode(name));
      }

      AddNodes(node, "names", PartKind::List, shown);
      const FuncDef* main = FindFunction(module, "main");

      if (main != nullptr && main->origin == file) {
        AddFact(node, "program");
      }

      return node;
    }

  }  // namespace

  namespace {

    // The generic a copy is made from (`drop` for `drop_i64`), "" for a declaration that is no copy.
    String genericOf(const Module& module, const String& name) {
      const FuncDef* function = FindFunction(module, name);
      const StructDef* def = FindStruct(module, name);

      return function != nullptr ? function->generic.of : (def != nullptr ? def->generic.of : "");
    }

  }  // namespace

  // A file that writes a copy of a generic writes the generic too, on a page that writes generics once.
  Map<String, String> ProvidedNames(const Module& module) {
    Map<String, String> provided;

    for (const String& file : importedPaths(module)) {
      addProvided(module, file, provided);
    }

    for (const auto& [name, file] : Map<String, String>(provided)) {
      if (const String of = genericOf(module, name); !of.empty()) {
        provided.emplace(of, file);
      }
    }

    return provided;
  }

  Set<String> OwnNames(const NodeContext& context) {
    Set<String> names = namesOf(*context.module, Written{"", nullptr, &context.provided});
    addCopiedGenerics(context, names);

    return names;
  }

  namespace {

    // Of an imported file (and not a copy of a generic, which is the checked file's own), or of the library and written by one.
    template <typename Decl>
    bool writtenElsewhere(const NodeContext& context, const Decl& d) {
      return (!d.origin.empty() && d.generic.of.empty()) || context.provided.count(d.name) > 0;
    }

  }  // namespace

  bool IsImported(const NodeContext& context, const DeclRef& ref) {
    const Module& module = *context.module;

    switch (ref.kind) {
      case DeclKind::Struct:
        return writtenElsewhere(context, module.structs[ref.index]);
      case DeclKind::Interface:
        return !module.interfaces[ref.index].origin.empty();
      case DeclKind::Function:
        return writtenElsewhere(context, module.functions[ref.index]);
      case DeclKind::Global:
        return !module.globals[ref.index].origin.empty();
      default:
        return false;
    }
  }

  Map<String, String> ImportedFiles(const Module& module, const Map<String, String>& provided) {
    Map<String, String> files;

    for (const auto& [name, shown] : shownNames(module, provided)) {
      files[name] = FileClassName(module, shown.file);
    }

    return files;
  }

  // One `use` per imported file the checked file's code names something of, by the file's path; its names in order.
  Vector<Node> UseNodes(const NodeContext& context) {
    const ShownNames shown = shownNames(*context.module, context.provided);
    Map<String, Vector<String>> names;

    for (const String& name : OwnNames(context)) {
      const auto found = shown.find(name);
      Vector<String>* taken = found == shown.end() ? nullptr : &names[found->second.file];

      if (taken != nullptr && find(taken->begin(), taken->end(), found->second.name) == taken->end()) {
        taken->push_back(found->second.name);
      }
    }

    Vector<Node> uses;

    for (const auto& [file, taken] : names) {
      uses.push_back(useNode(*context.module, file, taken));
    }

    return uses;
  }

}  // namespace polyir::translate
