// polyir-translatec: a `.translate` page, or a source's canon (a `.canon` file), compiled into C++ (polyir/translate/Compiled.hpp).
//
//   polyir-translatec PAGE.translate --canon SOURCE.canon --file SHOWN --function NAME --out OUT.cpp
//   polyir-translatec SOURCE.canon --file SHOWN --function NAME --out OUT.cpp
//
// A page is checked by its source's canon first; a page that starts from another (`base: Js`) finds it beside its own
// folder. Each problem is printed as `error: ...` and nothing is written, so a build that compiles its pages stops there.
// SHOWN is the path the rules name their page by (`targets/js/Js.translate`).
#include "polyir/translate/Compiled.hpp"

using namespace polyir;
using namespace polyir::translate;

namespace {

  struct Options {
    String input;
    String canon;
    String shown;
    String function;
    String out;
  };

  // `--canon`, `--file`, `--function`, `--out` and the word after each; the one other word is the input.
  bool readOptions(int argc, char** argv, Options& options) {
    for (int i = 1; i < argc; i++) {
      const String arg = argv[i];
      const bool flagged = i + 1 < argc && (arg == "--canon" || arg == "--file" || arg == "--function" || arg == "--out");
      String& slot = arg == "--canon" ? options.canon : arg == "--file" ? options.shown : arg == "--function" ? options.function : arg == "--out" ? options.out : options.input;

      slot = flagged ? argv[++i] : arg;
    }

    return !options.input.empty() && !options.function.empty() && !options.out.empty();
  }

  bool readText(const String& path, String& text) {
    IFStream in(path);
    StringStream buffer;
    buffer << in.rdbuf();
    text = buffer.str();

    return static_cast<bool>(in);
  }

  // Each problem as an error; true when there were none.
  bool reported(const Vector<String>& problems) {
    for (const String& problem : problems) {
      cerr << "error: " << problem << "\n";
    }

    return problems.empty();
  }

  bool written(const String& path, const String& text) {
    OFStream out(path);
    out << text;

    return static_cast<bool>(out);
  }

  int compileSchema(const Options& options) {
    String text;

    if (!readText(options.input, text)) {
      reported({options.input + ": there is no such file"});
      return 1;
    }

    const Schema schema = ReadSchema(text, options.shown.empty() ? options.input : options.shown);

    return reported(schema.problems) && written(options.out, CompiledSchemaText(schema, options.function, schema.file)) ? 0 : 1;
  }

  // The page finds its source's canon in the `.canon` file given, when none is beside it.
  int compilePage(const Options& options) {
    Optional<Target> page = ReadPage(options.input);
    String canonText;
    const bool hasCanon = !options.canon.empty() && readText(options.canon, canonText);
    const Schema schema = hasCanon ? ReadSchema(canonText, options.canon) : Schema{};

    if (!page) {
      reported({options.input + ": there is no such file"});
      return 1;
    }

    page->findSchema = [&](const String& source) { return hasCanon && source == schema.name ? Optional<Schema>(schema) : nullopt; };

    if (!reported(CompileProblems(*page))) {
      return 1;
    }

    const String shown = options.shown.empty() ? options.input : options.shown;

    return written(options.out, CompiledRulesText(ReadRules(page->text, shown), options.function, shown)) ? 0 : 1;
  }

}  // namespace

int main(int argc, char** argv) {
  Options options;

  if (!readOptions(argc, argv, options)) {
    cerr << "usage: polyir-translatec PAGE.translate|SOURCE.canon [--canon SOURCE.canon] [--file SHOWN] --function NAME --out OUT.cpp\n";
    return 2;
  }

  return Path(options.input).extension() == ".canon" ? compileSchema(options) : compilePage(options);
}
