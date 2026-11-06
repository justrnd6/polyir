// Import paths and duplicate checks (lang/src/Imports.nina): `NormalizePath`, and `CheckDuplicates`, which runs on the merged program once its
// imports are in (ImportMerge.cpp reads and merges the imported files). Two files may both declare the same extern or extern type; the copies
// are dropped here, so the checker sees each declaration once.
#include "front/Pipeline.hpp"

namespace polyir::front {

  namespace {

    // A path part, unless it is empty or `.` (which name the same directory).
    void addPart(Vector<String>& parts, String part) {
      if (!part.empty() && part != ".") {
        parts.push_back(move(part));
      }
    }

    // The parts between slashes, without empty ones and `.`.
    Vector<String> pathParts(const String& path) {
      Vector<String> parts;

      for (size_t from = 0; from <= path.size();) {
        const size_t at = min(path.find('/', from), path.size());
        addPart(parts, path.substr(from, at - from));
        from = at + 1;
      }

      return parts;
    }

    // `dir/..` cancels; a `..` with nothing before it stays.
    Vector<String> foldDots(const Vector<String>& parts) {
      Vector<String> folded;

      for (const String& part : parts) {
        if (part == ".." && !folded.empty() && folded.back() != "..") {
          folded.pop_back();
        } else {
          folded.push_back(part);
        }
      }

      return folded;
    }

  }  // namespace

  // Lexical normalization, as `Path.normalize`: drops `.` and empty parts and folds `dir/..`; a leading `..` stays.
  String NormalizePath(const String& path) {
    String out = !path.empty() && path[0] == '/' ? "/" : "";
    const Vector<String> parts = foldDots(pathParts(path));

    for (size_t i = 0; i < parts.size(); i++) {
      out += (i == 0 ? "" : "/") + parts[i];
    }

    return out;
  }

  namespace {

    // One top-level item: the program it is in, and its index in that program's `items`.
    struct ItemRef {
      const Program* program;
      uint32_t index;
    };

    const AstItem& itemOf(const ItemRef& ref) {
      return ref.program->items[ref.index];
    }

    // Every item of the units, in order.
    Vector<ItemRef> itemsOf(const Vector<Unit>& units) {
      Vector<ItemRef> items;

      for (const Unit& unit : units) {
        for (uint32_t i = unit.first; i < unit.last; i++) {
          items.push_back(ItemRef{unit.program, i});
        }
      }

      return items;
    }

    // The names of structs and interfaces (they share one namespace), in source order.
    Vector<Symbol> structNames(const Vector<Unit>& units) {
      Vector<Symbol> out;

      for (const ItemRef& ref : itemsOf(units)) {
        const AstItem& item = itemOf(ref);

        if (item.kind == ItemKind::Struct) {
          out.push_back(ref.program->structs[item.index].name.name);
        }

        if (item.kind == ItemKind::Interface) {
          out.push_back(ref.program->interfaces[item.index].name.name);
        }
      }

      return out;
    }

    // The names of funcs (not coroutines).
    Vector<Symbol> funcNames(const Vector<Unit>& units) {
      Vector<Symbol> out;

      for (const ItemRef& ref : itemsOf(units)) {
        if (itemOf(ref).kind == ItemKind::Func) {
          out.push_back(ref.program->funcs.items[itemOf(ref).index].name.name);
        }
      }

      return out;
    }

    Vector<Symbol> externNames(const Vector<Unit>& units) {
      Vector<Symbol> out;

      for (const ItemRef& ref : itemsOf(units)) {
        if (itemOf(ref).kind == ItemKind::ExternFunc) {
          out.push_back(ref.program->externFuncs[itemOf(ref).index].name.name);
        }
      }

      return out;
    }

    // Every (i, j) pair sharing a name -- a name repeated 3+ times prints more than one line, as in the Nina checker.
    void duplicatesIn(const Interner& names, const Vector<Symbol>& list, const char* kind, Vector<String>& out) {
      for (size_t i = 0; i < list.size(); i++) {
        for (size_t j = i + 1; j < list.size(); j++) {
          if (list[i] == list[j]) {
            out.push_back(String("duplicate ") + kind + " '" + String(TextOf(names, list[i])) + "'");
          }
        }
      }
    }

    // A name that is both a func and an extern func.
    void funcExternClashes(const Interner& names, const Vector<Symbol>& funcs, const Vector<Symbol>& externs, Vector<String>& out) {
      for (const Symbol funcName : funcs) {
        for (const Symbol externName : externs) {
          if (externName == funcName) {
            out.push_back("'" + String(TextOf(names, funcName)) + "' is both a func and an extern func");
          }
        }
      }
    }

    // Two structs sharing a name are always an error (and nothing else is checked then). So are two funcs, or a func and an extern, sharing one.
    Vector<String> nameClashes(const Vector<Unit>& units) {
      Vector<String> out;
      const Interner& names = *units[0].program->names;
      duplicatesIn(names, structNames(units), "struct", out);

      if (!out.empty()) {
        return out;
      }

      duplicatesIn(names, funcNames(units), "func", out);
      funcExternClashes(names, funcNames(units), externNames(units), out);

      return out;
    }

    // ── extern declarations that appear more than once ─────────────────────────────────────────────────────────────────────────────────────────────

    using ItemKey = Pair<const Program*, uint32_t>;  // one item of one program

    String typeText(const Program& program, TypeId id);

    String listText(const Program& program, Range<TypeId> ids) {
      String out;

      for (const TypeId id : program.typeRefs[ids]) {
        out += (out.empty() ? "" : ",") + typeText(program, id);
      }

      return out;
    }

    String wrapped(const String& head, const String& inner) {
      return head + "<" + inner + ">";
    }

    String pairText(const Program& program, TypeId id) {
      const AstType& type = program.types[id];

      return wrapped(type.kind == AstTypeKind::Map ? "map" : "mutmap", typeText(program, type.first) + "," + typeText(program, type.second));
    }

    String callableText(const Program& program, TypeId id) {
      const AstType& type = program.types[id];

      if (type.kind == AstTypeKind::Variant) {
        return wrapped("variant", listText(program, type.types));
      }

      const String head = type.kind == AstTypeKind::Func ? "func" : "closure";

      return head + "(" + listText(program, type.types) + ")->" + typeText(program, type.first);
    }

    String sumText(const Program& program, TypeId id) {
      const AstType& type = program.types[id];

      if (type.kind == AstTypeKind::Either) {
        return wrapped("either", typeText(program, type.first) + "," + typeText(program, type.second));
      }

      return wrapped(type.kind == AstTypeKind::Optional ? "optional" : "coro", typeText(program, type.first));
    }

    String namedText(const Program& program, TypeId id) {
      const AstType& type = program.types[id];
      const String name = String(TextOf(program, type.name.name));

      if (type.kind == AstTypeKind::Generic) {
        return wrapped(name, listText(program, type.types));
      }

      return Empty(type.sizes) ? name : name + "[" + String(program.literals[type.sizes][0].text) + "]";
    }

    // `ptr<T>`, `vec<T,4>`, `array<T>`, `grid<T,2,3>`, ...: the keyword, the element, then the sizes.
    String elementText(const Program& program, TypeId id) {
      static const char* const heads[] = {"ptr", "vec", "array", "mutarray", "buffer", "grid", "tensor", "sparse", "map", "mutmap", "set", "mutset"};
      const AstType& type = program.types[id];
      String sizes;

      for (const Literal& size : program.literals[type.sizes]) {
        sizes += "," + String(size.text);
      }

      return wrapped(heads[static_cast<size_t>(type.kind)], typeText(program, type.first) + sizes);
    }

    // A canonical text rendering of a type, so two types are compared by comparing text (as the Nina `typeRefText`).
    String typeText(const Program& program, TypeId id) {
      switch (program.types[id].kind) {
        case AstTypeKind::Map: case AstTypeKind::Mutmap:
          return pairText(program, id);
        case AstTypeKind::Func: case AstTypeKind::Closure: case AstTypeKind::Variant:
          return callableText(program, id);
        case AstTypeKind::Either: case AstTypeKind::Optional: case AstTypeKind::Coroutine:
          return sumText(program, id);
        case AstTypeKind::Generic: case AstTypeKind::Named:
          return namedText(program, id);
        default:
          return elementText(program, id);
      }
    }

    // An `extern func` item and its declaration.
    struct ExternRef {
      const Program* program;
      uint32_t item;
      const AstExternFunc* decl;
    };

    ExternRef externRef(const ItemRef& ref) {
      return ExternRef{ref.program, ref.index, &ref.program->externFuncs[itemOf(ref).index]};
    }

    bool sameResult(const ExternRef& first, const ExternRef& second) {
      return typeText(*first.program, first.decl->returnType) == typeText(*second.program, second.decl->returnType);
    }

    // The same result type and the same parameter types, in order (parameter names do not matter).
    bool externsMatch(const ExternRef& first, const ExternRef& second) {
      const View<AstParam> firstParams = first.program->params[first.decl->params];
      const View<AstParam> secondParams = second.program->params[second.decl->params];

      if (!sameResult(first, second) || firstParams.size() != secondParams.size()) {
        return false;
      }

      for (size_t i = 0; i < firstParams.size(); i++) {
        if (typeText(*first.program, firstParams[i].type) != typeText(*second.program, secondParams[i].type)) {
          return false;
        }
      }

      return true;
    }

    Optional<String> keepFirst(Vector<ExternRef>& kept, const ExternRef& first) {
      kept.push_back(first);

      return nullopt;
    }

    // The extern already kept under this name, or nullptr when there is none yet.
    const ExternRef* keptNamed(const Vector<ExternRef>& kept, Symbol name) {
      for (const ExternRef& each : kept) {
        if (each.decl->name.name == name) {
          return &each;
        }
      }

      return nullptr;
    }

    // The first of two externs of one name stays if their signatures match; different signatures are an error. The later one is dropped either way.
    Optional<String> noteExtern(const Interner& names, Vector<ExternRef>& kept, const ExternRef& next, Set<ItemKey>& dropped) {
      const ExternRef* prior = keptNamed(kept, next.decl->name.name);

      if (prior == nullptr) {
        return keepFirst(kept, next);
      }

      dropped.insert(ItemKey{next.program, next.item});

      if (externsMatch(*prior, next)) {
        return nullopt;
      }

      return "conflicting 'extern func " + String(TextOf(names, next.decl->name.name)) + "' declarations (different signatures)";
    }

    // Drops the repeated externs of one name; answers the error when two of them disagree.
    Optional<String> dedupeExterns(const Vector<Unit>& units, Set<ItemKey>& dropped) {
      Vector<ExternRef> kept;
      Optional<String> error;

      for (const ItemRef& ref : itemsOf(units)) {
        if (itemOf(ref).kind == ItemKind::ExternFunc && !error) {
          error = noteExtern(*ref.program->names, kept, externRef(ref), dropped);
        }
      }

      return error;
    }

    // ── extern types: two imported classes can both name a third (`List`) as an opaque type; the copies are one ───────────────────────────────────

    // The bindings as one text, so two extern types bound the same way compare equal.
    String bindingKey(const Program& program, Range<AstHostBinding> binding) {
      String out;

      for (const AstHostBinding& each : program.hostBindings[binding]) {
        out += String(TextOf(program, each.target.name)) + String(each.text.text) + "|";
      }

      return out;
    }

    // An `extern type` item and its declaration.
    struct HostRef {
      const Program* program;
      uint32_t item;
      const AstExternType* decl;
    };

    HostRef hostRef(const ItemRef& ref) {
      return HostRef{ref.program, ref.index, &ref.program->externTypes[itemOf(ref).index]};
    }

    // Two declarations of one host class: the same name and the same bindings.
    bool sameClass(const HostRef& a, const HostRef& b) {
      return a.decl->name.name == b.decl->name.name && bindingKey(*a.program, a.decl->binding) == bindingKey(*b.program, b.decl->binding);
    }

    // A copy is redundant when the other has members and it has none, or when the other came first and has as many.
    bool shadowedBy(const HostRef& copy, const HostRef& other, bool otherFirst) {
      const bool hasMembers = !Empty(copy.decl->members);
      const bool otherHasMembers = !Empty(other.decl->members);

      if (!hasMembers) {
        return otherHasMembers || otherFirst;
      }

      return otherHasMembers && otherFirst && other.decl->members.count == copy.decl->members.count;
    }

    // An extern type is kept when it is the first of its name, or has members while the first has none.
    bool standsForItsName(const Vector<HostRef>& all, size_t index) {
      for (size_t other = 0; other < all.size(); other++) {
        if (other != index && sameClass(all[other], all[index]) && shadowedBy(all[index], all[other], other < index)) {
          return false;
        }
      }

      return true;
    }

    // Every extern type that does not stand for its name is dropped.
    void dedupeHostTypes(const Vector<Unit>& units, Set<ItemKey>& dropped) {
      Vector<HostRef> all;

      for (const ItemRef& ref : itemsOf(units)) {
        if (itemOf(ref).kind == ItemKind::ExternType) {
          all.push_back(hostRef(ref));
        }
      }

      for (size_t i = 0; i < all.size(); i++) {
        if (!standsForItsName(all, i)) {
          dropped.insert(ItemKey{all[i].program, all[i].item});
        }
      }
    }

    // ── dropping items ──────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────

    // A run of items becomes a unit, unless it is empty.
    void pushRun(Vector<Unit>& out, const Unit& run) {
      if (run.last > run.first) {
        out.push_back(run);
      }
    }

    // The run so far ends before item `at`, which is left out; the next run starts after it.
    void cutAt(Vector<Unit>& out, Unit& run, uint32_t at) {
      run.last = at;
      pushRun(out, run);
      run.first = at + 1;
    }

    // A unit with dropped items in it becomes the runs of items around them.
    void splitUnit(const Unit& unit, const Set<ItemKey>& dropped, Vector<Unit>& out) {
      Unit run = unit;

      for (uint32_t i = unit.first; i < unit.last; i++) {
        if (dropped.count(ItemKey{unit.program, i}) != 0) {
          cutAt(out, run, i);
        }
      }

      run.last = unit.last;
      pushRun(out, run);
    }

    // The units with the dropped items left out.
    Vector<Unit> withoutItems(const Vector<Unit>& units, const Set<ItemKey>& dropped) {
      Vector<Unit> out;

      for (const Unit& unit : units) {
        splitUnit(unit, dropped, out);
      }

      return out;
    }

    // Drops the repeated host types and externs from the units; answers the conflict error, if any.
    Vector<String> dedupeDeclarations(Vector<Unit>& units) {
      Set<ItemKey> dropped;
      dedupeHostTypes(units, dropped);

      if (const Optional<String> conflict = dedupeExterns(units, dropped)) {
        return {*conflict};
      }

      if (!dropped.empty()) {
        units = withoutItems(units, dropped);
      }

      return {};
    }

  }  // namespace

  // Three rules, the whole policy. Two structs sharing a name is always an error. Two funcs (or a func and an extern) sharing a name is always an
  // error. Two externs sharing a name is fine when their signatures match exactly (two files both wanting the same libc function), else an error.
  Vector<String> CheckDuplicates(Vector<Unit>& units) {
    if (units.empty()) {
      return {};
    }

    Vector<String> clashes = nameClashes(units);

    return clashes.empty() ? dedupeDeclarations(units) : clashes;
  }

}  // namespace polyir::front
