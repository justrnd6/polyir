// A 64-bit integer is a bigint, brought back into 64 bits after an operation that can leave them.
function i64(value: bigint): bigint {
  return BigInt.asIntN(64, value);
}

class Point {
  constructor(public x: number, public y: number) {}

  dist(): number {
    return this.x * this.x + this.y * this.y;
  }
}

class Circle {
  constructor(public r: number) {}

  dist(): number {
    return this.r * this.r;
  }
}

function distSum(px: number, py: number, r: number): number {
  const p: Point = new Point(px, py);
  const c: Circle = new Circle(r);
  return p.dist() + c.dist();
}

class Pair {
  constructor(public a: bigint, public b: bigint) {}

  sum(): bigint {
    return i64(this.a + this.b);
  }

  doubledSum(): bigint {
    return i64(this.sum() * 2n);
  }
}

function doubledSumCheck(a: bigint, b: bigint): bigint {
  const p: Pair = new Pair(a, b);
  return p.doubledSum();
}

export { Point, Circle, distSum, Pair, doubledSumCheck };
