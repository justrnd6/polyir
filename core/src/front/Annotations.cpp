// The annotations written before a declaration (Resolve.hpp `AnnotationsOf`): `@Name` or `@Name(key: value, ...)`. They go into the HIR as
// written, for tools and back ends to read; an argument's value is a literal (a number, a string, a char, true or false) and is kept as its text.
#include "front/Resolve.hpp"
#include "front/HirBuild.hpp"
#include "front/Pipeline.hpp"

namespace polyir::front {

  namespace {

    bool isLiteral(AstExprKind kind) {
      return kind == AstExprKind::Int || kind == AstExprKind::Float || kind == AstExprKind::String || kind == AstExprKind::Char ||
             kind == AstExprKind::True || kind == AstExprKind::False;
    }

    // The value as written (`-3` for a negative number), or "" when it is no literal.
    String valueText(const Program& program, ExprId id) {
      const AstExpr& written = program.exprs[id];
      const bool negative = written.kind == AstExprKind::Neg && isLiteral(program.exprs[written.first].kind);
      const AstExpr& literal = negative ? program.exprs[written.first] : written;

      if (!isLiteral(literal.kind)) {
        return "";
      }

      return (negative ? "-" : "") + String(literal.literal.text);
    }

    void noteProblem(DeclContext& context, const String& message, Pos at) {
      const String& file = context.session.files[context.unit.file];
      const String problem = DiagnosticMessage(message, SpanAt(at));
      context.session.declProblems.push_back(file.empty() ? problem : file + ":" + problem);
    }

    Annotation annotationOf(DeclContext& context, const AstAnnotation& written) {
      Annotation made{Str(context.session, written.name.name)};

      for (const AstFieldInit& arg : context.program.inits[written.args]) {
        const String value = valueText(context.program, arg.value);

        if (value.empty()) {
          noteProblem(context, "an annotation's argument is a literal: a number, a string, a char, true or false", arg.name.pos);
        }

        made.args.push_back({Str(context.session, arg.name.name), value});
      }

      return made;
    }

  }  // namespace

  Vector<Annotation> AnnotationsOf(DeclContext& context, Range<AstAnnotation> written) {
    Vector<Annotation> out;

    for (const AstAnnotation& annotation : context.program.annotations[written]) {
      out.push_back(annotationOf(context, annotation));
    }

    return out;
  }

}  // namespace polyir::front
