// What every kernel writer (Metal, GLSL and SPIR-V) shares, see KernelCommon.hpp: how a math function a kernel calls is spelled in
// C, Metal and GLSL, the words each shading language reserves (so a kernel's own names never clash with them), how a float literal
// is written, the text helpers of the Metal and GLSL writers, and small questions about parameters and calls.
// The three math tables line up: entry i of each names the same function.
#include "gpu/KernelCommon.hpp"

namespace polyir {

  namespace {

    const Vector<String> kMathC = {"sqrtf", "sinf", "cosf", "tanf", "expf", "logf", "powf", "floorf", "ceilf", "fabsf", "fminf", "fmaxf", "fmodf",
      "dot", "cross", "normalize", "length", "identity4"};  // the last five are vector and matrix functions: a GPU has them built in, and they are declared `extern` for the checker (`identity4(1.0)` is the 4x4 identity)

    // Metal compiles with fast math by default, and its fast `fmod` answers a wrong non-zero remainder for some exactly divisible pairs
    // (`fmod(2867.0, 47.0)`), so `fmodf` is spelled `metal::precise::fmod`, which is exact in any mode.
    const Vector<String> kMathMsl = {"sqrt", "sin", "cos", "tan", "exp", "log", "pow", "floor", "ceil", "fabs", "fmin", "fmax", "metal::precise::fmod",
      "dot", "cross", "normalize", "length", "float4x4"};

    // GLSL's names for the same functions; `fmodf` has none (GLSL's `mod` rounds differently), so a shader that remainders floats is declined.
    const Vector<String> kMathGlsl = {"sqrt", "sin", "cos", "tan", "exp", "log", "pow", "floor", "ceil", "abs", "min", "max", "",
      "dot", "cross", "normalize", "length", "mat4"};

    const Vector<String> kMetalTypeWords = {"half", "float", "int", "uint", "short", "ushort", "char", "uchar", "long", "ulong", "bool"};

    const Vector<String> kMetalWords = {"kernel", "device", "constant", "thread", "threadgroup", "vertex", "fragment", "texture1d", "texture2d", "texture3d",
      "sampler", "namespace", "using", "template", "class", "struct", "union", "enum", "auto", "register", "volatile", "explicit", "typename", "inline",
      "static", "extern", "main", "metal", "simd", "atomic", "size_t", "ptrdiff_t", "uniform", "packed", "ray", "mesh", "object", "visible", "private",
      "public", "protected", "this", "new", "delete", "default", "switch", "case", "goto", "do", "while", "typedef", "const", "operator", "friend",
      "virtual", "mutable", "signed", "unsigned", "void", "sizeof", "true", "false", "nullptr", "NULL"};

    // The words GLSL reserves (KernelNames.nina `glslWords`); a kernel's name that is one of them, starts with `gl_` or is a vector type gets `_` added.
    const Vector<String> kGlslWords = {"input", "output", "filter", "sample", "shared", "buffer", "uniform", "layout", "in", "out", "inout", "attribute",
      "varying", "patch", "precision", "highp", "mediump", "lowp", "coherent", "volatile", "restrict", "readonly", "writeonly", "subroutine", "resource",
      "smooth", "flat", "centroid", "noperspective", "invariant", "discard", "common", "partition", "active", "asm", "class", "union", "enum", "typedef",
      "template", "this", "packed", "goto", "inline", "noinline", "public", "static", "extern", "external", "interface", "long", "short", "half", "fixed",
      "unsigned", "superp", "namespace", "using", "sizeof", "cast", "default", "switch", "case", "do", "main", "image1D", "image2D", "image3D",
      "sampler1D", "sampler2D", "sampler3D", "texture", "float16_t", "int8_t", "int16_t", "int32_t", "int64_t", "uint8_t", "uint16_t", "uint32_t",
      "uint64_t", "float32_t", "float64_t", "double", "float", "int", "uint", "bool", "void", "const", "break", "continue", "if", "else", "for", "while",
      "return", "struct", "true", "false", "mat", "vec"};

    const Vector<String> kGlslVectorWords = {"vec", "ivec", "uvec", "bvec", "dvec", "mat"};

    // The position of `name` in one of the word tables above, or -1.
    int indexIn(const Vector<String>& words, const String& name) {
      for (size_t i = 0; i < words.size(); i++) {
        if (name == words[i]) {
          return static_cast<int>(i);
        }
      }

      return -1;
    }

    // A Metal scalar type, or its vector of 2, 3 or 4 (`float3`).
    bool isMetalType(const String& name) {
      for (const String& base : kMetalTypeWords) {
        for (const char* suffix : {"", "2", "3", "4"}) {
          if (name == base + suffix) {
            return true;
          }
        }
      }

      return false;
    }

    // A GLSL vector or matrix type of 2, 3 or 4 (`vec3`, `mat4`).
    bool isGlslVector(const String& name) {
      for (const String& base : kGlslVectorWords) {
        for (const char* suffix : {"2", "3", "4"}) {
          if (name == base + suffix) {
            return true;
          }
        }
      }

      return false;
    }

    // Entry i is the operator BinOp declares i-th (core/include/polyir/Hir.hpp), so the order must follow that enum. The last two,
    // `===` and `!==`, have no shader spelling, hence "?".
    const char* const kOperatorSymbols[] = {"+", "-", "*", "/", "%", "&", "|", "^", "<<", ">>", "==", "!=", "<", "<=", ">", ">=", "&&", "||",
      "?", "?"};

  }  // namespace

  String GlslIdentifier(const String& name) {
    const bool isReserved = indexIn(kGlslWords, name) >= 0 || name.starts_with("gl_") || isGlslVector(name);

    return isReserved ? name + "_" : name;
  }

  bool IsKernelAtomic(const String& name) {
    return name == "atomicAdd" || name == "atomicAddFloat" || name == "atomicMin" || name == "atomicMax";
  }

  bool Listed(const Vector<String>& names, const String& name) {
    return find(names.begin(), names.end(), name) != names.end();
  }

  bool IsKernelMath(const String& name) {
    return indexIn(kMathC, name) >= 0;
  }

  String MslMathName(const String& name) {
    const int at = indexIn(kMathC, name);

    return at < 0 ? name : kMathMsl[static_cast<size_t>(at)];
  }

  String ScientificLiteral(double v) {
    char buffer[40];
    snprintf(buffer, sizeof buffer, "%.9g", v);

    return String(buffer).find_first_of(".e") == String::npos ? String(buffer) + ".0" : buffer;
  }

  String GlslMathName(const String& name) {
    const int at = indexIn(kMathC, name);

    return at < 0 ? name : kMathGlsl[static_cast<size_t>(at)];
  }

  // The six-decimal text of a number with its trailing zeros dropped (`3000000.0`), for one too big to round to nine decimals exactly.
  static String plainLiteral(double v) {
    String text = to_string(v);

    while (text.size() > 2 && text.back() == '0' && text[text.size() - 2] != '.') {
      text.pop_back();
    }

    return text;
  }

  // A number rounded to nine decimals: whole part, point, fraction without its trailing zeros (one digit is always kept).
  static String nineDecimals(double size) {
    const long long scaled = llround(size * 1e9);
    String fraction = to_string(scaled % 1000000000LL);
    fraction = String(9 - fraction.size(), '0') + fraction;

    while (fraction.size() > 1 && fraction.back() == '0') {
      fraction.pop_back();
    }

    return to_string(scaled / 1000000000LL) + "." + fraction;
  }

  // The literal for a number: `2.5`, `1.0`, `0.05`, `6.2831853`. A number from a ten-thousandth to a million is rounded to nine decimals. Its
  // trailing zeros are dropped (it always keeps a point, so it never reads as an integer). A bigger one up to a billion is written plain. A
  // smaller or bigger one (`1e-07`) is written with all the digits it needs.
  String FloatLiteral(double v) {
    const double size = v < 0 ? -v : v;

    if ((size < 1e-4 && size != 0.0) || size >= 1e9) {
      return ScientificLiteral(v);
    }

    if (size >= 1e6) {
      return plainLiteral(v);
    }

    return String(v < 0 ? "-" : "") + nineDecimals(size);
  }

  bool IsGroupFunction(const String& name) {
    return name == "groupSize" || name == "localIndex" || name == "groupIndex" || name == "barrier";
  }

  String MslIdentifier(const String& name) {
    const bool isReserved = isMetalType(name) || indexIn(kMetalWords, name) >= 0;

    return isReserved ? name + "_" : name;
  }

  bool HasBuffer(ParamRole role) {
    return role != ParamRole::Index && role != ParamRole::Workgroup;
  }

  int IndexCount(const Vector<ParamRole>& roles) {
    int count = 0;

    for (ParamRole role : roles) {
      if (role == ParamRole::Index) {
        count++;
      }
    }

    return count;
  }

  // ── text helpers of the Metal and GLSL writers ──

  String ShaderIndent(int depth) {
    return String(static_cast<size_t>(depth) * 4, ' ');
  }

  String Joined(const Vector<String>& parts, const String& separator) {
    String out;

    for (size_t i = 0; i < parts.size(); i++) {
      out += (i > 0 ? separator : "") + parts[i];
    }

    return out;
  }

  // The search goes on after the text put in, so a `to` that contains `from` is not replaced again.
  String ReplacedAll(String text, const String& from, const String& to) {
    for (size_t at = text.find(from); at != String::npos; at = text.find(from, at + to.size())) {
      text.replace(at, from.size(), to);
    }

    return text;
  }

  const char* OperatorSymbol(BinOp op) {
    return kOperatorSymbols[static_cast<int>(op)];
  }

  const char* UnaryPrefix(UnOp op) {
    switch (op) {
      case UnOp::Neg:
        return "(-";
      case UnOp::LogNot:
        return "(!";
      default:
        return "(~";
    }
  }

}  // namespace polyir
