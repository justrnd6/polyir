// The `nina_*` externs a Nina program declares when ninac lowers it through polyir_lower (ninac/codegen/polyir-lower: Runtime.cpp's
// kAbi, CollectionAbi.cpp), supplied by the VM itself so a `target bytecode` program runs in-process. Their semantics are the LLVM route's
// runtime pieces (ninac/codegen/llvm-polyir/PolyirRuntimeText.cpp, PolyirRuntimeStrings.cpp from runtime/NinaText.c): the llvm build is the
// reference a Nina program is checked against (targets_via_polyir.sh), so the VM prints, slices and files as that IR does, byte for byte.
//
// Bound here: printing (`nina_print_text`, `nina_println_text`, `nina_eprint_text`, `nina_eprintln_text`, `nina_process_exit`), number text
// (`nina_text_from_{bool,i64,u64,i128,u128,f64,f64_lit}`), the String natives (`nina_text_*`), the Console natives (`nina_console_read_line`,
// `nina_text_parse_{f64,i64}`), Math (`nina_math_*`) and the FileHost primitives (`nina_fs_*`). The externs the other lowering options
// declare stay unbound, so a program declaring one is declined before it runs (Preflight.cpp): `nina_task_later` (asyncTasks, js only),
// `<f>__host` (hostNatives; only the std Process natives are bound, in ProcessHost.cpp), `nina_host_error`, `nina_as_*` and
// `nina_host_text_*` (hostTypes), `nina_convert_*` (defined on no route) and `<f>__llvm` (a nativeimpl "llvm" action is raw LLVM IR,
// which the VM cannot run).
#pragma once

#include "vm/Machine.hpp"

namespace polyir::vm {

  bool BindsNinaRuntime(const String& name);

  // Consulted by Host.cpp's CallBoundExtern before the `polyir_*` host layer. Every text result carries a fresh identity (StrValue).
  Outcome CallNinaRuntime(Machine& m, const String& name, const Vector<Value>& args);

}  // namespace polyir::vm
