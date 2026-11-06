"use strict";

// A 64-bit integer is a BigInt, brought back into 64 bits after an operation that can leave them.
function i64(value) {
  return BigInt.asIntN(64, value);
}

function gcd(a, b) {
  let x = a;
  let y = b;
  while (y !== 0n) {
    const r = x % y;
    x = y;
    y = r;
  }
  return x;
}

function lcm(a, b) {
  return i64(a / gcd(a, b) * b);
}

export { gcd, lcm };
