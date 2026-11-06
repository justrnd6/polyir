// Reading a source's canon, a `.canon` file (polyir/translate/Schema.hpp): its forms and their parts' kinds, the facts and
// lower forms of its constructs, its families, and the problems of a file that is wrong, each at its line.
#include "polyir/translate/Schema.hpp"

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

  String firstOf(const Vector<String>& texts) {
    return texts.empty() ? "(none)" : texts[0];
  }

  // A made-up language of drawings: a statement draws a named shape, an expression is a circle or a number.
  const char* const kShapes = R"canon(# Shapes
// The canon of a made-up language.
source: shapes
typed: expressions
declarations: body

## kinds
block := body
name := name

## families
whole := int long
number := whole real

## statements
draw(name, body)
  facts: visible
  lowered
draw(name)

## expressions
circle(radius)
literal(value: text)

## types
int
long
real
box(elem)
  facts: sealed
)canon";

  const FormSpec* formNamed(const Schema& schema, const String& head) {
    for (const FormSpec& form : schema.forms) {
      if (FormHead(form) == head) {
        return &form;
      }
    }

    return nullptr;
  }

  // ── what a good file says ──

  void formsAndKinds() {
    const Schema schema = ReadSchema(kShapes, "Shapes.canon");
    const FormSpec* draw = formNamed(schema, "draw(name, body)");
    const FormSpec* literal = formNamed(schema, "literal(value)");
    const FormSpec* box = formNamed(schema, "box(elem)");

    expect(schema.problems.empty(), "Shapes.canon reads with no problem, got " + firstOf(schema.problems));
    expect(schema.name == "shapes" && schema.forms.size() == 8, "the source and its 8 forms");
    expect(draw != nullptr && draw->parts[0].kind == PartKind::Name && draw->parts[1].kind == PartKind::Block, "kinds by name");
    expect(literal != nullptr && literal->parts[0].kind == PartKind::Text, "a part's kind after `:`");
    expect(FindPart(*formNamed(schema, "circle(radius)"), "radius")->kind == PartKind::Value, "any other part is a value");

    expect(box != nullptr && box->isType && box->parts[0].kind == PartKind::Type, "a type's part is a type");
  }

  void factsAndLowerForms() {
    const Schema schema = ReadSchema(kShapes, "Shapes.canon");
    const Vector<String> drawFacts = FactsOf(schema, "draw", false);
    const Vector<String> boxFacts = FactsOf(schema, "box", true);

    expect(drawFacts.size() == 1 && drawFacts[0] == "visible", "a construct's facts");
    expect(boxFacts.size() == 1 && boxFacts[0] == "sealed", "a type's facts");
    expect(HasLowerForm(schema, "draw") && !HasLowerForm(schema, "circle"), "`lowered` marks a construct with a lower form");
    expect(IsTyped(schema, *formNamed(schema, "circle(radius)")) && !IsTyped(schema, *formNamed(schema, "draw(name)")),
      "a form of a `typed:` section has its value's type");
  }

  void families() {
    const Schema schema = ReadSchema(kShapes, "Shapes.canon");
    const Set<String>* number = FamilyNamed(schema, "number");

    expect(number != nullptr && number->size() == 3, "a family takes in a family it names");
    expect(number != nullptr && number->count("int") == 1 && number->count("real") == 1, "number is int, long and real");
    expect(FamilyNamed(schema, "circle") == nullptr, "a construct is no family");
  }

  // `declarations:` names the parts whose items are set apart by blank lines; a block part is a block a page may lay out.
  void declarationsAndBlocks() {
    const Schema schema = ReadSchema(kShapes, "Shapes.canon");
    const Vector<String> blocks = BlockContexts(schema);

    expect(schema.declarations.size() == 1 && schema.declarations.count("body") == 1, "a drawing's body holds declarations");
    expect(blocks.size() == 1 && blocks[0] == "draw.body", "the one block is draw's body, got " + firstOf(blocks));
  }

  void ids() {
    const Schema schema = ReadSchema(kShapes, "Shapes.canon");
    const ConstructId draw = ConstructNamed(schema, "draw", false);
    const ConstructId circle = ConstructNamed(schema, "circle", false);

    expect(draw != kNoId && ConstructNamed(schema, "draw", true) == kNoId, "a code construct is not a type");
    expect(schema.partsOf[static_cast<size_t>(draw)].size() == 4, "draw's parts: name, body, visible, lowered");
    expect(PartIdOf(schema, "type") != kNoId && schema.partsOf[static_cast<size_t>(circle)].size() == 2, "circle has its type");
  }

  // ── what a wrong file says ──

  bool hasProblem(const Schema& schema, const String& text) {
    for (const String& problem : schema.problems) {
      if (problem.find(text) != String::npos) {
        return true;
      }
    }

    return false;
  }

  void problems() {
    const String text = "# Bad\n\n## statements\ndraw(name\n  facts: x\n\n## kinds\nsize := width\n\n## types\nbox(elem)\n  sometimes\n";
    const Schema bad = ReadSchema(text, "Bad.canon");

    expect(hasProblem(bad, "Bad.canon: the canon names no `source:`"), "a file must name its source");
    expect(hasProblem(bad, "Bad.canon:4: `draw(name` is not a form"), "a form without its `)`, got " + firstOf(bad.problems));
    expect(hasProblem(bad, "Bad.canon:5: `facts: x` is under no form"), "an indented line under no form");
    expect(hasProblem(bad, "Bad.canon:8: `size` is no kind of part"), "a kind that is none");
    expect(hasProblem(bad, "Bad.canon:12: `sometimes` is neither"), "an indented line that is neither facts nor lowered");

    expect(bad.problems.size() == 5, "and nothing else, got " + to_string(bad.problems.size()));
  }

}  // namespace

int main() {
  formsAndKinds();
  factsAndLowerForms();
  families();

  declarationsAndBlocks();
  ids();
  problems();

  if (failures == 0) {
    cout << "SchemaTest ok\n";
  }

  return failures == 0 ? 0 : 1;
}
