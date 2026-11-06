// Name lookup for the checker (Checker.hpp): a name in a function body means the newest local binding of it, or else a global.
#include "front/Checker.hpp"

namespace polyir::front {

  // The scope (newest binding first), then the globals; null when the name means nothing here.
  const Binding* Lookup(const Checker& check, Symbol name) {
    if (const Binding* local = Lookup(check.scope, name)) {
      return local;
    }

    return Find(check.session.decls.globals, name);
  }

}  // namespace polyir::front
