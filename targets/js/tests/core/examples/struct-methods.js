"use strict";

// A 64-bit integer is a BigInt, brought back into 64 bits after an operation that can leave them.
function i64(value) {
  return BigInt.asIntN(64, value);
}

class Point {
  constructor(x, y) {
    this.x = x;
    this.y = y;
  }

  dist() {
    return this.x * this.x + this.y * this.y;
  }
}

class Circle {
  constructor(r) {
    this.r = r;
  }

  dist() {
    return this.r * this.r;
  }
}

function distSum(px, py, r) {
  const p = new Point(px, py);
  const c = new Circle(r);
  return p.dist() + c.dist();
}

class Pair {
  constructor(a, b) {
    this.a = a;
    this.b = b;
  }

  sum() {
    return i64(this.a + this.b);
  }

  doubledSum() {
    return i64(this.sum() * 2n);
  }
}

function doubledSumCheck(a, b) {
  const p = new Pair(a, b);
  return p.doubledSum();
}

export { Point, Circle, distSum, Pair, doubledSumCheck };
