// Debug information for the LLVM output: a compile unit, a subprogram for each function, a location for each statement and expression, and a
// description of each variable, as the DWARF metadata a debugger reads.
//
// The one idea to hold: the metadata is a numbered list of NODES, each written as one line `!N = ...` at the end of the module. Every `int`
// this file calls a node, a scope or a type is such a number N (the node's index in `nodes_`), and -1 means none (written `null`). A line
// or a column is a plain source position.
//
// A program another front end wrote says where its lines came from (`//# from Hops.nina:7:9`, Module::fromLines). Every position
// is mapped through those lines, so a debugger names the first source's file and lines: each file has its own DIFile, a
// function's subprogram is in the file its first line came from, and a location in another file is scoped to a DILexicalBlockFile.
#pragma once

#include "polyir/Std.hpp"
#include "polyir/Hir.hpp"

namespace polyir::llvm {

  struct DebugInfo {
    bool enabled_ = false;
    Vector<String> nodes_;  // the node texts in number order: node N is nodes_[N]
    Map<String, int> ids_;  // a node's text to its number, so identical nodes are written once
    Map<String, Vector<int>> named_;  // the named lists of nodes, such as `!llvm.dbg.cu`
    int file_ = 0;  // the number of the DIFile node
    int unit_ = 0;  // the number of the DICompileUnit node
    String directory_;               // the checked file's directory: a file another names is found from there
    Vector<FromLine> fromLines_;     // where the program's lines came from (`//# from`), mapped through for every position
    Map<String, int> files_;         // each other source file's DIFile node, by its name
    Map<int, String> scopeFiles_;    // each subprogram's file as the program names it: "" for the checked file, else its origin
    Map<int, int> scopeFileNodes_;   // each subprogram's DIFile node
  };

  // Every `!N` outside a quoted string in `text`, moved up by `offset`.
  String ShiftedMetadata(const String& text, int offset);

  DebugInfo MakeDebugInfo(const String& file, const String& directory);  // debug info switched on for one source file

  // Positions mapped through where the program's lines came from (Module::fromLines), so the debug info names that source.
  void MapThrough(DebugInfo& debug, const Vector<FromLine>& fromLines);

  int DebugFunction(DebugInfo& debug, const FuncDef& definition);  // the subprogram node of a function

  int Location(DebugInfo& debug, int line, int column, int scope);  // the node of a source position inside `scope`

  // The node that describes a variable. `argumentNumber` is a parameter's position counting from 1, and 0 for a local variable.
  int Variable(DebugInfo& debug, const String& name, int argumentNumber, int scope, int line, const TypePtr& type);

  String Trailer(const DebugInfo& debug);  // the metadata at the end of the module

  Vector<String> Named(const DebugInfo& debug);  // the named lists as "name=0,3", for a fragment

  int Adopt(DebugInfo& debug, const Vector<String>& nodes, const Vector<String>& named);  // a fragment's metadata; the number to add to its `!N`

  inline bool Enabled(const DebugInfo& debug) {
    return debug.enabled_;
  }

  // Every node, in number order.
  inline const Vector<String>& Nodes(const DebugInfo& debug) {
    return debug.nodes_;
  }

}  // namespace polyir::llvm
