// The C++ API in one page: build `gcd` and a `main` with PolyIRBuilder, compile to LLVM IR, print it.
// From the repo root, after the CMake build into build/:
//   c++ -std=c++20 -Icore/include core/examples/Gcd.cpp -Lbuild -lpolyir -o gcd && ./gcd | clang -x ir - -o gcd_program && ./gcd_program; echo $?   # 6
#include "polyir/Std.hpp"
#include "polyir/Builder.hpp"
#include "polyir/LlvmCompiler.hpp"

#include <iostream>

using namespace polyir;

namespace {

  void euclidStep(Block& loop) {
    Let(loop, "rest", Rem(Local(loop, "a"), Local(loop, "b")));
    SetLocal(loop, "a", Local(loop, "b"));
    SetLocal(loop, "b", Local(loop, "rest"));
  }

  void defineGcd(PolyIRBuilder& b) {
    FunctionBuilder& gcd = DefineFunction(b, "gcd", {{"a", MakeInt(64)}, {"b", MakeInt(64)}}, MakeInt(64));
    Block& body = gcd.body;
    WhileLoop(body, Ne(Local(body, "b"), IntConst(0)), [](Block& loop) { euclidStep(loop); });
    Return(body, Local(body, "a"));
  }

  void defineMain(PolyIRBuilder& b) {
    FunctionBuilder& entry = DefineFunction(b, "main", {}, MakeInt(64));
    Return(entry.body, Call(b, "gcd", {IntConst(48), IntConst(18)}));
  }

  int printIr(const CompileResult& result) {
    if (!Ok(result)) {
      cerr << result.errors[0] << "\n";
      return 1;
    }

    cout << result.ir;

    return 0;
  }

}  // namespace

int main() {
  PolyIRBuilder b;
  defineGcd(b);
  defineMain(b);

  return printIr(CompileToLlvm(Build(b)));
}
