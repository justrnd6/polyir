"use strict";

// A 64-bit integer is a BigInt, brought back into 64 bits after an operation that can leave them.
function i64(value) {
  return BigInt.asIntN(64, value);
}

function doubleIt(x) {
  return i64(x * 2n);
}

function applyTwice(f, x) {
  const once = f(x);
  return f(once);
}

function applyDoubleTwice(x) {
  return applyTwice(doubleIt, x);
}

function directDoubleCheck(x) {
  return doubleIt(x);
}

function addOne(x) {
  return i64(x + 1n);
}

function pickAndApply(x, useAddOne) {
  let fn = doubleIt;
  if (useAddOne !== 0n) {
    fn = addOne;
  }
  return fn(x);
}

function applyNTimes(f, x, n) {
  let acc = x;
  for (let i = 0n; i < n; i = i64(i + 1n)) {
    acc = f(acc);
  }
  return acc;
}

function doubleFourTimes(x) {
  return applyNTimes(doubleIt, x, 4n);
}

export { doubleIt, applyTwice, applyDoubleTwice, directDoubleCheck, addOne, pickAndApply, applyNTimes, doubleFourTimes };
