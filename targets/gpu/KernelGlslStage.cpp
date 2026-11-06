// GLSL for the graphics stages of a Module (a `Vertex` and a `Fragment` function). A stage is the declarations of what comes in and goes out, then
// `void main()` holding the function's body:
//
//   #version 330 core
//   in vec3 position;          an `Attribute` (vertex) or a `Varying` (fragment) parameter
//   uniform mat4 mvp;          a `Uniform` parameter
//   out vec3 vColor;           an `Output` parameter (`Position` is `gl_Position` and is not declared)
//   void main() { ... }
#include "polyir/Kernels.hpp"
#include "gpu/KernelCommon.hpp"

namespace polyir {

  namespace {

    const char* const kVersion = "#version 330 core\n";

    // `bvec`, `ivec` or `vec`, by the element type.
    const char* vectorPrefix(const TypePtr& element) {
      if (IsBoolean(*element)) {
        return "b";
      }

      if (IsInteger(*element)) {
        return "i";
      }

      return "";
    }

    String vectorName(const TypePtr& element, int count) {
      return String(vectorPrefix(element)) + "vec" + to_string(count);
    }

    // A vector or a matrix; any other type cannot be used in a shader.
    String aggregateType(const TypePtr& t) {
      if (t->kind == TypeKind::Vec) {
        return vectorName(t->element, t->size);
      }

      if (t->kind == TypeKind::Matrix) {
        return "mat" + to_string(t->size);
      }

      throw RuntimeError("the type " + TypeText(*t) + " cannot be used in a shader");
    }

    String glslType(const TypePtr& t) {
      switch (t->kind) {
        case TypeKind::Int:
          return IsBoolean(*t) ? "bool" : "int";
        case TypeKind::UInt:
          return "uint";
        case TypeKind::Float:
          return "float";
        default:
          return aggregateType(t);
      }
    }

    // Writes the statements and expressions of one stage body; a thing a shader cannot do throws, and the stage reports it.
    // The functions call each other, so they are declared first.
    namespace glsl_body {

      String block(const StmtList& stmts, int depth);

      String expr(const Expr& e);

      String stmt(const Stmt& s, int depth);

      String flow(const Stmt& s, int depth);

      String operation(const Expr& e);

      String compound(const Expr& e);

      String declaration(const Stmt& s, int depth);

      String conditional(const Stmt& s, int depth);

      String loop(const Stmt& s, int depth);

      String exprList(const Vector<ExprPtr>& es, size_t from = 0);

      String call(const Expr& e);

      String binary(const Expr& e);

      String remainder(const Expr& left, const Expr& right);

      String unary(const Expr& e);

      String block(const StmtList& stmts, int depth) {
        String out;

        for (const StmtPtr& s : stmts) {
          out += stmt(*s, depth);
        }

        return out;
      }

      String exprList(const Vector<ExprPtr>& es, size_t from) {
        Vector<String> parts;

        for (size_t i = from; i < es.size(); i++) {
          parts.push_back(expr(*es[i]));
        }

        return Joined(parts, ", ");
      }

      String stmt(const Stmt& s, int depth) {
        const String pad = ShaderIndent(depth);

        switch (s.kind) {
          case StmtKind::Let: case StmtKind::Var:
            return declaration(s, depth);
          case StmtKind::Assign:
            return pad + expr(*s.target) + " = " + expr(*s.value) + ";\n";
          case StmtKind::ExprStmt:
            return pad + expr(*s.value) + ";\n";
          default:
            return flow(s, depth);
        }
      }

      String flow(const Stmt& s, int depth) {
        switch (s.kind) {
          case StmtKind::Return:
            return ShaderIndent(depth) + "return;\n";
          case StmtKind::If:
            return conditional(s, depth);
          case StmtKind::For:
            return loop(s, depth);
          default:
            throw RuntimeError("a statement a shader cannot run");
        }
      }

      // A `var` with no value starts at zero.
      String declaration(const Stmt& s, int depth) {
        const String start = s.value ? expr(*s.value) : glslType(s.type) + "(0)";

        return ShaderIndent(depth) + glslType(s.type) + " " + s.name + " = " + start + ";\n";
      }

      String conditional(const Stmt& s, int depth) {
        const String head = ShaderIndent(depth) + "if (" + expr(*s.value) + ") {\n" + block(s.body, depth + 1);

        if (s.elseBody.empty()) {
          return head + ShaderIndent(depth) + "}\n";
        }

        return head + ShaderIndent(depth) + "} else {\n" + block(s.elseBody, depth + 1) + ShaderIndent(depth) + "}\n";
      }

      // A polyir `for` carries named values from one round to the next. Each round works them out from the OLD values first (see the Metal writer).
      String loop(const Stmt& s, int depth) {
        const String pad = ShaderIndent(depth);
        String head = pad + "{\n";

        for (size_t i = 0; i < s.names.size(); i++) {
          head += ShaderIndent(depth + 1) + glslType(s.initializers[i]->type) + " " + s.names[i] + " = " + expr(*s.initializers[i]) + ";\n";
        }

        String next;

        for (size_t i = 0; i < s.names.size(); i++) {
          next += ShaderIndent(depth + 2) + glslType(s.steps[i]->type) + " " + s.names[i] + "_next = " + expr(*s.steps[i]) + ";\n";
        }

        for (size_t i = 0; i < s.names.size(); i++) {
          next += ShaderIndent(depth + 2) + s.names[i] + " = " + s.names[i] + "_next;\n";
        }

        return head + pad + "    while (" + expr(*s.value) + ") {\n" + block(s.body, depth + 2) + next + pad + "    }\n" + pad + "}\n";
      }

      String expr(const Expr& e) {
        switch (e.kind) {
          case ExprKind::ConstInt:
            return to_string(e.intValue);
          case ExprKind::ConstFloat:
            return FloatLiteral(e.floatValue);
          case ExprKind::ConstBool:
            return e.boolValue ? "true" : "false";
          case ExprKind::Local: case ExprKind::LoadSlot:
            return e.name;
          default:
            return operation(e);
        }
      }

      String operation(const Expr& e) {
        switch (e.kind) {
          case ExprKind::Binary:
            return binary(e);
          case ExprKind::Unary:
            return unary(e);
          default:
            return compound(e);
        }
      }

      String compound(const Expr& e) {
        switch (e.kind) {
          case ExprKind::Cast: case ExprKind::Coerce:
            return glslType(e.type) + "(" + expr(*e.operands[0]) + ")";
          case ExprKind::VecLit:
            return glslType(e.type) + "(" + exprList(e.operands) + ")";
          case ExprKind::IndexGet:
            return expr(*e.operands[0]) + "[" + exprList(e.operands, 1) + "]";
          case ExprKind::Call:
            return call(e);
          default:
            throw RuntimeError("an expression a shader cannot evaluate");
        }
      }

      // C's `fmod`: the sign of the left one, a size below the right one's. GLSL's `mod` rounds the other way, so the quotient is truncated by hand.
      String remainder(const Expr& left, const Expr& right) {
        const String x = expr(left), y = expr(right);

        return "(" + x + " - " + y + " * trunc(" + x + " / " + y + "))";
      }

      String binary(const Expr& e) {
        if (e.binaryOp == BinOp::Rem && IsFloat(*e.operands[0]->type)) {
          return remainder(*e.operands[0], *e.operands[1]);
        }

        return "(" + expr(*e.operands[0]) + " " + OperatorSymbol(e.binaryOp) + " " + expr(*e.operands[1]) + ")";
      }

      String unary(const Expr& e) {
        return UnaryPrefix(e.unaryOp) + expr(*e.operands[0]) + ")";
      }

      String call(const Expr& e) {
        if (e.name == "fmodf" && e.operands.size() == 2) {
          return remainder(*e.operands[0], *e.operands[1]);
        }

        const String name = GlslMathName(e.name);

        if (!IsKernelMath(e.name) || name.empty()) {
          throw RuntimeError("the call to " + e.name + ", which a GPU does not have");
        }

        return name + "(" + exprList(e.operands) + ")";
      }
    }  // namespace glsl_body

    using glsl_body::block;

    bool isInput(const FuncDef& def, ParamRole role) {
      return role == ParamRole::Attribute || (def.stage == KernelStage::Fragment && role == ParamRole::Varying);
    }

    String declaration(const FuncDef& def, size_t i) {
      const String type = glslType(def.parameters[i].type), name = def.parameters[i].name;
      const ParamRole role = def.roles[i];

      if (isInput(def, role)) {
        return "in " + type + " " + name + ";\n";
      }

      if (role == ParamRole::Uniform) {
        return "uniform " + type + " " + name + ";\n";
      }

      return role == ParamRole::Output ? "out " + type + " " + name + ";\n" : "";
    }

    // `in vec3 position;` for what comes in, then `uniform mat4 mvp;`, in parameter order; then the `out` declarations.
    String declarations(const FuncDef& def) {
      String ins;
      String outs;

      for (size_t i = 0; i < def.parameters.size(); i++) {
        if (def.roles[i] == ParamRole::Output) {
          outs += declaration(def, i);
        } else {
          ins += declaration(def, i);
        }
      }

      return ins + outs;
    }

    bool hasRole(const FuncDef& def, ParamRole role) {
      return find(def.roles.begin(), def.roles.end(), role) != def.roles.end();
    }

    // A vertex stage that never writes `gl_Position`, or a fragment stage with nothing to write, would compile to a shader that does nothing useful.
    String stageText(const FuncDef& def) {
      if (def.stage == KernelStage::Vertex && !hasRole(def, ParamRole::Position)) {
        throw RuntimeError("a vertex stage must write its position (a `Position` parameter)");
      }

      if (def.stage == KernelStage::Fragment && !hasRole(def, ParamRole::Output)) {
        throw RuntimeError("a fragment stage must write a colour (an `Output` parameter)");
      }

      return String(kVersion) + declarations(def) + "void main() {\n" + block(def.body, 1) + "}\n";
    }

    // The shader of one stage function, or the reason it cannot be written.
    void shaderOf(const FuncDef& def, ShaderText& result) {
      try {
        result.text = stageText(def);
      } catch (const RuntimeError& problem) {
        result.errors.push_back(def.name + ": " + problem.what());
      }
    }

    bool isGraphics(const FuncDef& def) {
      return def.stage == KernelStage::Vertex || def.stage == KernelStage::Fragment;
    }

    // `// ==== name.vert ====` (or `.frag`) and the stage's shader, after the sections before it; or the reason it cannot be written.
    void addSection(const FuncDef& def, ShaderText& result) {
      try {
        const String head = "// ==== " + def.name + (def.stage == KernelStage::Vertex ? ".vert" : ".frag") + " ====\n";
        result.text += (result.text.empty() ? "" : "\n") + head + stageText(def);
      } catch (const RuntimeError& problem) {
        result.errors.push_back(def.name + ": " + problem.what());
      }
    }

    bool hasGraphicsStage(const Module& module) {
      for (const FuncDef& def : module.functions) {
        if (isGraphics(def)) {
          return true;
        }
      }

      return false;
    }

    // Every function of this stage, in module order.
    void addStageSections(const Module& module, KernelStage stage, ShaderText& result) {
      for (const FuncDef& def : module.functions) {
        if (def.stage == stage) {
          addSection(def, result);
        }
      }
    }

  }  // namespace

  // A module with no graphics stage is written as Vulkan compute shaders instead.
  ShaderText GlslSource(const Module& module) {
    if (!hasGraphicsStage(module)) {
      return ShaderText{GlslComputeSource(module), {}};
    }

    ShaderText result;

    for (KernelStage stage : {KernelStage::Vertex, KernelStage::Fragment}) {
      addStageSections(module, stage, result);
    }

    if (!result.errors.empty()) {
      result.text.clear();
    }

    return result;
  }

  ShaderText GlslStageSource(const Module& module, KernelStage stage) {
    ShaderText result;

    for (const FuncDef& def : module.functions) {
      if (def.stage == stage) {
        shaderOf(def, result);
      }
    }

    if (!result.errors.empty()) {
      result.text.clear();
    }

    return result;
  }

}  // namespace polyir
