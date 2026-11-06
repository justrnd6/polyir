// A 64-bit integer is a bigint, brought back into 64 bits after an operation that can leave them.
function i64(value: bigint): bigint {
  return BigInt.asIntN(64, value);
}

const sin = Math.sin;
const cos = Math.cos;
const sqrt = Math.sqrt;

function off(got: number, expected: number): number {
  return Math.fround(got - expected);
}

function offInt(got: bigint, expected: bigint): number {
  return Number(i64(got - expected));
}

function offRelative(got: number, expected: number): number {
  return Math.fround(Math.fround(got - expected) / expected);
}

function amplitudeAt(data: Array<number>, start: bigint, count: bigint, rate: bigint, hz: number): number {
  let re: number = 0.0;
  let im: number = 0.0;
  for (let k = 0n; k < count; k = i64(k + 1n)) {
    const angle: number = 6.283185307179586 * hz * Number(i64(start + k)) / Number(rate);
    re = re + data[Number(i64(start + k))] * cos(angle);
    im = im + data[Number(i64(start + k))] * sin(angle);
  }
  return Math.fround(2.0 * sqrt(re * re + im * im) / Number(count));
}

export { off, offInt, offRelative, amplitudeAt };
