"use strict";

import { off, offInt } from "./Check.js";

// A 64-bit integer is a BigInt, brought back into 64 bits after an operation that can leave them.
function i64(value) {
  return BigInt.asIntN(64, value);
}

function checkFloatRemainder() {
  const a = 7.5;
  const b = 2.0;
  return off(a % b, 1.5);
}

function checkFloatRemainderKeepsTheSignOfTheLeftNumber() {
  const a = Math.fround(0.0 - 7.5);
  const b = 2.0;
  return off(a % b, Math.fround(0.0 - 1.5));
}

function checkFloatRemainderIgnoresTheSignOfTheRightNumber() {
  const a = 7.5;
  const b = Math.fround(0.0 - 2.0);
  return off(a % b, 1.5);
}

function checkFloatRemainderOfNumbersThatDivideEvenly() {
  const b = 47.0;
  const c = 97.0;
  return off(Math.fround(2867.0 % b), 0.0) + off(Math.fround(5969.0 % b), 0.0) + off(Math.fround(9409.0 % c), 0.0);
}

function checkFloatRemainderOfZero() {
  const a = 0.0;
  const b = 5.0;
  return off(a % b, 0.0);
}

function checkFloatRemainderOfAFractionOfOne() {
  const a = 0.75;
  const b = 1.0;
  return off(a % b, 0.75);
}

function checkWholeNumberRemainder() {
  const a = 17n;
  const b = 5n;
  return offInt(a % b, 2n) + offInt(i64(0n - a) % b, i64(0n - 2n));
}

export { checkFloatRemainder, checkFloatRemainderKeepsTheSignOfTheLeftNumber, checkFloatRemainderIgnoresTheSignOfTheRightNumber, checkFloatRemainderOfNumbersThatDivideEvenly, checkFloatRemainderOfZero, checkFloatRemainderOfAFractionOfOne, checkWholeNumberRemainder };
