// The bytecode VM: runs a checked HIR Module (or a .pbc file) without LLVM or a toolchain. The module is compiled to a flat stack bytecode at
// load (src/vm/Bytecode.hpp), checked before anything runs (every extern it declares must have a binding, src/vm/Preflight.cpp), and run.
//
//   const VmOutcome ran = RunModule(module, options);
//   if (ran.declined) std::cerr << "declined: " << ran.declineReason << "\n";    // nothing ran: run it some other way
//   else if (!ran.ok) std::cerr << "trap: " << ran.trap << "\n"; else std::cout << ran.result << "\n";
#pragma once

#include "polyir/Std.hpp"
#include "polyir/Export.hpp"
#include "polyir/Hir.hpp"

#include <iostream>

namespace polyir {

  struct POLYIR_API VmOutcome {
    bool ok = false;
    String trap;  // the first trap's message (an uncaught `throw` reads "uncaught throw N"), when `ok` is false
    String result;  // the entry point's value as text ("void" for void, as the Nina CLI shows it)
    int64_t exitStatus = 0;  // the value when its text is a whole number (wrapped to 64 bits), else 0; 1 on a trap; 125 when declined
    // Nothing ran: an extern the program declares has no binding on the VM (none of its own, no symbol of a loaded library), so a host can
    // still run the program another way. `declineReason` says which: "'NAME' has no binding on the VM".
    bool declined = false;
    String declineReason;
    // What the garbage collector did (src/vm/Collector.cpp, docs/memory.md): a long-running program that holds a steady amount of data
    // shows a steady `peakObjects`, however many objects it made.
    int64_t collections = 0;  // collections run
    int64_t objectsReclaimed = 0;  // objects swept over the whole run
    int64_t peakObjects = 0;  // the most objects the heap held at once (reachable or not yet collected)
    // How long the program stood still while the collector worked (docs/memory.md, Part 3): the number of pauses, the longest, the median,
    // the 99th percentile (each to the microsecond below 1 ms) and their sum, in nanoseconds.
    int64_t pauses = 0;
    int64_t longestPauseNs = 0;
    int64_t medianPauseNs = 0;
    int64_t p99PauseNs = 0;
    int64_t totalPauseNs = 0;
  };

  // The default budget of one slice of the incremental collector: 10,000 values looked at (or cells swept), some tens of microseconds.
  inline constexpr int64_t DEFAULT_COLLECTOR_SLICE = 10000;

  // The default heap ceiling: 16M objects. A heap cell is about 224 bytes before what its lists hold, so this is some 3.5 GB of cells —
  // far more than a VM program keeps reachable (the corpus peaks in the thousands), and still short of what a laptop has, so a runaway
  // program inside a host (`nina up` runs its lifecycle script in-process) ends with a trap instead of the operating system ending the host.
  inline constexpr int64_t DEFAULT_HEAP_CEILING = int64_t{1} << 24;

  // Runs one lifecycle step (`compile`, `test`, `run`, `script`, `fetch`, `bindings`) inside the host that embeds the VM, for std
  // `Lifecycle.step`: the step runs in the host's own process instead of re-running nina as a child. In: the verb, the project
  // directory and the step's words; out: the step's exit status.
  using HostStep = Function<int64_t(const String& verb, const String& dir, const Vector<String>& args)>;

  struct POLYIR_API VmOptions {
    String entry = "main";
    Vector<String> args;  // the program's arguments: an entry that takes parameters gets them parsed by its types; `polyirArgs` hands them out
    OStream* out = &cout;  // where `print`, `puts` and `putchar` write
    HostStep hostStep;  // empty: a program that calls std `Lifecycle.step` is declined, since nothing here could run its steps
    // A collection that leaves more objects reachable than this ends the run with the trap
    // `out of memory: N objects are still reachable (limit L)`.
    int64_t heapCeiling = DEFAULT_HEAP_CEILING;
    // The stress mode: a collection at every safe point (between every two instructions), so a value the collector cannot see shows up
    // at once as a wrong answer. Also on when the environment sets POLYIR_VM_STRESS_GC=1 (to run whole test suites that way).
    bool collectEverySafePoint = false;
    // How the collector runs (docs/memory.md, Part 3): a cycle in slices of at most `collectorSlice` units of work between which the program
    // runs (incremental), or each cycle at once (stop-the-world). POLYIR_GC_MODE=stop|incremental and POLYIR_GC_SLICE=N in the environment
    // win over these, as they do for native builds.
    bool incrementalCollector = true;
    int64_t collectorSlice = DEFAULT_COLLECTOR_SLICE;
    // The incremental stress mode: a slice of one unit at every safe point, a new cycle as soon as one ends, and at the end of every marking
    // a check that nothing the program can reach was left unmarked (a missing write barrier ends the run with a `gc verify` trap). Also on
    // when the environment sets POLYIR_VM_STRESS_GC=incremental.
    bool verifyIncremental = false;
  };

  // Runs the entry point of a checked module.
  POLYIR_API VmOutcome RunModule(const Module& module, const VmOptions& options);

  // The instructions the VM compiles the module to, as text: one block per function, one `pc  mnemonic  operands` line per
  // instruction (docs/bytecode.md). Throws RuntimeError on a construct the VM cannot run, as running it would.
  POLYIR_API String BytecodeListing(const Module& module);

  // Reads .pbc bytes (ReadPbc) and runs them; bytes that are not a .pbc are a trap.
  POLYIR_API VmOutcome RunBytecode(const Vector<uint8_t>& bytes, const VmOptions& options);

  // Reads a .pbc file and runs it (RunBytecode); a file that cannot be read is a trap.
  POLYIR_API VmOutcome RunPbcFile(const String& path, const VmOptions& options);

  // The C++17 doors below take only standard types (a C++17 host declares them by hand, as it does RunPolyirCli). So they
  // cannot carry a HostStep: a program that calls std `Lifecycle.step` is declined through them. A host that runs lifecycle
  // steps calls RunBytecode with VmOptions (as libninai does).

  // Runs a .pbc file's entry (its value unprinted); status: the value's low 8 bits, 1 on a trap, 125 declined (both said on err).
  POLYIR_API int RunPbcEntry(const String& path, const String& entry, const Vector<String>& args, OStream& out,
                             OStream& err);

  // The same for .pbc bytes already in memory (what libninac's bytecode target hands back).
  POLYIR_API int RunPbcBytesEntry(const unsigned char* bytes, long size, const String& entry, const Vector<String>& args,
                                  OStream& out, OStream& err);

  // Runs .pbc bytes and says how it went as "value: TEXT", "trap: MSG" or "declined: REASON" (for `polyir --interp`, `poly`).
  POLYIR_API String RunPbcBytesOutcome(const unsigned char* bytes, long size, const String& entry, const Vector<String>& args,
                                       OStream& out);

}  // namespace polyir
