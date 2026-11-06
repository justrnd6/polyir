"use strict";

// A 64-bit integer is a BigInt, brought back into 64 bits after an operation that can leave them.
function i64(value) {
  return BigInt.asIntN(64, value);
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

class Pt {
  constructor(x, y) {
    this.x = x;
    this.y = y;
  }
}

class Counter {
  constructor(n, name) {
    this.n = n;
    this.name = name;
  }
}

class Sq {
  constructor(side) {
    this.side = side;
  }

  area() {
    return i64(this.side * this.side);
  }
}

class Rect {
  constructor(w, h) {
    this.w = w;
    this.h = h;
  }

  area() {
    return i64(this.w * this.h);
  }
}

function totalArea(shapes) {
  let sum = 0n;
  for (let i = 0n; i < BigInt(shapes.length); i = i64(i + 1n)) {
    sum = i64(sum + shapes[Number(i)].area());
  }
  return sum;
}

function risky(x) {
  if (x < 0n) {
    throw 7n;
  }
  return x;
}

function guarded(x) {
  let out = 0n;
  try {
    out = risky(x);
  } catch (code) {
    out = i64(code * 10n);
  }
  return out;
}

function main() {
  let score = 0n;
  const p = new Pt(3n, 4n);
  const q = new Pt(3n, 4n);
  if (same(p, q)) {
    score = i64(score + 1n);
  }
  const c = new Counter(5n, "hits");
  const d = c;
  d.n = i64(d.n + 1n);
  if (c === d) {
    score = i64(score + c.n);
  }
  const shapes = [];
  shapes.push(new Sq(3n));
  shapes.push(new Rect(2n, 5n));
  score = i64(score + totalArea(shapes));
  let calls = 0n;
  const tick = () => {
    calls = i64(calls + 1n);
    return calls;
  };
  const first = tick();
  const second = tick();
  score = i64(i64(i64(score + first) + second) + calls);
  const stock = new Map();
  stock.set("apple", 3n);
  stock.set("pear", 4n);
  score = i64(score + i64((stock.get("apple") ?? 0n) * (stock.get("pear") ?? 0n)));
  const b = new Box(7n);
  score = i64(score + b.value);
  score = i64(i64(score + guarded(5n)) + guarded(i64(0n - 1n)));
  return score;
}

class Box {
  constructor(value) {
    this.value = value;
  }
}

const status = main();
if (typeof process !== "undefined") {
  process.exitCode = Number(status);
}
