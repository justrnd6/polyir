// A 64-bit integer is a bigint, brought back into 64 bits after an operation that can leave them.
function i64(value: bigint): bigint {
  return BigInt.asIntN(64, value);
}

const sqrtf = (x: number): number => Math.fround(Math.sqrt(x));
const sinf = (x: number): number => Math.fround(Math.sin(x));
const cosf = (x: number): number => Math.fround(Math.cos(x));

function dft(k: number, x: Array<number>, p: Array<number>): number {
  const n: number = p[0] | 0;
  let re: number = 0.0;
  let im: number = 0.0;
  for (let j = 0n; j < BigInt(n); j = i64(j + 1n)) {
    const turns: number = Math.fround(Math.fround(Math.imul(k, Number(BigInt.asIntN(32, j))) % n) / Math.fround(n));
    re = Math.fround(re + Math.fround(x[Number(j)] * cosf(Math.fround(6.2831853 * turns))));
    im = Math.fround(im - Math.fround(x[Number(j)] * sinf(Math.fround(6.2831853 * turns))));
  }
  return Math.fround(2.0 * sqrtf(Math.fround(Math.fround(re * re) + Math.fround(im * im))) / Math.fround(n));
}

export { dft };
