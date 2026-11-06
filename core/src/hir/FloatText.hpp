// A float written exactly (C99 hexadecimal: 0x1.8p3), by the same steps as HirJson.nina's floatText so that the text is identical. Shared by the
// HIR JSON writer and the PBC writer: the two formats spell a float the same way, so nothing is lost on any way back.
#pragma once

#include "polyir/Std.hpp"

namespace polyir {

  // The next hex digit of a fraction in [0, 1): multiply by 16, take the whole part as the digit, keep the rest.
  inline char FloatHexDigit(double& rest) {
    rest = rest * 16.0;
    const int digit = static_cast<int>(rest);
    rest = rest - digit;

    return "0123456789abcdef"[digit];
  }

  // The hex digits of a fraction in [0, 1) after a point, or nothing for 0. FloatHexDigit takes one digit off `rest` each
  // time. The loop ends because a double's fraction has at most 52 bits, so at most 13 hex digits.
  inline String FloatHexFraction(double fraction) {
    String out;
    double rest = fraction;

    while (rest > 0.0) {
      out += FloatHexDigit(rest);
    }

    return out.empty() ? out : "." + out;
  }

  // A positive finite value as 0x1.FRACTIONpEXPONENT: halve or double it until it lies in [1, 2), counting the steps.
  inline String FloatHexNormal(double value) {
    double mantissa = value;
    int exponent = 0;

    while (mantissa >= 2.0) {
      mantissa = mantissa / 2.0;
      exponent = exponent + 1;
    }

    while (mantissa < 1.0) {
      mantissa = mantissa * 2.0;
      exponent = exponent - 1;
    }

    return "0x1" + FloatHexFraction(mantissa - 1.0) + "p" + to_string(exponent);
  }

  inline String FloatText(double value) {
    if (value != value) {
      return "nan";  // only NaN is unequal to itself
    }

    if (value == 0.0) {
      return "0x0p0";
    }

    const String sign = value < 0.0 ? "-" : "";
    const double magnitude = value < 0.0 ? 0.0 - value : value;

    // Only infinity is unchanged by halving (zero was handled above).
    return magnitude / 2.0 == magnitude ? sign + "inf" : sign + FloatHexNormal(magnitude);
  }

}  // namespace polyir
