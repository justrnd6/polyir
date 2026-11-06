// What a schema says (Schema.hpp, read by SchemaRead.cpp): a construct's forms and facts, its lower form, a section's `type`,
// a family's types, and the ids of constructs and parts. Also a form's parts (Constructs.hpp).
#include "polyir/translate/Schema.hpp"

namespace polyir::translate {

  Vector<const FormSpec*> FormsOf(const Schema& schema, const String& construct, bool isType) {
    Vector<const FormSpec*> forms;

    for (const Vector<FormSpec>* all : {&schema.forms, &schema.factNodes}) {
      for (const FormSpec& form : *all) {
        if (form.construct == construct && form.isType == isType) {
          forms.push_back(&form);
        }
      }
    }

    return forms;
  }

  Vector<String> FactsOf(const Schema& schema, const String& construct, bool isType) {
    const Map<String, Vector<String>>& facts = isType ? schema.typeFacts : schema.facts;
    const auto found = facts.find(construct);

    return found == facts.end() ? Vector<String>{} : found->second;
  }

  bool HasLowerForm(const Schema& schema, const String& construct) {
    return schema.lowered.count(construct) > 0;
  }

  bool IsTyped(const Schema& schema, const FormSpec& form) {
    return !form.isType && schema.typed.count(form.section) > 0;
  }

  const Set<String>* FamilyNamed(const Schema& schema, const String& word) {
    const auto found = schema.families.find(word);

    return found == schema.families.end() ? nullptr : &found->second;
  }

  Vector<String> BlockContexts(const Schema& schema) {
    Vector<String> contexts;

    for (const FormSpec& form : schema.forms) {
      for (const PartSpec& part : form.isType ? Vector<PartSpec>{} : form.parts) {
        const String context = form.construct + "." + part.name;
        const bool known = find(contexts.begin(), contexts.end(), context) != contexts.end();

        part.kind == PartKind::Block && !known ? contexts.push_back(context) : void();
      }
    }

    return contexts;
  }

  ConstructId ConstructNamed(const Schema& schema, const String& name, bool isType) {
    const Map<String, ConstructId>& ids = isType ? schema.typeIds : schema.codeIds;
    const auto found = ids.find(name);

    return found == ids.end() ? kNoId : found->second;
  }

  PartId PartIdOf(const Schema& schema, const String& name) {
    const auto found = schema.partIds.find(name);

    return found == schema.partIds.end() ? kNoId : found->second;
  }

  // ── a form's parts ──

  const PartSpec* FindPart(const FormSpec& form, const String& name) {
    for (const PartSpec& part : form.parts) {
      if (part.name == name) {
        return &part;
      }
    }

    return nullptr;
  }

  String FormHead(const FormSpec& form) {
    String parts;

    for (const PartSpec& part : form.parts) {
      parts += (parts.empty() ? "" : ", ") + part.name;
    }

    return form.parts.empty() ? form.construct : form.construct + "(" + parts + ")";
  }

  bool MayLeaveOut(PartKind kind) {
    return kind == PartKind::Type || kind == PartKind::Fact;
  }

}  // namespace polyir::translate
