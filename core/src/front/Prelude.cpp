// The libraries that are part of the language (lang/src/Prelude.nina, PreludeText.nina): the Math functions, the CPU side of the GPU functions,
// the Text functions (`split`, `trim`, `join`, `fill`, ... and `StringBuilder`, added when any of its methods is called) and the Lists
// functions (`first`, `last`, `drop`: generic, so a call is checked as a copy for its element type).
//
// A program that fails because it calls library functions it has not defined or imported is checked again with them added in front (`retry` in
// Pipeline.cpp). `MissingLibraryFunctions` reads the names out of the `unknown function X` problems; `LibraryUnits` answers the units that define
// them and what they call. The libraries are parsed once per process, the first time a program needs one; a program that needs none costs nothing.
#include "front/Pipeline.hpp"
#include "front/PreludeText.hpp"

namespace polyir::front {

  namespace {

    // The functions each library defines, by the name a program calls.
    const Vector<String> kMathNames = {"sign", "fract", "clamp", "mix"};
    const Vector<String> kGpuNames = {"atomicAdd", "atomicAddFloat", "atomicMin", "atomicMax", "groupSize", "localIndex", "groupIndex", "barrier"};
    const Vector<String> kTextNames = {"contains", "startsWith", "endsWith", "split", "lines", "join", "trim", "replace", "repeat", "indent",
      "indentRest", "fill", "StringBuilder"};
    const Vector<String> kListNames = {"first", "last", "drop"};
    // A call to any of these on a value the check could not type yet means the program builds a `StringBuilder`.
    const Vector<String> kBuilderMethods = {"add", "line", "blank", "lines", "indent", "dedent", "indentNow", "length", "isEmpty", "text"};

    // The four libraries, parsed with one interner (`owner`), so their symbols match the programs that use them.
    struct Library {
      Interner* owner;
      ParseResult math;
      ParseResult gpu;
      ParseResult text;
      ParseResult lists;
    };

    Library makeLibrary(Interner& names) {
      return Library{&names, ParseSource(String(MathPreludeText()), names), ParseSource(String(GpuPreludeText()), names),
                     ParseSource(String(TextPreludeText()), names), ParseSource(String(ListsPreludeText()), names)};
    }

    // The parsed libraries for this interner; the first call parses them, later calls (any program) share them.
    const Library& loaded(Interner& names) {
      static Vector<UniquePtr<Library>> all;

      for (const UniquePtr<Library>& each : all) {
        if (each->owner == &names) {
          return *each;
        }
      }

      all.push_back(make_unique<Library>(makeLibrary(names)));

      return *all.back();
    }

    bool isLibraryName(const String& name) {
      const bool lists = IsOneOf(name, kListNames);

      return lists || IsOneOf(name, kMathNames) || IsOneOf(name, kGpuNames) || IsOneOf(name, kTextNames) || IsOneOf(name, kBuilderMethods);
    }

    // What each library function calls: other library functions, the C math functions it declares, and for a builder method the builder itself.
    // The builder's methods come with it.
    Vector<String> needs(const String& name) {
      static const HashMap<String, Vector<String>> table = {{"fract", {"floorf"}}, {"lines", {"split"}}, {"trim", {"isSpaceChar"}},
        {"replace", {"split", "join"}}, {"indent", {"lines", "repeat", "join"}}, {"indentRest", {"indent"}}, {"fill", {"filledName"}},
        {"filledName", {"trim"}}, {"StringBuilder", {"repeat", "join"}}};

      if (table.count(name) != 0) {
        return table.at(name);
      }

      return IsOneOf(name, kBuilderMethods) ? Vector<String>{"StringBuilder"} : Vector<String>();
    }

    void addOnce(Vector<String>& names, const String& name) {
      if (!IsOneOf(name, names)) {
        names.push_back(name);
      }
    }

    // The wanted names and everything they need, however deep (five rounds is deeper than the libraries go).
    Set<String> closure(const Vector<String>& wanted) {
      Vector<String> names = wanted;

      for (int round = 0; round < 5; round++) {
        for (const String& name : Vector<String>(names)) {  // a copy, because the loop adds to `names`
          for (const String& needed : needs(name)) {
            addOnce(names, needed);
          }
        }
      }

      return Set<String>(names.begin(), names.end());
    }

    // The name a library item declares, for the kinds a program can need.
    Optional<String> libraryNameOf(const Program& program, const AstItem& item) {
      if (item.kind == ItemKind::Func) {
        return String(TextOf(program, program.funcs.items[item.index].name.name));
      }

      if (item.kind == ItemKind::ExternFunc) {
        return String(TextOf(program, program.externFuncs[item.index].name.name));
      }

      if (item.kind == ItemKind::Struct) {
        return String(TextOf(program, program.structs[item.index].name.name));
      }

      return nullopt;
    }

    bool isNeeded(const Program& program, uint32_t item, const Set<String>& needed) {
      const Optional<String> name = libraryNameOf(program, program.items[item]);

      return name && needed.count(*name) != 0;
    }

    // Whether item `item` of `program` comes right after the last unit's items, so it can join that unit.
    bool continuesLastUnit(const Vector<Unit>& units, const Program& program, uint32_t item) {
      return !units.empty() && units.back().program == &program && units.back().last == item;
    }

    // The needed items of one library as units, a run of neighbours being one unit.
    void neededUnits(const Program& program, const Set<String>& needed, Vector<Unit>& out) {
      for (uint32_t i = 0; i < program.items.size(); i++) {
        if (!isNeeded(program, i, needed)) {
          continue;
        }

        if (continuesLastUnit(out, program, i)) {
          out.back().last = i + 1;
        } else {
          out.push_back(Unit{&program, i, i + 1, 0});
        }
      }
    }

    String trimmed(const String& text) {
      const size_t first = text.find_first_not_of(" \t\r\n");

      if (first == String::npos) {
        return String();
      }

      const size_t last = text.find_last_not_of(" \t\r\n");

      return text.substr(first, last - first + 1);
    }

    // `unknown function X` in a problem: X (the text up to the next such phrase, trimmed).
    Optional<String> unknownName(const String& problem) {
      const String marker = "unknown function ";
      const size_t at = problem.find(marker);

      if (at == String::npos) {
        return nullopt;
      }

      const String rest = problem.substr(at + marker.size());

      return trimmed(rest.substr(0, rest.find(marker)));
    }

  }  // namespace

  // Which of the check's complaints (`12:5: unknown function clamp`) are about a library function, each name once.
  Vector<String> MissingLibraryFunctions(const Vector<String>& problems) {
    Vector<String> found;

    for (const String& problem : problems) {
      const Optional<String> name = unknownName(problem);

      if (name && isLibraryName(*name)) {
        addOnce(found, *name);
      }
    }

    return found;
  }

  // The units that define `wanted` and what they need, in the order the libraries write them. That is the Math functions, the GPU ones, the
  // Text ones, then the Lists ones.
  Vector<Unit> LibraryUnits(Interner& names, const Vector<String>& wanted) {
    Vector<Unit> out;
    const Library& library = loaded(names);
    const Set<String> needed = closure(wanted);

    neededUnits(library.math.program, needed, out);
    neededUnits(library.gpu.program, needed, out);
    neededUnits(library.text.program, needed, out);
    neededUnits(library.lists.program, needed, out);

    return out;
  }

}  // namespace polyir::front
