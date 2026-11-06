// A 64-bit integer is a bigint, brought back into 64 bits after an operation that can leave them.
function i64(value: bigint): bigint {
  return BigInt.asIntN(64, value);
}

function classify(x: bigint): bigint {
  return x < 0n ? i64(0n - 1n) : x === 0n ? 0n : 1n;
}

function maxVal(a: bigint, b: bigint): bigint {
  let result: bigint = 0n;
  if (a < b) {
    result = b;
  } else {
    result = a;
  }
  return result;
}

function absViaIf(x: bigint): bigint {
  let r: bigint = x;
  if (r < 0n) {
    r = i64(0n - r);
  }
  return r;
}

export { classify, maxVal, absViaIf };
