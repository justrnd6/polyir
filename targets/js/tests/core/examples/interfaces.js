"use strict";

// A 64-bit integer is a BigInt, brought back into 64 bits after an operation that can leave them.
function i64(value) {
  return BigInt.asIntN(64, value);
}

class Square {
  constructor(side) {
    this.side = side;
  }

  area() {
    return i64(this.side * this.side);
  }

  scaled(k) {
    return i64(this.side * k);
  }

  id() {
    return 1n;
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

  scaled(k) {
    return i64(this.w * k);
  }
}

class Summer {
  constructor(base) {
    this.base = base;
  }

  combine(other) {
    return i64(this.base + other.area());
  }
}

class Holder {
  constructor(shape, tag) {
    this.shape = shape;
    this.tag = tag;
  }
}

function totalArea(s) {
  return s.area();
}

function directCall() {
  return i64(i64(totalArea(new Square(3n)) * 100n) + totalArea(new Rect(4n, 5n)));
}

function pick(which) {
  if (which === 0n) {
    return new Square(6n);
  }
  return new Rect(2n, 7n);
}

function chosen() {
  const a = pick(0n);
  const b = pick(1n);
  return i64(i64(i64(a.area() * 1000n) + b.area()) + b.scaled(10n));
}

function sumAll() {
  const shapes = [];
  shapes.push(new Square(2n));
  shapes.push(new Rect(3n, 4n));
  shapes.push(new Square(5n));
  let total = 0n;
  for (let i = 0n; i < BigInt(shapes.length); i = i64(i + 1n)) {
    total = i64(total + shapes[Number(i)].area());
  }
  return total;
}

function viaField() {
  const h = new Holder(new Rect(5n, 6n), 7n);
  return i64(i64(h.shape.area() * 10n) + h.tag);
}

function twoInterfaces() {
  const sq = new Square(4n);
  const n = sq;
  const s = sq;
  return i64(i64(n.id() * 1000n) + s.area());
}

function interfaceArgument() {
  const c = new Summer(100n);
  return c.combine(new Rect(3n, 3n));
}

function main() {
  return i64(i64(i64(i64(i64(directCall() + chosen()) + sumAll()) + viaField()) + twoInterfaces()) + interfaceArgument());
}

const status = main();
if (typeof process !== "undefined") {
  process.exitCode = Number(status);
}
