// Pages and canons compiled ahead (polyir/translate/Compiled.hpp, polyir-translatec). Each built-in page's compiled rules
// are the rules its text reads as, and polyir's compiled canon is targets/Polyir.canon read now. A page that lacks a form of
// its source's canon, or has a problem, is refused, naming the rule.
#include "polyir/Translate.hpp"
#include "polyir/translate/Compiled.hpp"

using namespace polyir;
using namespace polyir::translate;

namespace {

  int failures = 0;

  void expect(bool ok, const String& what) {
    if (ok) {
      return;
    }

    cerr << "FAIL " << what << "\n";
    failures++;
  }

  bool has(const String& text, const String& part) {
    return text.find(part) != String::npos;
  }

  String firstOf(const Vector<String>& texts) {
    return texts.empty() ? "(none)" : texts[0];
  }

  String fileText(const String& path) {
    IFStream in(path);
    StringStream text;
    text << in.rdbuf();

    return text.str();
  }

  // ── compiled as read ──

  // Two sets of rules are the same when they compile to the same C++.
  void builtInPagesAreTheirText() {
    for (const Target& target : BuiltInTargets()) {
      const String compiled = CompiledRulesText(*target.rules, "F", "here");
      const String read = CompiledRulesText(ReadRules(target.text, target.file), "F", "here");

      expect(target.rules != nullptr, target.name + " has compiled rules");
      expect(compiled == read, target.name + "'s compiled rules are its page's");
    }
  }

  void canonIsItsFile() {
    const String path = String(POLYIR_SOURCE_DIR) + "/../targets/Polyir.canon";
    const String read = CompiledSchemaText(ReadSchema(fileText(path), "targets/Polyir.canon"), "F", "here");

    expect(CompiledSchemaText(PolyirSchema(), "F", "here") == read, "polyir's compiled canon is targets/Polyir.canon");
  }

  // ── what stops a page ──

  // A page that finds polyir's canon as a built-in page does.
  Target madePage(const String& text) {
    Target page{"go", "Go.translate", text};
    page.findSchema = FindSchema;

    return page;
  }

  // A new page with every rule `todo`, but the one for `head` left out.
  Target pageWithout(const String& head) {
    const String text = NewTargetText("Go", PolyirSchema());
    const size_t at = text.find(head + " ");

    return madePage(text.substr(0, at) + text.substr(text.find('\n', at) + 1));
  }

  void missingFormRefused() {
    const Vector<String> problems = CompileProblems(pageWithout("while(cond, body)"));

    expect(problems.size() == 1, "one problem, got " + to_string(problems.size()));
    expect(has(firstOf(problems), "Go.translate: the form `while(cond, body)` has no rule"), "it names the rule, got " + firstOf(problems));
    expect(CompileProblems(madePage(NewTargetText("Go", PolyirSchema()))).empty(), "a page with every form compiles");
  }

  void problemRefusedAtItsLine() {
    const Target page = madePage(NewTargetText("Go", PolyirSchema()) + "\n## statements\nif(cond, then, otherwise) := x\n");

    expect(has(firstOf(CompileProblems(page)), "`if` has no part `otherwise`"), "the page's problem, got " + firstOf(CompileProblems(page)));
  }

}  // namespace

int main() {
  builtInPagesAreTheirText();
  canonIsItsFile();
  missingFormRefused();
  problemRefusedAtItsLine();

  if (failures == 0) {
    cout << "PageCompileTest ok\n";
  }

  return failures == 0 ? 0 : 1;
}
