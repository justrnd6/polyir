// The native half of the polyir command line: LLVM IR text to an executable through the provisioned C compiler, and running the result.
#include "polyir/Cli.hpp"

#include <sys/wait.h>

namespace polyir {

  namespace {

    // `text` as one shell word: inside single quotes, where only a single quote itself needs care. A quote becomes '\'', which ends
    // the quoted part, adds an escaped quote, and starts a new quoted part.
    String shellQuote(const String& text) {
      String quoted = "'";

      for (char c : text) {
        quoted += c == '\'' ? String("'\\''") : String(1, c);
      }

      return quoted + "'";
    }

    // The C compiler: NINA_CC when the nina toolchain provisioned one, else clang from PATH.
    String compilerCommand() {
      const char* provisioned = getenv("NINA_CC");

      return provisioned != nullptr && *provisioned != '\0' ? String(provisioned) : String("clang");
    }

    // Whether the IR calls (or declares) `function`.
    bool mentions(const String& ir, const char* function) {
      return ir.find(String("@") + function + "(") != String::npos;
    }

    // What the GPU and audio host runtimes a program declares need at link time.
    String hostLibraries(const String& ir) {
      String flags;

      if (mentions(ir, "MTLCreateSystemDefaultDevice")) {
        flags += " -framework Metal -framework Foundation -lobjc";
      }

      if (mentions(ir, "AudioQueueNewOutput")) {
        flags += " -framework AudioToolbox -framework CoreFoundation";
      }

      if (mentions(ir, "snd_pcm_open")) {
        flags += " -lasound";
      }

      return flags;
    }

    // The folders a native build looks for `import native` libraries in besides the linker's own: POLYIR_LIBRARY_PATH's folders,
    // then Homebrew's. This is the same order the VM searches (librarySearchFolders in runtime/src/vm/Host.cpp).
    Vector<String> libraryFolders() {
      Vector<String> out;
      const char* listed = getenv("POLYIR_LIBRARY_PATH");
      StringStream in(listed != nullptr ? String(listed) : String());

      for (String folder; getline(in, folder, ':');) {
        if (!folder.empty()) {
          out.push_back(folder);
        }
      }

      for (const char* folder : {"/opt/homebrew/lib", "/usr/local/lib"}) {
        out.push_back(folder);
      }

      return out;
    }

    // `-L` for each library folder that exists.
    String folderFlags() {
      String flags;
      ErrorCode ignored;

      for (const String& folder : libraryFolders()) {
        if (is_directory(Path(folder), ignored)) {
          flags += " -L" + shellQuote(folder);
        }
      }

      return flags;
    }

    String libraryFlags(const Vector<String>& libraries) {
      if (libraries.empty()) {
        return String();
      }

      String flags = folderFlags();

      // A library named by its path is linked as that file; a bare name is `-lNAME`.
      for (const String& library : libraries) {
        flags += " " + shellQuote(library.find('/') != String::npos ? library : "-l" + library);
      }

      return flags;
    }

    // -Wno-override-module: the IR names its own target triple, so clang need not warn that it replaces its own.
    // 2>&1: the compiler's errors go to stdout, the only stream captureOutput reads.
    String compileCommand(const String& ir, const String& executablePath, bool release, const Vector<String>& libraries) {
      const String optimization = release ? " -O2" : " -O0 -g";

      return shellQuote(compilerCommand()) + " -Wno-override-module " + shellQuote(executablePath + ".ll") + optimization + hostLibraries(ir) +
        libraryFlags(libraries) + " -o " + shellQuote(executablePath) + " 2>&1";
    }

    bool writeText(const String& path, const String& text) {
      OFStream file(path);
      file << text;

      return static_cast<bool>(file);
    }

    // Runs `command` through the shell and returns what it printed; `status` is its exit status (-1 when it could not start).
    String captureOutput(const String& command, int& status) {
      String output;
      char buffer[512];
      FILE* pipe = popen(command.c_str(), "r");

      while (pipe != nullptr && fgets(buffer, sizeof buffer, pipe) != nullptr) {
        output += buffer;
      }

      status = pipe == nullptr ? -1 : pclose(pipe);

      return output;
    }

  }  // namespace

  bool BuildExecutable(const String& ir, const String& executablePath, bool release, String& diagnostics) {
    return BuildExecutable(ir, executablePath, release, {}, diagnostics);
  }

  bool BuildExecutable(const String& ir, const String& executablePath, bool release, const Vector<String>& libraries, String& diagnostics) {
    if (!writeText(executablePath + ".ll", ir)) {
      diagnostics = "cannot write " + executablePath + ".ll";
      return false;
    }

    int status = 0;
    diagnostics = captureOutput(compileCommand(ir, executablePath, release, libraries), status);

    return status == 0;
  }

  int RunExecutable(const String& executablePath, const Vector<String>& args) {
    String command = shellQuote(executablePath);

    for (const String& arg : args) {
      command += " " + shellQuote(arg);
    }

    const int status = system(command.c_str());

    return WIFEXITED(status) ? WEXITSTATUS(status) : 255;  // 255 when the program was killed by a signal
  }

}  // namespace polyir
