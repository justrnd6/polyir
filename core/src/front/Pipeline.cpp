// The two entry points of the front end, CheckSource and CheckFile (polyir/Frontend.hpp), and Analyze, which runs the checking phases in
// the order Pipeline.hpp lists. Read this file top to bottom to see what happens to a program, phase by phase.
//
// The one idea: every phase returns the problems it found, and the first phase that finds any stops the pipeline. The only second chance
// is the library retry: a program that calls a built-in library function (`fract`, `clamp`, ...) is checked again with that function added.
#include "front/Pipeline.hpp"
#include "front/Ast.hpp"
#include "front/Kernels.hpp"
#include "front/Lint.hpp"
#include "front/Resolve.hpp"
#include "polyir/Frontend.hpp"

namespace polyir::front {

  namespace {

    // `first` followed by `second`.
    Vector<String> concat(Vector<String> first, const Vector<String>& second) {
      first.insert(first.end(), second.begin(), second.end());

      return first;
    }

    // What is wrong with the declarations before generics are made concrete: host types, interfaces, then the coroutine rules.
    Vector<String> earlyProblems(Session& session) {
      const Vector<String> host = concat(ExternTypeProblems(session), InterfaceProblems(session));

      return host.empty() ? CoroutineRestrictions(session) : host;
    }

    // Generics made concrete, and the interface rules asked again of what that made.
    Vector<String> specializedProblems(Session& session) {
      Monomorphize(session);

      return InterfaceProblems(session);
    }

    // Phases 3-5 (the syntax rules, monomorphize, the check) and the diagnose step on a session: the problems found, or none.
    Vector<String> checkSession(Session& session) {
      CollectDeclared(session);
      const Vector<String> early = earlyProblems(session);

      if (!early.empty()) {
        return early;
      }

      const Vector<String> specialized = specializedProblems(session);

      if (!specialized.empty()) {
        return specialized;
      }

      ResolveDeclarations(session);
      DeclareGenericFunctions(session);
      CheckBodies(session);
      NoteParametric(session);

      const Vector<String> problems = Diagnose(session);
      SplitGenerics(session);

      return problems;
    }

    // One check of the units in a new session.
    Analysis analyzeOnce(Interner& names, const Vector<Unit>& units, const Vector<String>& files) {
      Analysis result;
      result.session = make_unique<Session>(MakeSession(names));
      result.session->units = units;
      result.session->files = files;
      result.problems = checkSession(*result.session);

      return result;
    }

    // The names `wanted` already has plus the ones `latest` reports missing, each once.
    Vector<String> withMissing(Vector<String> wanted, const Vector<String>& latest) {
      for (const String& name : MissingLibraryFunctions(latest)) {
        if (!IsOneOf(name, wanted)) {
          wanted.push_back(name);
        }
      }

      return wanted;
    }

    // The library functions in front of the program's own, or nothing when that clashes with what the program defines (its own `fract`, say).
    Vector<Unit> withLibrary(Interner& names, const Vector<Unit>& units, const Vector<String>& wanted) {
      Vector<Unit> all = LibraryUnits(names, wanted);

      if (all.empty()) {
        return all;
      }

      all.insert(all.end(), units.begin(), units.end());

      return CheckDuplicates(all).empty() ? all : Vector<Unit>();
    }

    // One round adds the library functions the last check reported missing. A round can reveal more, because the arguments of an unknown call are only
    // checked once the call is defined. So `clamp(fract(t), ...)` reports `clamp` first, `fract` next. It stops when nothing new is missing, and then
    // reports the latest problems (which no longer include the library's own names).
    // `rounds` caps the retries, so a program that keeps reporting new missing names cannot loop for ever.
    Analysis retry(Interner& names, const Vector<Unit>& units, const Vector<String>& files, Vector<String> wanted, Analysis latest, int rounds) {
      const Vector<String> all = withMissing(wanted, latest.problems);

      if (all.size() == wanted.size() || rounds == 0) {
        return latest;
      }

      const Vector<Unit> extended = withLibrary(names, units, all);

      if (extended.empty()) {
        return latest;
      }

      Analysis next = analyzeOnce(names, extended, files);

      return next.problems.empty() ? move(next) : retry(names, units, files, all, move(next), rounds - 1);
    }

  }  // namespace

  Analysis Analyze(Interner& names, const Vector<Unit>& units, const Vector<String>& files) {
    Analysis first = analyzeOnce(names, units, files);

    if (first.problems.empty()) {
      return first;
    }

    const int maxLibraryRounds = 4;

    return retry(names, units, files, {}, move(first), maxLibraryRounds);
  }

  namespace {

    // A result with no module: the errors and the headline the CLI prints above them.
    FrontResult failed(Vector<String> errors, const char* headline) {
      FrontResult result;
      result.errors = move(errors);
      result.headline = headline;

      return result;
    }

    // The result's module (taken from the session) and what a native build would refuse.
    FrontResult withModule(FrontResult result, Session& session) {
      result.nativeProblems = NativeProblems(session, session.module);
      result.module = move(session.module);

      return result;
    }

    // A checked program becomes a FrontResult: its problems, or its module with the kernel report.
    FrontResult resultOf(Analysis analysis) {
      if (!analysis.problems.empty()) {
        return failed(move(analysis.problems), "check failed");
      }

      FrontResult result;
      const KernelReport kernels = CheckKernels(*analysis.session);
      result.kernelCount = kernels.count;
      result.kernelProblems = kernels.problems;

      return withModule(move(result), *analysis.session);
    }

    String withFile(const String& file, const String& message) {
      return file.empty() ? message : file + ":" + message;
    }

    // Each error with `file:` in front.
    Vector<String> withFileNames(const String& file, const Vector<String>& errors) {
      Vector<String> named;

      for (const String& error : errors) {
        named.push_back(withFile(file, error));
      }

      return named;
    }

    // A parsed file: its imports merged, the duplicate-name check, then the analysis. The file is file number 1 of the session.
    FrontResult checkParsed(ParseResult& parsed, const String& file) {
      ImportMerger merger = MakeImportMerger(*parsed.program.names);

      if (!MergeRoot(merger, parsed.program, file)) {
        return failed(merger.errors, "check failed");
      }

      const Vector<String> duplicates = CheckDuplicates(merger.units);

      if (!duplicates.empty()) {
        return failed(duplicates, "check failed");
      }

      FrontResult result = resultOf(Analyze(*parsed.program.names, merger.units, merger.files));
      result.module.file = file;
      result.module.fromLines = merger.fromLines;

      return result;
    }

    FrontResult withWarnings(FrontResult result, Vector<String> warnings) {
      result.warnings = move(warnings);

      return result;
    }

  }  // namespace

  // One source in memory: parse, then analyze it as the program's only unit.
  FrontResult CheckSource(const String& text) {
    ParseResult parsed = ParseSource(text);

    if (!Ok(parsed)) {
      return failed(parsed.errors, "compile failed");
    }

    const Unit unit{&parsed.program, 0, static_cast<uint32_t>(parsed.program.items.size()), 0};
    FrontResult result = resultOf(Analyze(*parsed.program.names, {unit}, {String()}));
    result.module.fromLines = FromLinesOf(text, "");

    return result;
  }

  // A file: read, parse, merge its imports, analyze, and add the lint warnings.
  FrontResult CheckFile(const String& path) {
    String text;
    const String file = NormalizePath(path);

    if (!ReadTextFile(file, text)) {
      return failed({path}, "no such file");
    }

    ParseResult parsed = ParseSource(text);

    if (!Ok(parsed)) {
      return failed(withFileNames(file, parsed.errors), "compile failed");
    }

    FrontResult result = withWarnings(checkParsed(parsed, file), LintWarnings(parsed.program, file));
    const Vector<FromLine> own = FromLinesOf(text, "");
    result.module.fromLines.insert(result.module.fromLines.begin(), own.begin(), own.end());

    return result;
  }

}  // namespace polyir::front
