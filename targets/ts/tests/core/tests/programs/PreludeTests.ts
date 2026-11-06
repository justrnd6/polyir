import { off } from "./Check.js";

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

function checkSign(): number {
  return off(sign(Math.fround(0.0 - 3.0)), Math.fround(0.0 - 1.0)) + off(sign(0.0), 0.0) + off(sign(2.0), 1.0);
}

function checkFract(): number {
  return off(fract(2.75), 0.75) + off(fract(Math.fround(0.0 - 0.25)), 0.75);
}

function checkClamp(): number {
  return off(clamp(5.0, Math.fround(0.0 - 1.0), 2.0), 2.0) + off(clamp(Math.fround(0.0 - 5.0), Math.fround(0.0 - 1.0), 2.0), Math.fround(0.0 - 1.0));
}

function checkMix(): number {
  return off(mix(10.0, 20.0, 0.25), 12.5);
}

function checkNestedCalls(): number {
  return off(clamp(Math.fround(fract(1.75) * 2.0), Math.fround(0.0 - 1.0), 1.0), 1.0) + off(mix(sign(2.0), fract(0.5), 0.5), 0.75);
}

function rescale(x: number): number {
  return Math.fround(x * 3.0);
}

function checkOwnFunctionsStillWork(): number {
  return off(rescale(fract(1.25)), 0.75);
}

export { sign, fract, clamp, mix, checkSign, checkFract, checkClamp, checkMix, checkNestedCalls, rescale, checkOwnFunctionsStillWork };
