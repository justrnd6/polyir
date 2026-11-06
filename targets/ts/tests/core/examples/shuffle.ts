// A 64-bit integer is a bigint, brought back into 64 bits after an operation that can leave them.
function i64(value: bigint): bigint {
  return BigInt.asIntN(64, value);
}

function reverse4(a: bigint, b: bigint, c: bigint): bigint {
  const v: Array<bigint> = [a, b, c, 40n];
  const r: Array<bigint> = [3, 2, 1, 0].map((lane) => v[lane]);
  return i64(i64(i64(i64(r[0] * 1000n) + i64(r[1] * 100n)) + i64(r[2] * 10n)) + r[3]);
}

function duplicateFirstTwo(a: bigint, b: bigint, c: bigint): bigint {
  const v: Array<bigint> = [a, b, c, 4n];
  const r: Array<bigint> = [0, 0, 1, 1].map((lane) => v[lane]);
  return i64(i64(i64(i64(r[0] * 1000n) + i64(r[1] * 100n)) + i64(r[2] * 10n)) + r[3]);
}

export { reverse4, duplicateFirstTwo };
