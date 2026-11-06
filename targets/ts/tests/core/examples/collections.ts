declare const process: any;
export {};

// A 64-bit integer is a bigint, brought back into 64 bits after an operation that can leave them.
function i64(value: bigint): bigint {
  return BigInt.asIntN(64, value);
}

// A copy of a list, map, set or record: a new one holding the same items.
function copyOf<T>(value: T): T {
  if (Array.isArray(value)) {
    return [...value] as T;
  }
  if (value instanceof Map || value instanceof Set) {
    return new (value.constructor as any)(value);
  }
  return Object.assign(Object.create(Object.getPrototypeOf(value)), value);
}

// A text two values share exactly when polyir calls them equal: the kind and contents of a number or string, and the
// class and parts of an object (a record's fields, a list's items, a set's members in order).
function valueKey(value: any): string {
  if (typeof value === "string") {
    return JSON.stringify(value);
  }
  if (value === null || typeof value !== "object") {
    return typeof value + ":" + String(value);
  }
  if (value instanceof Map || value instanceof Set) {
    return value.constructor.name + "(" + [...value].map(valueKey).sort().join(",") + ")";
  }
  const parts: any[] = Array.isArray(value) ? value : Object.values(value);
  return value.constructor.name + "(" + parts.map(valueKey).join(",") + ")";
}

// A Set of records, lists or sets compared by what they hold: the first one added stands for all values equal to it.
class ValueSet<T = any> extends Set<T> {
  private byKey = new Map<string, T>();

  constructor(items?: Iterable<T>) {
    super();
    for (const item of items ?? []) {
      this.add(item);
    }
  }

  add(item: T): this {
    const key = valueKey(item);
    if (!this.byKey.has(key)) {
      this.byKey.set(key, item);
      super.add(item);
    }
    return this;
  }

  has(item: T): boolean {
    return this.byKey.has(valueKey(item));
  }

  delete(item: T): boolean {
    const key = valueKey(item);
    const held = this.byKey.get(key) as T;
    return this.byKey.delete(key) && super.delete(held);
  }
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

class Point {
  constructor(public x: bigint, public y: bigint) {}
}

function grow(): bigint {
  const none: Array<bigint> = [];
  const one: Array<bigint> = [...none, 10n];
  const two: Array<bigint> = [...one, 20n];
  return i64(i64(i64(i64(BigInt(none.length) * 1000n) + i64(BigInt(one.length) * 100n)) + i64(BigInt(two.length) * 10n)) + two[1] / 10n);
}

function build(): Array<bigint> {
  const xs: Array<bigint> = [];
  for (let i = 0n; i < 5n; i = i64(i + 1n)) {
    xs.push(i64(i * i));
  }
  return [...xs];
}

function lookup(): bigint {
  const ages: Map<string, bigint> = copyOf(copyOf(new Map()).set("ann", 31n)).set("bo", 27n);
  const seen: Set<bigint> = copyOf(copyOf(copyOf(new Set()).add(3n)).add(5n)).add(3n);
  let score: bigint = i64(i64((ages.get("ann") ?? 0n) + (ages.get("bo") ?? 0n)) + BigInt(seen.size));
  if (ages.has("cy") === false) {
    score = i64(score + 100n);
  }
  if (seen.has(5n)) {
    score = i64(score + 1000n);
  }
  return score;
}

function visited(): bigint {
  const seen: Set<Point> = new ValueSet();
  seen.add(new Point(1n, 2n));
  seen.add(new Point(3n, 4n));
  seen.add(new Point(1n, 2n));
  let score: bigint = BigInt(seen.size);
  if (seen.has(new Point(3n, 4n))) {
    score = i64(score + 10n);
  }
  return score;
}

function sameThings(): bigint {
  const a: Map<bigint, string> = copyOf(copyOf(new Map()).set(1n, "a")).set(2n, "b");
  const b: Map<bigint, string> = copyOf(copyOf(new Map()).set(2n, "b")).set(1n, "a");
  let score: bigint = 0n;
  if (same(a, b)) {
    score = i64(score + 1n);
  }
  if (a === b) {
    score = i64(score + 100n);
  }
  return score;
}

function main(): bigint {
  return i64(i64(i64(i64(grow() + i64(build()[4] * 10000n)) + lookup()) + visited()) + sameThings());
}

const status = main();
if (typeof process !== "undefined") {
  process.exitCode = Number(status);
}
