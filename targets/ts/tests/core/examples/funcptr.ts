// A 64-bit integer is a bigint, brought back into 64 bits after an operation that can leave them.
function i64(value: bigint): bigint {
  return BigInt.asIntN(64, value);
}

function doubleIt(x: bigint): bigint {
  return i64(x * 2n);
}

function applyTwice(f: Array<(...args: [bigint]) => bigint>, x: bigint): bigint {
  const once: bigint = f(x);
  return f(once);
}

function applyDoubleTwice(x: bigint): bigint {
  return applyTwice(doubleIt, x);
}

function directDoubleCheck(x: bigint): bigint {
  return doubleIt(x);
}

function addOne(x: bigint): bigint {
  return i64(x + 1n);
}

function pickAndApply(x: bigint, useAddOne: bigint): bigint {
  let fn: Array<(...args: [bigint]) => bigint> = doubleIt;
  if (useAddOne !== 0n) {
    fn = addOne;
  }
  return fn(x);
}

function applyNTimes(f: Array<(...args: [bigint]) => bigint>, x: bigint, n: bigint): bigint {
  let acc: bigint = x;
  for (let i = 0n; i < n; i = i64(i + 1n)) {
    acc = f(acc);
  }
  return acc;
}

function doubleFourTimes(x: bigint): bigint {
  return applyNTimes(doubleIt, x, 4n);
}

export { doubleIt, applyTwice, applyDoubleTwice, directDoubleCheck, addOne, pickAndApply, applyNTimes, doubleFourTimes };
