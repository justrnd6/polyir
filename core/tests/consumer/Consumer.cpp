// Uses only the installed library: builds a program with PolyIRBuilder, then checks and compiles the same program from source text
// (`func main() -> i64 { return 6 * 7 }`); prints the source-built LLVM module and exits 0 only when the two IR texts are identical.
#include <polyir/Builder.hpp>
#include <polyir/Frontend.hpp>
#include <polyir/LlvmCompiler.hpp>
#include <polyir/Std.hpp>

using namespace polyir;
using namespace polyir::front;

namespace {

  CompileResult fromBuilder() {
    PolyIRBuilder b;
    Return(DefineFunction(b, "main", {}, MakeInt(64)).body, Mul(IntConst(6), IntConst(7)));

    return CompileToLlvm(Build(b));
  }

  CompileResult fromSource() {
    const FrontResult checked = CheckSource("func main() -> i64 { return 6 * 7 }");

    return Ok(checked) ? CompileToLlvm(checked.module) : CompileResult{"", checked.errors};
  }

}  // namespace

int main() {
  const CompileResult built = fromBuilder();
  const CompileResult written = fromSource();
  cout << (Ok(built) && Ok(written) ? written.ir : "failed");

  return Ok(built) && Ok(written) && built.ir == written.ir ? 0 : 1;
}
