// A 64-bit integer is a bigint, brought back into 64 bits after an operation that can leave them.
function i64(value: bigint): bigint {
  return BigInt.asIntN(64, value);
}

function sumBuf(buf: Array<number>, len: bigint): number {
  let i = 0n, total = 0.0;
  for (; i < len; i = i64(i + 1n), total = total + buf[Number(i)]) {}
  return total;
}

export { sumBuf };
