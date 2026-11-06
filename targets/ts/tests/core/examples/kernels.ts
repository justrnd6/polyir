// A 64-bit integer is a bigint, brought back into 64 bits after an operation that can leave them.
function i64(value: bigint): bigint {
  return BigInt.asIntN(64, value);
}

const sqrtf = (x: number): number => Math.fround(Math.sqrt(x));

function scale(x: number, k: number): number {
  return Math.fround(x * k);
}

function norm(a: number, b: number): number {
  const s: number = Math.fround(scale(a, 2.0) + Math.fround(b * b));
  return sqrtf(s);
}

function sumTo(n: number): number {
  let acc: number = 0;
  for (let i = 0n; i < BigInt(n); i = i64(i + 1n)) {
    if (i % 2n === 0n) {
      acc = (acc + Number(BigInt.asIntN(32, i))) | 0;
    }
  }
  return acc;
}

function lane(v: Array<number>): number {
  return Math.fround(v[0] + v[3]);
}

function smooth(i: number, src: Array<number>, n: number): number {
  if (i === 0) {
    return src[i];
  }
  if (i === ((n - 1) | 0)) {
    return src[i];
  }
  return Math.fround(Math.fround(Math.fround(src[(i - 1) | 0] + src[i]) + src[(i + 1) | 0]) / 3.0);
}

export { scale, norm, sumTo, lane, smooth };
