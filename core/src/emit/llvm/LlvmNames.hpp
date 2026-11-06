// Small text helpers shared by the LLVM compiler's parts: type names, constants, identifiers.
#pragma once

#include "polyir/Std.hpp"
#include "polyir/Types.hpp"

#include <cstdint>
#include <string>

namespace polyir::llvm {

  String LlvmTypeText(const TypePtr& type);  // `i64`, `double`, `ptr`, `void`, `<4 x float>`

  String QuotedName(const String& name);     // `gcd`, or `"odd name"` when it needs quotes (the caller adds the `@` or `%`)

  String IntText(int64_t value, int bits);   // the number wrapped into the signed range of `bits`; `true` or `false` for 1 bit

  String FloatText(double value, int bits);  // the hexadecimal form LLVM wants: `0x3FF0000000000000` is 1.0

  String StringBytes(const String& text);    // the characters of `c"..."`, without the zero at the end

  String ZeroText(const TypePtr& type);      // `0`, `false`, `0x0000000000000000` (a float), `null`, `zeroinitializer` (a vector)

}  // namespace polyir::llvm
