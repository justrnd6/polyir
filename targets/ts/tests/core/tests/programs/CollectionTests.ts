import { offInt } from "./Check.js";

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

// A 64-bit integer is a bigint, brought back into 64 bits after an operation that can leave them.
function i64(value: bigint): bigint {
  return BigInt.asIntN(64, value);
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

// A Map whose keys are records, lists or sets compared by what they hold: the first key set stands for all keys equal to it.
class ValueMap<K = any, V = any> extends Map<K, V> {
  private byKey = new Map<string, K>();

  constructor(entries?: Iterable<[K, V]>) {
    super();
    for (const [key, value] of entries ?? []) {
      this.set(key, value);
    }
  }

  set(key: K, value: V): this {
    const text = valueKey(key);
    if (!this.byKey.has(text)) {
      this.byKey.set(text, key);
    }
    return super.set(this.byKey.get(text) as K, value);
  }

  get(key: K): V | undefined {
    return super.get(this.byKey.get(valueKey(key)) as K);
  }

  has(key: K): boolean {
    return this.byKey.has(valueKey(key));
  }

  delete(key: K): boolean {
    const text = valueKey(key);
    const held = this.byKey.get(text) as K;
    return this.byKey.delete(text) && super.delete(held);
  }
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

class Named {
  constructor(public name: string, public at: Point) {}
}

function checkAnArrayGrowsByMakingANewOne(): number {
  const a0: Array<bigint> = [];
  const a1: Array<bigint> = [...a0, 10n];
  const a2: Array<bigint> = [...a1, 20n];
  return offInt(BigInt(a0.length), 0n) + offInt(BigInt(a1.length), 1n) + offInt(BigInt(a2.length), 2n) + offInt(a2[0], 10n) + offInt(a2[1], 20n);
}

function checkThawAndFreezeCopy(): number {
  const a: Array<bigint> = [1n, 2n];
  const m: Array<bigint> = [...a];
  m.push(3n);
  const f: Array<bigint> = [...m];
  m.push(4n);
  return offInt(BigInt(a.length), 2n) + offInt(BigInt(m.length), 4n) + offInt(BigInt(f.length), 3n) + offInt(f[2], 3n);
}

function checkAMapGrowsByMakingANewOne(): number {
  const m0: Map<bigint, bigint> = new Map();
  const m1: Map<bigint, bigint> = copyOf(m0).set(1n, 100n);
  let m: Map<bigint, bigint> = m1;
  for (let i = 2n; i < 30n; i = i64(i + 1n)) {
    m = copyOf(m).set(i, i64(i * 100n));
  }
  return offInt(BigInt(m0.size), 0n) + offInt(BigInt(m1.size), 1n) + offInt(BigInt(m.size), 29n) + offInt(m.get(1n) ?? 0n, 100n) + offInt(m.get(29n) ?? 0n, 2900n) + offInt(m1.get(2n) ?? 0n, 0n);
}

function checkWithReplacesAnExistingKey(): number {
  const m: Map<bigint, bigint> = copyOf(copyOf(new Map()).set(1n, 10n)).set(1n, 11n);
  return offInt(BigInt(m.size), 1n) + offInt(m.get(1n) ?? 0n, 11n);
}

function checkHasTellsPresentFromAbsent(): number {
  const m: Map<string, bigint> = copyOf(new Map()).set("a", 0n);
  let score: bigint = 0n;
  if (m.has("a")) {
    score = i64(score + 1n);
  }
  if (m.has("b") === false) {
    score = i64(score + 2n);
  }
  return offInt(score, 3n);
}

function checkASetKeepsEachMemberOnce(): number {
  const s: Set<bigint> = copyOf(copyOf(copyOf(new Set()).add(7n)).add(8n)).add(7n);
  let score: bigint = 0n;
  if (s.has(7n)) {
    score = i64(score + 1n);
  }
  if (s.has(9n) === false) {
    score = i64(score + 2n);
  }
  return offInt(BigInt(s.size), 2n) + offInt(score, 3n);
}

function checkAMutsetChangesInPlace(): number {
  const s: Set<string> = new Set();
  s.add("x");
  s.add("y");
  s.add("x");
  let score: bigint = 0n;
  if (s.has("y")) {
    score = i64(score + 1n);
  }
  if (s.has("z") === false) {
    score = i64(score + 2n);
  }
  return offInt(BigInt(s.size), 2n) + offInt(score, 3n);
}

function checkAMutmapChangesInPlace(): number {
  const m: Map<bigint, bigint> = new Map();
  m.set(1n, 10n);
  m.set(2n, 20n);
  m.set(1n, 11n);
  return offInt(BigInt(m.size), 2n) + offInt(m.get(1n) ?? 0n, 11n) + offInt(m.get(2n) ?? 0n, 20n);
}

function checkAStructCanBeAKey(): number {
  const seen: Map<Point, bigint> = new ValueMap();
  seen.set(new Point(1n, 2n), 10n);
  seen.set(new Point(3n, 4n), 20n);
  seen.set(new Point(1n, 2n), 11n);
  let score: bigint = 0n;
  if (seen.has(new Point(3n, 4n))) {
    score = i64(score + 1n);
  }
  if (seen.has(new Point(9n, 9n)) === false) {
    score = i64(score + 2n);
  }
  return offInt(BigInt(seen.size), 2n) + offInt(seen.get(new Point(1n, 2n)) ?? 0n, 11n) + offInt(score, 3n);
}

function checkAStructWithAStringIsAKeyByContent(): number {
  const names: Set<Named> = copyOf(copyOf(copyOf(new ValueSet()).add(new Named("a" + "b", new Point(1n, 1n)))).add(new Named("ab", new Point(1n, 1n)))).add(new Named("c", new Point(2n, 2n)));
  let score: bigint = 0n;
  if (names.has(new Named("c", new Point(2n, 2n)))) {
    score = i64(score + 1n);
  }
  return offInt(BigInt(names.size), 2n) + offInt(score, 1n);
}

function checkAnImmutableArrayIsAKey(): number {
  const lists: Map<Array<bigint>, string> = copyOf(copyOf(new ValueMap()).set([1n, 2n], "one-two")).set([3n], "three");
  let score: bigint = 0n;
  if ((lists.get([1n, 2n]) ?? "") === "one-two") {
    score = i64(score + 1n);
  }
  if ((lists.get([3n]) ?? "") === "three") {
    score = i64(score + 2n);
  }
  return offInt(score, 3n);
}

function checkMapsAreEqualWhateverTheOrderTheyWereBuiltIn(): number {
  const a: Map<bigint, string> = copyOf(copyOf(new Map()).set(1n, "a")).set(2n, "b");
  const b: Map<bigint, string> = copyOf(copyOf(new Map()).set(2n, "b")).set(1n, "a");
  const c: Map<bigint, string> = copyOf(copyOf(new Map()).set(2n, "b")).set(1n, "z");
  let score: bigint = 0n;
  if (same(a, b)) {
    score = i64(score + 1n);
  }
  if (!same(a, c)) {
    score = i64(score + 2n);
  }
  return offInt(score, 3n);
}

function checkSetsAreEqualWhateverTheOrderTheyWereBuiltIn(): number {
  const a: Set<string> = copyOf(copyOf(new Set()).add("x")).add("y");
  const b: Set<string> = copyOf(copyOf(new Set()).add("y")).add("x");
  let score: bigint = 0n;
  if (same(a, b)) {
    score = i64(score + 1n);
  }
  if (!same(a, copyOf(new Set()).add("x"))) {
    score = i64(score + 2n);
  }
  return offInt(score, 3n);
}

function checkMutableAndImmutableListsAreEqualByContent(): number {
  const a: Array<bigint> = [1n, 2n];
  const b: Array<bigint> = [1n, 2n];
  let score: bigint = 0n;
  if (same(a, b)) {
    score = i64(score + 1n);
  }
  if (a === b) {
    score = i64(score + 100n);
  }
  return offInt(score, 1n);
}

function checkRepeatedWithInALoopIsNotQuadraticInMemory(): number {
  let total: bigint = 0n;
  for (let r = 0n; r < 200n; r = i64(r + 1n)) {
    let a: Array<bigint> = [];
    let m: Map<bigint, bigint> = new Map();
    let s: Set<bigint> = new Set();
    for (let i = 0n; i < 20n; i = i64(i + 1n)) {
      a = [...a, i];
      m = copyOf(m).set(i, i64(i * 2n));
      s = copyOf(s).add(i % 7n);
    }
    total = i64(i64(i64(total + BigInt(a.length)) + (m.get(19n) ?? 0n)) + BigInt(s.size));
  }
  return offInt(total, i64(200n * 65n));
}

export { Point, Named, checkAnArrayGrowsByMakingANewOne, checkThawAndFreezeCopy, checkAMapGrowsByMakingANewOne, checkWithReplacesAnExistingKey, checkHasTellsPresentFromAbsent, checkASetKeepsEachMemberOnce, checkAMutsetChangesInPlace, checkAMutmapChangesInPlace, checkAStructCanBeAKey, checkAStructWithAStringIsAKeyByContent, checkAnImmutableArrayIsAKey, checkMapsAreEqualWhateverTheOrderTheyWereBuiltIn, checkSetsAreEqualWhateverTheOrderTheyWereBuiltIn, checkMutableAndImmutableListsAreEqualByContent, checkRepeatedWithInALoopIsNotQuadraticInMemory };
