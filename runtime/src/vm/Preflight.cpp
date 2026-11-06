// Preflight: decides, before anything runs, whether the VM can run a program at all. Every extern the program declares needs a
// binding (one the VM supplies itself, or a symbol of a loaded `import native` library); the VM would only trap on a missing one
// when it is called, half way through the run, so a host could no longer run the program another way. Checking first lets it decline.
#include "polyir/Std.hpp"
#include "vm/LifecycleHost.hpp"
#include "vm/Machine.hpp"

namespace polyir::vm {

  namespace {

    // The lifecycle extern is bound only when the host gave this run a step runner (LifecycleHost.cpp).
    bool isBound(const Machine& m, const ExternInfo& declared) {
      if (BindsLifecycle(declared.name)) {
        return static_cast<bool>(m.hostStep);
      }

      return BindsExtern(declared.name) || HasForeign(m, declared.symbol);
    }

    // The name of the first extern nothing binds, "" when every one is bound.
    String firstUnboundExtern(Machine& m) {
      for (const ExternInfo& declared : m.program->externs) {
        if (!isBound(m, declared)) {
          return declared.name;
        }
      }

      return "";
    }

  }  // namespace

  // The libraries first: an extern a library supplies is only bound once that library is loaded.
  Preflight RunPreflight(Machine& m) {
    Preflight verdict;
    verdict.libraryFailure = LoadLibraries(m, m.program->nativeLibraries);

    if (!verdict.libraryFailure.empty()) {
      return verdict;
    }

    verdict.unboundExtern = firstUnboundExtern(m);

    return verdict;
  }

}  // namespace polyir::vm
