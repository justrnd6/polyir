// The polyir command line as a function: `polyir-check` and `nina polyir` are both this and nothing else.
//   polyir file.poly [--emit-hir | --emit-hir-json | --emit-llvm | --analyze | --check | --ast | --bench N | --emit-msl|glsl|spirv]
//                    [--strict] [--debug] [--release] [--no-gc] [--out PATH] [--run [-- args...]]
//   polyir file.poly|file.pbc --interp [--entry NAME] [-- args...]
//   polyir file.poly|file.pbc --vm-listing
// Output goes to `out` (results, the lint warnings, the error blocks) and `err` (usage, tool failures). The result is the exit status: 0, 1 for an error
// block, 2 for a usage error, or, with --run, the exit status of the program. --out and --run build a native executable by writing `<path>.ll` and calling
// the C compiler named by $NINA_CC (the toolchain `nina` provisions), else `clang`. --interp runs the checked program (or a .pbc file, as it is) in the
// bytecode VM (polyir/Vm.hpp) with no LLVM or toolchain: the entry's value (NAME, `main` by default; the words after `--` are its arguments) is printed on
// its own line (`void` for a void entry) and is the exit status when it is a whole number; a trap prints `trap: MESSAGE` on `err` and exits 1, and a
// program the VM cannot run prints `declined: REASON` on `err` and exits 125. --vm-listing prints the bytecode the VM would run.
#pragma once

#include "polyir/Std.hpp"
#include "polyir/Export.hpp"

namespace polyir {

  POLYIR_API int RunPolyirCli(const Vector<String>& args, OStream& out, OStream& err);

  // Builds the LLVM IR text `ir` into the executable `executablePath` (`<executablePath>.ll` is written next to it). `release` is -O2, otherwise -O0 -g. Returns the
  // compiler's own diagnostics in `diagnostics` when it fails.
  POLYIR_API bool BuildExecutable(const String& ir, const String& executablePath, bool release, String& diagnostics);

  // The same, linking `libraries`: what the program names with `import native "…"` (`z` is `-lz`, a path is linked as it is). They are
  // looked for in POLYIR_LIBRARY_PATH's folders and Homebrew's besides the linker's own.
  POLYIR_API bool BuildExecutable(const String& ir, const String& executablePath, bool release, const Vector<String>& libraries, String& diagnostics);

  // Runs `executablePath` with `args` and returns its exit status (255 when it cannot be started or is killed).
  POLYIR_API int RunExecutable(const String& executablePath, const Vector<String>& args);

}  // namespace polyir
