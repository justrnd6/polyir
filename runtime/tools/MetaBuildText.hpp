// meta-build (tools/metabuild/src) as text built into `poly`: MetaBuildText.cpp, which tools/metabuild/sync-embed.py writes.
#pragma once

namespace polyir::poly {

  const char* MetaBuildProgram();  // the runner and all it imports, as one program

  const char* MetaBuildNoExtension();  // what stands in for a project's build.poly when it has none

}  // namespace polyir::poly
