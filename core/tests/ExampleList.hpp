// The example programs the tests read: the build lists them in front_examples.txt (core/examples, the libraries' examples under
// ideas/*/examples, and core/tests/programs/mapping), one path per line.
#pragma once

#include "polyir/Std.hpp"

namespace polyir::testing {

  // The lines of a file, without the empty ones.
  inline Vector<String> LinesOf(const String& path) {
    IFStream in(path);
    Vector<String> lines;

    for (String line; getline(in, line);) {
      if (!line.empty()) {
        lines.push_back(line);
      }
    }

    return lines;
  }

}  // namespace polyir::testing
