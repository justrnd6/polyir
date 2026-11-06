// `poly`: polyir bytecode, or meta-build, run on the VM with no toolchain.
//
//   poly FILE.pbc [--entry NAME] [--args "WORDS"]   runs a .pbc file's entry (main) and prints the value it gives
//   poly up | gen | build | run | test | tasks | clean ...
//
// The second form is meta-build (tools/metabuild), built into this command (MetaBuildText.cpp). In a project's folder (one
// with a Polyfile) any word that names no file is a goal too, one the project's build.poly adds. The words after `poly` are
// meta-build's `polyirArgs()`, and the goal's exit status is this command's.
#include "polyir/Std.hpp"
#include "polyir/Frontend.hpp"
#include "polyir/Vm.hpp"
#include "MetaBuildText.hpp"

using namespace polyir;
using namespace polyir::front;
using namespace polyir::poly;
using std::filesystem::exists;
using std::filesystem::is_regular_file;

namespace {

  // What `poly FILE.pbc` was told: the file, the entry to call and the words given to it.
  struct PbcRun {
    String file;
    String entry = "main";
    Vector<String> args;
  };

  // `"12 3"` is ["12", "3"]: the words, with no empty ones.
  Vector<String> wordsOf(const String& text) {
    IStringStream in(text);
    Vector<String> words;

    for (String word; in >> word;) {
      words.push_back(word);
    }

    return words;
  }

  // The VM's options for running `entry` with `args`; the rest are its defaults.
  VmOptions optionsFor(const String& entry, const Vector<String>& args) {
    VmOptions options;
    options.entry = entry;
    options.args = args;

    return options;
  }

  int failed(const String& headline, const Vector<String>& problems, int status) {
    cout << headline << ":\n";

    for (const String& problem : problems) {
      cout << "  " << problem << "\n";
    }

    return status;
  }

  // ── meta-build ──

  // One of meta-build's goals, or, in a project's folder, any word that is not a file (a goal its build.poly adds).
  bool isGoal(const String& word) {
    const Vector<String> goals = {"up", "gen", "build", "run", "test", "tasks", "clean"};
    const bool known = find(goals.begin(), goals.end(), word) != goals.end();

    return known || (is_regular_file("Polyfile") && !exists(word));
  }

  String fileText(const String& path) {
    IFStream in(path);
    OStringStream text;
    text << in.rdbuf();

    return text.str();
  }

  // meta-build with the project's build.poly in place of its stand-in. The build.poly comes first, so a problem in it names its
  // own line.
  String metaBuildSource() {
    const String extension = is_regular_file("build.poly") ? fileText("build.poly") : String(MetaBuildNoExtension());

    return extension + "\n" + MetaBuildProgram();
  }

  int runMetaBuild(const Vector<String>& args) {
    const FrontResult checked = CheckSource(metaBuildSource());

    if (!Ok(checked)) {
      return failed("meta-build failed", checked.errors, 1);
    }

    const VmOutcome ran = RunModule(checked.module, optionsFor("main", args));

    if (!ran.ok) {
      return failed("meta-build failed", {ran.declined ? ran.declineReason : ran.trap}, 1);
    }

    return static_cast<int>(ran.exitStatus);
  }

  // ── a .pbc file ──

  // `--entry NAME` or `--args "WORDS"`.
  void takeOption(PbcRun& run, const String& flag, const String& value) {
    if (flag == "--entry") {
      run.entry = value;
    } else {
      run.args = wordsOf(value);
    }
  }

  PbcRun pbcRunOf(const Vector<String>& args) {
    PbcRun run;

    for (size_t i = 0; i < args.size(); i++) {
      const bool valued = i + 1 < args.size() && (args[i] == "--entry" || args[i] == "--args");

      if (valued) {
        takeOption(run, args[i], args[i + 1]);
        i++;
      } else {
        run.file = args[i];
      }
    }

    return run;
  }

  int runPbc(const PbcRun& run) {
    if (run.file.empty() || !exists(run.file)) {
      return failed("no such file", {run.file}, 1);
    }

    const VmOutcome ran = RunPbcFile(run.file, optionsFor(run.entry, run.args));

    if (ran.declined || !ran.ok) {
      return failed("run failed", {ran.declined ? ran.declineReason : ran.trap}, ran.declined ? 125 : 1);
    }

    cout << ran.result << "\n";

    return 0;
  }

}  // namespace

int main(int argc, char** argv) {
  const Vector<String> args(argv + 1, argv + argc);

  if (!args.empty() && isGoal(args[0])) {
    return runMetaBuild(args);
  }

  return runPbc(pbcRunOf(args));
}
