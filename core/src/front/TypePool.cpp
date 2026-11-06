// The types the checker hands out (TypePool.hpp). The scalar types (`i32`, `double`, `string`, ...) are made once per process and
// shared; a struct type is made the first time its name is asked for, so two uses of one name get the very same type object.
#include "front/TypePool.hpp"

namespace polyir::front {

  namespace {

    constexpr int kMaxBits = 129;  // one slot per width up to 128 bits

    // The shared scalar types; a width's slot is filled the first time that width is asked for.
    struct Scalars {
      TypePtr ints[kMaxBits];
      TypePtr uints[kMaxBits];
      TypePtr floats[kMaxBits];
      TypePtr stringType = MakeString();
      TypePtr voidType = MakeVoid();
    };

    Scalars& scalars() {
      static Scalars instance;

      return instance;
    }

    void addScalar(TypePool& pool, const char* spelling, const TypePtr& type) {
      Insert(pool.byName, Intern(pool.names, spelling), type);
    }

    // `i1` … `i64` and `u8` … `u64`.
    void addSizedIntegers(TypePool& pool) {
      for (const int bits : {1, 8, 16, 32, 64}) {
        addScalar(pool, ("i" + to_string(bits)).c_str(), IntType(bits));
      }

      for (const int bits : {8, 16, 32, 64}) {
        addScalar(pool, ("u" + to_string(bits)).c_str(), UintType(bits));
      }
    }

    // The scalars with a name of their own.
    void addNamedScalars(TypePool& pool) {
      addScalar(pool, "bool", IntType(1));
      addScalar(pool, "char", IntType(8));
      addScalar(pool, "float", FloatType(32));
      addScalar(pool, "double", FloatType(64));

      addScalar(pool, "string", StringType());
      addScalar(pool, "void", VoidType());
    }

  }  // namespace

  const TypePtr& IntType(int bits) {
    TypePtr& slot = scalars().ints[bits];

    if (!slot) {
      slot = MakeInt(bits);
    }

    return slot;
  }

  const TypePtr& UintType(int bits) {
    TypePtr& slot = scalars().uints[bits];

    if (!slot) {
      slot = MakeUInt(bits);
    }

    return slot;
  }

  const TypePtr& FloatType(int bits) {
    TypePtr& slot = scalars().floats[bits];

    if (!slot) {
      slot = MakeFloat(bits);
    }

    return slot;
  }

  const TypePtr& StringType() {
    return scalars().stringType;
  }

  const TypePtr& VoidType() {
    return scalars().voidType;
  }

  TypePool MakeTypePool(Interner& names) {
    TypePool pool{names};
    addSizedIntegers(pool);
    addNamedScalars(pool);

    return pool;
  }

  // A name that is no scalar is a struct, made the first time it is asked for.
  const TypePtr& Named(TypePool& pool, Symbol name) {
    if (const TypePtr* found = Find(pool.byName, name)) {
      return *found;
    }

    Insert(pool.byName, name, MakeStruct(String(TextOf(pool.names, name))));

    return *Find(pool.byName, name);
  }

}  // namespace polyir::front
