"use strict";

import { off } from "./Check.js";

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

function checkSign() {
  return off(sign(Math.fround(0.0 - 3.0)), Math.fround(0.0 - 1.0)) + off(sign(0.0), 0.0) + off(sign(2.0), 1.0);
}

function checkFract() {
  return off(fract(2.75), 0.75) + off(fract(Math.fround(0.0 - 0.25)), 0.75);
}

function checkClamp() {
  return off(clamp(5.0, Math.fround(0.0 - 1.0), 2.0), 2.0) + off(clamp(Math.fround(0.0 - 5.0), Math.fround(0.0 - 1.0), 2.0), Math.fround(0.0 - 1.0));
}

function checkMix() {
  return off(mix(10.0, 20.0, 0.25), 12.5);
}

function checkNestedCalls() {
  return off(clamp(Math.fround(fract(1.75) * 2.0), Math.fround(0.0 - 1.0), 1.0), 1.0) + off(mix(sign(2.0), fract(0.5), 0.5), 0.75);
}

function rescale(x) {
  return Math.fround(x * 3.0);
}

function checkOwnFunctionsStillWork() {
  return off(rescale(fract(1.25)), 0.75);
}

export { sign, fract, clamp, mix, checkSign, checkFract, checkClamp, checkMix, checkNestedCalls, rescale, checkOwnFunctionsStillWork };
