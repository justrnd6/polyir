"use strict";

// A 64-bit integer is a BigInt, brought back into 64 bits after an operation that can leave them.
function i64(value) {
  return BigInt.asIntN(64, value);
}

function riskyDouble(x) {
  if (x < 0n) {
    throw 99n;
  }
  return i64(x * 2n);
}

function safeDouble(x) {
  let result = 0n;
  try {
    result = riskyDouble(x);
  } catch (code) {
    result = code;
  }
  return result;
}

function nestedTryCheck(x) {
  let result = 0n;
  try {
    try {
      result = riskyDouble(x);
    } catch (innerCode) {
      result = i64(innerCode + 1n);
      throw result;
    }
  } catch (outerCode) {
    result = i64(outerCode * 10n);
  }
  return result;
}

export { riskyDouble, safeDouble, nestedTryCheck };
