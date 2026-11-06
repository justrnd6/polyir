// Resolving `import "x.poly"` and `import java "..."` (Imports.nina `resolveImports` / `mergeImports` / `importOne`): the file's items are cut into runs at each
// import, and the imported file's own runs go in between, depth first, so the units are the merged program in source order. A file is read and parsed once
// per merge (it is added to the visited set the moment it is entered, which also ends an import cycle). The programs live as long as the merger.
//
// Example: a.poly holds `f; import "b.poly"; g` and b.poly holds `h`. The units are [a: f] [b: h] [a: g] -- the program as if b were pasted in.
#include "front/Pipeline.hpp"
#include "front/HirBuild.hpp"

namespace polyir::front {

  namespace {

    // The directory part of a path (empty for a bare file name).
    String parentOf(const String& path) {
      const size_t at = path.rfind('/');

      return at == String::npos ? String() : path.substr(0, at);
    }

    // An absolute path stays as it is; a relative one is read from `directory`.
    String joined(const String& directory, const String& relative) {
      const bool isAbsolute = !relative.empty() && relative[0] == '/';

      if (directory.empty() || isAbsolute) {
        return relative;
      }

      return directory + "/" + relative;
    }

    // The name `{os}` stands for in an import path.
    String platformName() {
#if defined(__APPLE__)
      return "macos";
#elif defined(_WIN32)
      return "windows";
#else
      return "linux";
#endif
    }

    // `{os}` in an import path stands for the operating system, so one line picks a per-platform file.
    String withOs(String path) {
      const String marker = "{os}";
      const size_t at = path.find(marker);

      if (at == String::npos) {
        return path;
      }

      return withOs(path.substr(0, at) + platformName() + path.substr(at + marker.size()));
    }

    // A letter, a digit, or one of `_ . $` (a Java class name like `java.util.Map$Entry`).
    bool isClassChar(char c) {
      const String extra = "_.$";

      return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || extra.find(c) != String::npos;
    }

    bool isClassName(const String& text) {
      if (text.empty()) {
        return false;
      }

      for (char c : text) {
        if (!isClassChar(c)) {
          return false;
        }
      }

      return true;
    }

    // Whether the file is there to read. An import checks this before resolveFile reads it for real. That way a missing file is reported at
    // the `line:col` of the import in the importing file.
    bool canRead(const String& path) {
      String unused;

      return ReadTextFile(path, unused);
    }

    // `file:line:col: what`, for an error at a place in an importing file.
    String located(const String& file, const String& what, Pos at) {
      return file + ":" + DiagnosticMessage(what, SpanAt(at));
    }

    // ── the merge: walk a program's items, and at each import walk the imported file first ──────────────────────────────────────────────────

    bool walkProgram(ImportMerger& merger, const Program& program, uint32_t file);

    bool bringsFile(const ImportMerger& merger, const Program& program, const AstItem& item);

    bool splitAt(ImportMerger& merger, const Program& program, uint32_t item, uint32_t& start, uint32_t file);

    bool importOne(ImportMerger& merger, const AstImport& decl, const String& importerFile);

    bool importForeign(ImportMerger& merger, const AstImport& decl, const String& importerFile);

    bool importJavaDeclarations(ImportMerger& merger, const AstImport& decl, const String& importerFile, const String& className);

    bool resolveFile(ImportMerger& merger, const String& path);

    bool parseAndMerge(ImportMerger& merger, const String& file, String text);

    void addRun(ImportMerger& merger, const Program& program, uint32_t first, uint32_t last, uint32_t file);

    bool failWith(ImportMerger& merger, String message);

    // Records an error; answers false so a caller can `return failWith(...)`.
    bool failWith(ImportMerger& merger, String message) {
      merger.errors.push_back(move(message));

      return false;
    }

    // The items [first, last) of one program become one unit, unless there are none.
    void addRun(ImportMerger& merger, const Program& program, uint32_t first, uint32_t last, uint32_t file) {
      if (last > first) {
        merger.units.push_back(Unit{&program, first, last, file});
      }
    }

    // The runs of items between the imports, with each import's own units between them. A host import (`import c "zlib.h"`) brings in no file: it stays
    // in its run, an item like any other.
    bool walkProgram(ImportMerger& merger, const Program& program, uint32_t file) {
      uint32_t start = 0;

      for (uint32_t i = 0; i < program.items.size(); i++) {
        if (bringsFile(merger, program, program.items[i]) && !splitAt(merger, program, i, start, file)) {
          return false;
        }
      }

      addRun(merger, program, start, static_cast<uint32_t>(program.items.size()), file);

      return true;
    }

    // Ends the current run before the import `item`, merges the imported file, and starts the next run after it.
    bool splitAt(ImportMerger& merger, const Program& program, uint32_t item, uint32_t& start, uint32_t file) {
      addRun(merger, program, start, item, file);
      start = item + 1;

      return importOne(merger, program.imports[program.items[item].index], String(merger.files[file]));
    }

    // A plain import reads a file beside the importer; one with a language word (`import java ...`) is foreign.
    bool importOne(ImportMerger& merger, const AstImport& decl, const String& importerFile) {
      if (Known(decl.lang.pos)) {
        return importForeign(merger, decl, importerFile);
      }

      const String path = joined(parentOf(importerFile), withOs(String(StripQuotes(decl.path.text))));

      if (canRead(NormalizePath(path))) {
        return resolveFile(merger, path);
      }

      return failWith(merger, located(importerFile, "import not found: '" + NormalizePath(path) + "'", decl.path.pos));
    }

    // An import brings in a file when it has no language word, or the word is `java`.
    bool bringsFile(const ImportMerger& merger, const Program& program, const AstItem& item) {
      if (item.kind != ItemKind::Import) {
        return false;
      }

      const AstImport& decl = program.imports[item.index];

      return !Known(decl.lang.pos) || TextOf(merger.names, decl.lang.name) == "java";
    }

    // `import java "java.util.Arrays"`: the declaration file beside the importer (generating it needs `javap`, which this front end does not run).
    bool importForeign(ImportMerger& merger, const AstImport& decl, const String& importerFile) {
      const String className = String(StripQuotes(decl.path.text));

      if (isClassName(className)) {
        return importJavaDeclarations(merger, decl, importerFile, className);
      }

      return failWith(merger, located(importerFile, "import java: '" + className + "' is not a class name", decl.path.pos));
    }

    bool importJavaDeclarations(ImportMerger& merger, const AstImport& decl, const String& importerFile, const String& className) {
      const String file = joined(joined(parentOf(importerFile), ".polyir.d/java"), className + ".d.poly");

      if (canRead(file)) {
        return resolveFile(merger, file);
      }

      const String message = "import java \"" + className + "\": no declaration file " + file + " (generate it with the Nina polyir)";

      return failWith(merger, located(importerFile, message, decl.path.pos));
    }

    // A file seen before is already merged (or being merged: an import cycle), so it adds nothing.
    bool resolveFile(ImportMerger& merger, const String& path) {
      const String file = NormalizePath(path);

      if (merger.visited.count(file) != 0) {
        return true;
      }

      merger.visited.insert(file);
      String text;

      if (!ReadTextFile(file, text)) {
        return failWith(merger, "import not found: '" + file + "'");
      }

      return parseAndMerge(merger, file, move(text));
    }

    bool parseAndMerge(ImportMerger& merger, const String& file, String text) {
      const Vector<FromLine> fromLines = FromLinesOf(text, file);
      merger.fromLines.insert(merger.fromLines.end(), fromLines.begin(), fromLines.end());
      merger.programs.push_back(ParseSource(move(text), merger.names));
      const ParseResult& parsed = merger.programs.back();

      if (!Ok(parsed)) {
        return failWith(merger, file + ":" + parsed.errors[0]);
      }

      merger.files.push_back(file);

      return walkProgram(merger, parsed.program, static_cast<uint32_t>(merger.files.size() - 1));
    }

  }  // namespace

  bool ReadTextFile(const String& path, String& text) {
    IFStream in(path);

    if (!in) {
      return false;
    }

    StringStream buffer;
    buffer << in.rdbuf();
    text = buffer.str();

    return true;
  }

  ImportMerger MakeImportMerger(Interner& names) {
    ImportMerger merger{{}, Vector<String>(1), {}, names};

    return merger;
  }

  bool MergeRoot(ImportMerger& merger, const Program& root, const String& path) {
    merger.visited.insert(path);
    merger.files.push_back(path);

    return walkProgram(merger, root, 1);
  }

}  // namespace polyir::front
