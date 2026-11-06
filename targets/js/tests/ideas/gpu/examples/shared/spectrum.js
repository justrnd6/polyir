"use strict";

// A 64-bit integer is a BigInt, brought back into 64 bits after an operation that can leave them.
function i64(value) {
  return BigInt.asIntN(64, value);
}

const sqrtf = (x) => Math.fround(Math.sqrt(x));
const sinf = (x) => Math.fround(Math.sin(x));
const cosf = (x) => Math.fround(Math.cos(x));

function dft(k, x, p) {
  const n = p[0] | 0;
  let re = 0.0;
  let im = 0.0;
  for (let j = 0n; j < BigInt(n); j = i64(j + 1n)) {
    const turns = Math.fround(Math.fround(Math.imul(k, Number(BigInt.asIntN(32, j))) % n) / Math.fround(n));
    re = Math.fround(re + Math.fround(x[Number(j)] * cosf(Math.fround(6.2831853 * turns))));
    im = Math.fround(im - Math.fround(x[Number(j)] * sinf(Math.fround(6.2831853 * turns))));
  }
  return Math.fround(2.0 * sqrtf(Math.fround(Math.fround(re * re) + Math.fround(im * im))) / Math.fround(n));
}

export { dft };
