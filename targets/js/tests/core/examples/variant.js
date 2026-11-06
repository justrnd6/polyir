"use strict";

// An `either` holds its value as an `Ok` or an `Err`.
class Ok {
  constructor(value) {
    this.value = value;
  }
}

class Err {
  constructor(value) {
    this.value = value;
  }
}

// A variant: the number of its case and the value it holds.
class Variant {
  constructor(tag, value) {
    this.tag = tag;
    this.value = value;
  }
}

// A 64-bit integer is a BigInt, brought back into 64 bits after an operation that can leave them.
function i64(value) {
  return BigInt.asIntN(64, value);
}

function safeDivide(a, b) {
  if (b === 0n) {
    return new Err(99n);
  }
  return new Ok(a / b);
}

function divideOrDefault(a, b) {
  const r = safeDivide(a, b);
  if (r instanceof Err) {
    return r.value;
  }
  return r.value;
}

function eitherCoercion() {
  const r = new Ok(3.0);
  return BigInt(Math.trunc(r.value));
}

function variantSum(which) {
  let v = new Variant(0n, 0n);
  if (which === 0n) {
    v = new Variant(0n, 10n);
  }
  if (which === 1n) {
    v = new Variant(1n, 2.5);
  }
  if (which === 2n) {
    v = new Variant(2n, 30n);
  }
  if (v.tag === 0n) {
    return v.value;
  }
  if (v.tag === 1n) {
    return i64(BigInt(Math.trunc(v.value)) * 100n);
  }
  return i64(v.value * 1000n);
}

function castRoundTrip() {
  const d = 9.0;
  const asI64 = BigInt(Math.trunc(d));
  const backToD = Number(asI64);
  return i64(BigInt(Math.trunc(backToD)) + asI64);
}

export { safeDivide, divideOrDefault, eitherCoercion, variantSum, castRoundTrip };
