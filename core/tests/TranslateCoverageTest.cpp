// Whether every construct of .poly can reach a target (docs/translate.md). Every built-in target's page must have a rule for every
// construct. And with a page whose every rule is written, every program of the corpus must translate with no problem: then
// nothing the front end makes is left without a construct.
#include "polyir/Frontend.hpp"
#include "polyir/Translate.hpp"

using namespace polyir;
using namespace polyir::front;
using std::filesystem::is_directory;
using std::filesystem::recursive_directory_iterator;

namespace {

  int failures = 0;

  void expect(bool ok, const String& what) {
    if (ok) {
      return;
    }

    cerr << "FAIL " << what << "\n";
    failures++;
  }

  String trimmed(const String& text) {
    const size_t from = text.find_first_not_of(' ');
    const size_t to = text.find_last_not_of(' ');

    return from == String::npos ? "" : text.substr(from, to - from + 1);
  }

  // ── every built-in target lists every construct ──

  void builtInTargetsCoverEverything() {
    for (const Target& target : BuiltInTargets()) {
      const Vector<String> missing = MissingConstructs(target);
      const Vector<String> problems = TargetProblems(target);

      expect(missing.empty(), target.name + " has no rule for " + (missing.empty() ? "" : missing[0]));
      expect(problems.empty(), target.name + ": " + (problems.empty() ? "" : problems[0]));
    }

    expect(!BuiltInTargets().empty(), "the library has built-in targets");
  }

  // ── every program reaches only constructs a page can write ──

  // `if {cond} {then}` for the head `if(cond, then)`: the construct's name and each of its parts.
  String echoTemplate(const String& head) {
    const size_t open = head.find('(');
    String text = head.substr(0, open);

    if (open == String::npos) {
      return text;
    }

    IStringStream parts(head.substr(open + 1, head.size() - open - 2));

    for (String part; getline(parts, part, ',');) {
      text += " {" + trimmed(part) + "}";
    }

    return text;
  }

  // A new page with every `todo` written as its echo, so every construct has a rule that writes all of its parts.
  String echoPage() {
    IStringStream lines(NewTargetText("Echo", PolyirSchema()));
    String page;

    for (String line; getline(lines, line);) {
      const size_t mark = line.find(":= todo");
      const String head = trimmed(line.substr(0, mark));
      page += (mark == String::npos ? line : head + " := " + echoTemplate(head)) + "\n";
    }

    return page;
  }

  // The .poly files under `folder`, in order.
  Vector<String> programsIn(const String& folder) {
    Vector<String> paths;

    if (!is_directory(folder)) {
      return paths;
    }

    for (const auto& entry : recursive_directory_iterator(folder)) {
      if (entry.path().extension() == ".poly") {
        paths.push_back(entry.path().string());
      }
    }

    sort(paths.begin(), paths.end());

    return paths;
  }

  Vector<String> corpus() {
    const String core = POLYIR_SOURCE_DIR;
    Vector<String> paths = programsIn(core + "/examples");
    const Vector<String> tests = programsIn(core + "/tests/programs");
    const Vector<String> ideas = programsIn(core + "/../ideas");

    paths.insert(paths.end(), tests.begin(), tests.end());
    paths.insert(paths.end(), ideas.begin(), ideas.end());

    return paths;
  }

  void everyProgramTranslates() {
    Target echo{"echo", "Echo.translate", echoPage()};
    echo.findSchema = FindSchema;
    int translated = 0;

    expect(MissingConstructs(echo).empty() && TargetProblems(echo).empty(), "the echo page covers every construct");

    for (const String& path : corpus()) {
      const FrontResult checked = CheckFile(path);

      if (!Ok(checked)) {
        continue;
      }

      const Translation out = Translate(checked.module, echo);
      expect(out.problems.empty(), path + ": " + (out.problems.empty() ? "" : out.problems[0]));
      translated++;
    }

    expect(translated > 100, "the corpus was found, got " + to_string(translated) + " programs");
    cout << "translated " << translated << " programs with every rule written\n";
  }

}  // namespace

int main() {
  builtInTargetsCoverEverything();
  everyProgramTranslates();

  if (failures == 0) {
    cout << "TranslateCoverageTest ok\n";
  }

  return failures == 0 ? 0 : 1;
}
