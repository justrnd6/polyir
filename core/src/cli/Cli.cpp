// The polyir command line, see polyir/Cli.hpp. It lives in the library so `polyir-check` (tools/PolyirCheck.cpp) and `nina polyir` both
// call the same RunPolyirCli.
// The flow: parseOptions turns the arguments into an Options record (one `mode`, chosen by a flag such as --emit-llvm); runOptions
// picks the path for that mode: a .pbc given to --interp or --vm-listing goes straight to the VM; --ast and --bench read the file
// themselves (the syntax tree, or timed parse/check/lint runs) instead of going through the front end first; everything else is
// checked by the front end first and then handed to the function for that mode. The functions that run a mode write to the two
// Streams and return the process exit status.
#include "polyir/Cli.hpp"
#include "polyir/Frontend.hpp"
#include "polyir/HirJson.hpp"
#include "polyir/HirPrint.hpp"
#include "polyir/Kernels.hpp"
#include "polyir/LlvmCompiler.hpp"
#include "polyir/Pbc.hpp"
#include "polyir/PolySource.hpp"
#include "polyir/Translate.hpp"
#include "polyir/Vm.hpp"
#include "front/Ast.hpp"
#include "front/Lint.hpp"

#include <iterator>
#include <unistd.h>

namespace polyir {

  using front::FrontResult;
  using front::ParseResult;
  using front::CheckFile;
  using front::CheckSource;
  using front::DumpProgram;
  using front::LintWarnings;
  using front::ParseSource;
  using std::ios;
  using std::streamsize;
  using std::istreambuf_iterator;
  using std::filesystem::absolute;
  using std::filesystem::create_directories;
  using std::filesystem::exists;
  using std::filesystem::remove;
  using std::filesystem::remove_all;
  using std::filesystem::temp_directory_path;

  namespace {

    struct Options {
      String path;
      String mode;  // "" (check only), hir, json, llvm, analyze, check, msl, glsl, spirv, ast, bench, build, run, interp
      String outPath;
      String target;     // --emit-<lang>, --new-target NAME, --update-target NAME: the target language
      String rulesPath;  // --target-rules PATH: a .translate page, or a folder of them, read instead of the built-in ones
      String source = "polyir";  // --source S with --new-target: the canon the page writes (built in, or a .canon file)
      String entry = "main";  // the function --interp calls
      Vector<String> programArgs;
      bool strict = false;
      bool debug = false;
      bool release = false;
      bool noGc = false;  // --no-gc: a native build without the garbage collector (docs/memory.md, Part 2), to compare against
      bool spans = false;      // --spans: a translation writes its page's `line` lines, where each statement came from and its code
      bool sourceMap = false;  // --source-map: and a Source Map beside it (docs/translate.md)
      bool isWrong = false;  // the arguments do not make a command: print the usage
      int benchRuns = 100;
    };

    struct Streams {
      OStream& out;
      OStream& err;
    };

    // A flag that picks the mode, such as `--emit-llvm` → "llvm"; some modes have a short and a long flag.
    struct FlagMode {
      const char* flag;
      const char* mode;
    };

    const FlagMode kFlags[] = {{"--hir", "hir"}, {"--emit-hir", "hir"}, {"--json", "json"}, {"--emit-hir-json", "json"}, {"--emit-poly", "poly"}, {"--llvm", "llvm"},
      {"--emit-llvm", "llvm"}, {"--analyze", "analyze"}, {"--check", "check"}, {"--emit-msl", "msl"}, {"--emit-glsl", "glsl"},
      {"--emit-spirv", "spirv"}, {"--ast", "ast"}, {"--bench", "bench"}, {"--run", "run"}, {"--interp", "interp"},
      {"--vm-listing", "listing"}, {"--emit-pbc", "pbc"}};

    bool readFile(const String& path, String& text) {
      IFStream in(path);

      if (!in) {
        return false;
      }

      StringStream buffer;
      buffer << in.rdbuf();
      text = buffer.str();

      return true;
    }

    // ── what is printed ──

    int reportErrors(Streams& streams, const String& headline, const Vector<String>& errors) {
      streams.out << headline << ":\n";

      for (const String& error : errors) {
        streams.out << "  " << error << "\n";
      }

      return 1;
    }

    // The warnings, on the output; a translation's go to the error stream, so the program's text is all the output holds.
    void printWarnings(Streams& streams, const Vector<String>& warnings, bool translating) {
      OStream& to = translating ? streams.err : streams.out;
      to << "warning:\n";

      for (const String& warning : warnings) {
        to << "  " << warning << "\n";
      }
    }

    int printHir(Streams& streams, const FrontResult& result) {
      const String text = ModuleText(result.module);

      if (!text.empty()) {
        streams.out << text << "\n";
      }

      return 0;
    }

    int printJson(Streams& streams, const FrontResult& result) {
      streams.out << WriteHirJson(result.module) << "\n";

      return 0;
    }

    // `--emit-pbc`: the checked program as .pbc bytes (`poly` runs them), in `--out PATH` or beside the input (`f.pbc`).
    int writePbcFile(Streams& streams, const Options& options, const FrontResult& result) {
      const String path = options.outPath.empty() ? Path(options.path).replace_extension(".pbc").string() : options.outPath;
      const Vector<uint8_t> bytes = WritePbc(result.module);
      OFStream out(path, ios::binary);
      out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<streamsize>(bytes.size()));

      streams.out << "wrote " << path << "\n";

      return out ? 0 : 1;
    }

    // The checked program written back as .poly source; what it could not spell goes to the error stream.
    int printPoly(Streams& streams, const FrontResult& result) {
      const PolySource poly = WritePoly(result.module);
      streams.out << poly.text;

      for (const String& problem : poly.problems) {
        streams.err << "error: " << problem << "\n";
      }

      return poly.problems.empty() ? 0 : 1;
    }

    // ── compiling ──

    // The options of the LLVM compiler: --debug names the source file and its directory in the DWARF.
    CompileOptions compileOptions(const Options& options) {
      CompileOptions made;
      made.debugInfo = options.debug;
      made.collectGarbage = !options.noGc;

      const Path full = absolute(options.path);
      made.sourceFile = full.filename().string();
      made.sourceDirectory = full.parent_path().string();

      return made;
    }

    // The compiled program, or the errors of a native build that cannot be made (host-only declarations in use, then the LLVM compiler's own).
    CompileResult nativeBuild(const Options& options, const FrontResult& result) {
      if (result.nativeProblems.empty()) {
        return CompileToLlvm(result.module, compileOptions(options));
      }

      CompileResult refused;
      refused.errors = result.nativeProblems;

      return refused;
    }

    int printLlvm(Streams& streams, const Options& options, const FrontResult& result) {
      const CompileResult compiled = nativeBuild(options, result);

      if (!Ok(compiled)) {
        return reportErrors(streams, "compile failed", compiled.errors);
      }

      streams.out << compiled.ir;

      return 0;
    }

    int verifyLlvm(Streams& streams, const Options& options, const FrontResult& result) {
      const CompileResult compiled = nativeBuild(options, result);

      if (!Ok(compiled)) {
        return reportErrors(streams, "compile failed", compiled.errors);
      }

      streams.out << "ok\n";

      return 0;
    }

    bool isKernelMode(const String& mode) {
      return mode == "msl" || mode == "glsl" || mode == "spirv";
    }

    // The shader text of the kernels: Metal, Vulkan GLSL, or SPIR-V as hex words. What the SPIR-V writer cannot do is reported like a broken rule.
    ShaderText shaderText(const Options& options, const Module& module) {
      if (options.mode == "glsl") {
        return GlslSource(module);
      }

      return options.mode == "spirv" ? SpirvSource(module) : ShaderText{MetalSource(module), {}};
    }

    // The shader text of the kernels once the kernel rules hold.
    int kernelVerdict(Streams& streams, const Options& options, const FrontResult& result) {
      if (result.kernelCount == 0) {
        return reportErrors(streams, "kernel check failed", {"no `kernel func` found"});
      }

      if (!result.kernelProblems.empty()) {
        return reportErrors(streams, "kernel check failed", result.kernelProblems);
      }

      const ShaderText shader = shaderText(options, result.module);

      if (!shader.errors.empty()) {
        return reportErrors(streams, "kernel check failed", shader.errors);
      }

      streams.out << shader.text << "\n";

      return 0;
    }

    // ── building and running ──

    // A path for the executable --run builds when --out names none: a file in the temp directory, unique to this process.
    String temporaryExecutable() {
      const Path directory = temp_directory_path();

      return (directory / ("polyir-run-" + to_string(getpid()))).string();
    }

    // The libraries the program names with `import native "…"`, each once.
    Vector<String> nativeLibraries(const Module& module) {
      Vector<String> out;

      for (const HostImport& d : module.hostImports) {
        if (d.target == "native" && find(out.begin(), out.end(), d.module) == out.end()) {
          out.push_back(d.module);
        }
      }

      return out;
    }

    int buildFailed(Streams& streams, const String& diagnostics) {
      streams.out << "build failed:\n  " << diagnostics << (diagnostics.empty() || diagnostics.back() != '\n' ? "\n" : "");

      return 1;
    }

    int buildTo(Streams& streams, const Options& options, const FrontResult& result, const String& executablePath) {
      const CompileResult compiled = nativeBuild(options, result);

      if (!Ok(compiled)) {
        return reportErrors(streams, "compile failed", compiled.errors);
      }

      String diagnostics;

      if (!BuildExecutable(compiled.ir, executablePath, options.release, nativeLibraries(result.module), diagnostics)) {
        return buildFailed(streams, diagnostics);
      }

      return 0;
    }

    void removeBuild(const String& executablePath) {
      ErrorCode ignored;
      remove(executablePath, ignored);
      remove(executablePath + ".ll", ignored);
      remove_all(executablePath + ".dSYM", ignored);
    }

    int buildAndRun(Streams& streams, const Options& options, const FrontResult& result) {
      const String executablePath = options.outPath.empty() ? temporaryExecutable() : options.outPath;
      const int built = buildTo(streams, options, result, executablePath);

      if (built != 0) {
        return built;
      }

      streams.out.flush();
      const int status = RunExecutable(executablePath, options.programArgs);

      if (options.outPath.empty()) {
        removeBuild(executablePath);
      }

      return status;
    }

    int buildOnly(Streams& streams, const Options& options, const FrontResult& result) {
      const int built = buildTo(streams, options, result, options.outPath);

      if (built == 0) {
        streams.out << "compiled to " << options.outPath << "\n";
      }

      return built;
    }

    // ── interpreting ──

    VmOptions vmOptions(Streams& streams, const Options& options) {
      VmOptions made;
      made.entry = options.entry;
      made.args = options.programArgs;
      made.out = &streams.out;

      return made;
    }

    // What an operating system keeps of a whole-number result as an exit status: its low 8 bits (as RunPbcEntry).
    int exitStatusByte(int64_t value) {
      return static_cast<int>(((value % 256) + 256) % 256);
    }

    // A run that did not end with a value: a decline (nothing ran, 125) or a trap (1), said on the error stream.
    int reportNotRun(Streams& streams, const VmOutcome& ran) {
      if (ran.declined) {
        streams.err << "declined: " << ran.declineReason << "\n";
      } else {
        streams.err << "trap: " << ran.trap << "\n";
      }

      return static_cast<int>(ran.exitStatus);
    }

    // Nanoseconds as microseconds with one decimal.
    String micros(int64_t nanoseconds) {
      return to_string(nanoseconds / 1000) + "." + to_string(nanoseconds % 1000 / 100);
    }

    // POLYIR_GC_REPORT=1: what the VM's collector did, on the error stream, in the words a native build's report uses.
    void reportCollector(Streams& streams, const VmOutcome& ran) {
      const char* wanted = getenv("POLYIR_GC_REPORT");

      if (wanted == nullptr || String(wanted) != "1") {
        return;
      }

      streams.err << "gc: " << ran.collections << " collections, " << ran.objectsReclaimed << " objects reclaimed, peak " << ran.peakObjects << " objects; "
            << ran.pauses << " pauses, longest " << micros(ran.longestPauseNs) << " us, p50 " << micros(ran.medianPauseNs) << " us, p99 "
            << micros(ran.p99PauseNs) << " us, " << micros(ran.totalPauseNs) << " us in all\n";
    }

    // The entry's value on its own line (`void` for a void entry, as the Nina CLI prints).
    int reportInterpreted(Streams& streams, const VmOutcome& ran) {
      reportCollector(streams, ran);

      if (!ran.ok) {
        return reportNotRun(streams, ran);
      }

      if (!ran.result.empty()) {
        streams.out << ran.result << "\n";
      }

      return exitStatusByte(ran.exitStatus);
    }

    int interpret(Streams& streams, const Options& options, const FrontResult& result) {
      return reportInterpreted(streams, RunModule(result.module, vmOptions(streams, options)));
    }

    bool isPbc(const String& path) {
      return path.size() > 4 && path.compare(path.size() - 4, 4, ".pbc") == 0;
    }

    int interpretPbc(Streams& streams, const Options& options) {
      return reportInterpreted(streams, RunPbcFile(options.path, vmOptions(streams, options)));
    }

    // ── the VM's instructions ──

    int printListing(Streams& streams, const Module& module) {
      try {
        streams.out << BytecodeListing(module);
        return 0;
      } catch (const Exception& problem) {
        return reportErrors(streams, "the VM cannot run this program", {problem.what()});
      }
    }

    int listPbc(Streams& streams, const Options& options) {
      IFStream in(options.path, ios::binary);

      if (!in) {
        return reportErrors(streams, "no such file", {options.path});
      }

      const Vector<uint8_t> bytes{istreambuf_iterator<char>(in), istreambuf_iterator<char>()};

      return printListing(streams, ReadPbc(bytes));
    }

    // ── timing ──

    double millisecondsSince(SteadyClock::time_point start) {
      return Milliseconds(SteadyClock::now() - start).count();
    }

    struct Timing {
      double parseMs = 0;
      double totalMs = 0;
      double lintMs = 0;
    };

    double timeLoop(int runs, const Function<void()>& body) {
      const auto start = SteadyClock::now();

      for (int i = 0; i < runs; i++) {
        body();
      }

      return millisecondsSince(start) / runs;
    }

    // Parses `runs` times, parses and checks `runs` times, lints a parsed tree `runs` times.
    Timing timeRuns(const String& text, int runs) {
      Timing timing;
      const ParseResult parsed = ParseSource(text);
      timing.parseMs = timeLoop(runs, [&] { ParseSource(text); });
      timing.totalMs = timeLoop(runs, [&] { CheckSource(text); });
      timing.lintMs = timeLoop(runs, [&] { LintWarnings(parsed.program, ""); });

      return timing;
    }

    int benchmark(Streams& streams, const String& text, int runs) {
      const Timing timing = timeRuns(text, runs);
      char line[256];
      snprintf(line, sizeof line, "%zu bytes: parse %.4f ms, check %.4f ms, parse+check %.4f ms per run, %.1f MB/s; lint %.4f ms\n", text.size(),
        timing.parseMs, timing.totalMs - timing.parseMs, timing.totalMs, text.size() / 1e3 / timing.totalMs, timing.lintMs);
      streams.out << line;

      return 0;
    }

    int printAst(Streams& streams, const String& text) {
      const ParseResult parsed = ParseSource(text);

      if (Ok(parsed)) {
        streams.out << DumpProgram(parsed.program);
      }

      return Ok(parsed) ? 0 : 1;
    }

    // ── target languages (docs/translate.md) ──

    // Line `line` of `text`, without the spaces around it ("" past its end).
    String lineOfText(const String& text, int line) {
      IStringStream in(text);
      String found;

      for (int i = 0; i < line && getline(in, found); i++) {
      }

      const size_t from = found.find_first_not_of(" \t");

      return from == String::npos ? "" : found.substr(from, found.find_last_not_of(" \t\r") - from + 1);
    }

    // A source file's text, as named, else beside the program (`Hops.nina` next to `Hops.poly`); "" when neither reads.
    String sourceText(const String& file, const Path& folder) {
      String text;

      return readFile(file, text) || readFile((folder / file).string(), text) ? text : "";
    }

    // The code of a file's line, for a span its source gave no code (`--spans`). Each file is read once.
    Function<String(const String&, int)> codeFromFiles(const String& program) {
      const SharedPtr<Map<String, String>> texts = make_shared<Map<String, String>>();
      const Path folder = Path(program).parent_path();

      return [texts, folder](const String& file, int line) {
        const auto found = texts->find(file);

        return lineOfText(found != texts->end() ? found->second : (*texts)[file] = sourceText(file, folder), line);
      };
    }

    // The file a translation is written as, for its source map: the `--out` path, else the program's name with the target's extension.
    String translatedName(const Options& options, const Translation& translated) {
      return options.outPath.empty() ? Path(options.path).stem().string() + "." + translated.extension : options.outPath;
    }

    // The translation to the `--out` file or the output stream. With `--source-map`, the page's line naming the map ends it, and
    // the map is written beside it (`race.js.map`).
    void writeTranslated(Streams& streams, const Options& options, const Translation& translated) {
      const String name = translatedName(options, translated);
      const String mapName = Path(name).filename().string() + ".map";
      const String link = options.sourceMap && !translated.text.empty() ? SourceMapLink(translated, mapName) : "";
      const String text = translated.text + (link.empty() ? "" : link + "\n");
      options.outPath.empty() ? void(streams.out << text) : void(OFStream(options.outPath) << text);

      if (options.sourceMap && !translated.text.empty()) {
        OFStream(name + ".map") << SourceMapText(translated, Path(name).filename().string());
      }
    }

    // The checked program in the target's language; what stopped it goes to the error stream.
    int printTranslated(Streams& streams, const Options& options, const FrontResult& result) {
      Optional<Target> target = FindTarget(options.target, options.rulesPath);

      if (!target) {
        return reportErrors(streams, "no such target", {options.target});
      }

      target->spans = options.spans || options.sourceMap;
      target->spanLines = options.spans;
      target->codeOf = codeFromFiles(options.path);
      const Translation translated = Translate(result.module, *target);
      writeTranslated(streams, options, translated);

      for (const String& problem : translated.problems) {
        streams.err << "error: " << translated.source << " → " << translated.target << ": " << problem << "\n";
      }

      return translated.problems.empty() ? 0 : 1;
    }

    // `polyir → js: 40 written, 3 todo, 0 unimplemented, 1 unsupported, 2 lower; 0 forms without a rule`.
    String countsLine(const Target& target) {
      const RuleCounts counts = CountRules(target);
      const PageSettings pair = PageSettingsOf(target);
      const String status = to_string(counts.todo) + " todo, " + to_string(counts.unimplemented) + " unimplemented, " + to_string(counts.unsupported) + " unsupported, ";
      const String missing = to_string(MissingConstructs(target).size()) + " forms without a rule";

      return pair.source + " → " + pair.target + ": " + to_string(counts.written) + " written, " + status + to_string(counts.lowered) + " lower; " + missing;
    }

    // The built-in targets, or the one page --target-rules names.
    int listTargets(Streams& streams, const Options& options) {
      Vector<Target> targets = BuiltInTargets();

      if (!options.rulesPath.empty()) {
        const Optional<Target> target = ReadTarget(options.rulesPath);

        if (!target) {
          return reportErrors(streams, "no such file", {options.rulesPath});
        }

        targets = {*target};
      }

      for (const Target& target : targets) {
        streams.out << countsLine(target) << "\n";
      }

      return 0;
    }

    // `go` and `Go` for the name `go`: the folder's name and the page's.
    String lowerCase(String text) {
      for (char& c : text) {
        c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
      }

      return text;
    }

    String capitalized(String text) {
      text[0] = static_cast<char>(toupper(static_cast<unsigned char>(text[0])));

      return text;
    }

    // Where a new page goes: <folder>/<lang>/<Lang>.translate, the folder --target-rules names or `targets`.
    Path newPagePath(const Options& options) {
      const Path folder = options.rulesPath.empty() ? "targets" : options.rulesPath;

      return folder / lowerCase(options.target) / (capitalized(options.target) + ".translate");
    }

    int writeNewTarget(Streams& streams, const Options& options) {
      const Path path = newPagePath(options);

      if (exists(path)) {
        return reportErrors(streams, "the page is already there (use --update-target)", {path.string()});
      }

      const Optional<Schema> source = FindSchema(options.source);

      if (!source) {
        return reportErrors(streams, "no such source (a built-in one, or a .canon file)", {options.source});
      }

      create_directories(path.parent_path());
      OFStream(path) << NewTargetText(capitalized(options.target), *source);
      streams.out << "wrote " << path.string() << "\n";

      return 0;
    }

    // The page with a `todo` rule added for each construct it has no rule for.
    int updateTarget(Streams& streams, const Options& options) {
      const Path path = exists(newPagePath(options)) ? newPagePath(options) : Path(options.rulesPath);
      const Optional<Target> target = ReadTarget(path.string());

      if (!target) {
        return reportErrors(streams, "no such file", {path.string()});
      }

      const size_t missing = MissingConstructs(*target).size();
      OFStream(path) << UpdatedTargetText(*target);
      streams.out << "added " << missing << " todo rules to " << path.string() << "\n";

      return 0;
    }

    int targetCommand(Streams& streams, const Options& options) {
      if (options.mode == "targets") {
        return listTargets(streams, options);
      }

      return options.mode == "new-target" ? writeNewTarget(streams, options) : updateTarget(streams, options);
    }

    bool isTargetCommand(const String& mode) {
      return mode == "targets" || mode == "new-target" || mode == "update-target";
    }

    // ── the options ──

    int usage(Streams& streams) {
      streams.err << "usage: polyir file.poly [--emit-hir | --emit-hir-json | --emit-poly | --emit-pbc | --emit-llvm | --analyze | --check | --emit-msl |\n"
               "                         --emit-glsl | --emit-spirv | --ast |\n"
               "                         --bench N] [--strict] [--debug] [--release] [--no-gc] [--out PATH] [--run [-- args...]]\n"
               "       polyir file.poly|file.pbc --interp [--entry NAME] [-- args...]\n"
               "       polyir file.poly|file.pbc --vm-listing\n"
               "       polyir file.poly --emit-<lang> [--target-rules PATH] [--spans] [--source-map] [--out PATH]   (the program in a target language, e.g. --emit-js)\n"
               "       polyir --targets | --new-target NAME [--source S] | --update-target NAME [--target-rules PATH]\n";

      return 2;
    }

    bool isNumber(const String& text) {
      return !text.empty() && all_of(text.begin(), text.end(), [](char c) { return c >= '0' && c <= '9'; });
    }

    // A switch's option, or null when `arg` is no switch.
    bool* switchOf(Options& options, const String& arg) {
      const Map<String, bool*> switches = {{"--strict", &options.strict}, {"--debug", &options.debug}, {"--release", &options.release},
        {"--no-gc", &options.noGc}, {"--spans", &options.spans}, {"--source-map", &options.sourceMap}};
      const auto found = switches.find(arg);

      return found == switches.end() ? nullptr : found->second;
    }

    bool takeSwitch(Options& options, const String& arg) {
      bool* option = switchOf(options, arg);

      if (option != nullptr) {
        *option = true;
      }

      return option != nullptr;
    }

    bool takeFlag(Options& options, const String& arg) {
      for (const FlagMode& flagMode : kFlags) {
        if (arg == flagMode.flag) {
          options.mode = flagMode.mode;
          return true;
        }
      }

      return takeSwitch(options, arg);
    }

    // `--bench N`: how many times to run (the number is optional).
    void takeBenchRuns(Options& options, const Vector<String>& args, size_t& i) {
      if (options.mode == "bench" && i + 1 < args.size() && isNumber(args[i + 1])) {
        options.benchRuns = static_cast<int>(strtoll(args[++i].c_str(), nullptr, 10));
      }
    }

    // `--emit-<lang>` for any language that is not one of the modes above: the program written by that target's rules.
    bool takeEmitTarget(Options& options, const String& arg) {
      if (arg.rfind("--emit-", 0) != 0 || arg.size() <= 7) {
        return false;
      }

      options.mode = "translate";
      options.target = arg.substr(7);

      return true;
    }

    // `--target-rules PATH`, `--new-target NAME`, `--update-target NAME`, `--source S`: a flag and the word after it.
    bool takeTargetWord(Options& options, const Vector<String>& args, size_t& i) {
      const bool known = args[i] == "--target-rules" || args[i] == "--new-target" || args[i] == "--update-target" || args[i] == "--source";

      if (!known || i + 1 >= args.size()) {
        return false;
      }

      if (args[i] == "--target-rules") {
        options.rulesPath = args[++i];
      } else if (args[i] == "--source") {
        options.source = args[++i];
      } else {
        options.mode = args[i].substr(2);
        options.target = args[++i];
      }

      return true;
    }

    bool takeOut(Options& options, const Vector<String>& args, size_t& i) {
      if (args[i] != "--out" || i + 1 >= args.size()) {
        return false;
      }

      options.outPath = args[++i];

      return true;
    }

    bool takeEntry(Options& options, const Vector<String>& args, size_t& i) {
      if (args[i] != "--entry" || i + 1 >= args.size()) {
        return false;
      }

      options.entry = args[++i];

      return true;
    }

    bool takeProgramArguments(Options& options, const Vector<String>& args, size_t& i) {
      if (args[i] != "--") {
        return false;
      }

      options.programArgs.assign(args.begin() + i + 1, args.end());
      i = args.size();

      return true;
    }

    bool startsWith(const String& text, const String& prefix) {
      return text.compare(0, prefix.size(), prefix) == 0;
    }

    // The file to work on; a second file, or an unknown `--` flag, makes the arguments wrong.
    void takePositional(Options& options, const String& arg) {
      if (startsWith(arg, "--") || !options.path.empty()) {
        options.isWrong = true;
      } else {
        options.path = arg;
      }
    }

    void takeArgument(Options& options, const Vector<String>& args, size_t& i) {
      if (takeProgramArguments(options, args, i) || takeOut(options, args, i) || takeEntry(options, args, i) || takeTargetWord(options, args, i)) {
        return;
      }

      if (args[i] == "--targets") {
        options.mode = "targets";
      } else if (takeFlag(options, args[i])) {
        takeBenchRuns(options, args, i);
      } else if (!takeEmitTarget(options, args[i])) {
        takePositional(options, args[i]);
      }
    }

    Options parseOptions(const Vector<String>& args) {
      Options options;

      for (size_t i = 0; i < args.size(); i++) {
        takeArgument(options, args, i);
      }

      if (options.mode.empty() && !options.outPath.empty()) {
        options.mode = "build";
      }

      options.isWrong = options.isWrong || (options.path.empty() && !isTargetCommand(options.mode));

      return options;
    }

    // ── the run ──

    // The headline the Nina CLI gives a mistake in the program: building (--check, --llvm, --out, --run) says "compile failed", the kernel outputs
    // "kernel check failed".
    String failureHeadline(const Options& options, const FrontResult& result) {
      if (options.mode == "llvm" || options.mode == "check" || options.mode == "build" || options.mode == "run" || options.mode == "interp") {
        return "compile failed";
      }

      return isKernelMode(options.mode) ? "kernel check failed" : result.headline;
    }

    // The checking modes: --check verifies the LLVM, a kernel mode prints the shader text, and --analyze prints "ok".
    int printCheckResult(Streams& streams, const Options& options, const FrontResult& result) {
      if (options.mode == "check") {
        return verifyLlvm(streams, options, result);
      }

      if (isKernelMode(options.mode)) {
        return kernelVerdict(streams, options, result);
      }

      if (options.mode == "analyze") {
        streams.out << "ok\n";
      }

      return 0;
    }

    // The checked program written again: the HIR's own notation, its JSON, .poly source, or .pbc bytes.
    int emitWritten(Streams& streams, const Options& options, const FrontResult& result) {
      if (options.mode == "hir") {
        return printHir(streams, result);
      }

      if (options.mode == "pbc") {
        return writePbcFile(streams, options, result);
      }

      return options.mode == "json" ? printJson(streams, result) : printPoly(streams, result);
    }

    int emitPrinted(Streams& streams, const Options& options, const FrontResult& result) {
      const Set<String> written = {"hir", "json", "poly", "pbc"};

      if (written.count(options.mode) > 0) {
        return emitWritten(streams, options, result);
      }

      if (options.mode == "translate") {
        return printTranslated(streams, options, result);
      }

      if (options.mode == "llvm") {
        return printLlvm(streams, options, result);
      }

      return printCheckResult(streams, options, result);
    }

    int emit(Streams& streams, const Options& options, const FrontResult& result) {
      if (options.mode == "build") {
        return buildOnly(streams, options, result);
      }

      if (options.mode == "run") {
        return buildAndRun(streams, options, result);
      }

      if (options.mode == "interp") {
        return interpret(streams, options, result);
      }

      if (options.mode == "listing") {
        return printListing(streams, result.module);
      }

      return emitPrinted(streams, options, result);
    }

    int lintOrEmit(Streams& streams, const Options& options, const FrontResult& result) {
      if (!result.warnings.empty() && options.strict) {
        return reportErrors(streams, "lint failed", result.warnings);
      }

      if (!result.warnings.empty()) {
        printWarnings(streams, result.warnings, !options.target.empty());
      }

      if (!Ok(result)) {
        return reportErrors(streams, failureHeadline(options, result), result.errors);
      }

      return emit(streams, options, result);
    }

    int process(Streams& streams, const Options& options) {
      const FrontResult result = CheckFile(options.path);

      if (result.headline == "no such file" || result.headline == "compile failed") {
        return reportErrors(streams, result.headline, result.errors);
      }

      return lintOrEmit(streams, options, result);
    }

    int astOrBenchmark(Streams& streams, const Options& options) {
      String text;

      if (!readFile(options.path, text)) {
        return reportErrors(streams, "no such file", {options.path});
      }

      return options.mode == "ast" ? printAst(streams, text) : benchmark(streams, text, options.benchRuns);
    }

    int runOptions(Streams& streams, const Options& options) {
      if (options.isWrong) {
        return usage(streams);
      }

      if (isTargetCommand(options.mode)) {
        return targetCommand(streams, options);
      }

      if (options.mode == "interp" && isPbc(options.path)) {
        return interpretPbc(streams, options);  // a .pbc is already checked HIR: no front end
      }

      if (options.mode == "listing" && isPbc(options.path)) {
        return listPbc(streams, options);
      }

      return options.mode == "ast" || options.mode == "bench" ? astOrBenchmark(streams, options) : process(streams, options);
    }

  }  // namespace

  int RunPolyirCli(const Vector<String>& args, OStream& out, OStream& err) {
    Streams streams{out, err};

    return runOptions(streams, parseOptions(args));
  }

}  // namespace polyir
