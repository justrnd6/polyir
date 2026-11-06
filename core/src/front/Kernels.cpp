// Finds the `kernel func`s of a checked program and verifies each one (Kernels.nina `kernelNames`, `problems`, `callableBefore`; KernelRoles.nina `of`).
#include "front/Kernels.hpp"

namespace polyir::front {

  namespace {

    // A parameter's role, from its mark; a `uniform` one is a buffer when it is a pointer and a single value otherwise.
    KernelRole roleOf(const Program& program, const AstParam& param) {
      switch (param.mark) {
        case ParamMark::Index:
          return KernelRole::Index;
        case ParamMark::Writable:
          return KernelRole::Writable;
        case ParamMark::Workgroup:
          return KernelRole::Workgroup;
        case ParamMark::None:
          return KernelRole::Element;
        default:
          return program.types[param.type].kind == AstTypeKind::Ptr ? KernelRole::Buffer : KernelRole::Value;
      }
    }

    // A kernel declaration found in a unit: where it is and what it is called.
    struct KernelDecl {
      const Program* program = nullptr;
      const AstFuncDecl* decl = nullptr;
      String name;
    };

    // The `kernel func`s among a unit's items, in source order.
    void addKernels(const Session& session, const Unit& unit, Vector<KernelDecl>& out) {
      for (uint32_t i = unit.first; i < unit.last; i++) {
        const AstItem& item = unit.program->items[i];

        if (item.kind != ItemKind::Func) {
          continue;
        }

        const AstFuncDecl& decl = unit.program->funcs.items[item.index];

        if (decl.isKernel) {
          out.push_back(KernelDecl{unit.program, &decl, Str(session, decl.name.name)});
        }
      }
    }

    Vector<KernelDecl> kernelsOf(const Session& session) {
      Vector<KernelDecl> out;

      for (const Unit& unit : session.units) {
        addKernels(session, unit, out);
      }

      return out;
    }

    // The first function of the module called `name`, and (in `index`) where it is in the module; null when there is none.
    const FuncDef* functionNamed(const Session& session, const String& name, size_t& index) {
      for (index = 0; index < session.module.functions.size(); index++) {
        if (session.module.functions[index].name == name) {
          return &session.module.functions[index];
        }
      }

      return nullptr;
    }

    bool usesWorkgroupMemory(const KernelDecl& kernel) {
      const Vector<KernelRole> roles = KernelRoles(*kernel.program, *kernel.decl);

      return find(roles.begin(), roles.end(), KernelRole::Workgroup) != roles.end();
    }

    // The kernels declared before kernel number `position`, as KernelProblems wants them ("name", or "grouped name").
    Vector<String> callableBefore(const Vector<KernelDecl>& kernels, size_t position) {
      Vector<String> out;

      for (size_t i = 0; i < position; i++) {
        out.push_back(usesWorkgroupMemory(kernels[i]) ? "grouped " + kernels[i].name : kernels[i].name);
      }

      return out;
    }

    // The messages once each, first occurrence first, with the kernel's file in front.
    void addLabelled(const Session& session, size_t index, const Vector<String>& found, Vector<String>& out) {
      const String& file = session.files[session.units[session.sources[index].unit].file];
      Vector<String> seen;

      for (const String& message : found) {
        if (IsOneOf(message, seen)) {
          continue;
        }

        seen.push_back(message);
        out.push_back(file.empty() ? message : file + ":" + message);
      }
    }

    // The problems of kernel number `position`, checked on its HIR.
    void verifyKernel(const Session& session, const Vector<KernelDecl>& kernels, size_t position, Vector<String>& out) {
      size_t index = 0;
      const FuncDef* function = functionNamed(session, kernels[position].name, index);

      if (function == nullptr) {
        return;
      }

      const Vector<KernelRole> roles = KernelRoles(*kernels[position].program, *kernels[position].decl);
      addLabelled(session, index, KernelProblems(*function, roles, callableBefore(kernels, position)), out);
    }

  }  // namespace

  Vector<KernelRole> KernelRoles(const Program& program, const AstFuncDecl& decl) {
    Vector<KernelRole> roles;

    for (const AstParam& param : program.params[decl.params]) {
      roles.push_back(roleOf(program, param));
    }

    return roles;
  }

  namespace {

    // The HIR's name for a role. The cast is safe because KernelRole's numbers are ParamRole's first six (Kernels.hpp); only the names differ
    // in one place: KernelRole::Value is ParamRole::Uniform.
    ParamRole paramRoleOf(KernelRole role) {
      return static_cast<ParamRole>(role);
    }

  }  // namespace

  void MarkKernel(FuncDef& function, const Program& program, const AstFuncDecl& decl) {
    function.stage = KernelStage::Compute;

    for (KernelRole role : KernelRoles(program, decl)) {
      function.roles.push_back(paramRoleOf(role));
    }
  }

  KernelReport CheckKernels(const Session& session) {
    KernelReport report;
    const Vector<KernelDecl> kernels = kernelsOf(session);
    report.count = static_cast<int>(kernels.size());

    for (size_t i = 0; i < kernels.size(); i++) {
      verifyKernel(session, kernels, i, report.problems);
    }

    return report;
  }

}  // namespace polyir::front
