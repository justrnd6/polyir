"use strict";

// A 64-bit integer is a BigInt, brought back into 64 bits after an operation that can leave them.
function i64(value) {
  return BigInt.asIntN(64, value);
}

function pairSum(i, src, n) {
  const a = Number(BigInt.asIntN(32, i64(2n * BigInt(i))));
  let total = src[a];
  if (((a + 1) | 0) < n) {
    total = Math.fround(total + src[(a + 1) | 0]);
  }
  return total;
}

function pairSumInt(i, src, n) {
  const a = Number(BigInt.asIntN(32, i64(2n * BigInt(i))));
  let total = src[a];
  if (((a + 1) | 0) < n) {
    total = (total + src[(a + 1) | 0]) | 0;
  }
  return total;
}

function pairMax(i, src, n) {
  const a = Number(BigInt.asIntN(32, i64(2n * BigInt(i))));
  let best = src[a];
  if (((a + 1) | 0) < n) {
    if (src[(a + 1) | 0] > best) {
      best = src[(a + 1) | 0];
    }
  }
  return best;
}

function passCount(n) {
  let passes = 0n;
  let left = n;
  for (let k = 0n; k < 40n; k = i64(k + 1n)) {
    if (left > 1n) {
      left = i64(left + 1n) / 2n;
      passes = i64(passes + 1n);
    }
  }
  return passes;
}

function cpuSum(values, n) {
  let total = 0.0;
  for (let i = 0n; i < n; i = i64(i + 1n)) {
    total = Math.fround(total + values[Number(i)]);
  }
  return total;
}

function cpuMax(values, n) {
  let best = values[0];
  for (let i = 1n; i < n; i = i64(i + 1n)) {
    if (values[Number(i)] > best) {
      best = values[Number(i)];
    }
  }
  return best;
}

function fillTestData(values, n) {
  for (let i = 0n; i < n; i = i64(i + 1n)) {
    values[Number(i)] = Math.fround(Number(i % 7n));
  }
  values[Number(n / 3n)] = 1000.0;
  return 0n;
}

export { pairSum, pairSumInt, pairMax, passCount, cpuSum, cpuMax, fillTestData };
