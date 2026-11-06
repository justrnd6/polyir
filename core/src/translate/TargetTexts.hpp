// The targets built into the library: TargetTexts.cpp is generated in the build folder by cmake/EmbedTargets.cmake from each
// targets/<lang>/<Lang>.translate page and the files under its runtime/ folder, each with its rules as polyir-translatec
// compiled them. polyir's canon, targets/Polyir.canon, is compiled the same way.
#pragma once

#include "polyir/Translate.hpp"

namespace polyir::translate {

  Vector<Target> EmbeddedTargets();

  const Schema& CompiledSchemaPolyir();  // targets/Polyir.canon, compiled

}  // namespace polyir::translate
