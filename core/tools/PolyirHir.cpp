// polyir-hir: reads the checked HIR the Nina front end writes (`polyir f.poly --emit-hir-json`) and prints it or compiles it.
//   polyir-hir program.json            the HIR as text
//   polyir-hir program.json --llvm     the LLVM IR
//   polyir-hir program.json --json     the same JSON, written again by libpolyir (what the Nina checker wrote, byte for byte)
//   polyir-hir program.json --poly     the program as .poly source (what has no .poly spelling goes to standard error, and the exit status is 1)
//   polyir-hir program.json --msl      the compute and reduce kernels (the functions the JSON gives a `stage`) as Metal Shading Language
//   polyir-hir program.json --glsl     the Vertex and Fragment stages as OpenGL GLSL, one `// ==== name.vert ====` section each; a program without
//                                      them, its compute kernels as Vulkan GLSL compute shaders (`// ==== name.comp ====`), as `polyir --emit-glsl`
//   polyir-hir program.json --spirv    the compute kernels as SPIR-V modules in hex text (`// ==== name.spv ====`), as `polyir --emit-spirv`
//   ... --debug --source f.poly        with debug info (the lines, columns and variables of f.poly)
#include "polyir/Std.hpp"
#include "polyir/HirJson.hpp"
#include "polyir/HirPrint.hpp"
#include "polyir/Kernels.hpp"
#include "polyir/LlvmCompiler.hpp"
#include "polyir/PolySource.hpp"

#include <fstream>
#include <iostream>
#include <sstream>

using namespace polyir;

namespace {

  String readFile(const char* path) {
    IFStream in(path);
    StringStream text;
    text << in.rdbuf();

    return text.str();
  }

  // The text after a flag (`--source f.poly`), or nothing.
  String option(int argc, char** argv, const String& flag) {
    for (int i = 2; i + 1 < argc; i++) {
      if (flag == argv[i]) {
        return argv[i + 1];
      }
    }

    return "";
  }

  bool hasFlag(int argc, char** argv, const String& flag) {
    for (int i = 2; i < argc; i++) {
      if (flag == argv[i]) {
        return true;
      }
    }

    return false;
  }

  // The debug info names the source by its file name and its folder ("." when the path has none).
  void setSource(CompileOptions& options, const String& source) {
    const size_t slash = source.rfind('/');
    const bool hasFolder = slash != String::npos;
    options.sourceFile = hasFolder ? source.substr(slash + 1) : source;
    options.sourceDirectory = hasFolder ? source.substr(0, slash) : ".";
  }

  CompileOptions optionsFrom(int argc, char** argv) {
    CompileOptions options;
    options.debugInfo = hasFlag(argc, argv, "--debug");
    setSource(options, option(argc, argv, "--source"));

    return options;
  }

  int printLlvm(const Module& module, const CompileOptions& options) {
    const CompileResult result = CompileToLlvm(module, options);

    if (!Ok(result)) {
      cerr << "error: " << result.errors[0] << "\n";
      return 1;
    }

    cout << result.ir;

    return 0;
  }

  int printJson(const Module& module) {
    cout << WriteHirJson(module) << "\n";

    return 0;
  }

  // Shader text as the Nina CLI prints it, or the reasons it cannot be written.
  int printShader(const ShaderText& shader) {
    for (const String& problem : shader.errors) {
      cerr << "error: " << problem << "\n";
    }

    if (!shader.errors.empty()) {
      return 1;
    }

    cout << shader.text << "\n";

    return 0;
  }

  int usage() {
    cerr << "usage: polyir-hir program.json [--llvm [--debug] [--source file.poly] | --json | --poly | --msl | --glsl | --spirv]\n";

    return 2;
  }

  int printMsl(const Module& module) {
    cout << MetalSource(module) << "\n";

    return 0;
  }

  int printText(const Module& module) {
    cout << ModuleText(module) << "\n";

    return 0;
  }

  // The module as .poly source; what it could not spell goes to standard error.
  int printPoly(const Module& module) {
    const PolySource poly = WritePoly(module);
    cout << poly.text;

    for (const String& problem : poly.problems) {
      cerr << "error: " << problem << "\n";
    }

    return poly.problems.empty() ? 0 : 1;
  }

  // `--poly`, `--json`, or (with no mode) the HIR's text: the module written out again.
  int printWritten(const Module& module, const String& mode) {
    if (mode == "--poly") {
      return printPoly(module);
    }

    return mode == "--json" ? printJson(module) : printText(module);
  }

  // `--msl`, `--glsl`, `--spirv`, `--llvm`, or the module written out again (`--poly`, `--json`, or as text when there is no mode).
  int printAs(const Module& module, const String& mode, int argc, char** argv) {
    if (mode == "--msl") {
      return printMsl(module);
    }

    if (mode == "--glsl" || mode == "--spirv") {
      return printShader(mode == "--glsl" ? GlslSource(module) : SpirvSource(module));
    }

    if (mode == "--llvm") {
      return printLlvm(module, optionsFrom(argc, argv));
    }

    return printWritten(module, mode);
  }

  int run(int argc, char** argv) {
    if (argc < 2) {
      return usage();
    }

    const Module module = ReadHirJson(readFile(argv[1]));

    return printAs(module, argc > 2 ? argv[2] : "", argc, argv);
  }

  int report(const Exception& problem) {
    cerr << "error: " << problem.what() << "\n";

    return 1;
  }

}  // namespace

int main(int argc, char** argv) {
  try {
    return run(argc, argv);
  } catch (const Exception& problem) {
    return report(problem);
  }
}
