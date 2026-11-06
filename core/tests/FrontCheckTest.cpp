// The C++ checker against the Nina checker's own test programs. tests/data/front_cases.txt holds every polyir program the Nina tests
// (lang/tests/src/*.nina, core/tests/programs/*.poly) and tests/data/handwritten.txt build, each with the result the Nina CLI gives for it:
// the typed HIR text, or the error messages. The file was recorded from the Nina implementation, which is no longer in this repository;
// this test needs nothing but the file.
//
// Every case is tagged with the areas of the language it uses (core, containers, generics, ...). A case runs when all its areas are in `kEnabledAreas`;
// the others are skipped and counted. The piece of work that adds an area (the container builtins, generics, closures, ...) adds it to the list below, and the
// cases for it start to run. FRONT_AREAS=core,containers (a comma list, or `all`) overrides the list, and FRONT_SHOW=1 prints each failing case's source, FRONT_SUMMARY=1 a table of the cases by area.
#include "polyir/Std.hpp"
#include "polyir/Frontend.hpp"
#include "front/Ast.hpp"
#include "front/Lint.hpp"
#include "polyir/HirPrint.hpp"

#include <unistd.h>

using namespace polyir;
using namespace polyir::front;

namespace {

  // The areas whose cases must pass. Add an area here when its work is done.
  const char* const kEnabledAreas[] = {"core", "containers", "exceptions", "coroutines", "kernel", "imports", "prelude", "closures", "sums", "interfaces", "generics", "host"};

  struct Case {
    String name;
    Vector<String> areas;
    String origin;
    String source;
    String kind;    // hir, check failed, compile failed
    String expect;
  };

  Vector<String> splitOn(const String& text, char separator) {
    Vector<String> parts;

    for (size_t from = 0; from <= text.size();) {
      const size_t at = min(text.find(separator, from), text.size());
      parts.push_back(text.substr(from, at - from));
      from = at + 1;
    }

    return parts;
  }

  bool startsWith(const String& text, const char* prefix) {
    return text.compare(0, strlen(prefix), prefix) == 0;
  }

  // The text after ` key=` in a "@@@ case" line, up to the next space.
  String fieldOf(const String& head, const String& key) {
    const size_t at = head.find(" " + key + "=");

    if (at == String::npos) {
      return "";
    }

    const size_t from = at + key.size() + 2;

    return head.substr(from, min(head.find(' ', from), head.size()) - from);
  }

  // The part of the newest case that text lines go into: none (between sections), its source, or its expected output.
  enum class Section { None, Source, Expect };

  // Reading the data file line by line: a marker line starts a case or a section, any other line belongs to the open section.
  struct Reader {
    Vector<Case> cases;
    Section section = Section::None;
    bool fresh = true;
  };

  // "@@@ case NAME areas=A,B from=X"
  void startCase(Vector<Case>& cases, const String& line) {
    Case made;
    made.name = splitOn(line, ' ')[2];
    made.areas = splitOn(fieldOf(line, "areas"), ',');
    made.origin = fieldOf(line, "from");
    cases.push_back(made);
  }

  void markerLine(Reader& reader, const String& line) {
    reader.fresh = true;
    reader.section = Section::None;

    if (startsWith(line, "@@@ case ")) {
      startCase(reader.cases, line);
    }

    if (startsWith(line, "@@@ source")) {
      reader.section = Section::Source;
    }

    if (startsWith(line, "@@@ expect ")) {
      reader.cases.back().kind = line.substr(11);
    }

    if (startsWith(line, "@@@ expect ")) {
      reader.section = Section::Expect;
    }
  }

  // The text of `c` that an open section (Source or Expect) fills.
  String& sectionText(Case& c, Section section) {
    if (section == Section::Source) {
      return c.source;
    }

    return c.expect;
  }

  void textLine(Reader& reader, const String& line) {
    if (reader.section == Section::None) {
      return;
    }

    sectionText(reader.cases.back(), reader.section) += (reader.fresh ? "" : "\n") + line;
    reader.fresh = false;
  }

  String fileText(const String& path) {
    IFStream in(path);
    StringStream buffer;
    buffer << in.rdbuf();

    return buffer.str();
  }

  Vector<Case> readCases(const String& path) {
    Reader reader;

    for (const String& line : splitOn(fileText(path), '\n')) {
      if (startsWith(line, "@@@ ")) {
        markerLine(reader, line);
      } else {
        textLine(reader, line);
      }
    }

    return reader.cases;
  }

  // The areas that run: the list above, or the FRONT_AREAS override ("all" turns every area on).
  Set<String> enabledAreas() {
    const char* override = getenv("FRONT_AREAS");

    if (override == nullptr) {
      return Set<String>(begin(kEnabledAreas), end(kEnabledAreas));
    }

    const Vector<String> listed = splitOn(override, ',');

    return Set<String>(listed.begin(), listed.end());
  }

  bool enabled(const Set<String>& on, const Vector<String>& areas) {
    return on.count("all") != 0 || all_of(areas.begin(), areas.end(), [&](const String& area) { return on.count(area) != 0; });
  }

  String joinLines(const Vector<String>& lines) {
    String out;

    for (size_t i = 0; i < lines.size(); i++) {
      out += (i == 0 ? "" : "\n") + lines[i];
    }

    return out;
  }

  // The source is checked as a file (as the Nina CLI does, which includes its duplicate-name check). So the messages carry the
  // file's name; the expected text has it removed.
  String withoutFile(const String& message, const String& file) {
    return message.compare(0, file.size() + 1, file + ":") == 0 ? message.substr(file.size() + 1) : message;
  }

  Vector<String> withoutFiles(const Vector<String>& errors, const String& file) {
    Vector<String> out;

    for (const String& e : errors) {
      out.push_back(withoutFile(e, file));
    }

    return out;
  }

  FrontResult checkAsFile(const String& source) {
    const String file = "front_case_" + to_string(getpid()) + ".poly";
    OFStream(file) << source;
    FrontResult result = CheckFile(file);
    remove(file.c_str());
    result.errors = withoutFiles(result.errors, file);

    return result;
  }

  // What the C++ front end gives for the source, in the same form as the expected text.
  String actualFor(const Case& c, String& kind) {
    const FrontResult result = checkAsFile(c.source);
    kind = Ok(result) ? "hir" : result.headline;

    return Ok(result) ? ModuleText(result.module) : joinLines(result.errors);
  }

  String lineOr(const Vector<String>& lines, size_t i) {
    return i < lines.size() ? lines[i] : "(nothing)";
  }

  // The first line where two texts differ, with both versions.
  String firstDifference(const String& expected, const String& actual) {
    const Vector<String> a = splitOn(expected, '\n');
    const Vector<String> b = splitOn(actual, '\n');

    for (size_t i = 0; i < a.size() || i < b.size(); i++) {
      if (lineOr(a, i) != lineOr(b, i)) {
        return "  line " + to_string(i + 1) + "\n    Nina: " + lineOr(a, i) + "\n    C++:  " + lineOr(b, i);
      }
    }

    return "";
  }

  bool passes(const Case& c) {
    String kind;
    const String actual = actualFor(c, kind);

    if (kind == c.kind && actual == c.expect) {
      return true;
    }

    cout << "FAIL " << c.name << " (" << c.origin << ")\n  expected " << c.kind << ", got " << kind << "\n" << firstDifference(c.expect, actual) << "\n";

    if (getenv("FRONT_SHOW") != nullptr) {
      cout << "--- source\n" << c.source << "\n---\n";
    }

    return false;
  }

  // How many cases ran, and how many of those failed.
  struct AreaCount {
    int run = 0;
    int failed = 0;
  };

  struct Tally {
    int run = 0;
    int failed = 0;
    Map<String, AreaCount> byAreas;  // keyed by the areas of a case, joined with commas
  };

  String joinAreas(const Vector<String>& areas) {
    String out;

    for (size_t i = 0; i < areas.size(); i++) {
      out += (i == 0 ? "" : ",") + areas[i];
    }

    return out;
  }

  void record(Tally& tally, const Case& c, bool ok) {
    tally.run++;
    tally.failed += ok ? 0 : 1;
    tally.byAreas[joinAreas(c.areas)].run++;
    tally.byAreas[joinAreas(c.areas)].failed += ok ? 0 : 1;
  }

  Tally runCases(const Vector<Case>& cases) {
    Tally tally;
    const Set<String> on = enabledAreas();

    for (const Case& c : cases) {
      if (enabled(on, c.areas)) {
        record(tally, c, passes(c));
      }
    }

    return tally;
  }

  // FRONT_SUMMARY=1: how many cases of each combination of areas ran, and how many of those failed.
  void printSummary(const Tally& tally) {
    for (const auto& [areas, count] : tally.byAreas) {
      cout << "  " << areas << ": " << count.run << " cases, " << count.failed << " failed\n";
    }
  }

}  // namespace

int main() {
  const Vector<Case> cases = readCases(String(POLYIR_SOURCE_DIR) + "/tests/data/front_cases.txt");
  const Tally tally = runCases(cases);

  if (getenv("FRONT_SUMMARY") != nullptr) {
    printSummary(tally);
  }

  cout << tally.run << " cases run, " << tally.failed << " failed, " << cases.size() - tally.run << " skipped (areas not enabled yet)\n";

  return tally.failed == 0 && tally.run > 0 ? 0 : 1;
}
