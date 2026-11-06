import { offInt } from "./Check.js";

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

// A sparse grid: the cells written, as rows, columns and values. A read finds the last write to its cell; a cell never
// written reads as the element's zero.
class Sparse<T> {
  private rows: bigint[] = [];
  private cols: bigint[] = [];
  private values: T[] = [];

  constructor(private readonly zero: T) {}

  get length(): number {
    return this.values.length;
  }

  get(row: bigint, col: bigint): T {
    let found = this.zero;
    for (let i = 0; i < this.values.length; i++) {
      if (this.rows[i] === row && this.cols[i] === col) {
        found = this.values[i];
      }
    }
    return found;
  }

  set(row: bigint, col: bigint, value: T): void {
    this.rows.push(row);
    this.cols.push(col);
    this.values.push(value);
  }

  copy(): Sparse<T> {
    const out = new Sparse<T>(this.zero);
    out.rows = [...this.rows];
    out.cols = [...this.cols];
    out.values = [...this.values];
    return out;
  }
}

class Counter {
  constructor(public n: bigint, public id: bigint) {}

  bump(): bigint {
    this.n = i64(this.n + 1n);
    return this.n;
  }
}

class Pt {
  constructor(public x: bigint, public y: bigint) {}
}

class Holder {
  constructor(public items: Array<bigint>, public inner: Counter) {}
}

interface Reading {
  read(): bigint;
}

class Gauge {
  constructor(public level: bigint) {}

  read(): bigint {
    return this.level;
  }
}

function addTen(c: Counter): bigint {
  c.n = i64(c.n + 10n);
  return 0n;
}

function addTenOnACopy(c: Counter): bigint {
  const c: Counter = copyOf(c);
  c.n = i64(c.n + 10n);
  return c.n;
}

function checkTwoNamesForOneObjectShareIt(): number {
  const a: Counter = new Counter(1n, 7n);
  const b: Counter = a;
  b.n = 5n;
  return offInt(a.n, 5n) + offInt(a.id, 7n);
}

function checkACalleeChangesTheCallersObject(): number {
  const a: Counter = new Counter(1n, 0n);
  addTen(a);
  return offInt(a.n, 11n);
}

function checkAValParameterIsTheCalleesOwnCopy(): number {
  const a: Counter = new Counter(1n, 0n);
  const seen: bigint = addTenOnACopy(a);
  return offInt(seen, 11n) + offInt(a.n, 1n);
}

function checkValInFrontOfAnExpressionCopies(): number {
  const a: Counter = new Counter(1n, 3n);
  const b: Counter = copyOf(a);
  b.n = 50n;
  return offInt(a.n, 1n) + offInt(b.n, 50n) + offInt(b.id, 3n);
}

function checkCopyMethodAndValAgree(): number {
  const a: Counter = new Counter(4n, 0n);
  const b: Counter = copyOf(a);
  a.n = 9n;
  return offInt(b.n, 4n);
}

function checkAMethodChangesThis(): number {
  const a: Counter = new Counter(0n, 0n);
  a.bump();
  a.bump();
  return offInt(a.n, 2n);
}

function checkAnImmutableStructIsAValue(): number {
  const p: Pt = new Pt(1n, 2n);
  const q: Pt = copyOf(p);
  const r: Pt = copyOf(p);
  return offInt(i64(q.x + r.y), 3n);
}

function checkAFieldHoldingAnotherReferenceStruct(): number {
  const h: Holder = new Holder([], new Counter(1n, 0n));
  const alias: Holder = h;
  alias.inner.n = 8n;
  alias.items.push(5n);
  return offInt(h.inner.n, 8n) + offInt(BigInt(h.items.length), 1n);
}

function checkAFieldCanBeReplacedWithAnotherObject(): number {
  const h: Holder = new Holder([], new Counter(1n, 0n));
  const keep: Counter = h.inner;
  h.inner = new Counter(2n, 0n);
  return offInt(keep.n, 1n) + offInt(h.inner.n, 2n);
}

function checkAGenericStructWithAVarField(): number {
  const c: Cell<bigint> = new Cell(1n);
  const d: Cell<bigint> = c;
  d.v = 9n;
  return offInt(c.v, 9n);
}

function checkAnInterfaceValueSeesLaterChanges(): number {
  const g: Gauge = new Gauge(1n);
  const r: Reading = g;
  g.level = 42n;
  return offInt(r.read(), 42n);
}

function checkAStructInADarrayIsTheSameObject(): number {
  const all: Array<Counter> = [];
  const a: Counter = new Counter(1n, 0n);
  all.push(a);
  a.n = 6n;
  return offInt(all[0].n, 6n);
}

function checkAStructInAClosureIsShared(): number {
  const a: Counter = new Counter(1n, 0n);
  const f: () => bigint = (): bigint => {
    a.n = i64(a.n + 1n);
    return a.n;
  };
  f();
  return offInt(a.n, 2n);
}

function makeCounter(start: bigint): Counter {
  const c: Counter = new Counter(start, 1n);
  c.n = i64(c.n + 1n);
  return c;
}

function lend(c: Counter): bigint {
  return i64(c.n * 2n);
}

function keepIn(store: Array<Counter>, c: Counter): bigint {
  store.push(c);
  return 0n;
}

function checkAReturnedObjectSurvives(): number {
  const a: Counter = makeCounter(5n);
  const b: Counter = makeCounter(10n);
  return offInt(i64(i64(a.n * 100n) + b.n), 611n);
}

function checkAnObjectLentToACalleeIsUsableAfterwards(): number {
  let total: bigint = 0n;
  for (let i = 0n; i < 100n; i = i64(i + 1n)) {
    const c: Counter = new Counter(i, 0n);
    total = i64(i64(total + lend(c)) + c.n);
    c.n = 0n;
  }
  return offInt(total, i64(3n * 4950n));
}

function checkAnObjectAnotherCalleeKeepsSurvives(): number {
  const store: Array<Counter> = [];
  for (let i = 0n; i < 4n; i = i64(i + 1n)) {
    const c: Counter = new Counter(i64(i * 10n), 0n);
    keepIn(store, c);
  }
  return offInt(i64(i64(i64(store[0].n + store[1].n) + store[2].n) + store[3].n), 60n);
}

function checkAValParameterCopyIsReleasedAndTheOriginalIsNot(): number {
  const a: Counter = new Counter(1n, 0n);
  let total: bigint = 0n;
  for (let i = 0n; i < 50n; i = i64(i + 1n)) {
    total = i64(total + addTenOnACopy(a));
  }
  return offInt(total, i64(50n * 11n)) + offInt(a.n, 1n);
}

function earlyReturnWithObjects(n: bigint): bigint {
  const c: Counter = new Counter(n, 0n);
  if (n > 5n) {
    return i64(c.n * 2n);
  }
  c.bump();
  return c.n;
}

function checkAReturnInsideAnIfReleasesTheObjectAndKeepsTheValue(): number {
  return offInt(i64(i64(earlyReturnWithObjects(9n) * 100n) + earlyReturnWithObjects(2n)), i64(1800n + 3n));
}

function checkLetVarAndValOnReferenceStructs(): number {
  const a: Counter = new Counter(1n, 0n);
  const alias: Counter = a;
  const copy: Counter = copyOf(a);
  let moving: Counter = a;
  a.n = 10n;
  moving = new Counter(100n, 0n);
  moving.n = i64(moving.n + 1n);
  return offInt(alias.n, 10n) + offInt(copy.n, 1n) + offInt(moving.n, 101n) + offInt(a.n, 10n);
}

function checkValOnAContainerIsAnIndependentCopy(): number {
  const d: Array<bigint> = [];
  d.push(4n);
  const e: Array<bigint> = [...d];
  d.push(5n);
  return offInt(BigInt(e.length), 1n) + offInt(BigInt(d.length), 2n);
}

class Dot {
  constructor(public x: bigint, public y: bigint) {}
}

class Span {
  constructor(public lo: bigint, public hi: bigint) {}

  width(): bigint {
    return i64(this.hi - this.lo);
  }
}

class Meter {
  constructor(public reading: bigint, public id: bigint) {}

  bump(): bigint {
    this.reading = i64(this.reading + 1n);
    return this.reading;
  }
}

class Plate {
  constructor(public code: bigint) {}

  read(): bigint {
    return this.code;
  }
}

function checkShorthandStructsWork(): number {
  const d: Dot = new Dot(3n, 4n);
  const s: Span = new Span(2n, 9n);
  const w: Wrapper<bigint> = new Wrapper(5n);
  const r: Reading = new Plate(7n);
  return offInt(i64(d.x + d.y), 7n) + offInt(s.width(), 7n) + offInt(w.value, 5n) + offInt(r.read(), 7n);
}

function checkAShorthandStructWithAVarFieldIsAReference(): number {
  const m: Meter = new Meter(1n, 2n);
  const alias: Meter = m;
  alias.bump();
  return offInt(m.reading, 2n) + offInt(m.id, 2n);
}

function checkAHashmapOfObjectsIsUsableThenReleased(): number {
  let total: bigint = 0n;
  for (let i = 0n; i < 100n; i = i64(i + 1n)) {
    const m: Map<bigint, Counter> = new Map();
    m.set(1n, new Counter(i, 1n));
    m.set(2n, new Counter(5n, 2n));
    m.set(2n, new Counter(6n, 2n));
    m.get(1n).bump();
    total = i64(i64(i64(i64(total + m.get(1n).n) - i) + m.get(2n).n) + BigInt(m.size));
  }
  return offInt(total, i64(100n * i64(i64(1n + 6n) + 2n)));
}

function checkASparseOfStringsIsUsableThenReleased(): number {
  let total: bigint = 0n;
  for (let i = 0n; i < 100n; i = i64(i + 1n)) {
    const s: Sparse<string> = new Sparse("");
    s.set(0n, 0n, "a" + "b");
    s.set(2n, 3n, "c" + "d");
    if (s.get(2n, 3n) === "cd") {
      total = i64(total + BigInt(s.length));
    }
  }
  return offInt(total, 200n);
}

class Cell<T> {
  constructor(public v: T) {}
}

class Wrapper<T> {
  constructor(public value: T) {}
}

export { Counter, Pt, Holder, Reading, Gauge, addTen, addTenOnACopy, checkTwoNamesForOneObjectShareIt, checkACalleeChangesTheCallersObject, checkAValParameterIsTheCalleesOwnCopy, checkValInFrontOfAnExpressionCopies, checkCopyMethodAndValAgree, checkAMethodChangesThis, checkAnImmutableStructIsAValue, checkAFieldHoldingAnotherReferenceStruct, checkAFieldCanBeReplacedWithAnotherObject, checkAGenericStructWithAVarField, checkAnInterfaceValueSeesLaterChanges, checkAStructInADarrayIsTheSameObject, checkAStructInAClosureIsShared, makeCounter, lend, keepIn, checkAReturnedObjectSurvives, checkAnObjectLentToACalleeIsUsableAfterwards, checkAnObjectAnotherCalleeKeepsSurvives, checkAValParameterCopyIsReleasedAndTheOriginalIsNot, earlyReturnWithObjects, checkAReturnInsideAnIfReleasesTheObjectAndKeepsTheValue, checkLetVarAndValOnReferenceStructs, checkValOnAContainerIsAnIndependentCopy, Dot, Span, Meter, Plate, checkShorthandStructsWork, checkAShorthandStructWithAVarFieldIsAReference, checkAHashmapOfObjectsIsUsableThenReleased, checkASparseOfStringsIsUsableThenReleased, Cell, Wrapper };
