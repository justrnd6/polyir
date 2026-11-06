// How fast polyir's programs are translated (core/docs/translate.md): every program of the corpus checked and written by every
// built-in page, and the largest one (core/tests/programs/ContainerTests.poly) written to JavaScript many times. A program to
// run by hand, not a test: `build/TranslateBench` from the repository's root.
//
//   the goal: the corpus in 0.5 s or less (its checks too), the largest program in 2 ms or less
#include "polyir/Frontend.hpp"
#include "polyir/Translate.hpp"

using namespace polyir;
using namespace polyir::front;
using std::chrono::duration;
using std::chrono::steady_clock;
using std::filesystem::is_directory;
using std::filesystem::recursive_directory_iterator;

namespace {

  double secondsSince(steady_clock::time_point start) {
    return duration<double>(steady_clock::now() - start).count();
  }

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

    return paths;
  }

  Vector<String> corpus() {
    Vector<String> paths = programsIn("core/examples");

    for (const char* folder : {"core/stdlib", "core/tests/programs", "ideas"}) {
      const Vector<String> more = programsIn(folder);
      paths.insert(paths.end(), more.begin(), more.end());
    }

    return paths;
  }

  // Every program checked once, and written by each built-in page.
  void wholeCorpus(const Vector<Target>& targets) {
    const steady_clock::time_point start = steady_clock::now();
    double checking = 0;
    size_t written = 0;

    for (const String& path : corpus()) {
      const steady_clock::time_point checkStart = steady_clock::now();
      const FrontResult checked = CheckFile(path);
      checking += secondsSince(checkStart);

      for (const Target& target : Ok(checked) ? targets : Vector<Target>{}) {
        written += Translate(checked.module, target).text.size();
      }
    }

    printf("the corpus: %.3f s (%.3f s checking), %zu bytes written\n", secondsSince(start), checking, written);
  }

  void largestProgram(const Target& js) {
    const FrontResult checked = CheckFile("core/tests/programs/ContainerTests.poly");
    const int times = 50;
    const steady_clock::time_point start = steady_clock::now();

    for (int i = 0; i < times; i++) {
      Translate(checked.module, js);
    }

    printf("ContainerTests.poly to js: %.2f ms each\n", secondsSince(start) * 1000 / times);
    const steady_clock::time_point treeStart = steady_clock::now();

    for (int i = 0; i < times; i++) {
      TreeOf(checked.module, js);
    }

    printf("  of which its tree: %.2f ms\n", secondsSince(treeStart) * 1000 / times);
  }

}  // namespace

int main() {
  const Vector<Target> targets = BuiltInTargets();

  wholeCorpus(targets);
  largestProgram(*FindTarget("js", ""));

  return 0;
}
