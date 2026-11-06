"use strict";

import { offInt } from "./Check.js";

// A 64-bit integer is a BigInt, brought back into 64 bits after an operation that can leave them.
function i64(value) {
  return BigInt.asIntN(64, value);
}

// An unsigned 64-bit integer is a BigInt, brought back into 0 .. 2^64 - 1 after an operation that can leave them.
function u64(value) {
  return BigInt.asUintN(64, value);
}

// Whether two values hold the same: numbers, flags and strings by value, lists item by item, maps and sets by their
// entries, a record by its own `equals` method when it has one, else field by field.
function same(a, b) {
  if (a === b) {
    return true;
  }
  if (a === null || b === null || typeof a !== "object" || typeof b !== "object") {
    return false;
  }
  if (Array.isArray(a)) {
    return a.length === b.length && a.every((item, i) => same(item, b[i]));
  }
  if (a instanceof Map) {
    return a.size === b.size && [...a].every(([key, value]) => b.has(key) && same(value, b.get(key)));
  }
  if (a instanceof Set) {
    return a.size === b.size && [...a].every((item) => b.has(item));
  }
  if (typeof a.equals === "function") {
    return a.equals(b);
  }
  const keys = Object.keys(a);
  return a.constructor === b.constructor && keys.every((key) => same(a[key], b[key]));
}

function checkAnUnsignedComparisonDoesNotSeeTheTopBitAsASign() {
  const big = 4000000000;
  let seen = 0n;
  if (big > 3000000000) {
    seen = i64(seen + 1n);
  }
  if (big >= big) {
    seen = i64(seen + 10n);
  }
  if (big < 5) {
    seen = i64(seen + 100n);
  }
  return offInt(seen, 11n);
}

function checkAnUnsignedDivisionAndRemainder() {
  const big = 4000000000;
  const half = (big / 2) >>> 0;
  const rest = big % 7;
  return offInt(BigInt(half), 2000000000n) + offInt(BigInt(rest), 3n);
}

function checkAnUnsignedRightShiftFillsWithZeros() {
  const big = 4000000000;
  const shifted = big >>> 28;
  const m = u64(i64(0n - 1n));
  return offInt(BigInt(shifted), 14n) + offInt(i64(m >> 63n), 1n);
}

function checkAnUnsignedWidensWithZeroBits() {
  const big = 4000000000;
  const small = 200;
  return offInt(BigInt(big), 4000000000n) + offInt(BigInt(small), 200n);
}

function checkNarrowUnsignedArithmeticWrapsAtItsSize() {
  const b = 200;
  const c = 100;
  const sum = (b + c) & 255;
  const under = (c - b) & 255;
  return offInt(BigInt(sum), 44n) + offInt(BigInt(under), 156n);
}

function checkAnUnsignedGoesToAFloatAsItsValue() {
  const big = 4000000000;
  const asDouble = big;
  return offInt(BigInt(Math.trunc(asDouble)), 4000000000n);
}

function checkTheLargestU64IsLargerThanFive() {
  const m = u64(i64(0n - 1n));
  let seen = 0n;
  if (m > 5n) {
    seen = i64(seen + 1n);
  }
  if (m / 2n === 9223372036854775807n) {
    seen = i64(seen + 10n);
  }
  return offInt(seen, 11n);
}

class Pixel {
  constructor(r, g, b) {
    this.r = r;
    this.g = g;
    this.b = b;
  }
}

function checkAnUnsignedFieldKeepsItsValueAndStructsCompare() {
  const a = new Pixel(250, 128, 7);
  const b = new Pixel(250, 128, 7);
  let seen = 0n;
  if (same(a, b)) {
    seen = i64(seen + 1n);
  }
  seen = i64(seen + BigInt(a.g));
  return offInt(seen, 129n);
}

function checkUnsignedKeysAreOrdinaryKeys() {
  const m = new Map();
  const k = 4000000000;
  m.set(k, 7n);
  m.set(5, 9n);
  return offInt(m.get(k) ?? 0n, 7n) + offInt(m.get(5) ?? 0n, 9n);
}

export { checkAnUnsignedComparisonDoesNotSeeTheTopBitAsASign, checkAnUnsignedDivisionAndRemainder, checkAnUnsignedRightShiftFillsWithZeros, checkAnUnsignedWidensWithZeroBits, checkNarrowUnsignedArithmeticWrapsAtItsSize, checkAnUnsignedGoesToAFloatAsItsValue, checkTheLargestU64IsLargerThanFive, Pixel, checkAnUnsignedFieldKeepsItsValueAndStructsCompare, checkUnsignedKeysAreOrdinaryKeys };
