// Type-mismatch rules (CheckTypes.nina). Two values are compared by their KIND, not their exact type: integer, float, string, or a particular struct.
// Different integer widths are not a mismatch (`i32` and `i64` are both integers). A container (a list, map, set, grid, ...) is a mismatch against one of
// those kinds; anything else (two containers, pointers, vectors, failed expressions) is never reported, so one mistake cannot cause a flood of follow-on errors.
#include "front/Checker.hpp"
#include "polyir/HirPrint.hpp"

namespace polyir::front {

  namespace {

    // What a mismatch compares; Unchecked is every type the rules say nothing about.
    enum class Kind : uint8_t { Unchecked, Integer, Float, Text, Struct };

    Kind kindOf(const Type& type) {
      switch (type.kind) {
        case TypeKind::Int: case TypeKind::UInt:
          return Kind::Integer;
        case TypeKind::Float:
          return Kind::Float;
        case TypeKind::String:
          return Kind::Text;
        case TypeKind::Struct:
          return Kind::Struct;
        default:
          return Kind::Unchecked;
      }
    }

    bool sameKind(const Type& a, const Type& b) {
      const Kind kind = kindOf(a);

      return kind == kindOf(b) && (kind != Kind::Struct || a.name == b.name);
    }

    String kindText(const Type& type) {
      switch (kindOf(type)) {
        case Kind::Integer:
          return "an integer";
        case Kind::Float:
          return "a float";
        case Kind::Text:
          return "a string";
        default:
          return "a " + type.name;
      }
    }

    bool isContainer(const Type& type) {
      switch (type.kind) {
        case TypeKind::FixedArray: case TypeKind::Buffer: case TypeKind::DynamicArray: case TypeKind::Sparse:
        case TypeKind::Hashmap: case TypeKind::Set: case TypeKind::Grid: case TypeKind::Tensor:
          return true;
        default:
          return false;
      }
    }

    // A container counts only against a kind that is checked: then it is named by its type.
    bool comparable(const Type& type, const Type& other) {
      return kindOf(type) != Kind::Unchecked || (isContainer(type) && kindOf(other) != Kind::Unchecked);
    }

    String namedText(const TypePtr& type) {
      return isContainer(*type) ? "a " + TypeText(type) : kindText(*type);
    }

  }  // namespace

  // "expected an integer but found a string", when both types are of kinds the rules compare and the kinds differ.
  Optional<String> Mismatch(const TypePtr& expected, const TypePtr& found) {
    if (!comparable(*expected, *found) || !comparable(*found, *expected) || sameKind(*expected, *found)) {
      return nullopt;
    }

    return "expected " + namedText(expected) + " but found " + namedText(found);
  }

  // The value itself when it fits the type that was asked for; otherwise a problem node saying what was expected and what was found.
  ExprPtr Expecting(const TypePtr& expected, ExprPtr value) {
    if (const Optional<String> why = Mismatch(expected, value->type)) {
      return Unsupported(*why, value->at);
    }

    return value;
  }

  // A type parameter stands for every type while its generic is checked once for all of them (Parametric.cpp). An operator or a cast
  // needs a number or a value it can compare, so it does not work on every type.
  ExprPtr AnyTypeProblem(const ExprPtr& operand, const String& what, Span at) {
    if (!operand->type || operand->type->kind != TypeKind::Param) {
      return nullptr;
    }

    return Unsupported(operand->type->name + " can be any type, and " + what + " does not work on every type", at);
  }

  // Two operands of one operator must have the same kind: the problem node (at the right operand), or null.
  ExprPtr OperandsProblem(const ExprPtr& left, const ExprPtr& right) {
    if (const Optional<String> why = Mismatch(left->type, right->type)) {
      return Unsupported(*why, right->at);
    }

    return nullptr;
  }

}  // namespace polyir::front
