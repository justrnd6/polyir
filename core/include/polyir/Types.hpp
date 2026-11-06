// polyir's types: the numbers, strings, structs, pointers and function types a typed HIR is made of.
#pragma once

#include "polyir/Std.hpp"
#include "polyir/Export.hpp"

#include <memory>
#include <string>
#include <vector>

namespace polyir {

  enum class TypeKind {
    Int, UInt, Float, String, Void, Struct, Pointer, Function,
    Vec, FixedArray, Buffer, DynamicArray, Sparse, Hashmap, Set, Grid, Tensor, Closure, Variant, Either, Optional, Coroutine,
    Matrix,  // a square matrix of floats (`mat<f32,4>`): a GPU kernel type, multiplied by a vec or another matrix
    Param    // a generic's type parameter (`T`, its `name`): only in a generic as declared (Module::genericStructs, genericFunctions), never in code
  };

  struct Type;
  using TypePtr = SharedPtr<const Type>;

  // A type, made by one of the `Make*` functions below and never changed after. `i1` is the flag type (a bool); a struct is named, and a value
  // of it is a reference to shared fields. What `element` and `second` hold depends on the kind: the pointee, the result of a function type, a
  // container's element, a map's key, an either's `ok`, an optional's value, a coroutine's yield; `second` is a map's value or an either's `err`.
  // `parameters` are those of a function or closure type, or the cases of a variant. `size` is a vec's count, a matrix's side, a fixed array's or
  // buffer's length, or a grid's rows (`cols` its columns); `dims` a tensor's.
  struct POLYIR_API Type {
    TypeKind kind = TypeKind::Void;
    int bits = 0;
    String name;
    Vector<TypePtr> parameters;
    TypePtr element;
    TypePtr second;
    bool isMutable = false;  // true for `mutarray`, `mutmap` and `mutset`
    int size = 0;
    int cols = 0;
    Vector<int> dims;
  };

  POLYIR_API TypePtr MakeInt(int bits);

  POLYIR_API TypePtr MakeUInt(int bits);

  POLYIR_API TypePtr MakeFloat(int bits);

  POLYIR_API TypePtr MakeBool();

  POLYIR_API TypePtr MakeString();

  POLYIR_API TypePtr MakeVoid();

  POLYIR_API TypePtr MakeStruct(const String& name);

  POLYIR_API TypePtr MakeParam(const String& name);  // a type parameter, `T`

  POLYIR_API TypePtr MakePointer(TypePtr element);

  POLYIR_API TypePtr MakeFunction(Vector<TypePtr> parameters, TypePtr result);

  POLYIR_API TypePtr MakeVec(TypePtr element, int count);

  POLYIR_API TypePtr MakeMatrix(TypePtr element, int size);  // size x size

  POLYIR_API TypePtr MakeFixedArray(TypePtr element, int size);

  POLYIR_API TypePtr MakeBuffer(TypePtr element, int size);

  POLYIR_API TypePtr MakeDynamicArray(TypePtr element, bool mutability);

  POLYIR_API TypePtr MakeSparse(TypePtr element);

  POLYIR_API TypePtr MakeHashmap(TypePtr key, TypePtr value, bool mutability);

  POLYIR_API TypePtr MakeSet(TypePtr element, bool mutability);

  POLYIR_API TypePtr MakeGrid(TypePtr element, int rows, int cols);

  POLYIR_API TypePtr MakeTensor(TypePtr element, Vector<int> dims);

  POLYIR_API TypePtr MakeClosure(Vector<TypePtr> parameters, TypePtr result);

  POLYIR_API TypePtr MakeVariant(Vector<TypePtr> cases);

  POLYIR_API TypePtr MakeEither(TypePtr ok, TypePtr err);

  POLYIR_API TypePtr MakeOptional(TypePtr element);

  POLYIR_API TypePtr MakeCoroutine(TypePtr yields);

  POLYIR_API bool SameType(const Type& a, const Type& b);

  POLYIR_API bool SameType(const TypePtr& a, const TypePtr& b);  // two nulls are the same type

  POLYIR_API String TypeText(const Type& t);  // `i64`, `u8`, `f64`, `string`, `Pt`, `ptr<i64>`, `func(i64,i64)->i64`, `array<i64>`, `map<string,i64>`

  POLYIR_API String TypeText(const TypePtr& t);  // `?` for null

  inline bool IsInteger(const Type& t) {
    return t.kind == TypeKind::Int || t.kind == TypeKind::UInt;
  }

  inline bool IsSigned(const Type& t) {
    return t.kind == TypeKind::Int;
  }

  inline bool IsFloat(const Type& t) {
    return t.kind == TypeKind::Float;
  }

  inline bool IsBoolean(const Type& t) {
    return t.kind == TypeKind::Int && t.bits == 1;
  }

  inline bool IsNumber(const Type& t) {
    return IsInteger(t) || IsFloat(t);
  }

  inline bool IsVoid(const Type& t) {
    return t.kind == TypeKind::Void;
  }

  inline bool IsCallable(const Type& t) {
    return t.kind == TypeKind::Function || t.kind == TypeKind::Closure;
  }

}  // namespace polyir
