// The std Lifecycle native on the VM (LifecycleHost.hpp): the one extern that calls back into the host that runs the VM.
// The host's step prints straight to the terminal, so what the program printed so far goes out first.
#include "vm/LifecycleHost.hpp"

namespace polyir::vm {

  const char* const LIFECYCLE_IN_PROCESS_EXTERN = "Lifecycle__inProcess__host";

  bool BindsLifecycle(const String& name) {
    return name == LIFECYCLE_IN_PROCESS_EXTERN;
  }

  Outcome CallLifecycle(Machine& m, const Vector<Value>& args) {
    if (!m.hostStep) {
      return Trap(String("'") + LIFECYCLE_IN_PROCESS_EXTERN + "' has no binding on the VM");
    }

    m.out->flush();
    cout.flush();
    const int64_t status = m.hostStep(WordAt(args, 0), WordAt(args, 1), WordsAt(m.heap, args, 2));

    return ValueOutcome(I64Value(status));
  }

}  // namespace polyir::vm
