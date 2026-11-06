// Every example program, written back as .poly (WritePoly) and checked again, is the same program: the two HIR texts have the same lines,
// once the hidden names that carry a line and column (`forList_12_4`) are numbered away. A generic that works for every type is written
// once (`struct Box<T>`), so it must come back too, with what each copy is to it. The .poly writes any other generic as its copies. A program WritePoly cannot spell yet is counted
// as such, not failed; one whose .poly does not check again, or checks into another program, is a failure.
#include "polyir/Std.hpp"
#include "polyir/Frontend.hpp"
#include "polyir/HirPrint.hpp"
#include "polyir/PolySource.hpp"

using namespace polyir;
using namespace polyir::front;

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

  // `generic Box_T T`: a generic as declared that works for every type, and its parameters.
  String genericLine(const String& name, const Generic& generic) {
    String line = "generic " + name;

    for (const String& param : generic.params) {
      line += " " + param;
    }

    return line;
  }

  // Whether the generic struct named `of` works for every type.
  bool works(const Module& module, const String& of) {
    for (const StructDef& def : module.genericStructs) {
      if (def.generic.of == of && def.generic.notParametric.empty()) {
        return true;
      }
    }

    return false;
  }

  // One line per generic struct that works for every type, and one per copy of it (`Box_i64 of Box`).
  void addGenericLines(const Module& module, Vector<String>& lines) {
    for (const StructDef& def : module.genericStructs) {
      if (def.generic.notParametric.empty()) {
        lines.push_back(genericLine(def.name, def.generic));
      }
    }

    for (const StructDef& def : module.structs) {
      if (works(module, def.generic.of)) {
        lines.push_back(def.name + " of " + def.generic.of);
      }
    }
  }

  // The HIR text and the generics as sorted lines with the positions taken out, so two checks of one program compare equal.
  Vector<String> comparable(const Module& module) {
    IStringStream text(ModuleText(module));
    Vector<String> lines;

    for (String line; getline(text, line);) {
      lines.push_back(withoutPositions(line));
    }

    addGenericLines(module, lines);

    sort(lines.begin(), lines.end());

    return lines;
  }

  // How the round trips went.
  struct Tally {
    int same = 0;
    int differ = 0;
    int unspelled = 0;
    int broken = 0;
  };

  void checkAgain(const String& path, const Module& first, const String& poly, Tally& tally) {
    const FrontResult again = CheckSource(poly);

    if (!Ok(again)) {
      tally.broken++;
      cerr << "BROKEN " << path << ": " << again.errors[0] << "\n";
    } else if (comparable(first) != comparable(again.module)) {
      tally.differ++;
      cerr << "DIFFER " << path << "\n";
    } else {
      tally.same++;
    }
  }

  // One program: checked, written as .poly, checked again. A program with errors has nothing to write back.
  void roundTrip(const String& path, Tally& tally) {
    const FrontResult first = CheckFile(path);

    if (!Ok(first)) {
      return;
    }

    const PolySource poly = WritePoly(first.module);

    if (!poly.problems.empty()) {
      tally.unspelled++;

      return;
    }

    checkAgain(path, first.module, poly.text, tally);
  }

}  // namespace

int main() {
  Tally tally;

  for (const String& path : linesOf("front_examples.txt")) {
    roundTrip(path, tally);
  }

  cout << "round trip: " << tally.same << " same, " << tally.differ << " differ, " << tally.unspelled << " with no .poly spelling yet, "
       << tally.broken << " did not check again\n";
  expect(tally.same > 0, "some programs round trip");
  expect(tally.differ == 0, "no program checks into another program");
  expect(tally.broken == 0, "every written .poly checks again");

  return failures == 0 ? 0 : 1;
}
