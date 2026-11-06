// Debug information for the LLVM output (see LlvmDebug.hpp): the DWARF metadata lines (`!N = ...`) for the compile unit, each function,
// each source location and each variable, numbered so that IR fragments written apart can be joined without their numbers meeting.
#include "emit/llvm/LlvmDebug.hpp"

#include "emit/llvm/LlvmNames.hpp"

namespace polyir::llvm {

  namespace {

    String quoted(const String& text) {
      return "\"" + text + "\"";
    }

    // How one node names another: `!N`, or `null` for none.
    String nodeReference(int id) {
      return id < 0 ? "null" : "!" + to_string(id);
    }

    String basicTypeNode(const String& name, int size, const String& encoding) {
      return "!DIBasicType(name: " + quoted(name) + ", size: " + to_string(size) + ", encoding: " + encoding + ")";
    }

    // How a debugger reads a number's bits.
    String numberEncoding(const TypePtr& type) {
      if (IsFloat(*type)) {
        return "DW_ATE_float";
      }

      if (IsSigned(*type)) {
        return "DW_ATE_signed";
      }

      return "DW_ATE_unsigned";
    }

    // The basic type of a number, an integer or a float (a Bool never comes here: typeNode takes it first).
    String numberNode(const TypePtr& type) {
      return basicTypeNode(TypeText(*type), type->bits, numberEncoding(type));
    }

    // One `!N = node` line per node.
    String nodeLines(const Vector<String>& nodes) {
      String out;

      for (size_t i = 0; i < nodes.size(); i++) {
        out += "!" + to_string(i) + " = " + nodes[i] + "\n";
      }

      return out;
    }

    bool isDigit(char c) {
      return c >= '0' && c <= '9';
    }

    bool startsNode(const String& text, size_t at) {
      return text[at] == '!' && at + 1 < text.size() && isDigit(text[at + 1]);
    }

    // The digits at `at`, as the number they spell; `at` ends just past them.
    int numberAt(const String& text, size_t& at) {
      int value = 0;

      for (; at < text.size() && isDigit(text[at]); at++) {
        value = value * 10 + (text[at] - '0');
      }

      return value;
    }

    // The node number `!N` that starts at `at`, moved up by `offset`; `at` ends just past its digits.
    String shiftedNode(const String& text, size_t& at, int offset) {
      at++;  // past the `!`

      return "!" + to_string(numberAt(text, at) + offset);
    }

    // One step of ShiftedMetadata copies the character at `at` (a `"` opens or closes a string). Or, outside a string, it copies the whole
    // node number `!N` that starts there, moved up by `offset`. `at` ends past what was copied.
    void copyStep(const String& text, size_t& at, int offset, bool& inString, String& out) {
      if (text[at] == '"') {
        inString = !inString;
      }

      if (!inString && startsNode(text, at)) {
        out += shiftedNode(text, at, offset);
      } else {
        out += text[at++];
      }
    }

    String joined(const Vector<int>& ids) {
      String out;

      for (const int id : ids) {
        out += (out.empty() ? "" : ", ") + nodeReference(id);
      }

      return out;
    }

    // A new node; the answer is its number.
    int appendNode(DebugInfo& debug, const String& text) {
      debug.nodes_.push_back(text);

      return static_cast<int>(debug.nodes_.size()) - 1;
    }

    // The node with this text, added only the first time it is asked for.
    int sharedNode(DebugInfo& debug, const String& text) {
      const auto known = debug.ids_.find(text);

      if (known != debug.ids_.end()) {
        return known->second;
      }

      const int number = appendNode(debug, text);
      debug.ids_[text] = number;

      return number;
    }

    // ── where a position came from ──

    // A position as a debugger should see it: its file ("" for the checked file), line and column.
    struct Place {
      String file;
      int line = 0;
      int column = 0;
    };

    // The last `//# from` line before line `line` of file `in`, or null for none.
    const FromLine* fromLineBefore(const DebugInfo& debug, const String& in, int line) {
      const FromLine* from = nullptr;

      for (const FromLine& each : debug.fromLines_) {
        from = each.in == in && each.line < line ? &each : from;
      }

      return from;
    }

    // Line `line` (column `column`) of file `in`: its own place, or the place a `//# from` line before it names, counted on.
    Place placeOf(const DebugInfo& debug, const String& in, int line, int column) {
      const FromLine* from = fromLineBefore(debug, in, line);

      if (from == nullptr) {
        return Place{in, line, column};
      }

      const bool next = line == from->line + 1;

      return Place{from->file, from->fromLine + line - from->line - 1, next ? from->fromColumn : 0};
    }

    int sharedNode(DebugInfo& debug, const String& text);

    // The DIFile of a source file: the checked file's own, or one for another, found from the checked file's directory.
    int fileNode(DebugInfo& debug, const String& name) {
      if (name.empty()) {
        return debug.file_;
      }

      const String directory = debug.directory_;
      const String filename = Path(name).is_absolute() ? name : (Path(directory) / name).lexically_normal().string();

      return sharedNode(debug, "!DIFile(filename: " + quoted(filename) + ", directory: " + quoted(directory) + ")");
    }

    // The scope a location in `file` has inside the subprogram `scope`: the subprogram, or a block of it in that file.
    int scopeIn(DebugInfo& debug, int scope, int file) {
      const auto own = debug.scopeFileNodes_.find(scope);

      if (own == debug.scopeFileNodes_.end() || own->second == file) {
        return scope;
      }

      return sharedNode(debug, "!DILexicalBlockFile(scope: " + nodeReference(scope) + ", file: " + nodeReference(file) + ", discriminator: 0)");
    }

    // A line of the function whose subprogram is `scope`, as a debugger should see it.
    Place placeInScope(const DebugInfo& debug, int scope, int line, int column) {
      const auto in = debug.scopeFiles_.find(scope);

      return placeOf(debug, in == debug.scopeFiles_.end() ? "" : in->second, line, column);
    }

    int pointerTypeNode(DebugInfo& debug, int base, const String& name) {
      const String label = name.empty() ? "" : "name: " + quoted(name) + ", ";

      return sharedNode(debug, "!DIDerivedType(tag: DW_TAG_pointer_type, " + label + "baseType: " + nodeReference(base) + ", size: 64)");
    }

    // The node of a type: a Bool, a number or a string as a debugger knows it; anything else as a named pointer.
    int typeNode(DebugInfo& debug, const TypePtr& type) {
      if (IsVoid(*type)) {
        return -1;
      }

      if (IsBoolean(*type)) {
        return sharedNode(debug, basicTypeNode("bool", 8, "DW_ATE_boolean"));
      }

      if (IsNumber(*type)) {
        return sharedNode(debug, numberNode(type));
      }

      if (type->kind == TypeKind::String) {
        return pointerTypeNode(debug, sharedNode(debug, basicTypeNode("char", 8, "DW_ATE_signed_char")), "");
      }

      return pointerTypeNode(debug, -1, TypeText(*type));
    }

    // A function's result type, then its parameters' types.
    String signatureTypeList(DebugInfo& debug, const FuncDef& definition) {
      String list = nodeReference(typeNode(debug, definition.result));

      for (const Parameter& parameter : definition.parameters) {
        list += ", " + nodeReference(typeNode(debug, parameter.type));
      }

      return "!{" + list + "}";
    }

    int numberFrom(const String& digits) {
      return static_cast<int>(strtoll(digits.c_str(), nullptr, 10));
    }

    // One named list of a fragment, written `name=0,3` (see Named), its node numbers moved up by `offset`.
    void addNamedEntry(DebugInfo& debug, const String& entry, int offset) {
      const size_t equals = entry.find('=');
      IStringStream list(entry.substr(equals + 1));
      Vector<int>& ids = debug.named_[entry.substr(0, equals)];
      String number;

      while (getline(list, number, ',')) {
        ids.push_back(numberFrom(number) + offset);
      }
    }

    // Each named list as a line: `!llvm.dbg.cu = !{!1}`.
    String namedMetadataLines(const DebugInfo& debug) {
      String out;

      for (const auto& [name, ids] : debug.named_) {
        out += "!" + name + " = !{" + joined(ids) + "}\n";
      }

      return out;
    }

  }  // namespace

  String ShiftedMetadata(const String& text, int offset) {
    String out;
    bool inString = false;
    size_t at = 0;

    while (at < text.size()) {
      copyStep(text, at, offset, inString, out);
    }

    return out;
  }

  DebugInfo MakeDebugInfo(const String& file, const String& directory) {
    DebugInfo debug;
    debug.enabled_ = true;
    debug.directory_ = directory;

    debug.file_ = appendNode(debug, "!DIFile(filename: " + quoted(file) + ", directory: " + quoted(directory) + ")");
    debug.unit_ = appendNode(debug, "distinct !DICompileUnit(language: DW_LANG_C99, file: " + nodeReference(debug.file_) + ", producer: \"llvm\", isOptimized: false, runtimeVersion: 0, emissionKind: FullDebug)");
    debug.named_["llvm.dbg.cu"].push_back(debug.unit_);

    return debug;
  }

  void MapThrough(DebugInfo& debug, const Vector<FromLine>& fromLines) {
    debug.fromLines_ = fromLines;
  }

  int Adopt(DebugInfo& debug, const Vector<String>& nodes, const Vector<String>& named) {
    const int offset = static_cast<int>(debug.nodes_.size());

    for (const String& node : nodes) {
      debug.nodes_.push_back(ShiftedMetadata(node, offset));
    }

    for (const String& entry : named) {
      addNamedEntry(debug, entry, offset);
    }

    return offset;
  }

  Vector<String> Named(const DebugInfo& debug) {
    Vector<String> out;

    for (const auto& [name, ids] : debug.named_) {
      String list;

      for (const int id : ids) {
        list += (list.empty() ? "" : ",") + to_string(id);
      }

      out.push_back(name + "=" + list);
    }

    return out;
  }

  // The subprogram is in the file its function's first line came from, and is noted with that file.
  int DebugFunction(DebugInfo& debug, const FuncDef& definition) {
    const int types = sharedNode(debug, signatureTypeList(debug, definition));
    const int signature = sharedNode(debug, "!DISubroutineType(types: " + nodeReference(types) + ")");

    const Place place = placeOf(debug, definition.origin, definition.at.line, definition.at.column);
    const int file = fileNode(debug, place.file);
    const String line = to_string(place.line);

    const int subprogram = appendNode(debug, "distinct !DISubprogram(name: " + quoted(definition.name) + ", scope: " + nodeReference(file) + ", file: " +
      nodeReference(file) + ", line: " + line + ", type: " + nodeReference(signature) + ", scopeLine: " + line + ", spFlags: DISPFlagDefinition, unit: " +
      nodeReference(debug.unit_) + ")");
    debug.scopeFiles_[subprogram] = definition.origin;
    debug.scopeFileNodes_[subprogram] = file;

    return subprogram;
  }

  int Location(DebugInfo& debug, int line, int column, int scope) {
    const Place place = placeInScope(debug, scope, line, column);
    const int within = scopeIn(debug, scope, fileNode(debug, place.file));

    return sharedNode(debug, "!DILocation(line: " + to_string(place.line) + ", column: " + to_string(place.column) + ", scope: " + nodeReference(within) + ")");
  }

  int Variable(DebugInfo& debug, const String& name, int argumentNumber, int scope, int line, const TypePtr& type) {
    const String argumentField = argumentNumber > 0 ? "arg: " + to_string(argumentNumber) + ", " : "";
    const Place place = placeInScope(debug, scope, line, 0);

    return sharedNode(debug, "!DILocalVariable(name: " + quoted(name) + ", " + argumentField + "scope: " + nodeReference(scope) + ", file: " +
      nodeReference(fileNode(debug, place.file)) + ", line: " + to_string(place.line) + ", type: " + nodeReference(typeNode(debug, type)) + ")");
  }

  String Trailer(const DebugInfo& debug) {
    if (debug.nodes_.empty()) {
      return "";
    }

    const int flags = static_cast<int>(debug.nodes_.size());
    const String header = "\n" + namedMetadataLines(debug) + "!llvm.module.flags = !{" + nodeReference(flags) + ", " + nodeReference(flags + 1) + "}\n";
    const String dwarf = nodeReference(flags) + " = !{i32 7, !\"Dwarf Version\", i32 5}\n";

    return header + nodeLines(debug.nodes_) + dwarf + nodeReference(flags + 1) + " = !{i32 2, !\"Debug Info Version\", i32 3}\n";
  }

}  // namespace polyir::llvm
