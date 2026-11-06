"use strict";

// A 64-bit integer is a BigInt, brought back into 64 bits after an operation that can leave them.
function i64(value) {
  return BigInt.asIntN(64, value);
}

let counter = 0n;

function bump() {
  counter = i64(counter + 1n);
  return counter;
}

export { bump };
