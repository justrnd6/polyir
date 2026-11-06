// A function compiled alone with debug info carries its metadata, and the module it is added to renumbers it after its own, so both are described.
#include "polyir/Std.hpp"
#include "RunSupport.hpp"

using namespace polyir;
using namespace polyir::testing;

namespace {

  CompileOptions debugOptions(const String& file) {
    CompileOptions options;
    options.debugInfo = true;
    options.sourceFile = file;
    options.sourceDirectory = ".";

    return options;
  }

  void addTwice(PolyIRBuilder& b) {
    auto& twice = DefineFunction(b, "twice", {{"n", MakeInt(64)}}, MakeInt(64));
    Let(twice.body, "doubled", Add(Local(twice.body, "n"), Local(twice.body, "n")));
    Return(twice.body, Local(twice.body, "doubled"));
  }

  FunctionIr fragment() {
    PolyIRBuilder helper;
    addTwice(helper);

    return CompileFunctionToLlvm(Build(helper), "twice", debugOptions("helper.poly"));
  }

  Module hostWith(const FunctionIr& alone) {
    PolyIRBuilder b;
    DeclareFunction(b, "twice", {{"n", MakeInt(64)}}, MakeInt(64));
    AddLlvm(b, alone.text, alone.declarations, alone.metadata, alone.namedMetadata);
    Return(DefineFunction(b, "main", {}, MakeInt(64)).body, Add(Call(b, "twice", {IntConst(20)}), IntConst(2)));

    return Build(b);
  }

  int count(const String& text, const String& piece) {
    int found = 0;

    for (size_t at = text.find(piece); at != String::npos; at = text.find(piece, at + 1)) {
      found++;
    }

    return found;
  }

  void expectTrue(const String& what, bool good) {
    cout << (good ? "ok   " : "FAIL ") << what << "\n";
    failures += good ? 0 : 1;
  }

  void fragmentCarriesMetadata() {
    const FunctionIr alone = fragment();
    expectTrue("the fragment has debug metadata", Ok(alone) && !alone.metadata.empty() && alone.namedMetadata.size() == 1);
    expectTrue("a fragment without debug info has none", CompileFunctionToLlvm(hostWith(alone), "main").metadata.empty());
  }

  String assembleWithDebug(const String& ir) {
    OFStream("test_debug_fragment.ll") << ir;
    const int status = system("clang -x ir -g -O1 -Wno-override-module test_debug_fragment.ll -o test_debug_fragment.bin 2> test_debug_fragment.log");

    return status == 0 ? "" : "clang rejected the module (see test_debug_fragment.log)";
  }

  void hostAndFragmentTogether() {
    const CompileResult both = CompileToLlvm(hostWith(fragment()), debugOptions("host.poly"));
    expectTrue("the module compiles", Ok(both));

    expectTrue("both compile units are listed", count(both.ir, "DICompileUnit") == 2 && count(both.ir, "!llvm.dbg.cu = !{!1, !") == 1);
    expectTrue("the module flags are written once", count(both.ir, "!llvm.module.flags =") == 1 && count(both.ir, "Dwarf Version") == 1);

    expectTrue("clang -g accepts it", assembleWithDebug(both.ir).empty());
    const int status = system("./test_debug_fragment.bin");
    expectTrue("it runs", WIFEXITED(status) && WEXITSTATUS(status) == 42);
  }

  void hostWithoutDebugInfo() {
    const CompileResult plain = CompileToLlvm(hostWith(fragment()));
    expectTrue("a host without debug info still describes the fragment", Ok(plain) && count(plain.ir, "DICompileUnit") == 1 && assembleWithDebug(plain.ir).empty());
  }

}  // namespace

int main() {
  fragmentCarriesMetadata();
  hostAndFragmentTogether();
  hostWithoutDebugInfo();

  return Finish();
}
