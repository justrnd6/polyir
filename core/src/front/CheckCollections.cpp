// Lists, maps and sets come in two kinds (CheckCollections.nina). `array<T>`, `map<K,V>` and `set<T>` cannot be changed once made: `with` gives a new one
// with the change, and `thaw()` makes a changeable copy. `mutarray<T>`, `mutmap<K,V>` and `mutset<T>` change in place (`push`, `add`, `m[k] = v`) and
// `freeze()` makes an unchangeable copy; `remove` takes an entry out of a mutmap or mutset. This file knows which operations each kind allows and the
// types they give.
#include "front/Checker.hpp"

namespace polyir::front {

  namespace {

    String cannotChange(const String& what, const String& instead, const String& copy) {
      return what + " cannot be changed once it is made: use " + instead + " to get a changed copy, or " + copy + " for one you can change in place";
    }

    String inPlace(const String& kind, const String& instead) {
      return "with() makes a changed copy of an array, map or set; a " + kind + " changes in place: use " + instead;
    }

    // `push` needs a mutarray; `with` is for an array.
    Optional<String> listProblem(StringView name, bool isMutable) {
      if (name == "push" && !isMutable) {
        return cannotChange("an array", "with(x)", "thaw()");
      }

      if (name == "with" && isMutable) {
        return inPlace("mutarray", "push(x)");
      }

      return nullopt;
    }

    // A hash table's size is the host's own (a native table, a Java HashMap, a Python dict). So it is not the same number on every target;
    // only its entries are.
    String noCapacity(const char* kind, const char* name) {
      return String("a ") + kind + " has no capacity() that every target agrees on (each stores its table its own way): use len(" + name +
        ") for the number of entries";
    }

    // `remove` takes an entry out in place, so it needs the kind that changes.
    String cannotRemove(const String& what, const String& mutKind) {
      return what + " cannot be changed once it is made: thaw() it into a " + mutKind + " to remove from it";
    }

    Optional<String> mapProblem(StringView name, bool isMutable) {
      if (name == "capacity") {
        return noCapacity("map", "m");
      }

      if (name == "remove" && !isMutable) {
        return cannotRemove("a map", "mutmap");
      }

      if (name == "with" && isMutable) {
        return inPlace("mutmap", "m[k] = v");
      }

      return nullopt;
    }

    Optional<String> setProblem(StringView name, bool isMutable) {
      if (name == "capacity") {
        return noCapacity("set", "s");
      }

      if (name == "add" && !isMutable) {
        return cannotChange("a set", "with(x)", "thaw()");
      }

      if (name == "remove" && !isMutable) {
        return cannotRemove("a set", "mutset");
      }

      if (name == "with" && isMutable) {
        return inPlace("mutset", "add(x)");
      }

      return nullopt;
    }

    Optional<String> writeProblem(const String& what, const String& mutKind, bool isMutable) {
      if (isMutable) {
        return nullopt;
      }

      return what + " cannot be changed once it is made: use with(...) to get a changed copy, or thaw() into a " + mutKind + " to change in place";
    }

    bool isCollection(const TypePtr& type) {
      return type->kind == TypeKind::DynamicArray || type->kind == TypeKind::Hashmap || type->kind == TypeKind::Set;
    }

    // The same collection, changeable or not as `isMutable` says.
    TypePtr withMutability(const TypePtr& type, bool isMutable) {
      if (type->kind == TypeKind::DynamicArray) {
        return MakeDynamicArray(type->element, isMutable);
      }

      if (type->kind == TypeKind::Set) {
        return MakeSet(type->element, isMutable);
      }

      return MakeHashmap(type->element, type->second, isMutable);
    }

    // What `freeze()` (isMutable false) or `thaw()` (true) gives: the collection of the other kind; nothing when it already is that kind.
    Optional<TypePtr> otherKind(const TypePtr& type, bool isMutable) {
      if (!isCollection(type) || type->isMutable == isMutable) {
        return nullopt;
      }

      return withMutability(type, isMutable);
    }

    // What `with(...)` gives: a new collection of the same type, for the kinds that cannot change.
    Optional<TypePtr> whenImmutable(const TypePtr& type) {
      if (!isCollection(type) || type->isMutable) {
        return nullopt;
      }

      return type;
    }

    Optional<TypePtr> hasResult(const TypePtr& type) {
      if (type->kind == TypeKind::Hashmap || type->kind == TypeKind::Set) {
        return IntType(1);
      }

      return nullopt;
    }

    Optional<TypePtr> addResult(const TypePtr& type) {
      if (type->kind == TypeKind::Set && type->isMutable) {
        return type;
      }

      return nullopt;
    }

    // `m.remove(k)` and `s.remove(x)` say whether the entry was there.
    Optional<TypePtr> removeResult(const TypePtr& type) {
      if ((type->kind == TypeKind::Hashmap || type->kind == TypeKind::Set) && type->isMutable) {
        return IntType(1);
      }

      return nullopt;
    }

  }  // namespace

  // Why `recv.name(args)` is not allowed, or nothing.
  Optional<String> CollectionProblem(StringView name, const TypePtr& receiver) {
    switch (receiver->kind) {
      case TypeKind::DynamicArray:
        return listProblem(name, receiver->isMutable);
      case TypeKind::Hashmap:
        return mapProblem(name, receiver->isMutable);
      case TypeKind::Set:
        return setProblem(name, receiver->isMutable);
      default:
        return nullopt;
    }
  }

  // Why `base[i] = v` is not allowed, or nothing.
  Optional<String> AssignProblem(const TypePtr& base) {
    if (base->kind == TypeKind::DynamicArray) {
      return writeProblem("an array", "mutarray", base->isMutable);
    }

    if (base->kind == TypeKind::Hashmap) {
      return writeProblem("a map", "mutmap", base->isMutable);
    }

    return nullopt;
  }

  namespace {

    // `has`, `add` and `remove`: what a map or set says, or the set itself.
    Optional<TypePtr> entryResult(StringView name, const TypePtr& receiver) {
      if (name == "has") {
        return hasResult(receiver);
      }

      return name == "add" ? addResult(receiver) : removeResult(receiver);
    }

  }  // namespace

  // The result type of the collection operations, or nothing when `name` is not one of them for this receiver.
  Optional<TypePtr> CollectionResult(StringView name, const TypePtr& receiver) {
    if (name == "with") {
      return whenImmutable(receiver);
    }

    if (name == "has" || name == "add" || name == "remove") {
      return entryResult(name, receiver);
    }

    if (name == "freeze") {
      return otherKind(receiver, false);
    }

    if (name == "thaw") {
      return otherKind(receiver, true);
    }

    return nullopt;
  }

  namespace {

    Optional<String> partsKeyProblem(Checker& check, const TypePtr& type, Vector<TypePtr> parts);

    Optional<String> mutableKeyMessage(const TypePtr& type) {
      return "a " + TypeText(*type) + " cannot be a map or set key because it can change: freeze() it first";
    }

    bool hasVarField(const StructInfo& info) {
      for (bool isMutable : info.fieldMutable) {
        if (isMutable) {
          return true;
        }
      }

      return false;
    }

    // Why a struct cannot be a key, or nothing. A struct is a key when none of its fields can change and each field's type is a key.
    Optional<String> structKeyProblem(Checker& check, const TypePtr& type) {
      const StructInfo* info = FindStruct(DeclsOf(check), SymbolOf(check, type->name));

      if (info == nullptr) {
        return nullopt;
      }

      if (hasVarField(*info)) {
        return "struct " + type->name + " has var fields, so it can change and cannot be a map or set key";
      }

      if (IsInterface(DeclsOf(check), info->name)) {
        return String("an interface value cannot be a map or set key");
      }

      return partsKeyProblem(check, type, info->fieldTypes);
    }

    // Why a value cannot be a key, or nothing. A collection that can change is no key; any other value is a key when each of its parts is.
    Optional<String> partsKeyProblem(Checker& check, const TypePtr& type, Vector<TypePtr> parts) {
      const bool isContainer = type->kind == TypeKind::DynamicArray || type->kind == TypeKind::Set || type->kind == TypeKind::Hashmap;

      if (isContainer && type->isMutable) {
        return mutableKeyMessage(type);
      }

      for (const TypePtr& part : parts) {
        if (Optional<String> why = KeyProblem(check, part)) {
          return why;
        }
      }

      return nullopt;
    }

    // The types a container or sum type holds.
    Vector<TypePtr> partsOf(const TypePtr& type) {
      if (type->kind == TypeKind::Hashmap || type->kind == TypeKind::Either) {
        return {type->element, type->second};
      }

      if (type->kind == TypeKind::Variant) {
        return type->parameters;
      }

      return {type->element};
    }

    // Why a container or sum type cannot be a key, or nothing: it is a key when everything it holds is.
    Optional<String> compositeKeyProblem(Checker& check, const TypePtr& type) {
      switch (type->kind) {
        case TypeKind::DynamicArray: case TypeKind::Set: case TypeKind::Hashmap: case TypeKind::Either: case TypeKind::Optional: case TypeKind::Variant:
          return partsKeyProblem(check, type, partsOf(type));
        default:
          return "a " + TypeText(*type) + " cannot be a map or set key: use a number, a string, or an immutable struct or collection of those";
      }
    }

  }  // namespace

  // Why a map or set cannot use this type as its key, or nothing. A key must keep the same hash for as long as it is in the table.
  // So it must be a number, a string, or an immutable value made only of those.
  Optional<String> KeyProblem(Checker& check, const TypePtr& type) {
    switch (type->kind) {
      case TypeKind::Int: case TypeKind::UInt: case TypeKind::String:
        return nullopt;
      case TypeKind::Float:
        return String("a float cannot be a map or set key: use an integer or a string");
      case TypeKind::Struct:
        return structKeyProblem(check, type);
      default:
        return compositeKeyProblem(check, type);
    }
  }

}  // namespace polyir::front
