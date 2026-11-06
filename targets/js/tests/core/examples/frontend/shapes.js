"use strict";

// A 64-bit integer is a BigInt, brought back into 64 bits after an operation that can leave them.
function i64(value) {
  return BigInt.asIntN(64, value);
}

class Pt {
  constructor(x, y) {
    this.x = x;
    this.y = y;
  }
}

class Seg {
  constructor(a, b, tag) {
    this.a = a;
    this.b = b;
    this.tag = tag;
  }
}

class Counter {
  constructor(hits, name) {
    this.hits = hits;
    this.name = name;
  }
}

function measure(s, c, word) {
  const dx = i64(s.b.x - s.a.x);
  const xs = [];
  xs.push(dx);
  xs.push(7n);
  const cells = Array.from({ length: 3 }, () => 0n);
  c.hits = i64(c.hits + dx);
  return i64(BigInt(xs.length) + c.hits);
}

function main() {
  const seg = new Seg(new Pt(1n, 2n), new Pt(4n, 6n), 9n);
  const c = new Counter(0n, "ctr");
  return measure(seg, c, "hello");
}

const status = main();
if (typeof process !== "undefined") {
  process.exitCode = Number(status);
}
