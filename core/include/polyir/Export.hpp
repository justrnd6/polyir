// Marks what the dylib exports (everything else is hidden).
#pragma once

#if defined(_WIN32)
#  if defined(POLYIR_BUILDING)
#    define POLYIR_API __declspec(dllexport)
#  else
#    define POLYIR_API __declspec(dllimport)
#  endif
#else
#  define POLYIR_API __attribute__((visibility("default")))
#endif
