// `@Builder` on a struct (Parser.hpp `ExpandBuilder`). `@Builder struct Point(x: i64, y: i64, label: string = "")` reads as if Point's body also
// held
//
//   static func builder() -> Builder { return Builder(__x: None<i64>(), __y: None<i64>(), __label: "") }
//   struct Builder(var __x: optional<i64>, var __y: optional<i64>, var __label: string) {
//     func x(value: i64) -> Builder { this.__x = Some<i64>(value)  return this }       (one setter per field)
//     func build() -> Point { if (this.__x.isNone()) { fail("Point.build: x is not set") } ... return Point(x: this.__x.unwrap(), ...) }
//   }
//
// so `Point.builder().x(1).y(2).build()` makes a Point. A field is required unless it has a default, holds a container (which starts
// empty) or is an optional (which starts as None). The text is parsed inside the struct's body, so every later phase sees ordinary
// declarations; its tokens all sit at the struct's position, where any problem in it is reported.
#include "front/Parser.hpp"

namespace polyir::front {

  namespace {

    String joined(const Vector<String>& parts, const String& separator) {
      String out;

      for (const String& part : parts) {
        out += (out.empty() ? "" : separator) + part;
      }

      return out;
    }

    // What one field asks of the builder.
    struct BuilderField {
      String name;
      String type;         // as written
      String initial;      // the default as written, "" for none
      String optionalOf;   // for an `optional<T>` field, T; "" otherwise
      bool required = false;
    };

    bool startsEmpty(AstTypeKind kind) {
      return kind == AstTypeKind::Array || kind == AstTypeKind::Mutarray || kind == AstTypeKind::Map || kind == AstTypeKind::Mutmap ||
             kind == AstTypeKind::Set || kind == AstTypeKind::Mutset || kind == AstTypeKind::Sparse || kind == AstTypeKind::Grid ||
             kind == AstTypeKind::Tensor;
    }

    // `T` for the text `optional<T>`.
    String innerOf(const String& type) {
      const size_t open = type.find('<');

      return type.substr(open + 1, type.rfind('>') - open - 1);
    }

    BuilderField fieldOf(const Parser& parser, const AstField& field) {
      const AstTypeKind kind = parser.program.types[field.type].kind;
      BuilderField made{String(TextOf(*parser.program.names, field.name.name)), String(field.typeText), String(field.initialText), "", false};
      made.optionalOf = kind == AstTypeKind::Optional ? innerOf(made.type) : "";
      made.required = made.initial.empty() && made.optionalOf.empty() && !startsEmpty(kind);

      return made;
    }

    // Every field of the struct, those in its head first.
    Vector<BuilderField> fieldsOf(const Parser& parser, const AstStructDecl& decl) {
      Vector<BuilderField> fields;

      for (const AstField& field : parser.program.fields[decl.primary]) {
        fields.push_back(fieldOf(parser, field));
      }

      for (const AstMember& member : parser.program.members[decl.members]) {
        if (!member.isMethod) {
          fields.push_back(fieldOf(parser, member.field));
        }
      }

      return fields;
    }

    // What `builder()` gives a field's storage at the start; "" for a container, which starts empty when left out.
    String startOf(const BuilderField& field) {
      if (field.required) {
        return "None<" + field.type + ">()";
      }

      return field.optionalOf.empty() ? field.initial : "None<" + field.optionalOf + ">()";
    }

    String builderFunction(const Vector<BuilderField>& fields) {
      Vector<String> starts;

      for (const BuilderField& field : fields) {
        const String start = startOf(field);

        if (!start.empty()) {
          starts.push_back("__" + field.name + ": " + start);
        }
      }

      return "static func builder() -> Builder {\n  return Builder(" + joined(starts, ", ") + ")\n}\n";
    }

    String storageOf(const Vector<BuilderField>& fields) {
      Vector<String> storage;

      for (const BuilderField& field : fields) {
        storage.push_back("var __" + field.name + ": " + (field.required ? "optional<" + field.type + ">" : field.type));
      }

      return storage.empty() ? "" : "(" + joined(storage, ", ") + ")";
    }

    String setterOf(const BuilderField& field) {
      const String value = field.required ? "Some<" + field.type + ">(value)" : "value";

      return "  func " + field.name + "(value: " + field.type + ") -> Builder {\n    this.__" + field.name + " = " + value + "\n    return this\n  }\n";
    }

    // `build()`: each required field checked, then the struct made from the stored values.
    String buildOf(const Vector<BuilderField>& fields, const String& structName, const String& path) {
      String checks;
      Vector<String> inits;

      for (const BuilderField& field : fields) {
        const String stored = "this.__" + field.name;
        checks += field.required ? "    if (" + stored + ".isNone()) {\n      fail(\"" + path + ".build: " + field.name + " is not set\")\n    }\n" : "";
        inits.push_back(field.name + ": " + stored + (field.required ? ".unwrap()" : ""));
      }

      return "  func build() -> " + structName + " {\n" + checks + "    return " + structName + "(" + joined(inits, ", ") + ")\n  }\n";
    }

    String builderText(const Parser& parser, const AstStructDecl& decl, const String& structName) {
      const Vector<BuilderField> fields = fieldsOf(parser, decl);
      String setters;

      for (const BuilderField& field : fields) {
        setters += setterOf(field);
      }

      const String path = String(TextOf(*parser.program.names, decl.name.name));
      const String body = setters + buildOf(fields, structName, path);

      return builderFunction(fields) + "struct Builder" + storageOf(fields) + " {\n" + body + "}\n";
    }

    // The made text's tokens, each at `at`.
    Vector<Token> tokensAt(const LexResult& lexed, Pos at) {
      Vector<Token> tokens = lexed.tokens;

      for (Token& token : tokens) {
        token.line = static_cast<uint32_t>(at.line);
        token.column = static_cast<uint32_t>(at.column);
      }

      return tokens;
    }

    // The made text parsed as the members of the struct's body.
    void parseMade(Parser& parser, const AstStructDecl& decl, const String& structName, String text) {
      parser.program.made.push_back(make_shared<const String>(move(text)));
      const String& source = *parser.program.made.back();
      const Vector<Token> tokens = tokensAt(LexSource(source), decl.pos);
      Parser made{parser.program, tokens, source};

      made.modules = parser.modules;
      made.modules.push_back(structName);
      made.outer = ModulePath(made);

      while (!At(made, TokenKind::EOF_) && !made.hasFailed) {
        ParseMember(made);
      }

      if (made.hasFailed && !parser.hasFailed) {
        parser.hasFailed = true;
        parser.error = made.error;
      }
    }

  }  // namespace

  bool HasAnnotation(const Parser& parser, Range<AstAnnotation> annotations, StringView name) {
    for (const AstAnnotation& annotation : parser.program.annotations[annotations]) {
      if (TextOf(*parser.program.names, annotation.name.name) == name) {
        return true;
      }
    }

    return false;
  }

  void ExpandBuilder(Parser& parser, const AstStructDecl& decl) {
    const String path = String(TextOf(*parser.program.names, decl.name.name));
    const String structName = path.substr(path.rfind('.') == String::npos ? 0 : path.rfind('.') + 1);

    if (!Empty(decl.typeParams)) {
      parser.hasFailed = true;
      parser.error = to_string(decl.pos.line) + ":" + to_string(decl.pos.column) + ": @Builder does not take a generic struct yet";

      return;
    }

    parseMade(parser, decl, structName, builderText(parser, decl, structName));
  }

}  // namespace polyir::front
