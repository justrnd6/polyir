declare const process: any;
export {};

// An unsigned 64-bit integer is a bigint, brought back into 0 .. 2^64 - 1 after an operation that can leave them.
function u64(value: bigint): bigint {
  return BigInt.asUintN(64, value);
}

// A 64-bit integer is a bigint, brought back into 64 bits after an operation that can leave them.
function i64(value: bigint): bigint {
  return BigInt.asIntN(64, value);
}

// Whether two values hold the same: numbers, flags and strings by value, lists item by item, maps and sets by their
// entries, a record by its own `equals` method when it has one, else field by field.
function same(a: any, b: any): boolean {
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

class Pixel {
  constructor(public r: number, public g: number, public b: number) {}
}

function comparison(): bigint {
  const big: number = 4000000000;
  if (big > 3000000000 && big >= big && big < 5 === false) {
    return 1n;
  }
  return 0n;
}

function division(): bigint {
  const big: number = 4000000000;
  const half: number = (big / 2) >>> 0;
  const rest: number = big % 7;
  if (BigInt(half) === 2000000000n && BigInt(rest) === 3n) {
    return 1n;
  }
  return 0n;
}

function shifting(): bigint {
  const big: number = 4000000000;
  const top: bigint = u64(i64(0n - 1n));
  if (BigInt(big >>> 28) === 14n && i64(top >> 63n) === 1n) {
    return 1n;
  }
  return 0n;
}

function widening(): bigint {
  const big: number = 4000000000;
  const small: number = 200;
  if (BigInt(big) === 4000000000n && BigInt(small) === 200n) {
    return 1n;
  }
  return 0n;
}

function wrapping(): bigint {
  const a: number = 200;
  const b: number = 100;
  const sum: number = (a + b) & 255;
  const under: number = (b - a) & 255;
  const product: number = 300;
  const wide: number = (product * 300) & 65535;
  if (BigInt(sum) === 44n && BigInt(under) === 156n && BigInt(wide) === 24464n) {
    return 1n;
  }
  return 0n;
}

function floats(): bigint {
  const big: number = 4000000000;
  if (BigInt(Math.trunc(big)) === 4000000000n) {
    return 1n;
  }
  return 0n;
}

function largest(): bigint {
  const top: bigint = u64(i64(0n - 1n));
  if (top > 5n && top / 2n === 9223372036854775807n) {
    return 1n;
  }
  return 0n;
}

function fnv(n: bigint): number {
  let h: number = 2166136261;
  for (let i = 0n; i < n; i = i64(i + 1n)) {
    h = (((h ^ Number(BigInt.asUintN(32, i))) >>> 0) * 16777619) >>> 0;
  }
  return h;
}

function hashing(): bigint {
  if (BigInt(fnv(3n)) === 581859880n) {
    return 1n;
  }
  return 0n;
}

function fields(): bigint {
  const a: Pixel = new Pixel(250, 128, 7);
  const b: Pixel = new Pixel(250, 128, 7);
  if (same(a, b) && BigInt(a.g) === 128n && a.r > a.b) {
    return 1n;
  }
  return 0n;
}

function keys(): bigint {
  const m: Map<number, bigint> = new Map();
  const k: number = 4000000000;
  m.set(k, 7n);
  m.set(5, 9n);
  if ((m.get(k) ?? 0n) === 7n && (m.get(5) ?? 0n) === 9n) {
    return 1n;
  }
  return 0n;
}

function main(): bigint {
  return i64(i64(i64(i64(i64(i64(i64(i64(i64(comparison() + division()) + shifting()) + widening()) + wrapping()) + floats()) + largest()) + hashing()) + fields()) + keys());
}

const status = main();
if (typeof process !== "undefined") {
  process.exitCode = Number(status);
}
