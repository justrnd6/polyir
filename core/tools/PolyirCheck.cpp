// polyir-check: the polyir command line (include/polyir/Cli.hpp, src/cli/Cli.cpp) as a standalone program, behaving as the Nina `polyir` CLI does for the
// same flags. `nina polyir` runs the very same function.
//   polyir-check file.poly               parse, lint and check; print the warnings and errors, or nothing
//   polyir-check file.poly --hir         the checked program as typed HIR text, one declaration per line (also --emit-hir)
//   polyir-check file.poly --json        the checked program as JSON (also --emit-hir-json)
//   polyir-check file.poly --analyze     check without building: prints `ok`
//   polyir-check file.poly --check       check and compile to LLVM, verified: prints `ok`
//   polyir-check file.poly --llvm        the checked program compiled to LLVM IR (also --emit-llvm); --debug adds DWARF
//   polyir-check file.poly --out PATH    build an executable (--release: -O2); --run builds to a temporary file and runs it
//   polyir-check file.poly --emit-msl | --emit-glsl | --emit-spirv      the kernel rules only
//   polyir-check file.poly --ast         the syntax tree
//   polyir-check file.poly --bench N     parse, check and lint the file N times; print milliseconds per run
//   polyir-check file.poly|file.pbc --interp [--entry NAME] [-- args...]   run the entry in the bytecode VM and print its value
//   polyir-check file.poly|file.pbc --vm-listing                         the bytecode the VM would run
//   --strict                             lint warnings (an unused `var`) are errors
//   --no-gc                              compile to LLVM without the garbage collector (--llvm, --check, --out, --run)
#include "polyir/Cli.hpp"

using namespace polyir;

int main(int argc, char** argv) {
  return RunPolyirCli(Vector<String>(argv + 1, argv + argc), cout, cerr);
}
