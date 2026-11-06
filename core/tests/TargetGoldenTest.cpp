// Each built-in target's translation of every program in the corpus, against its golden file beside the target:
// targets/<lang>/tests/<the program's path>.<ext> holds the text, or <the program's path>.problems what stops it. Run it with
// `--update` to write the goldens again; a changed golden is reviewed by reading it.
#include "polyir/Frontend.hpp"
#include "polyir/Translate.hpp"

using namespace polyir;
using namespace polyir::front;
using std::filesystem::create_directories;
using std::filesystem::exists;
using std::filesystem::is_directory;
using std::filesystem::recursive_directory_iterator;
using std::filesystem::remove;

namespace {

  int failures = 0;
  int compared = 0;

  void expect(bool ok, const String& what) {
    if (ok) {
      return;
    }

    cerr << "FAIL " << what << "\n";
    failures++;
  }

  // One program's golden for one target: where it is, what it should hold, and where the other kind of golden would be.
  struct Golden {
    String path;
    String text;
    String otherPath;
  };

  String readFile(const String& path) {
    IFStream in(path);
    StringStream buffer;
    buffer << in.rdbuf();

    return buffer.str();
  }

  void writeFile(const String& path, const String& text) {
    create_directories(Path(path).parent_path());
    OFStream out(path);
    out << text;
  }

  // The .poly files under `folder` of the repository, as paths from its root, in order.
  Vector<String> programsIn(const String& root, const String& folder) {
    Vector<String> paths;

    if (!is_directory(root + "/" + folder)) {
      return paths;
    }

    for (const auto& entry : recursive_directory_iterator(root + "/" + folder)) {
      if (entry.path().extension() == ".poly") {
        paths.push_back(entry.path().lexically_relative(root).generic_string());
      }
    }

    sort(paths.begin(), paths.end());

    return paths;
  }

  String problemsText(const Vector<String>& problems) {
    String text;

    for (const String& problem : problems) {
      text += problem + "\n";
    }

    return text;
  }

  // The golden of `program` for `target`: the text it translates to, or the problems that stop it. A file with no `main` is a
  // library, which C++ writes as a `.hpp`.
  Golden goldenOf(const Target& target, const String& root, const String& program, const Translation& out) {
    const String stem = root + "/targets/" + target.name + "/tests/" + program.substr(0, program.size() - 5);
    const String textPath = stem + "." + out.extension;
    const String problemsPath = stem + ".problems";

    if (out.problems.empty()) {
      return Golden{textPath, out.text, problemsPath};
    }

    return Golden{problemsPath, problemsText(out.problems), textPath};
  }

  // The first line where `want` and `got` differ, with both of them.
  String firstDifference(const String& want, const String& got) {
    IStringStream wanted(want);
    IStringStream gotten(got);
    String a;
    String b;

    for (int line = 1;; line++) {
      const bool moreWanted = static_cast<bool>(getline(wanted, a));
      const bool moreGotten = static_cast<bool>(getline(gotten, b));

      if (!moreWanted && !moreGotten) {
        return "no line";
      }

      if (!moreWanted || !moreGotten || a != b) {
        return "line " + to_string(line) + ":\n  want: " + (moreWanted ? a : "(the end)") + "\n  got:  " + (moreGotten ? b : "(the end)");
      }
    }
  }

  void update(const Golden& golden) {
    writeFile(golden.path, golden.text);
    remove(golden.otherPath);
  }

  void check(const Golden& golden) {
    const bool there = exists(golden.path);
    const String want = there ? readFile(golden.path) : "";

    expect(there, golden.path + " is missing: run TargetGoldenTest --update, then read it");
    expect(!there || want == golden.text, golden.path + " differs at " + firstDifference(want, golden.text));
    expect(!exists(golden.otherPath), golden.otherPath + " is left from before: run TargetGoldenTest --update");
    compared++;
  }

  // Every built-in target's translation of the program at `program`, a path from `root`.
  void translateEverywhere(const String& root, const String& program, bool updating) {
    const FrontResult checked = CheckFile(root + "/" + program);

    if (!Ok(checked)) {
      return;
    }

    for (const Target& target : BuiltInTargets()) {
      const Golden golden = goldenOf(target, root, program, Translate(checked.module, target));

      if (updating) {
        update(golden);
      } else {
        check(golden);
      }
    }
  }

}  // namespace

int main(int argc, char** argv) {
  const bool updating = argc > 1 && String(argv[1]) == "--update";
  const String root = String(POLYIR_SOURCE_DIR) + "/..";
  Vector<String> programs = programsIn(root, "core/examples");

  for (const String& folder : {"core/stdlib", "core/tests/programs", "ideas"}) {
    const Vector<String> more = programsIn(root, folder);
    programs.insert(programs.end(), more.begin(), more.end());
  }

  for (const String& program : programs) {
    translateEverywhere(root, program, updating);
  }

  expect(updating || compared > 200, "the corpus was found and each target translated it, got " + to_string(compared) + " goldens");

  if (failures == 0) {
    cout << (updating ? "TargetGoldenTest wrote the goldens\n" : "TargetGoldenTest ok: " + to_string(compared) + " goldens\n");
  }

  return failures == 0 ? 0 : 1;
}
