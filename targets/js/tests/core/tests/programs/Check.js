"use strict";

// A 64-bit integer is a BigInt, brought back into 64 bits after an operation that can leave them.
function i64(value) {
  return BigInt.asIntN(64, value);
}

const sin = Math.sin;
const cos = Math.cos;
const sqrt = Math.sqrt;

function off(got, expected) {
  return Math.fround(got - expected);
}

function offInt(got, expected) {
  return Number(i64(got - expected));
}

function offRelative(got, expected) {
  return Math.fround(Math.fround(got - expected) / expected);
}

function amplitudeAt(data, start, count, rate, hz) {
  let re = 0.0;
  let im = 0.0;
  for (let k = 0n; k < count; k = i64(k + 1n)) {
    const angle = 6.283185307179586 * hz * Number(i64(start + k)) / Number(rate);
    re = re + data[Number(i64(start + k))] * cos(angle);
    im = im + data[Number(i64(start + k))] * sin(angle);
  }
  return Math.fround(2.0 * sqrt(re * re + im * im) / Number(count));
}

export { off, offInt, offRelative, amplitudeAt };
