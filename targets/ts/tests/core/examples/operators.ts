// A 64-bit integer is a bigint, brought back into 64 bits after an operation that can leave them.
function i64(value: bigint): bigint {
  return BigInt.asIntN(64, value);
}

function bitwisePrecedenceCheck(): bigint {
  const a: bigint = 6n;
  const b: bigint = 3n;
  const c: bigint = 1n;
  const d: bigint = 2n;
  return a | b & c ^ d;
}

function shiftPrecedenceCheck(): bigint {
  const x: bigint = 1n;
  return i64(x << 2n) & 3n;
}

export { bitwisePrecedenceCheck, shiftPrecedenceCheck };
