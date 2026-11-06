"use strict";

// An operator lane by lane: on two vectors, or on a vector and one number.
function lanes(a, b, op) {
  return a.map((x, i) => op(x, Array.isArray(b) ? b[i] : b));
}

// A 64-bit integer is a BigInt, brought back into 64 bits after an operation that can leave them.
function i64(value) {
  return BigInt.asIntN(64, value);
}

function vecMulSum(a, b) {
  const v1 = [a, a + 1.0, a + 2.0, a + 3.0];
  const v2 = [b, b, b, b];
  const p = lanes(v1, v2, (x, y) => x * y);
  return p[0] + p[1] + p[2] + p[3];
}

function sumI64Vec(a, b, c) {
  const v = [a, b, c];
  return i64(i64(v[0] + v[1]) + v[2]);
}

function vecMulSumViaVar(a, b) {
  const v1 = [a, a + 1.0, a + 2.0, a + 3.0];
  const v2 = [b, b, b, b];
  const p = lanes(v1, v2, (x, y) => x * y);
  let total = 0.0;
  for (let i = 0n; i < 4n; i = i64(i + 1n)) {
    total = total + p[Number(i)];
  }
  return total;
}

export { vecMulSum, sumI64Vec, vecMulSumViaVar };
