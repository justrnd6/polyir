// The direct WebAssembly writer (polyir/Wasm.hpp): a module of numbers is written without clang or wasm-ld and run by wasmtime when it is installed.
#include "polyir/Std.hpp"
#include "polyir/Builder.hpp"
#include "polyir/Wasm.hpp"

using namespace polyir;
using std::ios;
using std::streamsize;

namespace {

  int failures = 0;

  void fibFunction(PolyIRBuilder& b) {
    auto& fib = DefineFunction(b, "fib", {{"n", MakeInt(64)}}, MakeInt(64));
    IfThen(fib.body, Lt(Local(fib.body, "n"), IntConst(2)), [&](Block& then) { Return(then, Local(then, "n")); });
    Return(fib.body, Add(Call(b, "fib", {Sub(Local(fib.body, "n"), IntConst(1))}), Call(b, "fib", {Sub(Local(fib.body, "n"), IntConst(2))})));
  }

  void sumToFunction(PolyIRBuilder& b) {
    auto& sum = DefineFunction(b, "sumTo", {{"n", MakeInt(64)}}, MakeInt(64));
    Var(sum.body, "total", MakeInt(64), IntConst(0));
    ForRange(sum.body, "i", IntConst(0), Local(sum.body, "n"), [&](Block& each) { SetLocal(each, "total", Add(Local(each, "total"), Local(each, "i"))); });
    Return(sum.body, Local(sum.body, "total"));
  }

  void halfAndNegateFunctions(PolyIRBuilder& b) {
    auto& half = DefineFunction(b, "half", {{"x", MakeFloat(64)}}, MakeFloat(64));
    Return(half.body, Mul(Local(half.body, "x"), FloatConst(0.5)));
    auto& negate = DefineFunction(b, "negate", {{"x", MakeInt(64)}}, MakeInt(64));
    Return(negate.body, Neg(Local(negate.body, "x")));
  }

  Module numbers() {
    PolyIRBuilder b;
    fibFunction(b);
    sumToFunction(b);
    halfAndNegateFunctions(b);

    return Build(b);
  }

  void writeFile(const String& file, const Vector<uint8_t>& bytes) {
    OFStream(file, ios::binary).write(reinterpret_cast<const char*>(bytes.data()), static_cast<streamsize>(bytes.size()));
  }

  String commandOutput(const String& command) {
    FILE* pipe = popen(command.c_str(), "r");
    char buffer[256];
    String out;

    while (pipe != nullptr && fgets(buffer, sizeof(buffer), pipe) != nullptr) {
      out += buffer;
    }

    if (pipe != nullptr) {
      pclose(pipe);
    }

    return out;
  }

  String runWasm(const String& name, const Vector<uint8_t>& bytes, const String& function, const String& argument) {
    const String file = "test_wasm_" + name + ".wasm";
    writeFile(file, bytes);
    String out = commandOutput("wasmtime run --invoke " + function + " " + file + " " + argument + " 2>/dev/null");

    while (!out.empty() && (out.back() == '\n' || out.back() == ' ')) {
      out.pop_back();
    }

    return out;
  }

  void expect(const String& what, const String& want, const String& got) {
    if (want == got) {
      cout << "ok   " << what << " = " << got << "\n";
    } else {
      cout << "FAIL " << what << ": expected " << want << ", got " << got << "\n";
      failures++;
    }
  }

  void runNumbers(const WasmResult& wasm) {
    expect("fib", "55", runWasm("fib", wasm.bytes, "fib", "10"));
    expect("loop", "45", runWasm("sum", wasm.bytes, "sumTo", "10"));
    expect("float", "2.5", runWasm("half", wasm.bytes, "half", "5"));
    expect("negate", "-7", runWasm("neg", wasm.bytes, "negate", "7"));
  }

  void declinesText() {
    PolyIRBuilder b;
    auto& f = DefineFunction(b, "text", {}, MakeString());
    Return(f.body, StringConst("hi"));
    expect("declines text", "1", to_string(WasmModule(Build(b), {}).errors.size()));
  }

  // Says why the module could not be written; the exit status of a failed test.
  int failedToWrite(const WasmResult& wasm) {
    cout << "FAIL " << wasm.errors[0] << "\n";

    return 1;
  }

  bool hasWasmtime() {
    return system("which wasmtime > /dev/null 2>&1") == 0;
  }

  // Without wasmtime the module is written but not run: the test passes as skipped.
  int skippedWithoutWasmtime() {
    cout << "skipped: wasmtime is not installed\n";

    return 0;
  }

}  // namespace

int main() {
  const WasmResult wasm = WasmModule(numbers(), {{"fib", "fib"}, {"sumTo", "sumTo"}, {"half", "half"}, {"negate", "negate"}});

  if (!Ok(wasm)) {
    return failedToWrite(wasm);
  }

  if (!hasWasmtime()) {
    return skippedWithoutWasmtime();
  }

  runNumbers(wasm);
  declinesText();

  return failures == 0 ? 0 : 1;
}
