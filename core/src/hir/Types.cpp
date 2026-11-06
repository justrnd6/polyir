// polyir's types (polyir/Types.hpp): the Make* functions that build each kind, `SameType` (structural equality) and
// `TypeText`, the type as polyir source spells it (`i32`, `array<f64>`, `func(i32)->void`). A Type is one record for
// every kind; which of its fields matter depends on `kind` (an element type, a size, parameters, ...).
#include "polyir/Std.hpp"
#include "polyir/Types.hpp"

#include <utility>

namespace polyir {

  namespace {

    SharedPtr<Type> makeType(TypeKind kind, int bits) {
      SharedPtr<Type> made = make_shared<Type>();
      made->kind = kind;
      made->bits = bits;

      return made;
    }

    TypePtr callable(TypeKind kind, Vector<TypePtr> parameters, TypePtr result) {
      SharedPtr<Type> made = makeType(kind, 0);
      made->parameters = move(parameters);
      made->element = move(result);

      return made;
    }

    // A type that only wraps an element type (a pointer, an optional, ...).
    TypePtr elementOf(TypeKind kind, TypePtr element) {
      SharedPtr<Type> made = makeType(kind, 0);
      made->element = move(element);

      return made;
    }

    // A type with an element type and a size (a vec, a matrix, a fixed array, a buffer).
    TypePtr sized(TypeKind kind, TypePtr element, int size) {
      SharedPtr<Type> made = makeType(kind, 0);
      made->element = move(element);
      made->size = size;

      return made;
    }

    // A growable collection of elements that may or may not be mutable (an array or a set).
    TypePtr collectionOf(TypeKind kind, TypePtr element, bool isMutable) {
      SharedPtr<Type> made = makeType(kind, 0);
      made->element = move(element);
      made->isMutable = isMutable;

      return made;
    }

    String joinTypes(const Vector<TypePtr>& types) {
      String out;

      for (size_t i = 0; i < types.size(); i++) {
        out += (i == 0 ? "" : ",") + TypeText(types[i]);
      }

      return out;
    }

    String joinInts(const Vector<int>& values) {
      String out;

      for (const int v : values) {
        out += "," + to_string(v);
      }

      return out;
    }

    String generic(const String& name, const String& parts) {
      return name + "<" + parts + ">";
    }

    String withMutPrefix(const String& name, bool mutability) {
      return mutability ? "mut" + name : name;
    }

    bool sameTypes(const Vector<TypePtr>& a, const Vector<TypePtr>& b) {
      for (size_t i = 0; i < a.size() && i < b.size(); i++) {
        if (!SameType(a[i], b[i])) {
          return false;
        }
      }

      return a.size() == b.size();
    }

    // The kinds of type that carry a size: `vec<f32,4>`, `mat<f32,4>`, `i32[8]`, `buffer<u8,64>`. `elementText` is the
    // element type already written out.
    String sizedText(const Type& t, const String& elementText) {
      switch (t.kind) {
        case TypeKind::Vec:
          return generic("vec", elementText + "," + to_string(t.size));
        case TypeKind::Matrix:
          return generic("mat", elementText + "," + to_string(t.size));
        case TypeKind::FixedArray:
          return elementText + "[" + to_string(t.size) + "]";
        case TypeKind::Buffer:
          return generic("buffer", elementText + "," + to_string(t.size));
        default:
          return "?";
      }
    }

    String sequenceText(const Type& t, const String& elementText) {
      switch (t.kind) {
        case TypeKind::DynamicArray:
          return generic(withMutPrefix("array", t.isMutable), elementText);
        case TypeKind::Set:
          return generic(withMutPrefix("set", t.isMutable), elementText);
        case TypeKind::Sparse:
          return generic("sparse", elementText);
        default:
          return sizedText(t, elementText);
      }
    }

    String containerText(const Type& t) {
      const String elementText = TypeText(t.element);

      switch (t.kind) {
        case TypeKind::Hashmap:
          return generic(withMutPrefix("map", t.isMutable), elementText + "," + TypeText(t.second));
        case TypeKind::Grid:
          return generic("grid", elementText + "," + to_string(t.size) + "," + to_string(t.cols));
        case TypeKind::Tensor:
          return generic("tensor", elementText + joinInts(t.dims));
        default:
          return sequenceText(t, elementText);
      }
    }

    String wrappedText(const Type& t) {
      switch (t.kind) {
        case TypeKind::Either:
          return generic("either", TypeText(t.element) + "," + TypeText(t.second));
        case TypeKind::Optional:
          return generic("optional", TypeText(t.element));
        case TypeKind::Coroutine:
          return generic("coro", TypeText(t.element));
        default:
          return containerText(t);
      }
    }

    String compoundText(const Type& t) {
      switch (t.kind) {
        case TypeKind::Pointer:
          return "ptr<" + TypeText(t.element) + ">";
        case TypeKind::Function:
          return "func(" + joinTypes(t.parameters) + ")->" + TypeText(t.element);
        case TypeKind::Closure:
          return "closure(" + joinTypes(t.parameters) + ")->" + TypeText(t.element);
        case TypeKind::Variant:
          return generic("variant", joinTypes(t.parameters));
        default:
          return wrappedText(t);
      }
    }

    // TypeText is split by family (plain, compound, wrapped, container, sequence, sized) so each switch stays short.
    // Each one hands the kinds it does not know to the next.
    String plainText(const Type& t) {
      switch (t.kind) {
        case TypeKind::String:
          return "string";
        case TypeKind::Void:
          return "void";
        case TypeKind::Struct:
        case TypeKind::Param:
          return t.name;
        default:
          return compoundText(t);
      }
    }

  }  // namespace

  TypePtr MakeInt(int bits) {
    return makeType(TypeKind::Int, bits);
  }

  TypePtr MakeUInt(int bits) {
    return makeType(TypeKind::UInt, bits);
  }

  TypePtr MakeFloat(int bits) {
    return makeType(TypeKind::Float, bits);
  }

  TypePtr MakeBool() {
    return MakeInt(1);
  }

  TypePtr MakeString() {
    return makeType(TypeKind::String, 0);
  }

  TypePtr MakeVoid() {
    return makeType(TypeKind::Void, 0);
  }

  TypePtr MakeStruct(const String& name) {
    SharedPtr<Type> made = makeType(TypeKind::Struct, 0);
    made->name = name;

    return made;
  }

  TypePtr MakeParam(const String& name) {
    SharedPtr<Type> made = makeType(TypeKind::Param, 0);
    made->name = name;

    return made;
  }

  TypePtr MakePointer(TypePtr element) {
    return elementOf(TypeKind::Pointer, move(element));
  }

  TypePtr MakeFunction(Vector<TypePtr> parameters, TypePtr result) {
    return callable(TypeKind::Function, move(parameters), move(result));
  }

  TypePtr MakeClosure(Vector<TypePtr> parameters, TypePtr result) {
    return callable(TypeKind::Closure, move(parameters), move(result));
  }

  TypePtr MakeVec(TypePtr element, int count) {
    return sized(TypeKind::Vec, move(element), count);
  }

  TypePtr MakeMatrix(TypePtr element, int size) {
    return sized(TypeKind::Matrix, move(element), size);
  }

  TypePtr MakeFixedArray(TypePtr element, int size) {
    return sized(TypeKind::FixedArray, move(element), size);
  }

  TypePtr MakeBuffer(TypePtr element, int size) {
    return sized(TypeKind::Buffer, move(element), size);
  }

  TypePtr MakeSparse(TypePtr element) {
    return elementOf(TypeKind::Sparse, move(element));
  }

  TypePtr MakeOptional(TypePtr element) {
    return elementOf(TypeKind::Optional, move(element));
  }

  TypePtr MakeCoroutine(TypePtr yields) {
    return elementOf(TypeKind::Coroutine, move(yields));
  }

  TypePtr MakeDynamicArray(TypePtr element, bool mutability) {
    return collectionOf(TypeKind::DynamicArray, move(element), mutability);
  }

  TypePtr MakeSet(TypePtr element, bool mutability) {
    return collectionOf(TypeKind::Set, move(element), mutability);
  }

  TypePtr MakeHashmap(TypePtr key, TypePtr value, bool mutability) {
    SharedPtr<Type> made = makeType(TypeKind::Hashmap, 0);
    made->element = move(key);
    made->second = move(value);
    made->isMutable = mutability;

    return made;
  }

  TypePtr MakeGrid(TypePtr element, int rows, int cols) {
    SharedPtr<Type> made = makeType(TypeKind::Grid, 0);
    made->element = move(element);
    made->size = rows;
    made->cols = cols;

    return made;
  }

  TypePtr MakeTensor(TypePtr element, Vector<int> dims) {
    SharedPtr<Type> made = makeType(TypeKind::Tensor, 0);
    made->element = move(element);
    made->dims = move(dims);

    return made;
  }

  TypePtr MakeVariant(Vector<TypePtr> cases) {
    SharedPtr<Type> made = makeType(TypeKind::Variant, 0);
    made->parameters = move(cases);

    return made;
  }

  TypePtr MakeEither(TypePtr ok, TypePtr err) {
    SharedPtr<Type> made = makeType(TypeKind::Either, 0);
    made->element = move(ok);
    made->second = move(err);

    return made;
  }

  // Compares every field whatever the kind: the fields a kind does not use stay at their defaults, so they compare equal.
  bool SameType(const Type& a, const Type& b) {
    if (a.kind != b.kind || a.bits != b.bits || a.name != b.name || a.isMutable != b.isMutable) {
      return false;
    }

    if (a.size != b.size || a.cols != b.cols || a.dims != b.dims) {
      return false;
    }

    return sameTypes(a.parameters, b.parameters) && SameType(a.element, b.element) && SameType(a.second, b.second);
  }

  bool SameType(const TypePtr& a, const TypePtr& b) {
    if (a == b) {
      return true;
    }

    return a && b && SameType(*a, *b);
  }

  String TypeText(const Type& t) {
    switch (t.kind) {
      case TypeKind::Int:
        return "i" + to_string(t.bits);
      case TypeKind::UInt:
        return "u" + to_string(t.bits);
      case TypeKind::Float:
        return "f" + to_string(t.bits);
      default:
        return plainText(t);
    }
  }

  String TypeText(const TypePtr& t) {
    return t ? TypeText(*t) : "?";
  }

}  // namespace polyir
