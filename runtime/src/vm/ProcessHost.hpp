// The std Process natives (ninac/sem/std/process/Process.nina) bound on the VM, so a Nina program that runs commands (every lifecycle
// script does, through std Build) runs on the bytecode route. The lowering names each one's extern `<function>__host`
// (ninac/codegen/polyir-lower/HostNatives.cpp): `Process.shell` is `Process__shell__host`.
#pragma once

#include "vm/Machine.hpp"

namespace polyir::vm {

  bool BindsProcess(const String& name);

  // Consulted by Host.cpp's CallBoundExtern. A `Run` or `Job` answer is built as the struct the extern's declared result names.
  Outcome CallProcess(Machine& m, const String& name, const Vector<Value>& args);

}  // namespace polyir::vm
