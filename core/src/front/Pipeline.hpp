// The pipeline, front to back (PolyirPipeline.nina). Every entry point runs the same phases in the same order; this file is where to read WHEN each
// one happens, and where each later area plugs in. A phase is a free function in a file named after the Nina file it is ported from:
//
//   1 PARSE          ParseSource                             text -> syntax tree (Parser*.cpp)
//   2 IMPORTS        MergeRoot, CheckDuplicates              CheckFile only: merge imported files (ImportMerge.cpp), reject duplicate names (Imports.cpp)
//   3 SYNTAX RULES   ExternTypeProblems                      ExternTypes.cpp   `extern type` / `extern interface` declarations
//                    InterfaceProblems                       Interfaces.cpp    `interface` declarations and `struct X : Interface` claims
//                    CoroutineRestrictions                   Coroutines.cpp    coroutine rules that need the raw tree
//   4 MONOMORPHIZE   Monomorphize                            Generics.cpp      `Box<T>` -> one concrete struct per use (then InterfaceProblems
//                                                                             again, on the concrete structs)
//   5 CHECK          ResolveDeclarations + CheckBodies       Resolve.cpp, Check*.cpp: tree -> typed HIR, every name and type resolved
//     DIAGNOSE       Diagnose                                CheckDiagnose.cpp: unknown names, wrong argument counts, ... as `line:col: message`
//   + LIBRARY        LibraryUnits                            Prelude.cpp       a program that calls library functions it has not defined is checked
//                                                                             again with them added
//
// After a phase that reports problems the pipeline stops and returns them. A phase works on the session (`Session::units` is the program: the
// phases before the check may change it).
#pragma once

#include "front/Session.hpp"

namespace polyir::front {

  // ── the phases ──────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────

  Vector<String> ExternTypeProblems(Session& session);   // ExternTypes.cpp

  // NativeProblems.cpp: what a native build cannot take (host-only declarations in use).
  Vector<String> NativeProblems(const Session& session, const Module& module);

  String FileOf(const Session& session, Symbol name);    // ExternTypes.cpp: the file of the first declaration of a name

  Vector<String> InterfaceProblems(Session& session);    // Interfaces.cpp (called before and after monomorphize)

  Vector<String> CoroutineRestrictions(Session& session);  // Coroutines.cpp

  void Monomorphize(Session& session);                   // Generics.cpp: may replace session.units

  Vector<String> MissingLibraryFunctions(const Vector<String>& problems);  // Prelude.cpp: the library functions the problems say are unknown

  // Prelude.cpp: the units that define those library functions (and what they call), in the library's own source, parsed once per process.
  Vector<Unit> LibraryUnits(Interner& names, const Vector<String>& wanted);

  void CheckBodies(Session& session);                    // CheckBody.cpp

  Vector<String> Diagnose(const Session& session);       // CheckDiagnose.cpp

  String DiagnosticMessage(const String& what, Span at); // `line:col: what`

  // ── the generics as declared (Parametric.cpp, CheckGenericCall.cpp) ──

  Vector<String> FunctionProblems(const Session& session, size_t index);  // one function's problems, as Diagnose reports them

  bool IsParametric(const Module& module, const Generic& generic);        // a copy made from `Param` types: a generic as declared, not code

  void DeclareGenericFunctions(Session& session);                         // each generic function read once as `pick_T`

  void NoteParametric(Session& session);                                  // why each generic as declared does not work for every type

  void SplitGenerics(Session& session);                                   // the parametric copies moved to genericStructs / genericFunctions

  // ── imports (Imports.cpp) ───────────────────────────────────────────────────────────────────────────────────────────────────────────────────

  // Resolves the `import` lines of a parsed file (ImportMerge.cpp): the units of the merged program, in source order, the files they came from (files[0]
  // is empty; a unit's `file` indexes it) or the errors. The imported programs are owned by the merger, so it must outlive the check.
  struct ImportMerger {
    Vector<Unit> units;
    Vector<String> files;
    Vector<String> errors;

    Interner& names;
    Deque<ParseResult> programs{};  // the imported files, parsed (a deque, so a program never moves while units point at it)
    Set<String> visited{};          // the normalized paths of the files entered so far
    Vector<FromLine> fromLines{};   // the imported files' `//# from` lines
  };

  String NormalizePath(const String& path);              // `./a/../b.poly` -> `b.poly`, as the Nina Path.normalize

  // The three rules of the duplicate check. Two structs sharing a name are an error, and so are two funcs (or a func and an extern) sharing a
  // name. So are externs of one name with different signatures; two externs of one name with the same signature are one. Answers the errors, and
  // may split units to drop an item.
  Vector<String> CheckDuplicates(Vector<Unit>& units);

  // Reads a whole file; false when it cannot be opened.
  bool ReadTextFile(const String& path, String& text);

  // The `//# from FILE:LINE[:COLUMN]` lines of the text of file `in` ("" for the checked file) (FromLines.cpp).
  Vector<FromLine> FromLinesOf(const String& text, const String& in);

  // A merger with no files yet.
  ImportMerger MakeImportMerger(Interner& names);

  // Merges the root file and, depth first, everything it imports.
  bool MergeRoot(ImportMerger& merger, const Program& root, const String& path);  // `path` is the normalized path of the root file

  // ── the analysis ────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────

  // A checked program: the session holds the module; `problems` is empty when it is fine.
  struct Analysis {
    UniquePtr<Session> session;
    Vector<String> problems;
  };

  // Phases 3-5 (the syntax rules, monomorphize, the check) and the diagnose step, then (when they report problems) again with the library added.
  Analysis Analyze(Interner& names, const Vector<Unit>& units, const Vector<String>& files);

}  // namespace polyir::front
