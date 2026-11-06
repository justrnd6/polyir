"use strict";

const floorf = (x) => Math.fround(Math.floor(x));

function sign(x) {
  if (x > 0.0) {
    return 1.0;
  }
  if (x < 0.0) {
    return Math.fround(0.0 - 1.0);
  }
  return 0.0;
}

function fract(x) {
  return Math.fround(x - floorf(x));
}

function clamp(x, low, high) {
  if (x < low) {
    return low;
  }
  if (x > high) {
    return high;
  }
  return x;
}

function mix(a, b, t) {
  return Math.fround(a + Math.fround(Math.fround(b - a) * t));
}

export { sign, fract, clamp, mix };
