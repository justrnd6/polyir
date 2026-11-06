// A 64-bit integer is a bigint, brought back into 64 bits after an operation that can leave them.
function i64(value: bigint): bigint {
  return BigInt.asIntN(64, value);
}

const sin = Math.sin;
const cos = Math.cos;
const pow = Math.pow;
const sqrt = Math.sqrt;

class Biquad {
  constructor(public b0: number, public b1: number, public b2: number, public a1: number, public a2: number) {}

  run(data: Array<number>, count: bigint): bigint {
    let x1: number = 0.0;
    let x2: number = 0.0;
    let y1: number = 0.0;
    let y2: number = 0.0;
    for (let i = 0n; i < count; i = i64(i + 1n)) {
      const x: number = data[Number(i)];
      const y: number = this.b0 * x + this.b1 * x1 + this.b2 * x2 - this.a1 * y1 - this.a2 * y2;
      x2 = x1;
      x1 = x;
      y2 = y1;
      y1 = y;
      data[Number(i)] = Math.fround(y);
    }
    return 0n;
  }

  gain(hz: number, rate: bigint): number {
    const w: number = 6.283185307179586 * hz / Number(rate);
    const c1: number = cos(w);
    const s1: number = sin(w);
    const c2: number = cos(2.0 * w);
    const s2: number = sin(2.0 * w);
    const topReal: number = this.b0 + this.b1 * c1 + this.b2 * c2;
    const topImag: number = 0.0 - (this.b1 * s1 + this.b2 * s2);
    const bottomReal: number = 1.0 + this.a1 * c1 + this.a2 * c2;
    const bottomImag: number = 0.0 - (this.a1 * s1 + this.a2 * s2);
    const strength: number = (topReal * topReal + topImag * topImag) / (bottomReal * bottomReal + bottomImag * bottomImag);
    return Math.fround(sqrt(strength));
  }
}

function biquadFrom(b0: number, b1: number, b2: number, a0: number, a1: number, a2: number): Biquad {
  return new Biquad(b0 / a0, b1 / a0, b2 / a0, a1 / a0, a2 / a0);
}

function biquadAngle(rate: bigint, hz: number): number {
  return 6.283185307179586 * hz / Number(rate);
}

function biquadLowPass(rate: bigint, hz: number, q: number): Biquad {
  const w: number = biquadAngle(rate, hz);
  const c: number = cos(w);
  const alpha: number = sin(w) / (2.0 * q);
  return biquadFrom((1.0 - c) / 2.0, 1.0 - c, (1.0 - c) / 2.0, 1.0 + alpha, 0.0 - 2.0 * c, 1.0 - alpha);
}

function biquadHighPass(rate: bigint, hz: number, q: number): Biquad {
  const w: number = biquadAngle(rate, hz);
  const c: number = cos(w);
  const alpha: number = sin(w) / (2.0 * q);
  return biquadFrom((1.0 + c) / 2.0, 0.0 - (1.0 + c), (1.0 + c) / 2.0, 1.0 + alpha, 0.0 - 2.0 * c, 1.0 - alpha);
}

function biquadBandPass(rate: bigint, hz: number, q: number): Biquad {
  const w: number = biquadAngle(rate, hz);
  const c: number = cos(w);
  const alpha: number = sin(w) / (2.0 * q);
  return biquadFrom(alpha, 0.0, 0.0 - alpha, 1.0 + alpha, 0.0 - 2.0 * c, 1.0 - alpha);
}

function biquadNotch(rate: bigint, hz: number, q: number): Biquad {
  const w: number = biquadAngle(rate, hz);
  const c: number = cos(w);
  const alpha: number = sin(w) / (2.0 * q);
  return biquadFrom(1.0, 0.0 - 2.0 * c, 1.0, 1.0 + alpha, 0.0 - 2.0 * c, 1.0 - alpha);
}

function biquadBell(rate: bigint, hz: number, q: number, gainDb: number): Biquad {
  const w: number = biquadAngle(rate, hz);
  const c: number = cos(w);
  const alpha: number = sin(w) / (2.0 * q);
  const a: number = pow(10.0, gainDb / 40.0);
  return biquadFrom(1.0 + alpha * a, 0.0 - 2.0 * c, 1.0 - alpha * a, 1.0 + alpha / a, 0.0 - 2.0 * c, 1.0 - alpha / a);
}

function biquadLowShelf(rate: bigint, hz: number, gainDb: number): Biquad {
  const w: number = biquadAngle(rate, hz);
  const c: number = cos(w);
  const a: number = pow(10.0, gainDb / 40.0);
  const beta: number = sqrt(2.0 * a) * sin(w);
  return biquadFrom(a * (a + 1.0 - (a - 1.0) * c + beta), 2.0 * a * (a - 1.0 - (a + 1.0) * c), a * (a + 1.0 - (a - 1.0) * c - beta), a + 1.0 + (a - 1.0) * c + beta, 0.0 - 2.0 * (a - 1.0 + (a + 1.0) * c), a + 1.0 + (a - 1.0) * c - beta);
}

function biquadHighShelf(rate: bigint, hz: number, gainDb: number): Biquad {
  const w: number = biquadAngle(rate, hz);
  const c: number = cos(w);
  const a: number = pow(10.0, gainDb / 40.0);
  const beta: number = sqrt(2.0 * a) * sin(w);
  return biquadFrom(a * (a + 1.0 + (a - 1.0) * c + beta), 0.0 - 2.0 * a * (a - 1.0 + (a + 1.0) * c), a * (a + 1.0 + (a - 1.0) * c - beta), a + 1.0 - (a - 1.0) * c + beta, 2.0 * (a - 1.0 - (a + 1.0) * c), a + 1.0 - (a - 1.0) * c - beta);
}

export { Biquad, biquadFrom, biquadAngle, biquadLowPass, biquadHighPass, biquadBandPass, biquadNotch, biquadBell, biquadLowShelf, biquadHighShelf };
