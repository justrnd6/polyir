"use strict";

// A 64-bit integer is a BigInt, brought back into 64 bits after an operation that can leave them.
function i64(value) {
  return BigInt.asIntN(64, value);
}

// A copy of a list, map, set or record: a new one holding the same items.
function copyOf(value) {
  if (Array.isArray(value)) {
    return [...value];
  }
  if (value instanceof Map || value instanceof Set) {
    return new value.constructor(value);
  }
  return Object.assign(Object.create(Object.getPrototypeOf(value)), value);
}

// A text two values share exactly when polyir calls them equal: the kind and contents of a number or string, and the
// class and parts of an object (a record's fields, a list's items, a set's members in order).
function valueKey(value) {
  if (typeof value === "string") {
    return JSON.stringify(value);
  }
  if (value === null || typeof value !== "object") {
    return typeof value + ":" + String(value);
  }
  if (value instanceof Map || value instanceof Set) {
    return value.constructor.name + "(" + [...value].map(valueKey).sort().join(",") + ")";
  }
  const parts = Array.isArray(value) ? value : Object.values(value);
  return value.constructor.name + "(" + parts.map(valueKey).join(",") + ")";
}

// A Set of records, lists or sets compared by what they hold: the first one added stands for all values equal to it.
class ValueSet extends Set {
  constructor(items) {
    super();
    this.byKey = new Map();
    for (const item of items ?? []) {
      this.add(item);
    }
  }

  add(item) {
    const key = valueKey(item);
    if (!this.byKey.has(key)) {
      this.byKey.set(key, item);
      super.add(item);
    }
    return this;
  }

  has(item) {
    return this.byKey.has(valueKey(item));
  }

  delete(item) {
    const key = valueKey(item);
    const held = this.byKey.get(key);
    return this.byKey.delete(key) && super.delete(held);
  }
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

class Point {
  constructor(x, y) {
    this.x = x;
    this.y = y;
  }
}

function grow() {
  const none = [];
  const one = [...none, 10n];
  const two = [...one, 20n];
  return i64(i64(i64(i64(BigInt(none.length) * 1000n) + i64(BigInt(one.length) * 100n)) + i64(BigInt(two.length) * 10n)) + two[1] / 10n);
}

function build() {
  const xs = [];
  for (let i = 0n; i < 5n; i = i64(i + 1n)) {
    xs.push(i64(i * i));
  }
  return [...xs];
}

function lookup() {
  const ages = copyOf(copyOf(new Map()).set("ann", 31n)).set("bo", 27n);
  const seen = copyOf(copyOf(copyOf(new Set()).add(3n)).add(5n)).add(3n);
  let score = i64(i64((ages.get("ann") ?? 0n) + (ages.get("bo") ?? 0n)) + BigInt(seen.size));
  if (ages.has("cy") === false) {
    score = i64(score + 100n);
  }
  if (seen.has(5n)) {
    score = i64(score + 1000n);
  }
  return score;
}

function visited() {
  const seen = new ValueSet();
  seen.add(new Point(1n, 2n));
  seen.add(new Point(3n, 4n));
  seen.add(new Point(1n, 2n));
  let score = BigInt(seen.size);
  if (seen.has(new Point(3n, 4n))) {
    score = i64(score + 10n);
  }
  return score;
}

function sameThings() {
  const a = copyOf(copyOf(new Map()).set(1n, "a")).set(2n, "b");
  const b = copyOf(copyOf(new Map()).set(2n, "b")).set(1n, "a");
  let score = 0n;
  if (same(a, b)) {
    score = i64(score + 1n);
  }
  if (a === b) {
    score = i64(score + 100n);
  }
  return score;
}

function main() {
  return i64(i64(i64(i64(grow() + i64(build()[4] * 10000n)) + lookup()) + visited()) + sameThings());
}

const status = main();
if (typeof process !== "undefined") {
  process.exitCode = Number(status);
}
