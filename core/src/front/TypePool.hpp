// The types the checker makes. The scalar types and `string`/`void` are shared by the whole process, and a struct type is made once per name and
// shared, so two types of the same scalar or struct are the same object (`SameType` answers on the pointer first, so equality is mostly a pointer
// compare). Derived types (`ptr<T>`, `array<T>`, ...) are made where they are written and compared structurally.
#pragma once

#include "front/Symbols.hpp"
#include "polyir/Types.hpp"

namespace polyir::front {

  // Names to types for one check: `i64`, `char`, `string`, `double` ... are the shared scalars; any other name is a struct (made on first use).
  struct TypePool {
    Interner& names;
    SymbolMap<TypePtr> byName{};
  };

  const TypePtr& IntType(int bits);    // i1 (the flag type), i8, i16, i32, i64

  const TypePtr& UintType(int bits);   // u8, u16, u32, u64

  const TypePtr& FloatType(int bits);  // f32, f64

  const TypePtr& StringType();

  const TypePtr& VoidType();

  TypePool MakeTypePool(Interner& names);  // the shared scalars already in it

  const TypePtr& Named(TypePool& pool, Symbol name);

  const TypePtr& StructOf(TypePool& pool, Symbol name);

  // The struct type of a name (the same lookup as Named, for a caller that knows the name is a struct's).
  inline const TypePtr& StructOf(TypePool& pool, Symbol name) {
    return Named(pool, name);
  }

}  // namespace polyir::front
