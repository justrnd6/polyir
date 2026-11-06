"use strict";

// A 64-bit integer is a BigInt, brought back into 64 bits after an operation that can leave them.
function i64(value) {
  return BigInt.asIntN(64, value);
}

const sqrtf = (x) => Math.fround(Math.sqrt(x));

function scale(x, k) {
  return Math.fround(Math.fround(x * k) + 1.0);
}

function norm(a, b) {
  const s = Math.fround(scale(a, 2.0) + Math.fround(b * b));
  return sqrtf(s);
}

function sumTo(n) {
  let acc = 0;
  for (let i = 0n; i < BigInt(n); i = i64(i + 1n)) {
    if (i % 2n === 0n) {
      acc = (acc + Number(BigInt.asIntN(32, i))) | 0;
    }
  }
  return acc;
}

function lane(v) {
  return Math.fround(v[0] + v[3]);
}

function smooth(i, src, n) {
  if (i === 0) {
    return src[i];
  }
  if (i === ((n - 1) | 0)) {
    return src[i];
  }
  return Math.fround(Math.fround(Math.fround(src[(i - 1) | 0] + src[i]) + src[(i + 1) | 0]) / 3.0);
}

function leftover(a, b) {
  return a % b;
}

function close(a, b) {
  let diff = Math.fround(a - b);
  if (diff < 0.0) {
    diff = Math.fround(0.0 - diff);
  }
  let size = b;
  if (size < 0.0) {
    size = Math.fround(0.0 - size);
  }
  if (diff <= Math.fround(0.000001 * Math.fround(size + 1.0))) {
    return 1n;
  }
  return 0n;
}

export { scale, norm, sumTo, lane, smooth, leftover, close };
