"use strict";

// A 64-bit integer is a BigInt, brought back into 64 bits after an operation that can leave them.
function i64(value) {
  return BigInt.asIntN(64, value);
}

const sin = Math.sin;
const cos = Math.cos;
const pow = Math.pow;
const sqrt = Math.sqrt;

class Biquad {
  constructor(b0, b1, b2, a1, a2) {
    this.b0 = b0;
    this.b1 = b1;
    this.b2 = b2;
    this.a1 = a1;
    this.a2 = a2;
  }

  run(data, count) {
    let x1 = 0.0;
    let x2 = 0.0;
    let y1 = 0.0;
    let y2 = 0.0;
    for (let i = 0n; i < count; i = i64(i + 1n)) {
      const x = data[Number(i)];
      const y = this.b0 * x + this.b1 * x1 + this.b2 * x2 - this.a1 * y1 - this.a2 * y2;
      x2 = x1;
      x1 = x;
      y2 = y1;
      y1 = y;
      data[Number(i)] = Math.fround(y);
    }
    return 0n;
  }

  gain(hz, rate) {
    const w = 6.283185307179586 * hz / Number(rate);
    const c1 = cos(w);
    const s1 = sin(w);
    const c2 = cos(2.0 * w);
    const s2 = sin(2.0 * w);
    const topReal = this.b0 + this.b1 * c1 + this.b2 * c2;
    const topImag = 0.0 - (this.b1 * s1 + this.b2 * s2);
    const bottomReal = 1.0 + this.a1 * c1 + this.a2 * c2;
    const bottomImag = 0.0 - (this.a1 * s1 + this.a2 * s2);
    const strength = (topReal * topReal + topImag * topImag) / (bottomReal * bottomReal + bottomImag * bottomImag);
    return Math.fround(sqrt(strength));
  }
}

function biquadFrom(b0, b1, b2, a0, a1, a2) {
  return new Biquad(b0 / a0, b1 / a0, b2 / a0, a1 / a0, a2 / a0);
}

function biquadAngle(rate, hz) {
  return 6.283185307179586 * hz / Number(rate);
}

function biquadLowPass(rate, hz, q) {
  const w = biquadAngle(rate, hz);
  const c = cos(w);
  const alpha = sin(w) / (2.0 * q);
  return biquadFrom((1.0 - c) / 2.0, 1.0 - c, (1.0 - c) / 2.0, 1.0 + alpha, 0.0 - 2.0 * c, 1.0 - alpha);
}

function biquadHighPass(rate, hz, q) {
  const w = biquadAngle(rate, hz);
  const c = cos(w);
  const alpha = sin(w) / (2.0 * q);
  return biquadFrom((1.0 + c) / 2.0, 0.0 - (1.0 + c), (1.0 + c) / 2.0, 1.0 + alpha, 0.0 - 2.0 * c, 1.0 - alpha);
}

function biquadBandPass(rate, hz, q) {
  const w = biquadAngle(rate, hz);
  const c = cos(w);
  const alpha = sin(w) / (2.0 * q);
  return biquadFrom(alpha, 0.0, 0.0 - alpha, 1.0 + alpha, 0.0 - 2.0 * c, 1.0 - alpha);
}

function biquadNotch(rate, hz, q) {
  const w = biquadAngle(rate, hz);
  const c = cos(w);
  const alpha = sin(w) / (2.0 * q);
  return biquadFrom(1.0, 0.0 - 2.0 * c, 1.0, 1.0 + alpha, 0.0 - 2.0 * c, 1.0 - alpha);
}

function biquadBell(rate, hz, q, gainDb) {
  const w = biquadAngle(rate, hz);
  const c = cos(w);
  const alpha = sin(w) / (2.0 * q);
  const a = pow(10.0, gainDb / 40.0);
  return biquadFrom(1.0 + alpha * a, 0.0 - 2.0 * c, 1.0 - alpha * a, 1.0 + alpha / a, 0.0 - 2.0 * c, 1.0 - alpha / a);
}

function biquadLowShelf(rate, hz, gainDb) {
  const w = biquadAngle(rate, hz);
  const c = cos(w);
  const a = pow(10.0, gainDb / 40.0);
  const beta = sqrt(2.0 * a) * sin(w);
  return biquadFrom(a * (a + 1.0 - (a - 1.0) * c + beta), 2.0 * a * (a - 1.0 - (a + 1.0) * c), a * (a + 1.0 - (a - 1.0) * c - beta), a + 1.0 + (a - 1.0) * c + beta, 0.0 - 2.0 * (a - 1.0 + (a + 1.0) * c), a + 1.0 + (a - 1.0) * c - beta);
}

function biquadHighShelf(rate, hz, gainDb) {
  const w = biquadAngle(rate, hz);
  const c = cos(w);
  const a = pow(10.0, gainDb / 40.0);
  const beta = sqrt(2.0 * a) * sin(w);
  return biquadFrom(a * (a + 1.0 + (a - 1.0) * c + beta), 0.0 - 2.0 * a * (a - 1.0 + (a + 1.0) * c), a * (a + 1.0 + (a - 1.0) * c - beta), a + 1.0 - (a - 1.0) * c + beta, 2.0 * (a - 1.0 - (a + 1.0) * c), a + 1.0 - (a - 1.0) * c - beta);
}

export { Biquad, biquadFrom, biquadAngle, biquadLowPass, biquadHighPass, biquadBandPass, biquadNotch, biquadBell, biquadLowShelf, biquadHighShelf };
