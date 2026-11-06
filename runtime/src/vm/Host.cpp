// The externs the VM supplies itself: the `nina_*` runtime ABI a lowered Nina program declares (NinaRuntime.cpp, consulted first), the
// std Process natives (ProcessHost.cpp), the few C functions it binds directly (`putchar`, `puts`, the libm floats the examples use),
// the host layer (`polyirFsRead`, `polyirRun`, `polyirEnv` …: files, commands, the environment, the os and cpu, the program's
// arguments), and the C libraries an `import native` names, called through dlopen/dlsym with the extern's declared types.
//
// The semantics were matched to the Nina-written reference VM (removed 2026-10-28, see core/docs/language/vm-proposal.md); "the oracle"
// in the comments below means that reference behaviour, and runtime/tests/vm_vs_native.sh now holds the VM to the native build. Every
// message and result follows the oracle.
#include "polyir/Std.hpp"
#include "vm/Commands.hpp"
#include "vm/LifecycleHost.hpp"
#include "vm/Machine.hpp"
#include "vm/NinaRuntime.hpp"
#include "vm/ProcessHost.hpp"

#include <bit>
#include <cmath>
#include <dlfcn.h>

namespace polyir::vm {

  namespace {

    using std::bit_cast;
    using std::filesystem::create_directories;
    using std::filesystem::current_path;
    using std::filesystem::exists;
    using std::filesystem::is_regular_file;
    using std::filesystem::remove_all;
    using std::floor;
    using std::getenv;
    using std::pow;
    using std::sin;
    using std::copy_n;
    using std::sqrt;

    // ── the host layer's values ──

    const Vector<String>& hostNames() {
      static const Vector<String> names = {"polyirFsRead", "polyirFsWrite", "polyirFsExists", "polyirFsMkdirs", "polyirFsRemove",
        "polyirRun", "polyirShow", "polyirFetch", "polyirEnv", "polyirOs", "polyirCpu", "polyirCwd", "polyirArgs"};

      return names;
    }

    bool bindsHost(const String& name) {
      return find(hostNames().begin(), hostNames().end(), name) != hostNames().end();
    }

    bool isMath(const String& name) {
      return name == "powf" || name == "sinf" || name == "floorf" || name == "sqrtf";
    }

    bool bindsNative(const String& name) {
      return name == "putchar" || name == "puts" || isMath(name);
    }

    // A string result carries a fresh identity, as every string-producing expression does.
    Outcome textResult(Machine& m, String s) {
      return ValueOutcome(StrValue(m.heap, move(s)));
    }

    Outcome boolResult(bool value) {
      return ValueOutcome(BoolValue(value));
    }

    // ── commands (Commands.cpp) ──

    // `Ran(code, out, err)`: the program's own struct, made directly (the layout is the host library's, not looked up).
    Outcome ranResult(Machine& m, const ChildRun& r) {
      HeapObject ran;
      ran.kind = ObjectKind::Struct;
      ran.name = "Ran";
      ran.items = {I64Value(r.code), StrValue(m.heap, r.output), StrValue(m.heap, r.errorOutput)};

      return ValueOutcome(RefValue(Alloc(m.heap, move(ran))));
    }

    ChildRun run(const Vector<String>& words) {
      return RunCaptured(CommandLine(ChildCommand{words, "", {}}));
    }

    // `polyirShow` lets the output through to the terminal: what the program printed so far goes first.
    int64_t passThrough(Machine& m, const ChildCommand& command) {
      m.out->flush();
      cout.flush();

      return RunPassingThrough(CommandLine(command));
    }

    Outcome command(Machine& m, const String& name, const Vector<Value>& args) {
      ChildCommand child{WordsAt(m.heap, args, 0), WordAt(args, 1), {}};

      if (name != "polyirShow") {
        return ranResult(m, RunCaptured(CommandLine(child)));
      }

      child.environment = WordsAt(m.heap, args, 2);

      return ValueOutcome(I64Value(passThrough(m, child)));
    }

    // ── files ──

    String readAll(const String& path) {
      IFStream in{path};
      OStringStream text;
      text << in.rdbuf();

      return text.str();
    }

    // Everything before the last segment (`"/a/b" -> "/a"`, `"a" -> ""`), as the oracle gave it.
    String parentOf(const String& path) {
      const size_t slash = path.rfind('/');

      if (slash == String::npos) {
        return "";
      }

      if (slash == 0) {
        return "/";
      }

      return path.substr(0, slash);
    }

    // The oracle's host layer read and wrote through Nina's FileSystem, which does not fail softly. A failure puts
    // `FileSystem.readText: PATH` on the error stream, and the process ends with status 1. The VM does the same, so a program sees
    // one behaviour.
    [[noreturn]] void exitOnFileSystemFailure(Machine& m, const String& operation, const String& path) {
      m.out->flush();
      cerr << "FileSystem." << operation << ": " << path << "\n";
      exit(1);
    }

    String contentOf(Machine& m, const String& path) {
      ErrorCode ec;

      if (!exists(path, ec)) {
        return "";
      }

      if (!is_regular_file(path, ec)) {
        exitOnFileSystemFailure(m, "readText", path);
      }

      return readAll(path);
    }

    bool wroteFile(const String& path, const String& content) {
      OFStream out{path};

      if (!out) {
        return false;
      }

      out << content;

      return static_cast<bool>(out);
    }

    // Writes the file, making its folder first; whether the file is there afterwards.
    bool written(Machine& m, const String& path, const String& content) {
      const String folder = parentOf(path);
      ErrorCode ec;

      if (!folder.empty() && folder != ".") {
        create_directories(folder, ec);
      }

      if (!wroteFile(path, content)) {
        exitOnFileSystemFailure(m, "writeText", path);
      }

      return exists(path, ec);
    }

    bool madeDirs(const String& path) {
      ErrorCode ec;
      create_directories(path, ec);

      return !ec;  // `mkdir -p`: an existing folder is success
    }

    bool removed(const String& path) {
      ErrorCode ec;
      remove_all(path, ec);

      return !ec;  // `rm -rf`: a missing path is success
    }

    bool pathExists(const String& path) {
      ErrorCode ignored;

      return exists(path, ignored);
    }

    // The file-system calls on the path itself: whether it exists, `mkdir -p` and `rm -rf`.
    Outcome pathCall(const String& name, const String& path) {
      if (name == "polyirFsExists") {
        return boolResult(pathExists(path));
      }

      if (name == "polyirFsMkdirs") {
        return boolResult(madeDirs(path));
      }

      if (name == "polyirFsRemove") {
        return boolResult(removed(path));
      }

      return Trap("'" + name + "' is not part of the host layer");
    }

    Outcome files(Machine& m, const String& name, const Vector<Value>& args) {
      const String path = WordAt(args, 0);

      if (name == "polyirFsRead") {
        return textResult(m, contentOf(m, path));
      }

      if (name == "polyirFsWrite") {
        return boolResult(written(m, path, WordAt(args, 1)));
      }

      return pathCall(name, path);
    }

    // ── the machine's facts ──

    String envOf(const String& name) {
      const char* v = getenv(name.c_str());

      return v == nullptr ? "" : v;
    }

    String osName() {
#if defined(__APPLE__)
      return "macos";
#elif defined(__linux__)
      return "linux";
#elif defined(_WIN32)
      return "windows";
#else
      return "unknown";
#endif
    }

    String cpuName() {
#if defined(__aarch64__) || defined(__arm64__)
      return "arm64";
#elif defined(__x86_64__) || defined(_M_X64)
      return "x64";
#else
      return "unknown";
#endif
    }

    String workingDir() {
      ErrorCode ec;

      return current_path(ec).string();
    }

    // The program's arguments, as the oracle handed them out: POLYIR_ARGS (the words joined by newlines, set before the run) split
    // again. So a word with a newline inside comes out as two, and there are none when it is empty.
    Vector<String> programArgs() {
      Vector<String> out;
      const String words = envOf("POLYIR_ARGS");

      if (words.empty()) {
        return out;
      }

      IStringStream listed{words};

      for (String word; getline(listed, word);) {
        out.push_back(word);
      }

      if (words.back() == '\n') {
        out.push_back("");
      }

      return out;
    }

    Outcome machineFact(Machine& m, const String& name, const Vector<Value>& args) {
      if (name == "polyirEnv") {
        return textResult(m, envOf(WordAt(args, 0)));
      }

      if (name == "polyirOs") {
        return textResult(m, osName());
      }

      if (name == "polyirCpu") {
        return textResult(m, cpuName());
      }

      if (name == "polyirCwd") {
        return textResult(m, workingDir());
      }

      if (name == "polyirArgs") {
        return ValueOutcome(StringList(m.heap, programArgs()));
      }

      return Trap("'" + name + "' is not part of the host layer");
    }

    Outcome hostCall(Machine& m, const String& name, const Vector<Value>& args) {
      if (name.rfind("polyirFs", 0) == 0) {
        return files(m, name, args);
      }

      if (name == "polyirRun" || name == "polyirShow") {
        return command(m, name, args);
      }

      if (name == "polyirFetch") {
        return ranResult(m, run({"curl", "-fsSL", WordAt(args, 0)}));
      }

      return machineFact(m, name, args);
    }

    // ── the C functions the VM binds itself ──

    Outcome putcharCall(Machine& m, const Vector<Value>& args) {
      const int64_t code = IntOf(ArgumentAt(args, 0));
      *m.out << static_cast<char>(code);

      return ValueOutcome(IntValue(code, 32, true));
    }

    Outcome putsCall(Machine& m, const Vector<Value>& args) {
      *m.out << WordAt(args, 0) << "\n";

      return ValueOutcome(IntValue(0, 32, true));
    }

    // libm floats computed at double precision (floats carry a double here), which can differ from true f32 math in the last bits.
    Outcome mathCall(const String& name, const Vector<Value>& args) {
      const double x = FloatOf(ArgumentAt(args, 0));

      if (name == "powf") {
        return ValueOutcome(FloatValue(pow(x, FloatOf(ArgumentAt(args, 1))), 32));
      }

      if (name == "sinf") {
        return ValueOutcome(FloatValue(sin(x), 32));
      }

      if (name == "floorf") {
        return ValueOutcome(FloatValue(floor(x), 32));
      }

      return ValueOutcome(FloatValue(sqrt(x), 32));
    }

    Outcome nativeCall(Machine& m, const String& name, const Vector<Value>& args) {
      if (name == "putchar") {
        return putcharCall(m, args);
      }

      if (name == "puts") {
        return putsCall(m, args);
      }

      return mathCall(name, args);
    }

    // ── libraries ──

    // A path is loaded as it is. A name (`z`) is loaded as lib<name>.dylib or lib<name>.so in POLYIR_LIBRARY_PATH's folders, then
    // where the system looks (""), then where Homebrew installs.
    Vector<String> librarySearchFolders() {
      Vector<String> out;
      IStringStream listed{envOf("POLYIR_LIBRARY_PATH")};

      for (String dir; getline(listed, dir, ':');) {
        if (!dir.empty()) {
          out.push_back(dir);
        }
      }

      for (const char* fixed : {"", "/opt/homebrew/lib", "/usr/local/lib"}) {
        out.push_back(fixed);
      }

      return out;
    }

    String inFolder(const String& dir, const String& file) {
      return dir.empty() ? file : dir + "/" + file;
    }

    Vector<String> libraryFileCandidates(const String& name) {
      if (name.find('/') != String::npos) {
        return {name};
      }

      Vector<String> out;

      for (const String& dir : librarySearchFolders()) {
        for (const String& file : {"lib" + name + ".dylib", "lib" + name + ".so"}) {
          out.push_back(inFolder(dir, file));
        }
      }

      return out;
    }

    // RTLD_GLOBAL, so one lookup in the whole process finds a symbol in any loaded library (or in the VM itself, which links libc).
    void* openLibrary(const String& file) {
      return dlopen(file.c_str(), RTLD_NOW | RTLD_GLOBAL);
    }

    bool loadLibrary(Machine& m, const String& name) {
      for (const String& file : libraryFileCandidates(name)) {
        if (void* handle = openLibrary(file)) {
          m.libraries.push_back(Library{name, handle});
          return true;
        }
      }

      return false;
    }

    String notFound(const String& name) {
      return "import native \"" + name + "\": the library could not be loaded (put its folder on POLYIR_LIBRARY_PATH)";
    }

    // One lookup over the whole process, as the oracle's foreign calls did. The process's own images (libc among them) come before
    // the libraries loaded for `import native`. So a library's `abs` does not shadow libc's. The handles are kept only to keep them
    // loaded.
    void* symbolAddress(const String& symbol) {
      return dlsym(RTLD_DEFAULT, symbol.c_str());
    }

    // ── the arguments of a foreign call, as C takes them ──

    constexpr size_t INTEGER_REGISTERS = 6;  // arm64 and x86-64 System V
    constexpr size_t FLOAT_REGISTERS = 8;

    struct ForeignArgs {
      Vector<int64_t> ints;
      Vector<double> floats;
      Vector<char*> copies;  // the C strings to free after the call
      String problem;  // what makes the call impossible ("" when nothing does); the first problem is kept
    };

    void refuse(ForeignArgs& passing, const String& why) {
      if (passing.problem.empty()) {
        passing.problem = why;
      }
    }

    // A string goes as a C copy, freed after the call; a pointer held as an integer as its address.
    void addPointer(ForeignArgs& passing, const Value& v) {
      if (v.kind != ValueKind::Str) {
        passing.ints.push_back(IntOf(v));
        return;
      }

      char* copy = strdup(TextOf(v).c_str());
      passing.copies.push_back(copy);
      passing.ints.push_back(static_cast<int64_t>(reinterpret_cast<intptr_t>(copy)));
    }

    // An f32 argument travels in the low 32 bits of a floating-point register (s0… on arm64, the low lane of xmm0… on x86-64). The call
    // hands every float over as a double. So the double for an f32 carries the float's own bits there, and the callee reads exactly
    // them.
    double singleInRegister(double x) {
      return bit_cast<double>(static_cast<uint64_t>(bit_cast<uint32_t>(static_cast<float>(x))));
    }

    void addFloat(ForeignArgs& passing, int bits, const Value& v) {
      const double x = FloatOf(v);
      passing.floats.push_back(bits == 32 ? singleInRegister(x) : x);
    }

    void addArg(ForeignArgs& passing, const TypePtr& type, const Value& v) {
      const TypeKind kind = type ? type->kind : TypeKind::Void;

      switch (kind) {
        case TypeKind::Int: case TypeKind::UInt: {
          passing.ints.push_back(IntOf(v));
          return;
        }
        case TypeKind::Float:
          return addFloat(passing, type->bits, v);
        case TypeKind::String: case TypeKind::Pointer:
          return addPointer(passing, v);
        default:
          return refuse(passing, "a C function takes whole numbers, floats, strings and pointers");
      }
    }

    // Up to 6 integer and 8 floating-point arguments: the registers of the arm64 and x86-64 (System V) C calling conventions.
    void refuseTooManyArguments(ForeignArgs& passing) {
      if (passing.ints.size() > INTEGER_REGISTERS) {
        refuse(passing, "more than 6 integer arguments");
      }

      if (passing.floats.size() > FLOAT_REGISTERS) {
        refuse(passing, "more than 8 float arguments");
      }
    }

    ForeignArgs foreignArgs(const ExternInfo& declared, const Vector<Value>& args) {
      ForeignArgs passing;
      const size_t count = min(declared.parameters.size(), args.size());

      for (size_t i = 0; i < count; i++) {
        addArg(passing, declared.parameters[i], args[i]);
      }

      refuseTooManyArguments(passing);

      return passing;
    }

    void freeCopies(ForeignArgs& passing) {
      for (char* copy : passing.copies) {
        free(copy);
      }

      passing.copies.clear();
    }

    // ── the call ──
    // The function is called as one taking 6 integers and 8 doubles: on arm64 and x86-64 (System V) the integer and the floating-point
    // arguments take their own registers in order, so a function of fewer arguments, of any mix, reads exactly the ones it declares. An f32
    // reads the low half of its register (singleInRegister); an f32 result comes back in the same place and is read with a float signature.

    // The three shapes of C function the VM calls through: the same 14 arguments, returning an integer, a double or a float.
    using IntFunction = int64_t (*)(int64_t, int64_t, int64_t, int64_t, int64_t, int64_t, double, double, double, double, double, double, double, double);
    using DoubleFunction = double (*)(int64_t, int64_t, int64_t, int64_t, int64_t, int64_t, double, double, double, double, double, double, double, double);
    using FloatFunction = float (*)(int64_t, int64_t, int64_t, int64_t, int64_t, int64_t, double, double, double, double, double, double, double, double);

    struct Registers {
      int64_t integers[INTEGER_REGISTERS] = {};
      double floats[FLOAT_REGISTERS] = {};
    };

    Registers registersOf(const ForeignArgs& passing) {
      Registers registers;
      copy_n(passing.ints.begin(), min(passing.ints.size(), INTEGER_REGISTERS), registers.integers);
      copy_n(passing.floats.begin(), min(passing.floats.size(), FLOAT_REGISTERS), registers.floats);

      return registers;
    }

    // Calls the C function at `address` with every register filled. A template over the function's type (IntFunction,
    // DoubleFunction or FloatFunction above), so it returns what that type returns.
    template <typename Function>
    auto callWith(void* address, const Registers& registers) {
      const Function function = reinterpret_cast<Function>(address);
      const int64_t* ints = registers.integers;
      const double* floats = registers.floats;

      return function(ints[0], ints[1], ints[2], ints[3], ints[4], ints[5],
                      floats[0], floats[1], floats[2], floats[3], floats[4], floats[5], floats[6], floats[7]);
    }

    String textAt(int64_t address) {
      const char* p = reinterpret_cast<const char*>(static_cast<intptr_t>(address));

      return p == nullptr ? String() : String(p);
    }

    // A whole number cut to its declared width (C leaves the bits above it unset), a bool (bit 0), or a pointer's address.
    Outcome wholeResult(const TypePtr& resultType, int64_t returned) {
      const int bits = resultType->bits;

      if (resultType->kind == TypeKind::Int && bits == 1) {
        return ValueOutcome(BoolValue((returned & 1) == 1));
      }

      if (resultType->kind == TypeKind::Int) {
        return ValueOutcome(IntValue(returned, bits, true));
      }

      if (resultType->kind == TypeKind::UInt) {
        return ValueOutcome(IntValue(returned, bits, false));
      }

      return ValueOutcome(IntValue(returned, 64, false));
    }

    Outcome floatResult(void* address, int bits, const Registers& registers) {
      if (bits == 32) {
        return ValueOutcome(FloatValue(static_cast<double>(callWith<FloatFunction>(address, registers)), 32));
      }

      return ValueOutcome(FloatValue(callWith<DoubleFunction>(address, registers), 64));
    }

    // `stamp` is the identity a string result carries; the oracle took it before every foreign call, whatever the call returns.
    Outcome resultOf(void* address, const TypePtr& resultType, const Registers& registers, int64_t stamp) {
      const TypeKind kind = resultType ? resultType->kind : TypeKind::Void;

      if (kind == TypeKind::Float) {
        return floatResult(address, resultType->bits, registers);
      }

      const int64_t returned = callWith<IntFunction>(address, registers);

      if (kind == TypeKind::Void) {
        return VoidOutcome();
      }

      if (kind == TypeKind::String) {
        return ValueOutcome(StrStamped(textAt(returned), stamp));
      }

      return wholeResult(resultType, returned);
    }

    // The call itself, once the arguments are known to be passable; a symbol that vanished since `HasForeign` traps instead of crashing.
    Outcome called(Machine& m, const String& symbol, const TypePtr& resultType, const ForeignArgs& passing) {
      void* address = symbolAddress(symbol);

      if (address == nullptr) {
        return Trap(symbol + ": the symbol is not in any loaded library");
      }

      return resultOf(address, resultType, registersOf(passing), NewStamp(m.heap));
    }

  }  // namespace

  // A bound name shadows any same-named C symbol dlsym could find (Run.cpp asks here first). So a Nina program's `nina_*` calls never
  // fall through to a C runtime that happens to be linked into the process.
  bool BindsExtern(const String& name) {
    return BindsNinaRuntime(name) || BindsProcess(name) || BindsLifecycle(name) || bindsHost(name) || bindsNative(name);
  }

  Outcome CallBoundExtern(Machine& m, const String& name, const Vector<Value>& args) {
    if (BindsNinaRuntime(name)) {
      return CallNinaRuntime(m, name, args);
    }

    if (BindsProcess(name)) {
      return CallProcess(m, name, args);
    }

    if (BindsLifecycle(name)) {
      return CallLifecycle(m, args);
    }

    if (bindsHost(name)) {
      return hostCall(m, name, args);
    }

    if (bindsNative(name)) {
      return nativeCall(m, name, args);
    }

    return UnboundExternTrap(name);
  }

  Outcome UnboundExternTrap(const String& name) {
    return Trap("'" + name + "' has no binding on the VM, and no library the program imports (import native \"…\") has it");
  }

  String LoadLibraries(Machine& m, const Vector<String>& names) {
    for (const String& name : names) {
      if (!loadLibrary(m, name)) {
        return notFound(name);
      }
    }

    return "";
  }

  bool HasForeign(const Machine&, const String& symbol) {
    return symbolAddress(symbol) != nullptr;
  }

  Outcome CallForeign(Machine& m, const ExternInfo& declared, const String& symbol, const Vector<Value>& args) {
    ForeignArgs passing = foreignArgs(declared, args);
    const Outcome out = passing.problem.empty() ? called(m, symbol, declared.result, passing) : Trap(symbol + ": " + passing.problem);
    freeCopies(passing);

    return out;
  }

}  // namespace polyir::vm
