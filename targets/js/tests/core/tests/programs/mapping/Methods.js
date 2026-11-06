"use strict";

// A 64-bit integer is a BigInt, brought back into 64 bits after an operation that can leave them.
function i64(value) {
  return BigInt.asIntN(64, value);
}

class Acc {
  constructor(total, tag, items) {
    this.total = total;
    this.tag = tag;
    this.items = items;
  }

  add(x) {
    this.total = i64(this.total + x);
    this.items.push(x);
    return this.total;
  }

  later() {
    return () => i64(this.total * 2n);
  }

  describe() {
    return this.tag + ":" + this.tag;
  }

  self() {
    return this;
  }
}

class Dog {
  constructor(age) {
    this.age = age;
  }

  legs(n) {
    return i64(i64(4n * n) + this.age);
  }

  name() {
    return "dog";
  }
}

class Bird {
  constructor(age) {
    this.age = age;
  }

  legs(n) {
    return i64(2n * n);
  }

  name() {
    return "bird";
  }
}

function risky(x) {
  if (x < 0n) {
    throw 7n;
  }
  return x;
}

function tryReturn(x) {
  try {
    return i64(risky(x) + 1n);
  } catch (code) {
    return i64(code * 100n);
  }
  return 0n;
}

function nested(x) {
  let r = 0n;
  try {
    try {
      r = risky(x);
    } catch (a) {
      r = i64(a + 1n);
      throw r;
    }
  } catch (b) {
    r = i64(b * 10n);
  }
  return r;
}

function makeAdder(n) {
  return (x) => i64(x + n);
}

function twice(f, x) {
  return f(f(x));
}

function counterPair() {
  let c = 0n;
  const inc = () => {
    c = i64(c + 1n);
    return c;
  };
  const get = () => c;
  inc();
  inc();
  return i64(i64(get() * 10n) + c);
}

function main() {
  let score = 0n;
  const a = new Acc(1n, "ab", []);
  score = i64(i64(score + a.add(5n)) + a.add(6n));
  const f = a.later();
  score = i64(score + f());
  score = i64(score + BigInt(a.items.length));
  if (a.describe() === "ab:ab") {
    score = i64(score + 1000n);
  }
  const s = a.self();
  if (s === a) {
    score = i64(score + 2000n);
  }
  a.tag = "zz";
  if (s.tag === "zz") {
    score = i64(score + 3000n);
  }
  const zoo = [];
  zoo.push(new Dog(3n));
  zoo.push(new Bird(1n));
  for (let i = 0n; i < BigInt(zoo.length); i = i64(i + 1n)) {
    score = i64(score + zoo[Number(i)].legs(2n));
    if (zoo[Number(i)].name() === "bird") {
      score = i64(score + 7n);
    }
  }
  score = i64(i64(score + tryReturn(3n)) + tryReturn(i64(0n - 1n)));
  score = i64(i64(score + nested(4n)) + nested(i64(0n - 2n)));
  const add3 = makeAdder(3n);
  score = i64(score + twice(add3, 10n));
  score = i64(score + counterPair());
  return score % 251n;
}

const status = main();
if (typeof process !== "undefined") {
  process.exitCode = Number(status);
}
