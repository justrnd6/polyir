// A 64-bit integer is a bigint, brought back into 64 bits after an operation that can leave them.
function i64(value: bigint): bigint {
  return BigInt.asIntN(64, value);
}

function riskyDouble(x: bigint): bigint {
  if (x < 0n) {
    throw 99n;
  }
  return i64(x * 2n);
}

function safeDouble(x: bigint): bigint {
  let result: bigint = 0n;
  try {
    result = riskyDouble(x);
  } catch (code: any) {
    result = code;
  }
  return result;
}

function nestedTryCheck(x: bigint): bigint {
  let result: bigint = 0n;
  try {
    try {
      result = riskyDouble(x);
    } catch (innerCode: any) {
      result = i64(innerCode + 1n);
      throw result;
    }
  } catch (outerCode: any) {
    result = i64(outerCode * 10n);
  }
  return result;
}

export { riskyDouble, safeDouble, nestedTryCheck };
