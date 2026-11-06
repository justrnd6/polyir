// What the program tests share: compile a Module to LLVM IR, have clang assemble and run it, and compare the exit status.
#pragma once

#include "polyir/Std.hpp"
#include "polyir/Builder.hpp"
#include "polyir/LlvmCompiler.hpp"

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <iostream>
#include <sys/wait.h>

namespace polyir::testing {

  inline int failures = 0;

  inline int ClangFailed(const String& name) {
    cout << "FAIL " << name << ": clang rejected test_" << name << ".ll (see test_" << name << ".log)\n";

    return -1;
  }

  inline bool Assemble(const String& name, const String& ir) {
    const String ll = "test_" + name + ".ll";
    OFStream(ll) << ir;
    const String build = "clang -x ir -O1 -Wno-override-module " + ll + " -o ./test_" + name + ".bin 2> test_" + name + ".log";

    return system(build.c_str()) == 0;
  }

  inline int Execute(const String& name) {
    const int status = system(("exec 2>/dev/null; ./test_" + name + ".bin > test_" + name + ".out").c_str());

    return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
  }

  // Compile `program`, build it with clang and run it: the exit status, or -1 when something went wrong.
  inline int Run(const String& name, const Module& program) {
    const CompileResult result = CompileToLlvm(program);

    if (!Ok(result)) {
      cout << "FAIL " << name << ": " << result.errors[0] << "\n";
      return -1;
    }

    return Assemble(name, result.ir) ? Execute(name) : ClangFailed(name);
  }

  inline void Check(const String& name, int expected, const Module& program) {
    const int got = Run(name, program);

    if (got == expected) {
      cout << "ok   " << name << " = " << got << "\n";
      return;
    }

    cout << "FAIL " << name << ": expected " << expected << ", got " << got << "\n";
    failures++;
  }

  // func main() -> i64 { <body> }
  inline Module MakeProgram(const Function<void(PolyIRBuilder&, Block&)>& body, const Function<void(PolyIRBuilder&)>& before = [](PolyIRBuilder&) {}) {
    PolyIRBuilder b;
    before(b);
    auto& main = DefineFunction(b, "main", {}, MakeInt(64));
    body(b, main.body);

    return Build(b);
  }


  inline int Finish() {
    cout << (failures == 0 ? "all checks passed" : "some checks failed") << "\n";

    return failures == 0 ? 0 : 1;
  }

}  // namespace polyir::testing
