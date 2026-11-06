"use strict";

// A 64-bit integer is a BigInt, brought back into 64 bits after an operation that can leave them.
function i64(value) {
  return BigInt.asIntN(64, value);
}

function transform(p, m) {
  return [Math.fround(Math.fround(Math.fround(Math.fround(m[0] * p[0]) + Math.fround(m[1] * p[1])) + Math.fround(m[2] * p[2])) + Math.fround(m[3] * p[3])), Math.fround(Math.fround(Math.fround(Math.fround(m[4] * p[0]) + Math.fround(m[5] * p[1])) + Math.fround(m[6] * p[2])) + Math.fround(m[7] * p[3])), Math.fround(Math.fround(Math.fround(Math.fround(m[8] * p[0]) + Math.fround(m[9] * p[1])) + Math.fround(m[10] * p[2])) + Math.fround(m[11] * p[3])), Math.fround(Math.fround(Math.fround(Math.fround(m[12] * p[0]) + Math.fround(m[13] * p[1])) + Math.fround(m[14] * p[2])) + Math.fround(m[15] * p[3]))];
}

function identity(m) {
  for (let k = 0n; k < 16n; k = i64(k + 1n)) {
    m[Number(k)] = 0.0;
  }
  m[0] = 1.0;
  m[5] = 1.0;
  m[10] = 1.0;
  m[15] = 1.0;
  return 0n;
}

function multiply4(a, b, out) {
  for (let row = 0n; row < 4n; row = i64(row + 1n)) {
    for (let col = 0n; col < 4n; col = i64(col + 1n)) {
      let sum = 0.0;
      for (let k = 0n; k < 4n; k = i64(k + 1n)) {
        sum = Math.fround(sum + Math.fround(a[Number(i64(i64(row * 4n) + k))] * b[Number(i64(i64(k * 4n) + col))]));
      }
      out[Number(i64(i64(row * 4n) + col))] = sum;
    }
  }
  return 0n;
}

export { transform, identity, multiply4 };
