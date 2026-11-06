"use strict";

// A 64-bit integer is a BigInt, brought back into 64 bits after an operation that can leave them.
function i64(value) {
  return BigInt.asIntN(64, value);
}

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

function boxedI64() {
  const b = new Box(42n);
  return b.getValue();
}

function boxedBoth() {
  const bi = new Box(100n);
  const bd = new Box(2.0);
  return i64(bi.getValue() + BigInt(Math.trunc(bd.getValue())));
}

function pairOfIntAndDouble() {
  const p = new Pair_i64_double(7n, 8.5);
  return p.sumAsInt();
}

function doubledI64() {
  const d = new Doubler_i64(21n);
  return d.doubled();
}

function containerOk() {
  const c = new Container(new Ok(42n));
  return c.unwrapOrDefault(0n);
}

function makePairResult(ok) {
  if (ok === 0n) {
    return new Err(-1n);
  }
  return new Ok(new Pair_i64_i64(11n, 22n));
}

function pairResultOrErr(ok) {
  const r = makePairResult(ok);
  if (r instanceof Err) {
    return r.value;
  }
  const p = r.value;
  return i64(i64(p.a * 100n) + p.b);
}

class Box {
  constructor(value) {
    this.value = value;
  }

  getValue() {
    return this.value;
  }
}

class Pair_i64_double {
  constructor(a, b) {
    this.a = a;
    this.b = b;
  }

  sumAsInt() {
    return i64(this.a + BigInt(Math.trunc(this.b)));
  }
}

class Doubler_i64 {
  constructor(value) {
    this.value = value;
  }

  doubled() {
    return i64(this.value + this.value);
  }
}

class Container {
  constructor(value) {
    this.value = value;
  }

  unwrapOrDefault(def) {
    if (this.value instanceof Err) {
      return def;
    }
    return this.value.value;
  }
}

class Pair_i64_i64 {
  constructor(a, b) {
    this.a = a;
    this.b = b;
  }

  sumAsInt() {
    return i64(this.a + this.b);
  }
}

export { boxedI64, boxedBoth, pairOfIntAndDouble, doubledI64, containerOk, makePairResult, pairResultOrErr, Box, Pair_i64_double, Doubler_i64, Container, Pair_i64_i64 };
