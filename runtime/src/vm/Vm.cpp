// The public API of the bytecode VM (polyir/Vm.hpp): a checked Module (or .pbc bytes) compiled to bytecode, checked (Preflight.cpp) and run
// from its entry point. The command-line words become the entry's arguments, read by its own parameter types, and the Outcome becomes text
// and an exit status; a program the VM cannot run is declined before anything runs.
//
// The semantics were matched to the Nina-written reference VM (removed 2026-10-28, see core/docs/language/vm-proposal.md); "the oracle"
// in the comments below means that reference behaviour, and runtime/tests/vm_vs_native.sh now holds the VM to the native build.
#include "polyir/Vm.hpp"
#include "polyir/Pbc.hpp"
#include "vm/Bytecode.hpp"
#include "vm/Machine.hpp"

#include <cstdlib>
#include <iterator>
#include <pthread.h>

namespace polyir {

  using vm::CallFunction;
  using vm::CallNamed;
  using vm::CompileProgram;
  using vm::ExternInfo;
  using vm::Flow;
  using vm::FloatValue;
  using vm::IntValue;
  using vm::Machine;
  using vm::MemoryPolicy;
  using vm::MemorySettings;
  using vm::Pauses;
  using vm::PausePercentile;
  using vm::RunPreflight;
  using vm::Outcome;
  using vm::Preflight;
  using vm::Program;
  using vm::Trap;
  using vm::Value;
  using vm::ValueOutcome;
  using vm::StringList;
  using std::ios;
  using std::istreambuf_iterator;

  namespace {

    VmOutcome failed(String trap) {
      VmOutcome out;
      out.trap = move(trap);
      out.exitStatus = 1;

      return out;
    }

    // The status a host sees when nothing ran, distinct from a trap's 1 and from what a program's own exit is likely to be.
    constexpr int64_t DECLINED_STATUS = 125;

    VmOutcome declined(const String& externName) {
      VmOutcome out;
      out.declined = true;
      out.declineReason = "'" + externName + "' has no binding on the VM";
      out.exitStatus = DECLINED_STATUS;

      return out;
    }

    // ── numbers read the way Nina's String.toInt / toDouble read them (as the oracle read its arguments and exit status) ──

    bool isDigit(char c) {
      return c >= '0' && c <= '9';
    }

    bool allDigits(const String& s) {
      return all_of(s.begin(), s.end(), isDigit);
    }

    // The text after its optional sign.
    String unsignedPart(const String& word) {
      return word.empty() || (word[0] != '-' && word[0] != '+') ? word : word.substr(1);
    }

    bool isNegative(const String& word) {
      return !word.empty() && word[0] == '-';
    }

    // An optional sign, then digits and nothing else; the digits accumulate in an Int that wraps at 64 bits, as Nina's does.
    Optional<int64_t> wholeNumber(const String& word) {
      const String digits = unsignedPart(word);

      if (digits.empty() || !allDigits(digits)) {
        return {};
      }

      uint64_t magnitude = 0;

      for (const char c : digits) {
        magnitude = magnitude * 10 + static_cast<uint64_t>(c - '0');
      }

      return static_cast<int64_t>(isNegative(word) ? 0 - magnitude : magnitude);
    }

    // `digits[.digits]` with a digit somewhere, then an optional `e/E[sign]digits`: what String.toDouble accepts.
    bool isMantissa(const String& s) {
      const size_t dot = s.find('.');

      if (dot == String::npos) {
        return !s.empty() && allDigits(s);
      }

      const bool hasDigit = s.size() > 1;  // "." alone has none

      return hasDigit && allDigits(s.substr(0, dot)) && allDigits(s.substr(dot + 1));
    }

    bool isExponent(const String& s) {
      const String digits = unsignedPart(s);

      return !digits.empty() && allDigits(digits);
    }

    bool isNumberBody(const String& s) {
      const size_t mark = s.find_first_of("eE");

      if (mark == String::npos) {
        return isMantissa(s);
      }

      return isMantissa(s.substr(0, mark)) && isExponent(s.substr(mark + 1));
    }

    // The value is strtod's (correctly rounded; the oracle's own digit accumulation could differ in the last bit of a long literal).
    Optional<double> realNumber(const String& word) {
      if (!isNumberBody(unsignedPart(word))) {
        return {};
      }

      return strtod(word.c_str(), nullptr);
    }

    String trimmed(const String& s) {
      const size_t from = s.find_first_not_of(" \t\r\n");

      if (from == String::npos) {
        return "";
      }

      return s.substr(from, s.find_last_not_of(" \t\r\n") - from + 1);
    }

    // The exit status is read from the value's TEXT, as the Nina CLI does (`value.trim().toInt()`). A whole number carries it (its
    // digits wrapped to 64 bits, the low byte is what the OS keeps). Anything else (true, 2.5, a string, void) is 0.
    VmOutcome succeeded(const Value& v) {
      VmOutcome out;
      out.ok = true;
      out.result = Show(v);
      out.exitStatus = wholeNumber(trimmed(out.result)).value_or(0);

      return out;
    }

    // What the entry's Outcome means to the caller: an uncaught `throw` and a trap both end the run with a message.
    VmOutcome outcomeOf(const Outcome& ran) {
      if (ran.flow == Flow::Value) {
        return succeeded(ran.value);
      }

      if (ran.flow == Flow::Throw) {
        return failed("uncaught throw " + to_string(ran.thrownCode));
      }

      if (ran.flow == Flow::Trap) {
        return failed(ran.message);
      }

      return failed("the entry point did not return");
    }

    // ── the arguments, read by the entry's parameter types ──

    Outcome notANumber(const String& word) {
      return Trap("'" + word + "' is not a number");
    }

    // The oracle read every parameter that is not a float as an integer, at the type's width and signedness, without wrapping. That
    // includes a bool or a string parameter: "true" and "hello" are not numbers. The width is an integer's bits, 64 for anything
    // else.
    Outcome intArgument(const String& word, const TypePtr& type) {
      const Optional<int64_t> value = wholeNumber(word);

      if (!value) {
        return notANumber(word);
      }

      const int bits = type && IsInteger(*type) ? type->bits : 64;

      return ValueOutcome(IntValue(*value, bits, !(type && type->kind == TypeKind::UInt)));
    }

    Outcome floatArgument(const String& word, const TypePtr& type) {
      const Optional<double> value = realNumber(word);

      if (!value) {
        return notANumber(word);
      }

      return ValueOutcome(FloatValue(*value, type->bits));
    }

    Outcome argument(const String& word, const TypePtr& type) {
      if (type && IsFloat(*type)) {
        return floatArgument(word, type);
      }

      return intArgument(word, type);
    }

    Outcome wrongCount(size_t wanted, size_t got) {
      return Trap("the entry takes " + to_string(wanted) + " argument(s), got " + to_string(got));
    }

    // The entry's arguments: the values, or the trap that stopped reading them.
    struct Arguments {
      Vector<Value> values;
      Outcome problem;
    };

    Arguments readArguments(const Vector<TypePtr>& types, const Vector<String>& words) {
      Arguments read;

      for (size_t i = 0; i < words.size(); i++) {
        read.problem = argument(words[i], types[i]);

        if (!IsValue(read.problem)) {
          return read;
        }

        read.values.push_back(read.problem.value);
      }

      return read;
    }

    bool isStringList(const TypePtr& type) {
      return type && type->kind == TypeKind::DynamicArray && type->element && type->element->kind == TypeKind::String;
    }

    // An entry whose one parameter is a list of strings takes every word as that list, the way a C `main` takes argv. That is a
    // Nina `main(args: List<String>)`, lowered with its arguments kept.
    bool takesWordList(const Vector<TypePtr>& types) {
      return types.size() == 1 && isStringList(types[0]);
    }

    // The words as values, one per parameter; an entry with no parameters takes none (the words reach it through `polyirArgs`).
    Arguments arguments(const Vector<TypePtr>& types, const Vector<String>& words) {
      if (types.empty()) {
        return Arguments{};
      }

      if (words.size() != types.size()) {
        return Arguments{{}, wrongCount(types.size(), words.size())};
      }

      return readArguments(types, words);
    }

    // ── the run ──

    // The entry point: a declared function by index, or an extern (the oracle found those too, and called them as externs); its
    // parameter types read the arguments.
    struct Entry {
      int32_t function = -1;
      const ExternInfo* declaredExtern = nullptr;
      Vector<TypePtr> parameterTypes;
    };

    Vector<TypePtr> typesOf(const Vector<Parameter>& parameters) {
      Vector<TypePtr> out;

      for (const Parameter& p : parameters) {
        out.push_back(p.type);
      }

      return out;
    }

    Entry declaredEntry(const Module& module, const String& name, int32_t function) {
      Entry e;
      e.function = function;
      e.parameterTypes = typesOf(FindFunction(module, name)->parameters);

      return e;
    }

    // No extern of the name leaves the entry empty (`isEntry` says so).
    Entry externEntry(const Program& program, const String& name) {
      for (const ExternInfo& declared : program.externs) {
        if (declared.name == name) {
          return Entry{-1, &declared, declared.parameters};
        }
      }

      return Entry{};
    }

    Entry entryNamed(const Program& program, const Module& module, const String& name) {
      const auto found = program.functionIndex.find(name);

      if (found != program.functionIndex.end()) {
        return declaredEntry(module, name, found->second);
      }

      return externEntry(program, name);
    }

    bool isEntry(const Entry& e) {
      return e.function >= 0 || e.declaredExtern != nullptr;
    }

    Outcome callEntry(Machine& m, const Entry& e, Vector<Value> args) {
      if (e.function >= 0) {
        return CallFunction(m, e.function, move(args));
      }

      return CallNamed(m, e.declaredExtern->name, move(args));
    }

    // The words joined by newlines, in the environment before the run, as the oracle did it. `polyirArgs` reads them back
    // from there, `polyirEnv` sees them, and the commands the program runs inherit them.
    void exportArguments(const Vector<String>& words) {
      String joined;

      for (size_t i = 0; i < words.size(); i++) {
        joined += (i == 0 ? "" : "\n") + words[i];
      }

      setenv("POLYIR_ARGS", joined.c_str(), 1);
    }

    // An environment setting, "" when it is not set.
    String setting(const char* name) {
      const char* value = getenv(name);

      return value != nullptr ? String(value) : String();
    }

    // POLYIR_GC_MODE=stop|incremental, when it is set, wins over VmOptions (as for native builds): anything but "stop" is incremental.
    bool incrementalFrom(const VmOptions& options) {
      const String mode = setting("POLYIR_GC_MODE");

      if (mode.empty()) {
        return options.incrementalCollector;
      }

      return mode != "stop";
    }

    // POLYIR_GC_SLICE=N, when it is set, wins over VmOptions (as for native builds).
    int64_t sliceFrom(const VmOptions& options) {
      const String slice = setting("POLYIR_GC_SLICE");

      if (slice.empty()) {
        return options.collectorSlice;
      }

      return strtoll(slice.c_str(), nullptr, 10);
    }

    // The memory settings of a run, from its options and the environment. POLYIR_VM_STRESS_GC=1 (a whole collection at every safe
    // point) or =incremental (the incremental stress mode) applies to every run in the process. That is the way a whole test suite
    // is run under it.
    MemorySettings memorySettings(const VmOptions& options) {
      MemorySettings settings;
      settings.ceiling = options.heapCeiling;

      settings.collectEverySafePoint = options.collectEverySafePoint || setting("POLYIR_VM_STRESS_GC") == "1";
      settings.verify = options.verifyIncremental || setting("POLYIR_VM_STRESS_GC") == "incremental";

      settings.incremental = incrementalFrom(options);
      settings.slice = sliceFrom(options);

      return settings;
    }

    Machine machineFor(const Program& program, const VmOptions& options) {
      Machine m;
      m.program = &program;
      m.out = options.out;
      m.hostStep = options.hostStep;
      m.memory = MemoryPolicy(memorySettings(options));

      return m;
    }

    // How long the program stood still while the collector worked.
    VmOutcome withPauses(VmOutcome out, const Pauses& pauses) {
      out.pauses = pauses.count;
      out.longestPauseNs = pauses.longest;
      out.medianPauseNs = PausePercentile(pauses, 50);
      out.p99PauseNs = PausePercentile(pauses, 99);
      out.totalPauseNs = pauses.total;

      return out;
    }

    // The collector's statistics go out with the outcome; the heap's last count may be its peak (no collection has run since).
    VmOutcome withMemoryStats(VmOutcome out, const Machine& m) {
      out.collections = m.memory.collections;
      out.objectsReclaimed = m.memory.reclaimed;
      out.peakObjects = max(m.memory.peakObjects, m.heap.objects);

      return withPauses(out, m.memory.pauses);
    }

    // The entry called with the words read by its parameter types (a word that does not read is the outcome instead).
    VmOutcome callWithWords(Machine& m, const Entry& entry, const Vector<String>& words) {
      if (takesWordList(entry.parameterTypes)) {
        return outcomeOf(callEntry(m, entry, {StringList(m.heap, words)}));
      }

      Arguments args = arguments(entry.parameterTypes, words);

      if (!IsValue(args.problem)) {
        return outcomeOf(args.problem);
      }

      return outcomeOf(callEntry(m, entry, move(args.values)));
    }

    VmOutcome runPrepared(Machine& m, const Entry& entry, const Vector<String>& words);

    // Nothing runs unless the VM can run all of it (Preflight.cpp). A library that does not load is a trap, an extern nothing binds
    // a decline.
    VmOutcome runEntry(Machine& m, const Module& module, const VmOptions& options) {
      const Entry entry = entryNamed(*m.program, module, options.entry);

      if (!isEntry(entry)) {
        return failed("entry point '" + options.entry + "' not found");
      }

      const Preflight verdict = RunPreflight(m);

      if (!verdict.libraryFailure.empty()) {
        return failed(verdict.libraryFailure);
      }

      if (!verdict.unboundExtern.empty()) {
        return declined(verdict.unboundExtern);
      }

      return runPrepared(m, entry, options.args);
    }

    // The globals first (an initializer may already read the arguments), then the entry.
    VmOutcome runPrepared(Machine& m, const Entry& entry, const Vector<String>& words) {
      exportArguments(words);
      const Outcome prepared = Prepare(m);

      if (!IsValue(prepared)) {
        return outcomeOf(prepared);
      }

      return callWithWords(m, entry, words);
    }

    VmOutcome runCompiled(const Module& module, const VmOptions& options) {
      const Program program = CompileProgram(module);
      Machine m = machineFor(program, options);

      return withMemoryStats(runEntry(m, module, options), m);
    }

    // The file's bytes as they are (a .pbc is binary: no text-mode translation); none when it cannot be opened.
    Optional<Vector<uint8_t>> readBytes(const String& path) {
      IFStream in{path, ios::binary};

      if (!in) {
        return {};
      }

      return Vector<uint8_t>(istreambuf_iterator<char>(in), istreambuf_iterator<char>());
    }

    VmOutcome runGuarded(const Module& module, const VmOptions& options) {
      try {
        return runCompiled(module, options);
      } catch (const RuntimeError& problem) {
        return failed(problem.what());  // CompileProgram refuses a construct the VM cannot run
      }
    }

    // The machine's calls nest on the host's own stack (about 2 KB per level), so the 20000-deep "stack overflow" guard needs more room
    // than a main thread's 8 MB: the run happens on a thread whose stack is large enough for the guard to fire first.
    constexpr size_t RUN_STACK_BYTES = size_t{256} << 20;  // 256 MB

    struct RunRequest {
      const Module* module = nullptr;
      const VmOptions* options = nullptr;
      VmOutcome outcome;
    };

    void* runRequested(void* raw) {
      RunRequest& r = *static_cast<RunRequest*>(raw);
      r.outcome = runGuarded(*r.module, *r.options);

      return nullptr;
    }

    // True when the request ran to its end on a thread with the large stack.
    bool ranOnThread(RunRequest& r) {
      pthread_attr_t attr;
      pthread_attr_init(&attr);
      pthread_attr_setstacksize(&attr, RUN_STACK_BYTES);

      pthread_t thread;
      const bool started = pthread_create(&thread, &attr, runRequested, &r) == 0;
      pthread_attr_destroy(&attr);

      return started && pthread_join(thread, nullptr) == 0;
    }

    VmOutcome onLargeStack(const Module& module, const VmOptions& options) {
      RunRequest r{&module, &options, {}};

      if (!ranOnThread(r)) {
        runRequested(&r);  // no thread to be had: run here, where the guard may come too late
      }

      return r.outcome;
    }

    // `source` names the bytes in the trap a bad file gives ("'hello.pbc'", "the bytecode").
    VmOutcome runBytesFrom(const String& source, const Vector<uint8_t>& bytes, const VmOptions& options) {
      try {
        return RunModule(ReadPbc(bytes), options);
      } catch (const HirReadError& problem) {
        return failed(source + " is not a .pbc file: " + problem.what());
      }
    }

  }  // namespace

  VmOutcome RunModule(const Module& module, const VmOptions& options) {
    return onLargeStack(module, options);
  }

  VmOutcome RunBytecode(const Vector<uint8_t>& bytes, const VmOptions& options) {
    return runBytesFrom("the bytecode", bytes, options);
  }

  VmOutcome RunPbcFile(const String& path, const VmOptions& options) {
    const Optional<Vector<uint8_t>> bytes = readBytes(path);

    if (!bytes) {
      return failed("cannot read '" + path + "'");
    }

    return runBytesFrom("'" + path + "'", *bytes, options);
  }

  namespace {

    // What an operating system keeps of a whole-number result as an exit status: its low 8 bits (as the CLI does).
    int exitStatusByte(int64_t value) {
      return static_cast<int>(((value % 256) + 256) % 256);
    }

    VmOptions entryOptions(const String& entry, const Vector<String>& args, OStream& out) {
      VmOptions options;
      options.entry = entry;
      options.args = args;
      options.out = &out;

      return options;
    }

    // What a host embedding the VM sees of a run: nothing on success, one line on `err` for a trap or a decline. The answer is the
    // status.
    int reportedStatus(const VmOutcome& ran, OStream& err) {
      if (ran.ok) {
        return exitStatusByte(ran.exitStatus);
      }

      if (ran.declined) {
        err << "declined: " << ran.declineReason << "\n";
      } else {
        err << "trap: " << ran.trap << "\n";
      }

      return static_cast<int>(ran.exitStatus);
    }

    // The caller's bytes copied as they are; a null pointer or a negative size is no bytes (ReadPbc then refuses them).
    Vector<uint8_t> copiedBytes(const unsigned char* bytes, long size) {
      if (bytes == nullptr || size <= 0) {
        return {};
      }

      return Vector<uint8_t>(bytes, bytes + size);
    }

    // The outcome of a run as the one line RunPbcBytesOutcome hands back.
    String outcomeLine(const VmOutcome& ran) {
      if (ran.ok) {
        return "value: " + ran.result;
      }

      if (ran.declined) {
        return "declined: " + ran.declineReason;
      }

      return "trap: " + ran.trap;
    }

  }  // namespace

  int RunPbcEntry(const String& path, const String& entry, const Vector<String>& args, OStream& out, OStream& err) {
    return reportedStatus(RunPbcFile(path, entryOptions(entry, args, out)), err);
  }

  int RunPbcBytesEntry(const unsigned char* bytes, long size, const String& entry, const Vector<String>& args, OStream& out,
                       OStream& err) {
    return reportedStatus(RunBytecode(copiedBytes(bytes, size), entryOptions(entry, args, out)), err);
  }

  String RunPbcBytesOutcome(const unsigned char* bytes, long size, const String& entry, const Vector<String>& args, OStream& out) {
    return outcomeLine(RunBytecode(copiedBytes(bytes, size), entryOptions(entry, args, out)));
  }

}  // namespace polyir
