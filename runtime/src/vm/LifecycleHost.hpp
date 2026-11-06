// The std Lifecycle native on the VM (ninac/sem/std/build/Lifecycle.nina): `Lifecycle.inProcess` (behind `Lifecycle.step`)
// hands one lifecycle step to the host that embeds the VM (VmOptions::hostStep), so a lifecycle script's `Compiler.compile` runs in nina's own process instead
// of re-running nina as a child. The lowering names the extern `<function>__host` (ninac/codegen/polyir-lower/HostNatives.cpp).
#pragma once

#include "vm/Machine.hpp"

namespace polyir::vm {

  // The extern std `Lifecycle.inProcess` lowers to.
  extern const char* const LIFECYCLE_IN_PROCESS_EXTERN;

  bool BindsLifecycle(const String& name);

  // `(verb, dir, args)` in, the step's exit status out.
  Outcome CallLifecycle(Machine& m, const Vector<Value>& args);

}  // namespace polyir::vm
