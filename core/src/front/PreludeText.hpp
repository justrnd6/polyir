// The source text of the built-in libraries (PreludeText.cpp). PreludeText.cpp is GENERATED from core/stdlib/{Math,Gpu,Text,Lists}.poly by
// core/stdlib/sync-prelude.py: edit the .poly files and rerun the script, never the .cpp. Prelude.cpp parses these texts.
#pragma once

#include "polyir/Std.hpp"

namespace polyir::front {

  StringView MathPreludeText();  // the Math functions: sign, fract, clamp, mix

  StringView GpuPreludeText();   // the CPU side of the GPU functions: atomicAdd, barrier, ...

  StringView TextPreludeText();  // the Text functions: split, trim, join, ..., and StringBuilder

  StringView ListsPreludeText();  // the Lists functions: first, last, drop

}  // namespace polyir::front
