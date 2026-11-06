"use strict";

// A 64-bit integer is a BigInt, brought back into 64 bits after an operation that can leave them.
function i64(value) {
  return BigInt.asIntN(64, value);
}

function drop(xs, n) {
  const out = [];
  for (let i = n; i < BigInt(xs.length); i = i64(i + 1n)) {
    out.push(xs[Number(i)]);
  }
  return out;
}

function first(xs) {
  return xs[0];
}

function last(xs) {
  return xs[Number(i64(BigInt(xs.length) - 1n))];
}

export { drop, first, last };
