// A chain that makes a struct with its builder (Checker.hpp `BuilderChainProblem`), `Point.builder().x(1).y(2).build()`, must set every
// required field: one it leaves out is reported where the chain is written, with the message build() would stop with when it runs. A field is
// required unless it has a default, holds a container or is an optional (see ParserBuilder.cpp). A builder that is passed around and filled in
// elsewhere is checked when build() runs.
#include "front/Checker.hpp"

namespace polyir::front {

  namespace {

    // The parts of one chain: the struct's path as written (`Geometry.Point`), the setters it calls, and the `.build` it ends with.
    struct Chain {
      String path;
      Vector<String> setters;
      const AstSuffix* build = nullptr;
    };

    bool isCall(const AstSuffix& suffix, Checker& check, const char* name) {
      return suffix.kind == AstSuffixKind::Method && Str(check, suffix.name.name) == name;
    }

    // `Name.Name...builder()`: the path before `.builder()`, and where `.builder()` is; nothing when the suffixes do not start that way.
    Optional<size_t> builderAt(Checker& check, const AstExpr& base, View<AstSuffix> suffixes, String& path) {
      path = Str(check, base.name.name);
      size_t at = 0;

      for (; at < suffixes.size() && suffixes[at].kind == AstSuffixKind::Field; at++) {
        path += "." + Str(check, suffixes[at].name.name);
      }

      const bool found = at < suffixes.size() && isCall(suffixes[at], check, "builder") && Empty(suffixes[at].args);

      return found ? Optional<size_t>(at) : nullopt;
    }

    // The chain written in `node`, or nothing when it is not one builder chain from `X.builder()` to `.build()`.
    Optional<Chain> chainOf(Checker& check, const AstExpr& node) {
      const AstExpr& base = check.program->exprs[node.first];
      const View<AstSuffix> suffixes = check.program->suffixes[node.suffixes];
      Chain chain;
      const Optional<size_t> builder = base.kind == AstExprKind::Name ? builderAt(check, base, suffixes, chain.path) : nullopt;

      if (!builder || !isCall(suffixes.back(), check, "build")) {
        return nullopt;
      }

      for (size_t at = *builder + 1; at + 1 < suffixes.size(); at++) {
        if (suffixes[at].kind != AstSuffixKind::Method) {
          return nullopt;
        }

        chain.setters.push_back(Str(check, suffixes[at].name.name));
      }

      chain.build = &suffixes.back();

      return chain;
    }

    // A type that starts empty or absent when its field is left out: a container or an optional.
    bool startsEmpty(const TypePtr& type) {
      const TypeKind kind = type->kind;

      return kind == TypeKind::Hashmap || kind == TypeKind::Set || kind == TypeKind::FixedArray || kind == TypeKind::Grid ||
             kind == TypeKind::Tensor || kind == TypeKind::DynamicArray || kind == TypeKind::Sparse || kind == TypeKind::Optional;
    }

    // The first required field the chain does not set, or "".
    String missingField(Checker& check, const StructInfo& info, const Chain& chain) {
      for (size_t i = 0; i < info.fieldNames.size(); i++) {
        const bool required = !info.fieldDefaults[i] && !startsEmpty(info.fieldTypes[i]);
        const String field = Str(check, info.fieldNames[i]);

        if (required && find(chain.setters.begin(), chain.setters.end(), field) == chain.setters.end()) {
          return field;
        }
      }

      return "";
    }

  }  // namespace

  ExprPtr BuilderChainProblem(Checker& check, const AstExpr& node) {
    const Optional<Chain> chain = chainOf(check, node);
    const Symbol name = chain ? InScope(check.session, Intern(check.session.names, chain->path)) : Symbol();
    const StructDef* def = chain ? FindStruct(check.session.module, Str(check, name)) : nullptr;
    const StructInfo* info = def ? FindStruct(DeclsOf(check), name) : nullptr;
    const bool builder = def && any_of(def->annotations.begin(), def->annotations.end(), [](const Annotation& a) { return a.name == "Builder"; });

    if (!builder || info == nullptr) {
      return nullptr;
    }

    const String field = missingField(check, *info, *chain);

    return field.empty() ? nullptr : Unsupported(def->name + ".build: " + field + " is not set", SpanAt(chain->build->name.pos));
  }

}  // namespace polyir::front
