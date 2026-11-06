export {};

// A 64-bit integer is a bigint, brought back into 64 bits after an operation that can leave them.
function i64(value: bigint): bigint {
  return BigInt.asIntN(64, value);
}

namespace Geometry {
  export class Point {
    constructor(public x: bigint, public y: bigint) {}

    manhattan(): bigint {
      return i64(this.absolute(this.x) + this.absolute(this.y));
    }

    absolute(n: bigint): bigint {
      return n < 0n ? i64(0n - n) : n;
    }
  }

  export function clamp(n: bigint, low: bigint, high: bigint): bigint {
    return n < low ? low : n > high ? high : n;
  }

  export function distance(a: Geometry.Point, b: Geometry.Point): bigint {
    const step: Geometry.Point = new Geometry.Point(i64(a.x - b.x), i64(a.y - b.y));
    return Geometry.clamp(step.manhattan(), 0n, 100n);
  }

  export namespace Shapes {
    export interface Shape {
      area(): bigint;
    }

    export class Square {
      constructor(public side: bigint) {}

      area(): bigint {
        return i64(this.side * this.side);
      }
    }

    export class Rect {
      constructor(public w: bigint, public h: bigint) {}

      area(): bigint {
        return i64(this.w * this.h);
      }
    }

    export function total(shapes: Array<Geometry.Shapes.Shape>): bigint {
      let sum: bigint = 0n;
      for (const s of shapes) {
        sum = i64(sum + s.area());
      }
      return sum;
    }
  }
}

function main(): void {
  const a: Geometry.Point = new Geometry.Point(1n, 2n);
  const b: Geometry.Point = new Geometry.Point(4n, -2n);
  const shapes: Array<Geometry.Shapes.Shape> = [new Geometry.Shapes.Square(3n), new Geometry.Shapes.Rect(2n, 5n)];
  const size: string = Geometry.Shapes.total(shapes) > 10n ? "big" : "small";
  console.log(`${Geometry.distance(a, b)} ${size} ${larger(3n, 7n, false)} ${larger("x", "y", true)} ${firstOf(["first", "second"])}`);
}

function larger<T>(a: T, b: T, aFirst: boolean): T {
  return aFirst ? a : b;
}

function firstOf<T>(xs: Array<T>): T {
  return xs[0];
}

main();
