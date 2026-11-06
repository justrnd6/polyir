// A 64-bit integer is a bigint, brought back into 64 bits after an operation that can leave them.
function i64(value: bigint): bigint {
  return BigInt.asIntN(64, value);
}

function drop<T>(xs: Array<T>, n: bigint): Array<T> {
  const out: Array<T> = [];
  for (let i = n; i < BigInt(xs.length); i = i64(i + 1n)) {
    out.push(xs[Number(i)]);
  }
  return out;
}

function first<T>(xs: Array<T>): T {
  return xs[0];
}

function last<T>(xs: Array<T>): T {
  return xs[Number(i64(BigInt(xs.length) - 1n))];
}

export { drop, first, last };
