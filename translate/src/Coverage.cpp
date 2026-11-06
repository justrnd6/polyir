// A source's coverage table against its canon, and the form a node is (polyir/translate/Coverage.hpp).
#include "polyir/translate/Coverage.hpp"

namespace polyir::translate {

  namespace {

    // How often the table names each form, by its name.
    Map<String, int> namedCounts(const Vector<FormCoverage>& table) {
      Map<String, int> counts;

      for (const FormCoverage& row : table) {
        counts[row.form]++;
      }

      return counts;
    }

    Vector<const FormSpec*> allForms(const Schema& schema) {
      Vector<const FormSpec*> forms;

      for (const Vector<FormSpec>* all : {&schema.forms, &schema.factNodes}) {
        for (const FormSpec& form : *all) {
          forms.push_back(&form);
        }
      }

      return forms;
    }

    // The node's part is one of the form's, one of its construct's facts, or a typed form's `type`.
    bool partFits(const Schema& schema, const FormSpec& form, const NodePart& part) {
      const Vector<String> facts = FactsOf(schema, form.construct, form.isType);
      const bool fact = find(facts.begin(), facts.end(), part.name) != facts.end();

      return FindPart(form, part.name) != nullptr || fact || (IsTyped(schema, form) && part.name == "type");
    }

    // How many of the form's parts the node leaves out, or -1 when the node is not the form.
    int partsLeftOut(const Schema& schema, const FormSpec& form, const Node& node) {
      const bool fits = all_of(node.parts.begin(), node.parts.end(), [&](const NodePart& part) { return partFits(schema, form, part); });
      int leftOut = 0;

      for (const PartSpec& part : form.parts) {
        const bool has = PartNamed(node, part.name) != nullptr;

        if (!has && !MayLeaveOut(part.kind)) {
          return -1;
        }

        leftOut += has ? 0 : 1;
      }

      return fits ? leftOut : -1;
    }

  }  // namespace

  String CoverageName(const FormSpec& form) {
    return FormHead(form) + (form.isType ? " (a type)" : "");
  }

  Vector<String> CoverageProblems(const Schema& schema, const Vector<FormCoverage>& table) {
    Map<String, int> counts = namedCounts(table);
    Vector<String> problems;

    for (const FormSpec* form : allForms(schema)) {
      const int count = counts[CoverageName(*form)];
      counts.erase(CoverageName(*form));

      if (count != 1) {
        problems.push_back("`" + CoverageName(*form) + "` is named " + to_string(count) + " times, not once");
      }
    }

    for (const auto& [name, count] : counts) {
      problems.push_back("`" + name + "` is no form of " + schema.name + "'s canon");
    }

    return problems;
  }

  const FormSpec* FormOfNode(const Schema& schema, const Node& node) {
    const FormSpec* best = nullptr;
    int bestLeftOut = 0;

    for (const FormSpec* form : FormsOf(schema, node.construct, node.isType)) {
      const int leftOut = partsLeftOut(schema, *form, node);

      if (leftOut >= 0 && (best == nullptr || leftOut < bestLeftOut)) {
        best = form;
        bestLeftOut = leftOut;
      }
    }

    return best;
  }

}  // namespace polyir::translate
