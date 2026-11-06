// Pages (polyir/translate/Page.hpp): where a page comes from, the page it starts from, and its source's canon. What a page
// lacks and what is wrong in it is PageChecks.cpp.
#include "Pages.hpp"

namespace polyir::translate {

  using std::filesystem::directory_iterator;
  using std::filesystem::is_directory;
  using std::filesystem::recursive_directory_iterator;

  namespace {

    // ── where pages come from ──

    bool readText(const Path& path, String& text) {
      IFStream in(path);

      if (!in) {
        return false;
      }

      StringStream buffer;
      buffer << in.rdbuf();
      text = buffer.str();

      return true;
    }

    String lowered(String text) {
      for (char& c : text) {
        c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
      }

      return text;
    }

    // The files under the page's runtime/ folder, by their path from the page's folder.
    Map<String, String> runtimeFiles(const Path& folder) {
      Map<String, String> files;
      const Path runtime = folder / "runtime";

      if (!is_directory(runtime)) {
        return files;
      }

      for (const auto& entry : recursive_directory_iterator(runtime)) {
        String text;

        if (entry.is_regular_file() && readText(entry.path(), text)) {
          files[entry.path().lexically_relative(folder).generic_string()] = text;
        }
      }

      return files;
    }

    // The page for `name` in a folder of targets: <folder>/<name>/<Name>.translate, whatever case the file's name is in.
    Optional<Target> pageIn(const Path& folder, const String& name) {
      const Path inner = folder / lowered(name);

      if (!is_directory(inner)) {
        return nullopt;
      }

      for (const auto& entry : directory_iterator(inner)) {
        if (entry.path().extension() == ".translate" && lowered(entry.path().stem().string()) == lowered(name)) {
          return ReadPage(entry.path().string());
        }
      }

      return nullopt;
    }

    // The page a page starts from (`base: Js`): beside a page read from disk (targets/js beside targets/ts), else the one its
    // `findBase` finds. The base finds its own base the same way.
    Optional<Target> baseTarget(const Target& target, const String& name) {
      const Path folder = Path(target.file).parent_path().parent_path();
      Optional<Target> found = !target.isBuiltIn && is_directory(folder) ? pageIn(folder, name) : nullopt;

      if (!found && target.findBase) {
        found = target.findBase(name);
      }

      if (found && !found->findBase) {
        found->findBase = target.findBase;
      }

      if (found && !found->findSchema) {
        found->findSchema = target.findSchema;
      }

      return found;
    }

    // The canon of `source` in a `.canon` file in `folder`, if one there names it.
    Optional<Schema> schemaIn(const Path& folder, const String& source) {
      if (!is_directory(folder)) {
        return nullopt;
      }

      for (const auto& entry : directory_iterator(folder)) {
        String text;

        if (entry.path().extension() == ".canon" && readText(entry.path(), text)) {
          Schema schema = ReadSchema(text, entry.path().string());

          if (schema.name == source) {
            return schema;
          }
        }
      }

      return nullopt;
    }

    struct KeptPage {
      String text;
      Map<String, String> files;
      SharedPtr<const Page> page;
    };

    // ── a page and the page it starts from ──

    // The base's imports, then the page's own; a support file or an import of the page keeps its own `uses:`. The page's span
    // lines replace its base's.
    void addBaseImports(TranslateRules& rules, const TranslateRules& base) {
      Vector<String> order = base.importOrder;

      for (const String& name : rules.importOrder) {
        if (base.imports.count(name) == 0) {
          order.push_back(name);
        }
      }

      rules.importOrder = order;
      rules.imports.insert(base.imports.begin(), base.imports.end());
      rules.needs.insert(base.needs.begin(), base.needs.end());
      rules.spans.insert(base.spans.begin(), base.spans.end());
    }

    // The base's rules and settings, then the page's: a rule of the page replaces one alike of its base, and the style is the page's.
    void addBase(TranslateRules& rules, const TranslateRules& base) {
      if (!rules.source.empty() && !base.source.empty() && rules.source != base.source) {
        rules.problems.push_back(rules.file + ": the page is for `" + rules.source + "`, and its base page for `" + base.source + "`");
      }

      rules.source = rules.source.empty() ? base.source : rules.source;

      rules.rules.insert(rules.rules.begin(), base.rules.begin(), base.rules.end());
      rules.levels = rules.levels.empty() ? base.levels : rules.levels;
      rules.reserved.insert(base.reserved.begin(), base.reserved.end());
      rules.apart.insert(base.apart.begin(), base.apart.end());
      rules.problems.insert(rules.problems.begin(), base.problems.begin(), base.problems.end());

      for (const auto& [token, spelling] : base.operators) {
        rules.operators.insert({token, spelling});
      }

      for (const auto& [name, path] : base.support) {
        rules.support.insert({name, path});
      }

      addBaseImports(rules, base);
    }

    Page pageOf(const Target& target, int depth) {
      Page page{target.rules != nullptr ? *target.rules : ReadRules(target.text, target.file), target.files, {target.name}};
      const String base = page.rules.base;
      const Optional<Target> found = base.empty() || depth > 8 ? nullopt : baseTarget(target, base);

      if (!base.empty() && !found) {
        page.rules.problems.push_back(target.file + ": there is no page `" + base + "` to start from");
      }

      if (found) {
        const Page below = pageOf(*found, depth + 1);
        addBase(page.rules, below.rules);
        page.files.insert(below.files.begin(), below.files.end());
        page.names.insert(page.names.end(), below.names.begin(), below.names.end());
      }

      return page;
    }

  }  // namespace

  // A page kept, by its file, with the text and support files it had: another text or other files are read again.
  SharedPtr<const Page> PageOf(const Target& target) {
    static Mutex mutex;
    static Map<String, KeptPage> kept;
    const LockGuard<Mutex> lock(mutex);
    KeptPage& entry = kept[target.file];

    if (entry.page == nullptr || entry.text != target.text || entry.files != target.files) {
      entry = KeptPage{target.text, target.files, make_shared<Page>(pageOf(target, 0))};
    }

    return entry.page;
  }

  Optional<Target> ReadPage(const String& path) {
    Target target;

    if (!readText(path, target.text)) {
      return nullopt;
    }

    target.name = lowered(Path(path).stem().string());
    target.file = path;
    target.files = runtimeFiles(Path(path).parent_path());

    return target;
  }

  Optional<Target> PageInFolder(const String& folder, const String& name) {
    return pageIn(folder, name);
  }

  PageSettings PageSettingsOf(const Target& target) {
    const SharedPtr<const Page> page = PageOf(target);

    return PageSettings{page->rules.source, page->rules.target, page->rules.style, page->names};
  }

  Optional<Schema> SchemaFor(const Target& target, const String& source) {
    const Path folder = Path(target.file).parent_path();
    Optional<Schema> beside = target.isBuiltIn ? nullopt : schemaIn(folder, source);

    if (!beside && !target.isBuiltIn) {
      beside = schemaIn(folder.parent_path(), source);
    }

    if (!beside && target.findSchema) {
      beside = target.findSchema(source);
    }

    return beside;
  }

  Optional<Schema> SchemaOf(const Target& target) {
    return SchemaFor(target, PageOf(target)->rules.source);
  }

  String FileExtension(const Style& style, const String& name, bool library) {
    if (library && !style.library.empty()) {
      return style.library;
    }

    return style.extension.empty() ? name : style.extension;
  }

  String TargetExtension(const Target& target) {
    return FileExtension(PageOf(target)->rules.style, target.name, false);
  }

}  // namespace polyir::translate
