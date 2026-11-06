"use strict";

// A 64-bit integer is a BigInt, brought back into 64 bits after an operation that can leave them.
function i64(value) {
  return BigInt.asIntN(64, value);
}

class Geometry {
  static Point = class {
    constructor(x, y) {
      this.x = x;
      this.y = y;
    }

    manhattan() {
      return i64(this.absolute(this.x) + this.absolute(this.y));
    }

    absolute(n) {
      return n < 0n ? i64(0n - n) : n;
    }
  };

  static clamp(n, low, high) {
    return n < low ? low : n > high ? high : n;
  }

  static distance(a, b) {
    const step = new Geometry.Point(i64(a.x - b.x), i64(a.y - b.y));
    return Geometry.clamp(step.manhattan(), 0n, 100n);
  }

  static Shapes = class {
    static Square = class {
      constructor(side) {
        this.side = side;
      }

      area() {
        return i64(this.side * this.side);
      }
    };

    static Rect = class {
      constructor(w, h) {
        this.w = w;
        this.h = h;
      }

      area() {
        return i64(this.w * this.h);
      }
    };

    static total(shapes) {
      let sum = 0n;
      for (const s of shapes) {
        sum = i64(sum + s.area());
      }
      return sum;
    }
  };
}

function main() {
  const a = new Geometry.Point(1n, 2n);
  const b = new Geometry.Point(4n, -2n);
  const shapes = [new Geometry.Shapes.Square(3n), new Geometry.Shapes.Rect(2n, 5n)];
  const size = Geometry.Shapes.total(shapes) > 10n ? "big" : "small";
  console.log(`${Geometry.distance(a, b)} ${size} ${larger(3n, 7n, false)} ${larger("x", "y", true)} ${firstOf(["first", "second"])}`);
}

function larger(a, b, aFirst) {
  return aFirst ? a : b;
}

function firstOf(xs) {
  return xs[0];
}

main();
