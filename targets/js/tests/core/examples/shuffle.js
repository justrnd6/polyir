"use strict";

// A 64-bit integer is a BigInt, brought back into 64 bits after an operation that can leave them.
function i64(value) {
  return BigInt.asIntN(64, value);
}

function reverse4(a, b, c) {
  const v = [a, b, c, 40n];
  const r = [3, 2, 1, 0].map((lane) => v[lane]);
  return i64(i64(i64(i64(r[0] * 1000n) + i64(r[1] * 100n)) + i64(r[2] * 10n)) + r[3]);
}

function duplicateFirstTwo(a, b, c) {
  const v = [a, b, c, 4n];
  const r = [0, 0, 1, 1].map((lane) => v[lane]);
  return i64(i64(i64(i64(r[0] * 1000n) + i64(r[1] * 100n)) + i64(r[2] * 10n)) + r[3]);
}

export { reverse4, duplicateFirstTwo };
