declare const process: any;
export {};

// A 64-bit integer is a bigint, brought back into 64 bits after an operation that can leave them.
function i64(value: bigint): bigint {
  return BigInt.asIntN(64, value);
}

class Pt {
  constructor(public x: bigint, public y: bigint) {}
}

class Seg {
  constructor(public a: Pt, public b: Pt, public tag: bigint) {}
}

class Counter {
  constructor(public hits: bigint, public name: string) {}
}

function measure(s: Seg, c: Counter, word: string): bigint {
  const dx: bigint = i64(s.b.x - s.a.x);
  const xs: Array<bigint> = [];
  xs.push(dx);
  xs.push(7n);
  const cells: Array<bigint> = Array.from({ length: 3 }, () => 0n);
  c.hits = i64(c.hits + dx);
  return i64(BigInt(xs.length) + c.hits);
}

function main(): bigint {
  const seg: Seg = new Seg(new Pt(1n, 2n), new Pt(4n, 6n), 9n);
  const c: Counter = new Counter(0n, "ctr");
  return measure(seg, c, "hello");
}

const status = main();
if (typeof process !== "undefined") {
  process.exitCode = Number(status);
}
