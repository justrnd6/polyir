"use strict";

// A 64-bit integer is a BigInt, brought back into 64 bits after an operation that can leave them.
function i64(value) {
  return BigInt.asIntN(64, value);
}

function bitwisePrecedenceCheck() {
  const a = 6n;
  const b = 3n;
  const c = 1n;
  const d = 2n;
  return a | b & c ^ d;
}

function shiftPrecedenceCheck() {
  const x = 1n;
  return i64(x << 2n) & 3n;
}

export { bitwisePrecedenceCheck, shiftPrecedenceCheck };
