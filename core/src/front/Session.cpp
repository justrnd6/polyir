// Small queries on a Session's tables (Session.hpp): making a session, finding a local in a scope, a field in a struct, a method in an
// interface, and the mangled name `Owner_method` a struct method is compiled under.
#include "front/Session.hpp"

namespace polyir::front {

  // A session with no units yet; files[0] is the empty name of "no file".
  Session MakeSession(Interner& interner) {
    Session session{interner, MakeTypePool(interner)};
    session.files.resize(1);

    return session;
  }

  // The newest binding of `name`, so an inner name hides an outer one; null when the name is not in scope.
  const Binding* Lookup(const Scope& scope, Symbol name) {
    for (size_t i = scope.bindings.size(); i > 0; i--) {
      if (scope.bindings[i - 1].name == name) {
        return &scope.bindings[i - 1];
      }
    }

    return nullptr;
  }

  int IndexOf(const StructInfo& info, Symbol field) {
    for (size_t i = 0; i < info.fieldNames.size(); i++) {
      if (info.fieldNames[i] == field) {
        return static_cast<int>(i);
      }
    }

    return -1;
  }

  int IndexOf(const Instance& instance, Symbol param) {
    for (size_t i = 0; i < instance.params.size(); i++) {
      if (instance.params[i] == param) {
        return static_cast<int>(i);
      }
    }

    return -1;
  }

  int SlotOf(const InterfaceInfo& interface, Symbol method) {
    for (size_t i = 0; i < interface.methodNames.size(); i++) {
      if (interface.methodNames[i] == method) {
        return static_cast<int>(i);
      }
    }

    return -1;
  }

  bool IsImplementedBy(const InterfaceInfo& interface, Symbol structName) {
    return IsOneOf(structName, interface.implementers);
  }

  // The two 32-bit symbol ids side by side make one 64-bit key for the pair.
  Symbol Mangled(Session& session, Symbol owner, Symbol method) {
    const uint64_t key = (static_cast<uint64_t>(owner.id) << 32) | method.id;

    if (session.mangledNames.count(key) != 0) {
      return session.mangledNames.at(key);
    }

    const Symbol made = Intern(session.names, String(TextOf(session, owner)) + "_" + String(TextOf(session, method)));
    session.mangledNames.emplace(key, made);

    return made;
  }

}  // namespace polyir::front
