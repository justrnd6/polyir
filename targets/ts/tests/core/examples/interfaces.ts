declare const process: any;
export {};

// A 64-bit integer is a bigint, brought back into 64 bits after an operation that can leave them.
function i64(value: bigint): bigint {
  return BigInt.asIntN(64, value);
}

interface Shape {
  area(): bigint;
  scaled(p0: bigint): bigint;
}

interface Named {
  id(): bigint;
}

interface Combiner {
  combine(p0: Shape): bigint;
}

class Square {
  constructor(public side: bigint) {}

  area(): bigint {
    return i64(this.side * this.side);
  }

  scaled(k: bigint): bigint {
    return i64(this.side * k);
  }

  id(): bigint {
    return 1n;
  }
}

class Rect {
  constructor(public w: bigint, public h: bigint) {}

  area(): bigint {
    return i64(this.w * this.h);
  }

  scaled(k: bigint): bigint {
    return i64(this.w * k);
  }
}

class Summer {
  constructor(public base: bigint) {}

  combine(other: Shape): bigint {
    return i64(this.base + other.area());
  }
}

class Holder {
  constructor(public shape: Shape, public tag: bigint) {}
}

function totalArea(s: Shape): bigint {
  return s.area();
}

function directCall(): bigint {
  return i64(i64(totalArea(new Square(3n)) * 100n) + totalArea(new Rect(4n, 5n)));
}

function pick(which: bigint): Shape {
  if (which === 0n) {
    return new Square(6n);
  }
  return new Rect(2n, 7n);
}

function chosen(): bigint {
  const a: Shape = pick(0n);
  const b: Shape = pick(1n);
  return i64(i64(i64(a.area() * 1000n) + b.area()) + b.scaled(10n));
}

function sumAll(): bigint {
  const shapes: Array<Shape> = [];
  shapes.push(new Square(2n));
  shapes.push(new Rect(3n, 4n));
  shapes.push(new Square(5n));
  let total: bigint = 0n;
  for (let i = 0n; i < BigInt(shapes.length); i = i64(i + 1n)) {
    total = i64(total + shapes[Number(i)].area());
  }
  return total;
}

function viaField(): bigint {
  const h: Holder = new Holder(new Rect(5n, 6n), 7n);
  return i64(i64(h.shape.area() * 10n) + h.tag);
}

function twoInterfaces(): bigint {
  const sq: Square = new Square(4n);
  const n: Named = sq;
  const s: Shape = sq;
  return i64(i64(n.id() * 1000n) + s.area());
}

function interfaceArgument(): bigint {
  const c: Combiner = new Summer(100n);
  return c.combine(new Rect(3n, 3n));
}

function main(): bigint {
  return i64(i64(i64(i64(i64(directCall() + chosen()) + sumAll()) + viaField()) + twoInterfaces()) + interfaceArgument());
}

const status = main();
if (typeof process !== "undefined") {
  process.exitCode = Number(status);
}
