// Every example program, translated to .poly by the built-in poly page (targets/poly/Poly.translate) and checked again, is
// the same program: the two HIR texts have the same lines, once the hidden names that carry a line and column (`forList_12_4`)
// are numbered away. The translation is checked as a file of its own, its imports read from the program's folder. A program the
// page cannot spell yet is counted as such, not failed; so is one with host types, bridges or host members, which polyir's phi
// rules have no construct for. A translation that does not check again, or checks into another program, is a failure. It is
// counted apart when the program imports another file (a file imports only what its own code uses), or is a known gap.
#include "polyir/Std.hpp"
#include "polyir/Frontend.hpp"
#include "polyir/HirPrint.hpp"
#include "polyir/Translate.hpp"

using namespace polyir;
using namespace polyir::front;
using std::filesystem::remove;
using std::filesystem::temp_directory_path;

namespace {

  int failures = 0;

  void expect(bool ok, const String& what) {
    if (ok) {
      return;
    }

    cerr << "FAIL " << what << "\n";
    failures++;
  }

  // The lines of a file, without the empty ones (the build lists the example programs in front_examples.txt).
  Vector<String> linesOf(const String& path) {
    IFStream in(path);
    Vector<String> lines;

    for (String line; getline(in, line);) {
      if (!line.empty()) {
        lines.push_back(line);
      }
    }

    return lines;
  }

  bool isDigit(char c) {
    return c >= '0' && c <= '9';
  }

  // How many characters of `_12_4` start at `at` in `text` (0 when there is no such position there).
  size_t positionAt(const String& text, size_t at) {
    size_t end = at;

    for (int part = 0; part < 2 && end < text.size() && text[end] == '_' && end + 1 < text.size() && isDigit(text[end + 1]); part++) {
      end++;

      while (end < text.size() && isDigit(text[end])) {
        end++;
      }
    }

    return end - at;
  }

  // `forList_12_4` is `forList`: a hidden name keeps its stem and loses the position it was made at.
  String withoutPositions(String line) {
    for (const char* stem : {"forList", "forIndex", "forTable", "forSlot", "forGen", "forStep", "ifValue"}) {
      const String word = stem;

      for (size_t at = line.find(word); at != String::npos; at = line.find(word, at + word.size())) {
        line.erase(at + word.size(), positionAt(line, at + word.size()));
      }
    }

    return line;
  }

  // The HIR text as sorted lines with the positions taken out, so two checks of one program compare equal.
  Vector<String> comparable(const Module& module) {
    IStringStream text(ModuleText(module));
    Vector<String> lines;

    for (String line; getline(text, line);) {
      lines.push_back(withoutPositions(line));
    }

    sort(lines.begin(), lines.end());

    return lines;
  }

  // Whether the program has a declaration another file wrote: it imports that file.
  bool importsAFile(const Module& module) {
    for (const FuncDef& def : module.functions) {
      if (!def.origin.empty()) {
        return true;
      }
    }

    for (const StructDef& def : module.structs) {
      if (!def.origin.empty()) {
        return true;
      }
    }

    return false;
  }

  // Whether the program declares what polyir's canon has no form for: host types, bridges, host members.
  bool hasHostParts(const Module& module) {
    return !module.hostTypes.empty() || !module.bridges.empty() || !module.hostMembers.empty();
  }

  // The programs that are known not to come back the same, and why: a gap in polyir's source, not in the page.
  struct KnownGap {
    const char* file;
    const char* why;
  };

  const KnownGap kKnownGaps[] = {
    {"core/examples/buffer.poly", "polyir's source writes a var it never assigns as a let, and a var buffer is not a let one"}};

  const char* knownGap(const String& path) {
    for (const KnownGap& gap : kKnownGaps) {
      if (path.size() >= strlen(gap.file) && path.compare(path.size() - strlen(gap.file), String::npos, gap.file) == 0) {
        return gap.why;
      }
    }

    return nullptr;
  }

  // How the round trips went.
  struct Tally {
    int same = 0;
    int differ = 0;
    int unspelled = 0;
    int host = 0;
    int imports = 0;  // a program that imports another file and does not come back the same
    int known = 0;
    int broken = 0;
  };

  // A program that is not checked again: one the page cannot spell, one with host parts.
  bool countedApart(const Module& module, const Translation& out, Tally& tally) {
    const bool unspelled = !out.problems.empty();
    const bool host = !unspelled && hasHostParts(module);

    tally.unspelled += unspelled ? 1 : 0;
    tally.host += host ? 1 : 0;

    return unspelled || host;
  }

  // The .poly with each import's path made from the folder the program is in (`import "Check.poly"` read from there).
  String importsFrom(const String& poly, const String& folder) {
    const String mark = "import \"";
    String text = poly;

    for (size_t at = text.find(mark); at != String::npos; at = text.find(mark, at + mark.size())) {
      if (at == 0 || text[at - 1] == '\n') {
        text.insert(at + mark.size(), folder + "/");
      }
    }

    return text;
  }

  // The .poly checked as a file of its own, in the temporary folder: its imports name the program's folder.
  FrontResult checkedAgain(const String& path, const String& poly) {
    const Path file = temp_directory_path() / "PolyRoundTripTest.poly";
    OFStream(file) << importsFrom(poly, Path(path).parent_path().string());
    FrontResult again = CheckFile(file.string());
    remove(file);

    return again;
  }

  // A program that does not come back the same: a failure, unless it imports another file or is a known gap. A file's
  // translation imports only the files its own code uses, so an imported file that needs another may not find it.
  void notTheSame(const String& path, const Module& first, const String& what, Tally& tally) {
    const char* gap = knownGap(path);
    const bool imports = gap == nullptr && importsAFile(first);
    int& count = gap != nullptr ? tally.known : imports ? tally.imports : what == "DIFFER" ? tally.differ : tally.broken;

    count++;
    cerr << (gap != nullptr ? "KNOWN GAP" : imports ? "IMPORTS " + what : what) << " " << path << (gap != nullptr ? String(": ") + gap : "") << "\n";
  }

  void checkAgain(const String& path, const Module& first, const String& poly, Tally& tally) {
    const FrontResult again = checkedAgain(path, poly);

    if (!Ok(again)) {
      cerr << "  " << again.errors[0] << "\n";
      notTheSame(path, first, "BROKEN", tally);
    } else if (comparable(first) != comparable(again.module)) {
      notTheSame(path, first, "DIFFER", tally);
    } else {
      expect(knownGap(path) == nullptr, String("the known gap is gone, so take it from the list: ") + path);
      tally.same++;
    }
  }

  // One program: checked, translated to .poly, checked again. A program with errors has nothing to translate.
  void roundTrip(const String& path, const Target& poly, Tally& tally) {
    const FrontResult first = CheckFile(path);

    if (!Ok(first)) {
      return;
    }

    const Translation out = Translate(first.module, poly);

    if (!countedApart(first.module, out, tally)) {
      checkAgain(path, first.module, out.text, tally);
    }
  }

}  // namespace

int main() {
  const Optional<Target> poly = FindTarget("poly", "");
  Tally tally;

  expect(poly.has_value(), "the poly page is built in");

  for (const String& path : poly ? linesOf("front_examples.txt") : Vector<String>{}) {
    roundTrip(path, *poly, tally);
  }

  cout << "poly round trip: " << tally.same << " same, " << tally.differ << " differ, " << tally.broken << " did not check again; "
       << tally.unspelled << " with no .poly spelling yet, " << tally.host << " with host parts, " << tally.imports << " importing a file "
       << "not the same, " << tally.known << " known gaps\n";
  expect(tally.same > 0, "some programs round trip");
  expect(tally.differ == 0, "no program checks into another program");
  expect(tally.broken == 0, "every translated .poly checks again");

  return failures == 0 ? 0 : 1;
}
