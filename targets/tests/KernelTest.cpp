// The GPU side of the HIR: a kernel's stage and parameter roles (set by the checker, written and read as JSON, printed), and the shader text libpolyir
// writes from them (Metal for compute and reduce kernels, GLSL for the graphics stages, Vulkan GLSL and SPIR-V for compute kernels). Modules are made
// three ways: checked from polyir source, built by hand (as ninac lowers `@Kernel("gpu")`), and read back from JSON. The Vulkan GLSL and SPIR-V are
// checked against the Nina writer's output, recorded before that writer left the repository: tests/data/kernels/goldens.txt has the size and hash
// of what it printed for every kernel example, and handmade.glsl|spirv its output for the kernels made here
// and triangle.glsl the graphics stages (`KernelTest --hand-made-json` / `--triangle-json` write their HIR JSON for
// `polyir --from-hir-json`).
#include "polyir/Std.hpp"
#include "polyir/Frontend.hpp"
#include "polyir/HirJson.hpp"
#include "polyir/HirPrint.hpp"
#include "polyir/Kernels.hpp"

using namespace polyir;
using front::CheckFile;
using front::CheckSource;
using front::FrontResult;

namespace {

  int failures = 0;

  void expectEqual(const String& what, const String& want, const String& got) {
    if (want == got) {
      return;
    }

    failures++;
    cerr << "FAIL " << what << "\n--- want\n" << want << "\n--- got\n" << got << "\n";
  }

  void expectContains(const String& what, const String& text, const String& part) {
    if (text.find(part) != String::npos) {
      return;
    }

    failures++;
    cerr << "FAIL " << what << ": missing `" << part << "` in\n" << text << "\n";
  }

  // ── making nodes by hand ──

  ExprPtr number(double value) {
    auto e = make_shared<Expr>();
    e->kind = ExprKind::ConstFloat;
    e->type = MakeFloat(32);
    e->floatValue = value;

    return e;
  }

  ExprPtr local(const String& name, TypePtr type) {
    auto e = make_shared<Expr>();
    e->kind = ExprKind::Local;
    e->type = move(type);
    e->name = name;

    return e;
  }

  ExprPtr binary(BinOp op, ExprPtr left, ExprPtr right) {
    auto e = make_shared<Expr>();
    e->kind = ExprKind::Binary;
    e->type = left->type->kind == TypeKind::Matrix ? right->type : left->type;
    e->binaryOp = op;
    e->operands = {move(left), move(right)};

    return e;
  }

  StmtPtr returns(ExprPtr value) {
    auto s = make_shared<Stmt>();
    s->kind = StmtKind::Return;
    s->value = move(value);

    return s;
  }

  ExprPtr slot(const String& name, TypePtr type) {
    auto e = make_shared<Expr>();
    e->kind = ExprKind::LoadSlot;
    e->type = move(type);
    e->name = name;

    return e;
  }

  StmtPtr assigns(const String& name, TypePtr type, ExprPtr value) {
    auto s = make_shared<Stmt>();
    s->kind = StmtKind::Assign;
    s->target = slot(name, move(type));
    s->value = move(value);

    return s;
  }

  ExprPtr vector(TypePtr type, Vector<ExprPtr> parts) {
    auto e = make_shared<Expr>();
    e->kind = ExprKind::VecLit;
    e->type = move(type);
    e->operands = move(parts);

    return e;
  }

  FuncDef stageFunction(const String& name, KernelStage stage, Vector<Parameter> parameters, Vector<ParamRole> roles, TypePtr result, StmtList body) {
    FuncDef def{name, move(parameters), move(result), move(body), false, Span{}};
    def.stage = stage;
    def.roles = move(roles);

    return def;
  }

  Module moduleOf(FuncDef def) {
    Module module;
    module.functions.push_back(move(def));
    NoteAdded(module, Vector<size_t>(Counts(module).size(), 0));

    return module;
  }

  TypePtr f32() {
    return MakeFloat(32);
  }

  // ── from source ──

  const char* const kSource =
    "kernel func scale(x: float, uniform k: float) -> float {\n    return x * k\n}\n"
    "kernel func smooth(index i: i32, uniform src: ptr<float>, uniform n: i32) -> float {\n    return src[i]\n}\n";

  // The `kernel name stage roles` lines of a module's HIR text.
  String kernelLines(const Module& module) {
    IStringStream lines(ModuleText(module));
    String kept;

    for (String line; getline(lines, line);) {
      if (line.rfind("kernel ", 0) == 0) {
        kept += (kept.empty() ? "" : "\n") + line;
      }
    }

    return kept;
  }

  void sourceKernelsCarryTheirRoles() {
    const FrontResult checked = CheckSource(kSource);
    expectEqual("the checker marks the kernels", "kernel scale compute 0,1\nkernel smooth compute 3,2,1", kernelLines(checked.module));
    const FuncDef* scale = FindFunction(checked.module, "scale");
    expectEqual("scale is a compute kernel", "compute", scale == nullptr ? "missing" : StageName(scale->stage));
  }

  void jsonRoundTripKeepsThem() {
    const Module module = CheckSource(kSource).module;
    const String json = WriteHirJson(module);
    expectContains("the JSON has the stage", json, "\"stage\":\"compute\",\"roles\":[0,1]");

    const Module again = ReadHirJson(json);
    expectEqual("the JSON reads back to the same module", ModuleText(module), ModuleText(again));
    expectEqual("and writes the same text", json, WriteHirJson(again));
    expectEqual("the Metal is the same from the module read back", MetalSource(module), MetalSource(again));
  }

  void metalFromSourceHasTheEntryPoint() {
    const String metal = MetalSource(CheckSource(kSource).module);
    expectContains("the element function", metal, "inline float scale_item(float x, float k) {\n    return (x * k);\n}");
    expectContains("the entry point's buffers", metal, "kernel void scale(device const float* x_in [[buffer(0)]],\n    constant float& k [[buffer(1)]],\n    device float* __out [[buffer(2)]],");
    expectContains("an index parameter takes no buffer", metal, "kernel void smooth(device const float* src [[buffer(0)]],\n    constant int& n [[buffer(1)]],\n    device float* __out [[buffer(2)]],");
  }

  // ── made by hand ──

  void matrixAndVectorKernel() {
    const TypePtr vec4 = MakeVec(f32(), 4), mat4 = MakeMatrix(f32(), 4);
    const Module module = moduleOf(stageFunction("transform", KernelStage::Compute, {{"p", vec4}, {"m", mat4}}, {ParamRole::Element, ParamRole::Uniform}, vec4,
      {returns(binary(BinOp::Mul, local("m", mat4), local("p", vec4)))}));
    const String metal = MetalSource(module);

    expectContains("a matrix uniform", metal, "constant float4x4& m [[buffer(1)]]");
    expectContains("a vector element buffer", metal, "device const float4* p_in [[buffer(0)]]");
    expectContains("the product", metal, "return (m * p);");
  }

  void floatLiteralsReadLikeHandWrittenOnes() {
    const Module module = moduleOf(stageFunction("lit", KernelStage::Compute, {{"x", f32()}}, {ParamRole::Element}, f32(),
      {returns(binary(BinOp::Add, binary(BinOp::Mul, local("x", f32()), number(2.5)), binary(BinOp::Mul, number(0.05), number(6.2831853))))}));
    expectContains("short constants", MetalSource(module), "return ((x * 2.5) + (0.05 * 6.2831853));");
  }

  void reductionKernel() {
    const Module module = moduleOf(stageFunction("sumD", KernelStage::Reduce, {{"a", f32()}, {"b", f32()}}, {ParamRole::Element, ParamRole::Element}, f32(),
      {returns(binary(BinOp::Add, local("a", f32()), local("b", f32())))}));
    const String metal = MetalSource(module);

    expectContains("the combine", metal, "inline float sumD_combine(float a, float b) {\n    return (a + b);\n}");
    expectContains("the entry", metal, "kernel void sumD(device const float* __in [[buffer(0)]],\n    constant uint& __count [[buffer(1)]],");
    expectContains("the fold", metal, "__acc = sumD_combine(__acc, __in[__i]);");
    expectContains("the answer", metal, "__out[0] = __shared[0];");
  }

  FuncDef vertexStage() {
    const TypePtr vec3 = MakeVec(f32(), 3), vec4 = MakeVec(f32(), 4), mat4 = MakeMatrix(f32(), 4);
    const ExprPtr lifted = vector(vec4, {local("position", vec3), number(1.0)});

    return stageFunction("mainVertex", KernelStage::Vertex, {{"position", vec3}, {"mvp", mat4}, {"gl_Position", vec4}, {"vColor", vec3}},
      {ParamRole::Attribute, ParamRole::Uniform, ParamRole::Position, ParamRole::Output}, MakeVoid(),
      {assigns("gl_Position", vec4, binary(BinOp::Mul, local("mvp", mat4), lifted)), assigns("vColor", vec3, local("position", vec3))});
  }

  FuncDef fragmentStage() {
    const TypePtr vec3 = MakeVec(f32(), 3), vec4 = MakeVec(f32(), 4);

    return stageFunction("mainFragment", KernelStage::Fragment, {{"vColor", vec3}, {"fragColor", vec4}}, {ParamRole::Varying, ParamRole::Output},
      MakeVoid(), {assigns("fragColor", vec4, vector(vec4, {local("vColor", vec3), number(1.0)}))});
  }

  Module triangle() {
    Module module = moduleOf(vertexStage());
    module.functions.push_back(fragmentStage());
    NoteAdded(module, {0, 0, 0, 0, 0, 0, 0, 1});

    return module;
  }

  const char* const kVertexShader =
    "#version 330 core\nin vec3 position;\nuniform mat4 mvp;\nout vec3 vColor;\nvoid main() {\n    gl_Position = (mvp * vec4(position, 1.0));\n    vColor = position;\n}\n";

  const char* const kFragmentShader = "#version 330 core\nin vec3 vColor;\nout vec4 fragColor;\nvoid main() {\n    fragColor = vec4(vColor, 1.0);\n}\n";

  void graphicsStages() {
    const Module module = triangle();
    expectEqual("the vertex shader", kVertexShader, GlslStageSource(module, KernelStage::Vertex).text);
    expectEqual("the fragment shader", kFragmentShader, GlslStageSource(module, KernelStage::Fragment).text);
    expectEqual("graphics stages are not Metal", "", MetalSource(module));

    expectEqual("graphics stages survive JSON", kVertexShader, GlslStageSource(ReadHirJson(WriteHirJson(module)), KernelStage::Vertex).text);
    expectContains("their roles are written", WriteHirJson(module), "\"stage\":\"vertex\",\"roles\":[6,1,9,8]");
  }

  void aStageMustWriteSomething() {
    const TypePtr vec4 = MakeVec(f32(), 4);
    const Module vertex = moduleOf(stageFunction("noPosition", KernelStage::Vertex, {{"x", vec4}}, {ParamRole::Attribute}, MakeVoid(), {}));
    expectEqual("a vertex stage without a position is refused", "noPosition: a vertex stage must write its position (a `Position` parameter)",
      GlslStageSource(vertex, KernelStage::Vertex).errors.empty() ? String() : GlslStageSource(vertex, KernelStage::Vertex).errors[0]);
  }

  // ── Vulkan GLSL and SPIR-V, against the Nina writer ──

  const String kKernelData = String(POLYIR_SOURCE_DIR) + "/tests/data/kernels/";

  ExprPtr callOf(const String& name, TypePtr type, Vector<ExprPtr> args) {
    auto e = make_shared<Expr>(*local(name, move(type)));
    e->kind = ExprKind::Call;
    e->operands = move(args);

    return e;
  }

  // A vertex stage that multiplies by the 4x4 identity (`identity4(1.0)`: `float4x4(1.0)` / `mat4(1.0)`) and takes a float remainder.
  Module stageExtras() {
    const TypePtr vec4 = MakeVec(f32(), 4), mat4 = MakeMatrix(f32(), 4);
    const ExprPtr identity = callOf("identity4", mat4, {number(1.0)});

    return moduleOf(stageFunction("extras", KernelStage::Vertex, {{"p", vec4}, {"k", f32()}, {"gl_Position", vec4}, {"shade", f32()}},
      {ParamRole::Attribute, ParamRole::Uniform, ParamRole::Position, ParamRole::Output}, MakeVoid(),
      {assigns("gl_Position", vec4, binary(BinOp::Mul, identity, local("p", vec4))), assigns("shade", f32(), binary(BinOp::Rem, local("k", f32()), number(3.0)))}));
  }

  ExprPtr castOf(ExprPtr x, TypePtr type) {
    auto e = make_shared<Expr>(*local("", move(type)));
    e->kind = ExprKind::Cast;
    e->operands = {move(x)};

    return e;
  }

  // `twice` calls the kernel `wrap` and scales its answer by the thread's index.
  FuncDef twiceKernel() {
    const TypePtr i32 = MakeInt(32);

    return stageFunction("twice", KernelStage::Compute, {{"x", f32()}, {"i", i32}}, {ParamRole::Element, ParamRole::Index}, f32(),
      {returns(binary(BinOp::Mul, callOf("wrap", f32(), {local("x", f32()), number(2.0)}), castOf(local("i", i32), f32())))});
  }

  // `wrap` takes a float remainder by a uniform.
  Module handMadeKernels() {
    Module module = moduleOf(stageFunction("wrap", KernelStage::Compute, {{"x", f32()}, {"k", f32()}}, {ParamRole::Element, ParamRole::Uniform}, f32(),
      {returns(binary(BinOp::Rem, local("x", f32()), local("k", f32())))}));
    const Vector<size_t> before = Counts(module);
    module.functions.push_back(twiceKernel());
    NoteAdded(module, before);

    return module;
  }

  // What `polyir --emit-glsl|--emit-spirv` prints: the text and a newline.
  String cliOutput(const Module& module, const String& mode) {
    const ShaderText shader = mode == "glsl" ? GlslSource(module) : SpirvSource(module);

    return shader.errors.empty() ? shader.text + "\n" : "errors: " + shader.errors[0];
  }

  String readText(const String& path) {
    IFStream in(path);
    StringStream text;
    text << in.rdbuf();

    return text.str();
  }

  // The size of a text and its 64-bit FNV-1a hash, as goldens.txt holds them.
  String fingerprint(const String& text) {
    uint64_t hash = 0xcbf29ce484222325ULL;

    for (unsigned char c : text) {
      hash = (hash ^ c) * 0x100000001b3ULL;
    }

    char hex[17];
    snprintf(hex, sizeof hex, "%016llx", static_cast<unsigned long long>(hash));

    return to_string(text.size()) + " " + hex;
  }

  // One line of goldens.txt: an example, the output, and the size and hash of what the Nina CLI printed. The example's path is from the
  // repository root (core/examples or ideas/*/examples).
  void exampleMatchesNina(const String& line) {
    IStringStream fields(line);
    String path, mode, size, hash;
    fields >> path >> mode >> size >> hash;
    const Module module = CheckFile(String(POLYIR_SOURCE_DIR) + "/../" + path).module;
    expectEqual(path + " --emit-" + mode + " (size and hash of the Nina output)", size + " " + hash, fingerprint(cliOutput(module, mode)));
  }

  void examplesMatchNina() {
    IFStream goldens(kKernelData + "goldens.txt");
    int count = 0;

    for (String line; getline(goldens, line); count++) {
      exampleMatchesNina(line);
    }

    expectEqual("every kernel example was compared", "true", count >= 100 ? "true" : "false");
  }

  // A compute kernel `ident(p: float4) -> float4 { return identity4(1.0) * p }`.
  Module identityCompute() {
    return moduleOf(stageFunction("ident", KernelStage::Compute, {{"p", MakeVec(f32(), 4)}}, {ParamRole::Element}, MakeVec(f32(), 4),
      {returns(binary(BinOp::Mul, callOf("identity4", MakeMatrix(f32(), 4), {number(1.0)}), local("p", MakeVec(f32(), 4))))}));
  }

  // The identity matrix and the remainder of floats in a graphics stage, which used to be refused. Checks the same text as the Nina writer's,
  // and Metal's spelling.
  void identityAndRemainderInStages() {
    const Module module = stageExtras();
    expectEqual("the stage as --emit-glsl writes it", readText(kKernelData + "stages.glsl"), cliOutput(module, "glsl"));

    const String shader = GlslStageSource(module, KernelStage::Vertex).text;
    expectContains("the identity", shader, "gl_Position = (mat4(1.0) * p);");
    expectContains("the remainder, truncated by hand", shader, "shade = (k - 3.0 * trunc(k / 3.0));");
    expectContains("Metal's identity", MetalSource(identityCompute()), "return (float4x4(1.0) * p);");
  }

  void handMadeMatchesNina() {
    const Module module = handMadeKernels();
    expectEqual("hand-made kernels as Vulkan GLSL", readText(kKernelData + "handmade.glsl"), cliOutput(module, "glsl"));
    expectEqual("hand-made kernels as SPIR-V", readText(kKernelData + "handmade.spirv"), cliOutput(module, "spirv"));
    expectEqual("graphics stages as --emit-glsl writes them", readText(kKernelData + "triangle.glsl"), cliOutput(triangle(), "glsl"));
    expectEqual("the same SPIR-V after a JSON round trip", cliOutput(module, "spirv"), cliOutput(ReadHirJson(WriteHirJson(module)), "spirv"));
  }

  void spirvModuleWords() {
    const Module module = handMadeKernels();
    const SpirvBinary binary = SpirvModule(module, "twice");
    expectEqual("a SPIR-V module starts with the magic number", "119734787", binary.words.empty() ? "empty" : to_string(binary.words[0]));
    expectContains("its hex is the section --emit-spirv writes", cliOutput(module, "spirv"), "// ==== twice.spv ====\n" + SpirvHex(binary.words));
    expectEqual("an unknown kernel", "no compute kernel named nope", SpirvModule(module, "nope").errors.at(0));
  }

  // A uniform value that cannot travel in the push constants is the writer's reason, not a module.
  void spirvRefusesWideUniforms() {
    const TypePtr i64 = MakeInt(64);
    const Module module = moduleOf(stageFunction("wide", KernelStage::Compute, {{"x", f32()}, {"n", i64}}, {ParamRole::Element, ParamRole::Uniform}, f32(),
      {returns(local("x", f32()))}));
    expectEqual("a 64-bit uniform", "wide: the uniform value n must be a 32-bit number to travel in the push constants", SpirvSource(module).errors.at(0));
  }

  // The checks of the kernels from source and of the Metal writer, in the order they run.
  void sourceAndMetalChecks() {
    sourceKernelsCarryTheirRoles();
    jsonRoundTripKeepsThem();
    metalFromSourceHasTheEntryPoint();

    matrixAndVectorKernel();
    floatLiteralsReadLikeHandWrittenOnes();
    reductionKernel();
  }

  // The graphics stages, the comparisons with the Nina writer, and SPIR-V, in the order they run.
  void stageNinaAndSpirvChecks() {
    graphicsStages();
    aStageMustWriteSomething();

    examplesMatchNina();
    handMadeMatchesNina();
    identityAndRemainderInStages();

    spirvModuleWords();
    spirvRefusesWideUniforms();
  }

  void runChecks() {
    sourceAndMetalChecks();
    stageNinaAndSpirvChecks();
  }

  // Writes the HIR JSON of `kernels`; the exit status says whether the output stream took it.
  int printJson(const Module& kernels) {
    cout << WriteHirJson(kernels) << "\n";

    return cout ? 0 : 1;
  }

}  // namespace

// `KernelTest --hand-made-json` (and `--triangle-json`, `--stage-extras-json`) writes the HIR JSON of the kernels made here by hand. It
// holds the graphics stages and the extra stage features, for another writer to be compared on.
int main(int argc, char** argv) {
  const String flag = argc > 1 ? String(argv[1]) : String();

  if (flag == "--hand-made-json") {
    return printJson(handMadeKernels());
  }

  if (flag == "--triangle-json") {
    return printJson(triangle());
  }

  if (flag == "--stage-extras-json") {
    return printJson(stageExtras());
  }

  runChecks();
  cout << (failures == 0 ? "ok" : "FAILED") << "\n";

  return failures == 0 ? 0 : 1;
}
