// Types (the grammar's typeRef): `ptr<T>`, `vec<T, N>`, `array<T>`, ..., `func(T) -> R`, `variant<A, B>`, `Name<T>` and `Name[N]`.
// A type such as `ptr<T>`, `map<K, V>` or `func(T) -> R` starts with a keyword; the table kTypeForms says, for each keyword, which
// kind of type it makes and what shape of text follows it (TypeShape in Parser.hpp). A type that starts with a name instead is `Name`
// (a scalar such as `i64`, `f64` or `bool`, a struct, interface, extern type or type parameter), `Name[N]` (a fixed array) or
// `Name<A, B>` (a generic).
#include "front/Parser.hpp"
#include "front/ParserSets.hpp"

namespace polyir::front {

  namespace {

    using Shape = TypeShape;

    // One row of the table: the keyword, the type it makes, and the shape of the text after it.
    struct TypeForm {
      TokenKind token;
      AstTypeKind kind;
      Shape shape;
    };

    constexpr TypeForm kTypeForms[] = {
      {K::PTR, AstTypeKind::Ptr, Shape::One},          {K::ARRAY, AstTypeKind::Array, Shape::One},
      {K::MUTARRAY, AstTypeKind::Mutarray, Shape::One}, {K::SPARSE, AstTypeKind::Sparse, Shape::One},
      {K::SET, AstTypeKind::Set, Shape::One},          {K::MUTSET, AstTypeKind::Mutset, Shape::One},
      {K::OPTIONAL, AstTypeKind::Optional, Shape::One}, {K::CORO, AstTypeKind::Coroutine, Shape::One},
      {K::MAP, AstTypeKind::Map, Shape::Pair},         {K::MUTMAP, AstTypeKind::Mutmap, Shape::Pair},
      {K::EITHER, AstTypeKind::Either, Shape::Pair},   {K::VEC, AstTypeKind::Vec, Shape::Sized},
      {K::BUFFER, AstTypeKind::Buffer, Shape::Sized},  {K::GRID, AstTypeKind::Grid, Shape::Sized},
      {K::TENSOR, AstTypeKind::Tensor, Shape::Sized},  {K::FUNC, AstTypeKind::Func, Shape::Func},
      {K::CLOSURE, AstTypeKind::Closure, Shape::Func}, {K::VARIANT, AstTypeKind::Variant, Shape::Variant}};

    // The row for a keyword, or nullptr when the type does not start with a keyword.
    const TypeForm* formOf(TokenKind kind) {
      for (const TypeForm& form : kTypeForms) {
        if (form.token == kind) {
          return &form;
        }
      }

      return nullptr;
    }

    // The number of `, INT_LIT` sizes a keyword type takes after its element type (a tensor takes one or more).
    size_t sizesOf(AstTypeKind kind) {
      return kind == AstTypeKind::Grid ? 2 : 1;
    }

  }  // namespace

  TypeId ParseType(Parser& parser) {
    RequireAny(parser, kTypeStart, kTypeStartMsg);
    AstType type;
    type.pos = PosOf(parser, Current(parser));
    FillType(parser, type);

    return Add(parser.program.types, type);
  }

  // A type that starts with a keyword is read by the shape its table row gives; any other is a name.
  void FillType(Parser& parser, AstType& type) {
    const TypeForm* form = formOf(CurrentKind(parser));

    if (form == nullptr) {
      return ParseNamedType(parser, type);
    }

    type.kind = form->kind;
    Take(parser);
    FillKeywordType(parser, type, form->shape);
  }

  void FillKeywordType(Parser& parser, AstType& type, TypeShape shape) {
    if (shape == Shape::Func) {
      return ParseFuncType(parser, type);
    }

    if (shape == Shape::Variant) {
      return ParseTypeArgs(parser, type);
    }

    ExpectTypeOpen(parser);
    type.first = ParseType(parser);

    if (shape == Shape::Pair) {
      ParsePairType(parser, type);
    } else if (shape == Shape::Sized) {
      ParseSizedType(parser, type);
    } else {
      ExpectTypeClose(parser);
    }
  }

  // `, V>` of `map<K, V>`, `mutmap<K, V>` and `either<T, E>`.
  void ParsePairType(Parser& parser, AstType& type) {
    type.second = CommaType(parser);
    ExpectTypeClose(parser);
  }

  // `, N>` of `vec<T, N>` and `buffer<T, N>`, `, R, C>` of `grid<T, R, C>`, `, N, ...>` of `tensor<T, N, ...>`.
  void ParseSizedType(Parser& parser, AstType& type) {
    ParseSizes(parser, type, sizesOf(type.kind), type.kind == AstTypeKind::Tensor);
    ExpectTypeClose(parser);
  }

  // `, INT_LIT` `required` times, then (for a tensor) as many more as there are.
  void ParseSizes(Parser& parser, AstType& type, size_t required, bool allowsMore) {
    const size_t mark = parser.scratch.literals.size();

    for (size_t i = 0; i < required; i++) {
      parser.scratch.literals.push_back(CommaSize(parser));
    }

    while (allowsMore && At(parser, K::COMMA)) {
      parser.scratch.literals.push_back(CommaSize(parser));
    }

    type.sizes = Take(parser.program.literals, parser.scratch.literals, mark);
  }

  // `, type`
  TypeId CommaType(Parser& parser) {
    Expect(parser, K::COMMA);

    return ParseType(parser);
  }

  // `, INT_LIT`
  Literal CommaSize(Parser& parser) {
    Expect(parser, K::COMMA);

    return ExpectLiteral(parser, K::INT_LIT);
  }

  // `func(A, B) -> R` and `closure(A, B) -> R`.
  void ParseFuncType(Parser& parser, AstType& type) {
    Expect(parser, K::LPAREN);
    type.types =
      OptionalCommaList(parser, kTypeStart, parser.scratch.types, parser.program.typeRefs, [&] { return ParseType(parser); });
    Expect(parser, K::RPAREN);
    Expect(parser, K::ARROW);
    type.first = ParseType(parser);
  }

  // `A, B, ...` (at least one type).
  Range<TypeId> ParseTypeList(Parser& parser) {
    return CommaList(parser, parser.scratch.types, parser.program.typeRefs, [&] { return ParseType(parser); });
  }

  // A type's name; one named through its module, `Geometry.Point`, is its path, `Geometry.Point`.
  Ident ParseTypeName(Parser& parser) {
    Ident name = ExpectIdent(parser);

    while (At(parser, K::DOT) && KindAt(parser, 1) == K::IDENT) {
      Take(parser);
      name.name = InModule(parser, String(TextOf(*parser.program.names, name.name)), ExpectIdent(parser).name);
    }

    return name;
  }

  // `Name<A, B>` when a `<` follows the name (the grammar's predicate), else `Name` or `Name[N]`.
  void ParseNamedType(Parser& parser, AstType& type) {
    type.name = ParseTypeName(parser);
    const bool isGeneric = At(parser, K::LT);
    type.kind = isGeneric ? AstTypeKind::Generic : AstTypeKind::Named;

    if (isGeneric) {
      ParseTypeArgs(parser, type);
    } else {
      ParseArraySize(parser, type);
    }
  }

  // `<A, B, ...>`: the arguments of a generic name, and the cases of a `variant`.
  void ParseTypeArgs(Parser& parser, AstType& type) {
    ExpectTypeOpen(parser);
    type.types = ParseTypeList(parser);
    ExpectTypeClose(parser);
  }

  // `[N]` after a name (a fixed array), or nothing.
  void ParseArraySize(Parser& parser, AstType& type) {
    if (!Accept(parser, K::LBRACKET)) {
      return;
    }

    const size_t mark = parser.scratch.literals.size();
    parser.scratch.literals.push_back(ExpectLiteral(parser, K::INT_LIT));
    type.sizes = Take(parser.program.literals, parser.scratch.literals, mark);
    Expect(parser, K::RBRACKET);
  }

}  // namespace polyir::front
