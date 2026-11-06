const floorf = (x: number): number => Math.fround(Math.floor(x));

function sign(x: number): number {
  if (x > 0.0) {
    return 1.0;
  }
  if (x < 0.0) {
    return Math.fround(0.0 - 1.0);
  }
  return 0.0;
}

function fract(x: number): number {
  return Math.fround(x - floorf(x));
}

function clamp(x: number, low: number, high: number): number {
  if (x < low) {
    return low;
  }
  if (x > high) {
    return high;
  }
  return x;
}

function mix(a: number, b: number, t: number): number {
  return Math.fround(a + Math.fround(Math.fround(b - a) * t));
}

export { sign, fract, clamp, mix };
