"use strict";

// A 64-bit integer is a BigInt, brought back into 64 bits after an operation that can leave them.
function i64(value) {
  return BigInt.asIntN(64, value);
}

function classify(x) {
  return x < 0n ? i64(0n - 1n) : x === 0n ? 0n : 1n;
}

function maxVal(a, b) {
  let result = 0n;
  if (a < b) {
    result = b;
  } else {
    result = a;
  }
  return result;
}

function absViaIf(x) {
  let r = x;
  if (r < 0n) {
    r = i64(0n - r);
  }
  return r;
}

export { classify, maxVal, absViaIf };
