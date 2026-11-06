// polyir as a source of the translate library (core/src/translate, core/docs/translate.md). Its canon, targets/Polyir.canon,
// read with no problem. Its coverage table names every form once. And every node it makes of the corpus is one of its
// canon's forms, a form the table says it produces.
#include "polyir/Frontend.hpp"
#include "polyir/Translate.hpp"

using namespace polyir;
using namespace polyir::front;
using namespace polyir::translate;
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

  String firstOf(const Vector<String>& texts) {
    return texts.empty() ? "" : texts[0];
  }

  // ── the canon and the table ──

  void canonReads() {
    const Schema& schema = PolyirSchema();

    expect(schema.problems.empty(), "Polyir.canon has a problem: " + firstOf(schema.problems));
    expect(schema.name == "polyir", "Polyir.canon names the source polyir, got " + schema.name);
    expect(schema.forms.size() > 150, "Polyir.canon has every form, got " + to_string(schema.forms.size()));
    expect(HasLowerForm(schema, "if") && FamilyNamed(schema, "int") != nullptr, "Polyir.canon gives lower forms and families");
  }

  void tableNamesEveryForm() {
    const Vector<String> problems = CoverageProblems(PolyirSchema(), PolyirCoverage());

    expect(problems.empty(), "the coverage table: " + firstOf(problems));
  }

  // ── what the corpus makes ──

  bool isProduced(const String& form) {
    for (const FormCoverage& row : PolyirCoverage()) {
      if (row.form == form) {
        return row.status == Coverage::Produced;
      }
    }

    return false;
  }

  // `if(cond, then)` for a node with the parts cond and then: what a node that is no form has.
  String partsOf(const Node& node) {
    String parts;

    for (const NodePart& part : node.parts) {
      parts += (parts.empty() ? "" : ", ") + part.name;
    }

    return node.construct + "(" + parts + ")";
  }

  // Each form the tree's nodes are, once; a node that is no form, or a form the table does not say is produced, is wrong.
  void checkNode(const Node& node, Set<String>& seen, Vector<String>& wrong) {
    const FormSpec* form = FormOfNode(PolyirSchema(), node);
    const String name = form == nullptr ? partsOf(node) + " is no form" : CoverageName(*form);

    if (seen.insert(name).second && (form == nullptr || !isProduced(name))) {
      wrong.push_back(name);
    }

    for (const NodePart& part : node.parts) {
      for (const Node& item : part.nodes) {
        checkNode(item, seen, wrong);
      }
    }

    for (const Node& item : node.lowered) {
      checkNode(item, seen, wrong);
    }
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
    const String core = POLYIR_SOURCE_DIR;
    Vector<String> paths = programsIn(core + "/examples");
    const Vector<String> tests = programsIn(core + "/tests/programs");
    const Vector<String> ideas = programsIn(core + "/../ideas");

    paths.insert(paths.end(), tests.begin(), tests.end());
    paths.insert(paths.end(), ideas.begin(), ideas.end());

    return paths;
  }

  // The produced forms no program of the corpus makes: the builder makes them, but nothing here checks it.
  void reportUnmade(const Set<String>& seen) {
    cout << "the corpus makes " << seen.size() << " of polyir's " << PolyirCoverage().size() << " forms; not:";

    for (const FormCoverage& row : PolyirCoverage()) {
      cout << (seen.count(row.form) == 0 ? " `" + row.form + "`" : "");
    }

    cout << "\n";
  }

  // The trees as js asks for them (generics kept) and as c does (generics as their copies, modules flat).
  void corpusMakesProducedForms() {
    Set<String> seen;
    Vector<String> wrong;

    for (const String& path : corpus()) {
      const FrontResult checked = CheckFile(path);

      for (const char* name : {"js", "c"}) {
        if (Ok(checked)) {
          checkNode(TreeOf(checked.module, *FindTarget(name, "")), seen, wrong);
        }
      }
    }

    for (const String& name : wrong) {
      expect(false, "the corpus makes `" + name + "`, which the table does not say is produced");
    }

    expect(seen.size() > 150, "the corpus makes most forms, got " + to_string(seen.size()));
    reportUnmade(seen);
  }

}  // namespace

int main() {
  canonReads();
  tableNamesEveryForm();
  corpusMakesProducedForms();

  if (failures == 0) {
    cout << "SourceCoverageTest ok\n";
  }

  return failures == 0 ? 0 : 1;
}
