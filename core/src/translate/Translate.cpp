// polyir as a source of the translate library (polyir/Translate.hpp): the pages built into libpolyir, where a target's page
// comes from, and a checked module made into a tree of polyir's forms and written by the page.
#include "polyir/Translate.hpp"
#include "translate/TargetTexts.hpp"
#include "translate/Nodes.hpp"
#include "polyir/translate/BuiltTree.hpp"

namespace polyir {

  using namespace translate;
  using std::filesystem::exists;
  using std::filesystem::is_regular_file;

  namespace {

    String lowered(String text) {
      for (char& c : text) {
        c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
      }

      return text;
    }

    // The canons built into the library that name `source`: polyir's own.
    Optional<Schema> builtInSchema(const String& source) {
      return source == PolyirSchema().name ? Optional<Schema>(PolyirSchema()) : nullopt;
    }

    Optional<Target> builtInNamed(const String& name) {
      for (const Target& target : BuiltInTargets()) {
        if (target.name == lowered(name)) {
          return target;
        }
      }

      return nullopt;
    }

    // A page from disk finds the page it starts from, and its source's canon, beside it, else among the built-in ones.
    Optional<Target> withBuiltInBases(Optional<Target> target) {
      if (target && !target->findBase) {
        target->findBase = builtInNamed;
      }

      if (target && !target->findSchema) {
        target->findSchema = builtInSchema;
      }

      return target;
    }

    Optional<Target> targetAt(const String& path, const String& name) {
      if (is_regular_file(path)) {
        return ReadTarget(path);
      }

      return exists(path) ? withBuiltInBases(PageInFolder(path, name)) : nullopt;
    }

    NodeContext contextFor(const Module& module, const PageSettings& settings) {
      NodeContext context;
      context.module = &module;
      context.copies = settings.style.copies;
      context.fullNames = settings.style.fullNames;
      context.flatNames = settings.style.flatNames;

      context.uniqueLocals = settings.style.uniqueLocals;
      context.bindingWords = settings.names;
      context.qualifyImported = settings.style.qualifyImported;

      context.provided = ProvidedNames(module);
      context.fileName = FileClassName(module, module.file);
      context.importedFiles = ImportedFiles(module, context.provided);

      if (!context.copies) {
        context.generics = FindWrittenGenerics(module, [](const StructDef& def) { return WorksForEveryType(def.generic); });
      }

      return context;
    }

  }  // namespace

  Vector<Target> BuiltInTargets() {
    Vector<Target> targets = EmbeddedTargets();

    for (Target& target : targets) {
      target.isBuiltIn = true;
      target.findBase = builtInNamed;
      target.findSchema = builtInSchema;
    }

    return targets;
  }

  const Schema& PolyirSchema() {
    return CompiledSchemaPolyir();
  }

  // A path ends `.canon`; anything else names canons built in.
  Optional<Schema> FindSchema(const String& source) {
    if (Path(source).extension() != ".canon") {
      return builtInSchema(source);
    }

    IFStream in(source);
    StringStream text;
    text << in.rdbuf();

    return in ? Optional<Schema>(ReadSchema(text.str(), source)) : nullopt;
  }

  Optional<Target> ReadTarget(const String& path) {
    return withBuiltInBases(ReadPage(path));
  }

  Optional<Target> FindTarget(const String& name, const String& rulesPath) {
    if (!rulesPath.empty()) {
      return targetAt(rulesPath, name);
    }

    if (const char* folder = getenv("POLYIR_TARGETS"); folder != nullptr && *folder != '\0') {
      if (Optional<Target> target = withBuiltInBases(PageInFolder(folder, name))) {
        return target;
      }
    }

    return builtInNamed(name);
  }

  Node TreeOf(const Module& module, const Target& target) {
    const Optional<Target> withBases = target.findBase && target.findSchema ? nullopt : withBuiltInBases(target);
    NodeContext context = contextFor(module, PageSettingsOf(withBases ? *withBases : target));
    context.spans = target.spans;

    return ProgramNode(context);
  }

  // polyir's program as its own source: its checked HIR made into a built tree of polyir's forms, then written. A target
  // that cannot find its base page or its canon is given the built-in ones (a copy of it, so only then).
  Translation Translate(const Module& module, const Target& target) {
    const Optional<Target> withBases = target.findBase && target.findSchema ? nullopt : withBuiltInBases(target);
    const Target& page = withBases ? *withBases : target;
    NodeContext context = contextFor(module, PageSettingsOf(page));
    context.spans = target.spans;
    Node built = ProgramNode(context);

    return TranslateBuilt(move(built), PolyirSchema(), page, context.problems);
  }

}  // namespace polyir
