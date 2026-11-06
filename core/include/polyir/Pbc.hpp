// `.pbc` — polyir's bytecode: a Module as the compact binary the Nina implementation defines (lang/src/pbc/, docs/language/vm-proposal.md).
// `ReadPbc` loads what `polyir FILE.poly --emit-pbc` wrote (and `WritePbc`'s own output); the two implementations are interchangeable,
// checked against each other in tests/PbcTest.cpp. The container is `magic "PBC\1"`, a 4-byte little-endian version, then sections
// (`id: u8`, `byteLength: varint`, payload); v1 has the declarations (section 1). Section 2, written only when a declaration is in a
// `module` or `internal`, lists each one's kind, name, module and mark; a reader that does not know it skips it. Every integer is a
// zigzag LEB128 varint, a string a count of code points then one varint per code point, a float the exact C99 hexadecimal text HIR JSON
// uses.
#pragma once

#include "polyir/Std.hpp"
#include "polyir/Export.hpp"
#include "polyir/Hir.hpp"
#include "polyir/HirJson.hpp"

#include <cstdint>

namespace polyir {

  POLYIR_API Vector<uint8_t> WritePbc(const Module& module);

  POLYIR_API Module ReadPbc(const Vector<uint8_t>& bytes);  // throws HirReadError

}  // namespace polyir
