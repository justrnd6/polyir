declare const process: any;

import { offInt } from "./Check.js";

// A 64-bit integer is a bigint, brought back into 64 bits after an operation that can leave them.
function i64(value: bigint): bigint {
  return BigInt.asIntN(64, value);
}

// A push used as a value gives the list, as in polyir (JavaScript's push gives the length).
function pushed<T>(items: T[], item: T): T[] {
  items.push(item);
  return items;
}

// An `either`: an ok value or an error, and which one it holds.
class Either<T, E> {
  private constructor(readonly isOk: boolean, private readonly held: T | E) {}

  static ok<T, E>(value: T): Either<T, E> {
    return new Either<T, E>(true, value);
  }

  static err<T, E>(value: E): Either<T, E> {
    return new Either<T, E>(false, value);
  }

  get okValue(): T {
    return this.held as T;
  }

  get errValue(): E {
    return this.held as E;
  }
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

// A grid, matrix or tensor: its sizes, and its cells in one row-major list. `get(i, j)` reads a cell, `set(i, j, value)`
// writes one.
class Grid<T> {
  readonly cells: T[];

  constructor(readonly dims: number[], zero: T) {
    this.cells = Array.from({ length: dims.reduce((count, size) => count * size, 1) }, () => zero);
  }

  place(indices: bigint[]): number {
    let at = 0;
    for (let i = 0; i < this.dims.length; i++) {
      at = at * this.dims[i] + Number(indices[i]);
    }
    return at;
  }

  get(...indices: bigint[]): T {
    return this.cells[this.place(indices)];
  }

  set(...indicesAndValue: any[]): void {
    const value = indicesAndValue.pop();
    this.cells[this.place(indicesAndValue)] = value;
  }

  copy(): Grid<T> {
    const out = new Grid<T>(this.dims, this.cells[0]);
    out.cells.splice(0, out.cells.length, ...this.cells);
    return out;
  }
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

// Stops the program with a message, as polyir's `fail` does.
function fail(message: string): never {
  console.error("trap: " + message);
  if (typeof process !== "undefined") {
    process.exit(1);
  }
  throw new Error(message);
}

// The value an optional holds; an empty one stops the program.
function present<T>(value: T | null): T {
  if (value === null) {
    fail("unwrap of an empty optional");
  }
  return value;
}

class Bag {
  constructor(public items: Array<bigint>, public total: bigint) {}
}

class Pt {
  constructor(public x: bigint, public y: bigint) {}
}

function pushThree(xs: Array<bigint>): bigint {
  xs.push(3n);
  return BigInt(xs.length);
}

function pushMany(xs: Array<bigint>, count: bigint): bigint {
  for (let i = 0n; i < count; i = i64(i + 1n)) {
    xs.push(i64(i * 2n));
  }
  return BigInt(xs.length);
}

function checkAPushInsideAFunctionIsSeenByTheCaller(): number {
  const xs: Array<bigint> = [];
  xs.push(1n);
  pushThree(xs);
  return offInt(BigInt(xs.length), 2n) + offInt(xs[1], 3n);
}

function checkManyPushesInsideAFunctionLeaveTheCallerValid(): number {
  const xs: Array<bigint> = [];
  xs.push(7n);
  pushMany(xs, 100n);
  return offInt(BigInt(xs.length), 101n) + offInt(xs[0], 7n) + offInt(xs[1], 0n) + offInt(xs[100], 198n);
}

function checkTwoNamesForOneDarrayShareIt(): number {
  const a: Array<bigint> = [];
  const b: Array<bigint> = a;
  a.push(5n);
  b.push(6n);
  return offInt(BigInt(a.length), 2n) + offInt(BigInt(b.length), 2n) + offInt(a[1], 6n) + offInt(b[0], 5n);
}

function checkAnElementWrittenInAFunctionIsSeenByTheCaller(): number {
  const xs: Array<bigint> = [];
  xs.push(1n);
  xs.push(2n);
  overwriteFirst(xs, 40n);
  return offInt(xs[0], 40n) + offInt(xs[1], 2n);
}

function overwriteFirst(xs: Array<bigint>, value: bigint): bigint {
  xs[0] = value;
  return 0n;
}

function makeFilled(count: bigint): Array<bigint> {
  const xs: Array<bigint> = [];
  for (let i = 0n; i < count; i = i64(i + 1n)) {
    xs.push(i64(i + 1n));
  }
  return xs;
}

function checkAReturnedDarrayIsTheOneTheFunctionBuilt(): number {
  const xs: Array<bigint> = makeFilled(4n);
  xs.push(50n);
  return offInt(BigInt(xs.length), 5n) + offInt(xs[3], 4n) + offInt(xs[4], 50n);
}

function checkAStructFieldIsTheSameDarray(): number {
  const xs: Array<bigint> = [];
  const bag: Bag = new Bag(xs, 0n);
  xs.push(8n);
  bag.items.push(9n);
  return offInt(BigInt(bag.items.length), 2n) + offInt(BigInt(xs.length), 2n) + offInt(bag.items[1], 9n);
}

function checkAMethodStylePushOnAFieldWorks(): number {
  const bag: Bag = new Bag([], 0n);
  bag.items.push(4n);
  bag.items.push(5n);
  return offInt(BigInt(bag.items.length), 2n) + offInt(bag.items[1], 5n);
}

function checkConcatenationMakesANewDarray(): number {
  const a: Array<bigint> = [];
  const b: Array<bigint> = [];
  a.push(1n);
  b.push(2n);
  const c: Array<bigint> = [...a, ...b];
  a.push(99n);
  return offInt(BigInt(c.length), 2n) + offInt(c[0], 1n) + offInt(c[1], 2n) + offInt(BigInt(a.length), 2n);
}

function checkAClosureSharesTheDarrayItCaptures(): number {
  const xs: Array<bigint> = [];
  const add: (...args: [bigint]) => bigint = (v: bigint): bigint => {
    xs.push(v);
    return BigInt(xs.length);
  };
  add(10n);
  add(20n);
  return offInt(BigInt(xs.length), 2n) + offInt(xs[1], 20n);
}

function checkPushGivesBackTheSameDarray(): number {
  let xs: Array<bigint> = [];
  xs = pushed(xs, 5n);
  xs = pushed(xs, 6n);
  return offInt(BigInt(xs.length), 2n) + offInt(xs[1], 6n);
}

function checkReassigningAVarSwitchesDarrays(): number {
  let xs: Array<bigint> = [];
  const other: Array<bigint> = [];
  other.push(1n);
  other.push(2n);
  other.push(3n);
  xs = other;
  xs.push(4n);
  return offInt(BigInt(other.length), 4n) + offInt(BigInt(xs.length), 4n);
}

function checkAListOfListsSharesItsInnerLists(): number {
  const outer: Array<Array<bigint>> = [];
  const a: Array<bigint> = [];
  a.push(1n);
  a.push(2n);
  outer.push(a);
  const b: Array<bigint> = [];
  b.push(10n);
  outer.push(b);
  a.push(3n);
  const first: Array<bigint> = outer[0];
  const second: Array<bigint> = outer[1];
  return offInt(BigInt(outer.length), 2n) + offInt(BigInt(first.length), 3n) + offInt(first[2], 3n) + offInt(second[0], 10n);
}

function checkADarrayOfStructsKeepsTheStructs(): number {
  const ps: Array<Pt> = [];
  ps.push(new Pt(1n, 2n));
  ps.push(new Pt(3n, 4n));
  const p: Pt = ps[1];
  return offInt(BigInt(ps.length), 2n) + offInt(p.x, 3n) + offInt(p.y, 4n);
}

function checkAThousandPushesKeepEveryElement(): number {
  const xs: Array<bigint> = [];
  for (let i = 0n; i < 1000n; i = i64(i + 1n)) {
    xs.push(i);
  }
  let total: bigint = 0n;
  for (let i = 0n; i < BigInt(xs.length); i = i64(i + 1n)) {
    total = i64(total + xs[Number(i)]);
  }
  return offInt(BigInt(xs.length), 1000n) + offInt(total, 499500n);
}

function checkAListInsideAnEitherIsTheSameList(): number {
  const xs: Array<bigint> = [];
  xs.push(4n);
  const r: Either<Array<bigint>, bigint> = Either.ok<Array<bigint>, bigint>(xs);
  xs.push(5n);
  const back: Array<bigint> = r.okValue;
  return offInt(BigInt(back.length), 2n) + offInt(back[1], 5n);
}

function fillSparse(sp: Sparse<bigint>): bigint {
  sp.set(1n, 1n, 5n);
  sp.set(2n, 3n, 7n);
  return 0n;
}

function checkASparseWrittenInAFunctionIsSeenByTheCaller(): number {
  const sp: Sparse<bigint> = new Sparse(0n);
  fillSparse(sp);
  return offInt(sp.get(1n, 1n), 5n) + offInt(sp.get(2n, 3n), 7n) + offInt(sp.get(9n, 9n), 0n) + offInt(BigInt(sp.length), 2n);
}

function checkLastWriteWinsForASparse(): number {
  const sp: Sparse<bigint> = new Sparse(0n);
  sp.set(3n, 3n, 1n);
  sp.set(3n, 3n, 2n);
  sp.set(3n, 3n, 3n);
  return offInt(sp.get(3n, 3n), 3n);
}

function checkOneDimensionalSparseSharesToo(): number {
  const sp: Sparse<bigint> = new Sparse(0n);
  const alias: Sparse<bigint> = sp;
  alias.set(0n, 0n, 100n);
  sp.set(7n, 0n, 200n);
  return offInt(sp.get(0n, 0n), 100n) + offInt(alias.get(7n, 0n), 200n);
}

function fillMap(hm: Map<bigint, bigint>): bigint {
  hm.set(1n, 10n);
  hm.set(2n, 20n);
  hm.set(1n, 11n);
  return 0n;
}

function checkAHashmapWrittenInAFunctionIsSeenByTheCaller(): number {
  const hm: Map<bigint, bigint> = new Map();
  fillMap(hm);
  return offInt(hm.get(1n) ?? 0n, 11n) + offInt(hm.get(2n) ?? 0n, 20n) + offInt(hm.get(3n) ?? 0n, 0n) + offInt(BigInt(hm.size), 2n);
}

function checkTwoNamesForOneHashmapShareIt(): number {
  const a: Map<bigint, bigint> = new Map();
  const b: Map<bigint, bigint> = a;
  a.set(5n, 50n);
  b.set(6n, 60n);
  return offInt(BigInt(a.size), 2n) + offInt(b.get(5n) ?? 0n, 50n) + offInt(a.get(6n) ?? 0n, 60n);
}

function checkAStringKeyedHashmapIsShared(): number {
  const hm: Map<string, bigint> = new Map();
  addWord(hm, "one", 1n);
  addWord(hm, "two", 2n);
  return offInt(hm.get("one") ?? 0n, 1n) + offInt(hm.get("two") ?? 0n, 2n) + offInt(BigInt(hm.size), 2n);
}

function addWord(hm: Map<string, bigint>, word: string, value: bigint): bigint {
  hm.set(word, value);
  return 0n;
}

function checkAHashmapOfListsKeepsTheLists(): number {
  const hm: Map<bigint, Array<bigint>> = new Map();
  const l: Array<bigint> = [];
  hm.set(7n, l);
  l.push(5n);
  l.push(6n);
  const got: Array<bigint> = hm.get(7n) ?? [];
  return offInt(BigInt(got.length), 2n) + offInt(got[1], 6n);
}

function fillGrid(g: Grid<bigint>): bigint {
  for (let r = 0n; r < 2n; r = i64(r + 1n)) {
    for (let c = 0n; c < 3n; c = i64(c + 1n)) {
      g.set(r, c, i64(i64(r * 10n) + c));
    }
  }
  return 0n;
}

function checkAGridFilledInAFunctionIsSeenByTheCaller(): number {
  const g: Grid<bigint> = new Grid([2, 3], 0n);
  fillGrid(g);
  return offInt(g.get(1n, 2n), 12n) + offInt(g.get(0n, 1n), 1n);
}

function checkTwoNamesForOneGridShareIt(): number {
  const a: Grid<bigint> = new Grid([2, 3], 0n);
  const b: Grid<bigint> = a;
  a.set(1n, 1n, 9n);
  return offInt(b.get(1n, 1n), 9n);
}

function checkAGridStartsZeroed(): number {
  const t: Grid<bigint> = new Grid([2, 2, 2], 0n);
  t.set(1n, 1n, 1n, 5n);
  return offInt(t.get(0n, 1n, 0n), 0n) + offInt(t.get(1n, 1n, 1n), 5n);
}

function checkDarrayCopyIsIndependent(): number {
  const a: Array<bigint> = [];
  a.push(1n);
  a.push(2n);
  const b: Array<bigint> = [...a];
  b.push(3n);
  a.push(9n);
  return offInt(BigInt(a.length), 3n) + offInt(BigInt(b.length), 3n) + offInt(b[2], 3n) + offInt(a[2], 9n) + offInt(b[0], 1n);
}

function checkGridAndTensorCopyAreIndependent(): number {
  const g: Grid<bigint> = new Grid([2, 2], 0n);
  g.set(1n, 1n, 5n);
  const h: Grid<bigint> = g.copy();
  g.set(1n, 1n, 6n);
  const t: Grid<bigint> = new Grid([2, 2, 2], 0n);
  t.set(1n, 0n, 1n, 7n);
  const u: Grid<bigint> = t.copy();
  t.set(1n, 0n, 1n, 8n);
  return offInt(h.get(1n, 1n), 5n) + offInt(g.get(1n, 1n), 6n) + offInt(u.get(1n, 0n, 1n), 7n);
}

function checkSparseCopyIsIndependent(): number {
  const s: Sparse<bigint> = new Sparse(0n);
  s.set(1n, 2n, 7n);
  const c: Sparse<bigint> = s.copy();
  s.set(3n, 4n, 8n);
  c.set(5n, 6n, 9n);
  return offInt(BigInt(s.length), 2n) + offInt(BigInt(c.length), 2n) + offInt(c.get(1n, 2n), 7n);
}

function checkHashmapCopyIsIndependent(): number {
  const m: Map<bigint, bigint> = new Map();
  m.set(1n, 10n);
  const c: Map<bigint, bigint> = copyOf(m);
  m.set(2n, 20n);
  c.set(3n, 30n);
  return offInt(BigInt(m.size), 2n) + offInt(BigInt(c.size), 2n) + offInt(c.get(1n) ?? 0n, 10n) + offInt(c.get(3n) ?? 0n, 30n);
}

function checkCopyOfAStringKeyedHashmap(): number {
  const m: Map<string, bigint> = new Map();
  m.set("a", 1n);
  const c: Map<string, bigint> = copyOf(m);
  m.set("b", 2n);
  return offInt(c.get("a") ?? 0n, 1n) + offInt(BigInt(c.size), 1n);
}

function makeBag(n: bigint): Array<bigint> {
  const d: Array<bigint> = [];
  d.push(n);
  d.push(i64(n + 1n));
  return d;
}

function sumGrid(g: Grid<bigint>): bigint {
  return i64(g.get(0n, 0n) + g.get(1n, 1n));
}

function holdsOnto(store: Array<Array<bigint>>, d: Array<bigint>): bigint {
  store.push(d);
  return 0n;
}

function checkAReturnedContainerSurvives(): number {
  const a: Array<bigint> = makeBag(10n);
  const b: Array<bigint> = makeBag(20n);
  return offInt(i64(i64(i64(a[0] + a[1]) + b[0]) + b[1]), 62n);
}

function checkAContainerLentToACalleeIsStillUsableAfterwards(): number {
  let total: bigint = 0n;
  for (let i = 0n; i < 50n; i = i64(i + 1n)) {
    const g: Grid<bigint> = new Grid([2, 2], 0n);
    g.set(0n, 0n, i);
    g.set(1n, 1n, 1n);
    total = i64(i64(total + sumGrid(g)) + g.get(0n, 0n));
  }
  return offInt(total, i64(i64(i64(2n * 1225n) + 50n) + 0n));
}

function checkAContainerGivenToACalleeThatKeepsItSurvives(): number {
  const store: Array<Array<bigint>> = [];
  for (let i = 0n; i < 3n; i = i64(i + 1n)) {
    const d: Array<bigint> = [];
    d.push(i64(i * 10n));
    holdsOnto(store, d);
  }
  return offInt(i64(i64(store[0][0] + store[1][0]) + store[2][0]), 30n);
}

function checkAContainerReturnedFromInsideAnIfSurvives(): number {
  const keep: Array<bigint> = [];
  keep.push(4n);
  const other: Array<bigint> = [];
  other.push(5n);
  let total: bigint = 0n;
  for (let i = 0n; i < 20n; i = i64(i + 1n)) {
    const scratch: Array<bigint> = [];
    scratch.push(i);
    if (i === 7n) {
      total = i64(total + scratch[0]);
    }
  }
  return offInt(i64(i64(total + keep[0]) + other[0]), 16n);
}

function earlyExit(n: bigint): bigint {
  const scratch: Array<bigint> = [];
  scratch.push(n);
  if (n > 5n) {
    return i64(scratch[0] * 2n);
  }
  scratch.push(1n);
  return i64(BigInt(scratch.length) + scratch[0]);
}

function checkAReturnInsideAnIfStillReleasesAndReturnsTheRightValue(): number {
  return offInt(i64(i64(earlyExit(9n) * 100n) + earlyExit(2n)), i64(i64(18n * 100n) + 4n));
}

function greeting(name: string): string {
  const hello: string = "hello, ";
  const full: string = hello + name + "!";
  return full;
}

function shout(s: string): bigint {
  if (s === "hi!") {
    return 1n;
  }
  return 0n;
}

function counterFrom(start: bigint): () => bigint {
  let n: bigint = start;
  const next: () => bigint = (): bigint => {
    n = i64(n + 1n);
    return n;
  };
  return next;
}

function applyTwice(f: () => bigint): bigint {
  f();
  return f();
}

function checkAReturnedStringSurvives(): number {
  const a: string = greeting("ann");
  const b: string = greeting("bo");
  if (a !== "hello, ann!") {
    return 1.0;
  }
  if (b !== "hello, bo!") {
    return 1.0;
  }
  return 0.0;
}

function checkAStringLentToACalleeIsUsableAfterwards(): number {
  let hits: bigint = 0n;
  for (let i = 0n; i < 20n; i = i64(i + 1n)) {
    const s: string = "h" + "i" + "!";
    hits = i64(hits + shout(s));
    if (s === "hi!") {
      hits = i64(hits + 1n);
    }
  }
  return offInt(hits, 40n);
}

function checkAStringUsedAsAHashmapKeyIsKept(): number {
  const m: Map<string, bigint> = new Map();
  for (let i = 0n; i < 3n; i = i64(i + 1n)) {
    const key: string = "k" + "ey";
    m.set(key, i);
  }
  return offInt(m.get("key") ?? 0n, 2n) + offInt(BigInt(m.size), 1n);
}

function checkAClosureThatIsReturnedKeepsItsBox(): number {
  const c: () => bigint = counterFrom(10n);
  c();
  return offInt(c(), 12n);
}

function checkAClosureGivenToACalleeKeepsWorking(): number {
  let n: bigint = 0n;
  const tick: () => bigint = (): bigint => {
    n = i64(n + 5n);
    return n;
  };
  return offInt(applyTwice(tick), 10n) + offInt(n, 10n);
}

function checkAClosureOnlyCalledLocallyInALoop(): number {
  let total: bigint = 0n;
  for (let i = 0n; i < 50n; i = i64(i + 1n)) {
    let count: bigint = i;
    const bump: () => bigint = (): bigint => {
      count = i64(count + 1n);
      return count;
    };
    bump();
    total = i64(i64(total + bump()) - i);
  }
  return offInt(total, 100n);
}

function checkTwoClosuresShareOneCapturedName(): number {
  let n: bigint = 0n;
  const inc: () => bigint = (): bigint => {
    n = i64(n + 1n);
    return n;
  };
  const read: () => bigint = () => i64(n * 10n);
  inc();
  inc();
  return offInt(read(), 20n);
}

class Pocket {
  constructor(public items: Array<bigint>) {}
}

function madeHere(): Array<bigint> {
  const d: Array<bigint> = [];
  d.push(1n);
  return d;
}

function madeThroughAnother(): Array<bigint> {
  const d: Array<bigint> = madeHere();
  d.push(2n);
  return d;
}

function madeInPlace(): Array<bigint> {
  return [];
}

function sameAsArgument(d: Array<bigint>): Array<bigint> {
  return d;
}

function elementOf(xs: Array<Array<bigint>>): Array<bigint> {
  return xs[0];
}

function fieldOf(p: Pocket): Array<bigint> {
  return p.items;
}

function keptAndReturned(store: Array<Array<bigint>>): Array<bigint> {
  const d: Array<bigint> = [];
  store.push(d);
  return d;
}

function sometimesTheArgument(flag: bigint, d: Array<bigint>): Array<bigint> {
  if (flag === 0n) {
    return d;
  }
  return [];
}

function checkAFreshResultIsUsableAndReleased(): number {
  let total: bigint = 0n;
  for (let i = 0n; i < 100n; i = i64(i + 1n)) {
    const a: Array<bigint> = madeHere();
    const b: Array<bigint> = madeThroughAnother();
    const c: Array<bigint> = madeInPlace();
    c.push(i);
    total = i64(i64(i64(total + BigInt(a.length)) + BigInt(b.length)) + BigInt(c.length));
  }
  return offInt(total, i64(100n * 4n));
}

function checkAnAliasReturnedByACallIsNotReleasedUnderItsOwner(): number {
  const src: Array<bigint> = [];
  src.push(7n);
  const same_: Array<bigint> = sameAsArgument(src);
  same_.push(8n);
  const outer: Array<Array<bigint>> = [];
  outer.push(src);
  const inner: Array<bigint> = elementOf(outer);
  const pocket: Pocket = new Pocket(src);
  const field: Array<bigint> = fieldOf(pocket);
  const maybe: Array<bigint> = sometimesTheArgument(0n, src);
  return offInt(BigInt(src.length), 2n) + offInt(BigInt(inner.length), 2n) + offInt(BigInt(field.length), 2n) + offInt(BigInt(maybe.length), 2n);
}

function checkAnObjectKeptByTheCalleeSurvivesItsCaller(): number {
  const store: Array<Array<bigint>> = [];
  let total: bigint = 0n;
  for (let i = 0n; i < 5n; i = i64(i + 1n)) {
    const d: Array<bigint> = keptAndReturned(store);
    d.push(i);
  }
  for (let j = 0n; j < 5n; j = i64(j + 1n)) {
    total = i64(total + store[Number(j)][0]);
  }
  return offInt(total, 10n);
}

function handedOn(): Array<bigint> {
  const a: Array<bigint> = madeHere();
  return a;
}

function checkAResultHandedOnAgainIsStillFresh(): number {
  const a: Array<bigint> = handedOn();
  return offInt(a[0], 1n);
}

function checkADiscardedFreshResultIsHarmless(): number {
  madeHere();
  madeThroughAnother();
  return 0.0;
}

function madeStruct(n: bigint): Pocket {
  const p: Pocket = new Pocket([]);
  p.items.push(n);
  return p;
}

function makeLabel(a: string): string {
  const s: string = a + "-" + a;
  return s;
}

function checkAStructAndAStringHandedOverByACall(): number {
  let total: bigint = 0n;
  for (let i = 0n; i < 50n; i = i64(i + 1n)) {
    const p: Pocket = madeStruct(i);
    const s: string = makeLabel("x");
    if (s === "x-x") {
      total = i64(i64(total + p.items[0]) + 1n);
    }
  }
  return offInt(total, i64(1225n + 50n));
}

class Shelf {
  constructor(public books: Array<bigint>, public tags: string) {}
}

class Cabinet {
  constructor(public drawer: Array<bigint>) {}
}

class Crate {
  constructor(public items: Array<bigint>) {}
}

function stockedShelf(n: bigint): Shelf {
  const s: Shelf = new Shelf([], "a" + "b");
  s.books.push(n);
  s.books.push(i64(n + 1n));
  return s;
}

function openDrawer(c: Cabinet): Array<bigint> {
  return c.drawer;
}

function checkAnOwnedFieldIsUsableThenReleasedWithItsStruct(): number {
  let total: bigint = 0n;
  for (let i = 0n; i < 100n; i = i64(i + 1n)) {
    const s: Shelf = stockedShelf(i);
    total = i64(i64(i64(total + BigInt(s.books.length)) + s.books[0]) - i);
    if (s.tags === "ab") {
      total = i64(total + 1n);
    }
  }
  return offInt(total, i64(100n * 3n));
}

function checkAReplacedFieldValueIsReleasedAndTheNewOneWorks(): number {
  const s: Shelf = stockedShelf(5n);
  s.books = [];
  s.books.push(40n);
  s.tags = "x" + "y";
  if (s.tags !== "xy") {
    return 1.0;
  }
  return offInt(BigInt(s.books.length), 1n) + offInt(s.books[0], 40n);
}

function checkAFieldThatIsReadOutIsNeverReleasedWithItsStruct(): number {
  const c: Cabinet = new Cabinet([]);
  c.drawer.push(9n);
  const drawer: Array<bigint> = openDrawer(c);
  drawer.push(10n);
  return offInt(BigInt(c.drawer.length), 2n) + offInt(drawer[1], 10n);
}

function checkACopiedStructSharesNothingItMustNotFree(): number {
  const a: Crate = new Crate([]);
  a.items.push(3n);
  const b: Crate = copyOf(a);
  b.items.push(4n);
  return offInt(BigInt(a.items.length), 2n) + offInt(BigInt(b.items.length), 2n);
}

function checkAListOfListsReleasesItsElements(): number {
  let total: bigint = 0n;
  for (let i = 0n; i < 100n; i = i64(i + 1n)) {
    const outer: Array<Array<bigint>> = [];
    for (let j = 0n; j < 3n; j = i64(j + 1n)) {
      outer.push([]);
      outer[Number(j)].push(i64(i + j));
    }
    total = i64(i64(i64(i64(total + BigInt(outer.length)) + BigInt(outer[2].length)) + outer[1][0]) - i);
  }
  return offInt(total, i64(100n * i64(i64(3n + 1n) + 1n)));
}

function firstOf(xs: Array<Array<bigint>>): Array<bigint> {
  const first: Array<bigint> = xs[0];
  return first;
}

function checkAnElementReadOutOfAListIsNotReleasedWithIt(): number {
  const store: Array<Array<bigint>> = [];
  store.push([]);
  store[0].push(6n);
  const got: Array<bigint> = firstOf(store);
  got.push(7n);
  return offInt(BigInt(store[0].length), 2n) + offInt(got[1], 7n);
}

function checkAMapPastItsStartingCapacityKeepsEveryEntry(): number {
  const m: Map<bigint, bigint> = new Map();
  for (let i = 0n; i < 1000n; i = i64(i + 1n)) {
    m.set(i, i64(i * 3n));
  }
  let wrong: bigint = 0n;
  for (let i = 0n; i < 1000n; i = i64(i + 1n)) {
    if ((m.get(i) ?? 0n) !== i64(i * 3n)) {
      wrong = i64(wrong + 1n);
    }
  }
  return offInt(wrong, 0n) + offInt(BigInt(m.size), 1000n);
}

function checkAGrownMapIsStillTheSameMapForItsAliases(): number {
  const m: Map<bigint, bigint> = new Map();
  const alias: Map<bigint, bigint> = m;
  for (let i = 0n; i < 20n; i = i64(i + 1n)) {
    alias.set(i, i);
  }
  return offInt(BigInt(m.size), 20n) + offInt(m.get(19n) ?? 0n, 19n);
}

function checkStringKeysSurviveGrowing(): number {
  const m: Map<string, bigint> = new Map();
  m.set("a", 1n);
  m.set("b", 2n);
  m.set("c", 3n);
  m.set("d", 4n);
  m.set("a", 10n);
  return offInt(m.get("a") ?? 0n, 10n) + offInt(m.get("b") ?? 0n, 2n) + offInt(m.get("c") ?? 0n, 3n) + offInt(m.get("d") ?? 0n, 4n) + offInt(BigInt(m.size), 4n);
}

class GrowKey {
  constructor(public a: bigint, public b: bigint) {}
}

function checkStructKeysSurviveGrowing(): number {
  const m: Map<GrowKey, bigint> = new ValueMap();
  for (let i = 0n; i < 50n; i = i64(i + 1n)) {
    m.set(new GrowKey(i, i64(i + 1n)), i);
  }
  let wrong: bigint = 0n;
  for (let i = 0n; i < 50n; i = i64(i + 1n)) {
    if ((m.get(new GrowKey(i, i64(i + 1n))) ?? 0n) !== i) {
      wrong = i64(wrong + 1n);
    }
  }
  return offInt(wrong, 0n) + offInt(BigInt(m.size), 50n);
}

function checkASetPastItsStartingCapacityKeepsEveryMember(): number {
  const seen: Set<bigint> = new Set();
  for (let i = 0n; i < 100n; i = i64(i + 1n)) {
    seen.add(i % 40n);
  }
  let absent: bigint = 0n;
  if (seen.has(39n) === false) {
    absent = i64(absent + 1n);
  }
  if (seen.has(40n)) {
    absent = i64(absent + 1n);
  }
  return offInt(BigInt(seen.size), 40n) + offInt(absent, 0n);
}

function readAt(xs: Array<bigint>, i: bigint): bigint {
  return xs[Number(i)];
}

function caughtCode(xs: Array<bigint>, i: bigint): bigint {
  let code: bigint = 0n;
  try {
    code = readAt(xs, i);
  } catch (c: any) {
    code = c;
  }
  return code;
}

function checkAnIndexPastTheEndOfAListThrowsMinusOne(): number {
  const xs: Array<bigint> = [];
  xs.push(5n);
  return offInt(caughtCode(xs, 1n), i64(0n - 1n)) + offInt(caughtCode(xs, 100000n), i64(0n - 1n));
}

function checkANegativeIndexThrowsMinusOne(): number {
  const xs: Array<bigint> = [];
  xs.push(5n);
  return offInt(caughtCode(xs, i64(0n - 1n)), i64(0n - 1n));
}

function checkAnIndexInsideTheListStillReads(): number {
  const xs: Array<bigint> = [];
  xs.push(5n);
  xs.push(6n);
  return offInt(caughtCode(xs, 0n), 5n) + offInt(caughtCode(xs, 1n), 6n);
}

function checkAnEmptyListCannotBeRead(): number {
  const xs: Array<bigint> = [];
  return offInt(caughtCode(xs, 0n), i64(0n - 1n));
}

function divideBy(a: bigint, b: bigint): bigint {
  let out: bigint = 0n;
  try {
    out = a / b;
  } catch (c: any) {
    out = c;
  }
  return out;
}

function checkDividingByZeroThrowsMinusFour(): number {
  return offInt(divideBy(10n, 0n), i64(0n - 4n)) + offInt(divideBy(10n, 2n), 5n);
}

function checkAFixedArrayAndAGridAreCheckedPerDimension(): number {
  let code: bigint = 0n;
  const g: Grid<bigint> = new Grid([2, 2], 0n);
  try {
    g.set(0n, 5n, 1n);
  } catch (c: any) {
    code = c;
  }
  return offInt(code, i64(0n - 1n));
}

class OptPoint {
  constructor(public x: bigint, public y: bigint) {}
}

function optionalFind(xs: Array<bigint>, want: bigint): bigint | null {
  for (let i = 0n; i < BigInt(xs.length); i = i64(i + 1n)) {
    if (xs[Number(i)] === want) {
      return i;
    }
  }
  return null;
}

function optionalPoint(ok: bigint): OptPoint | null {
  if (ok === 1n) {
    return new OptPoint(7n, 8n);
  }
  return null;
}

function optionalName(ok: bigint): string | null {
  if (ok === 1n) {
    return "ann";
  }
  return null;
}

function unwrapCode(o: bigint | null): bigint {
  let code: bigint = 0n;
  try {
    code = present(o);
  } catch (c: any) {
    code = c;
  }
  return code;
}

function checkAnOptionalHoldsAValueOrNone(): number {
  const xs: Array<bigint> = [];
  xs.push(10n);
  xs.push(20n);
  let seen: bigint = 0n;
  if (optionalFind(xs, 20n) !== null) {
    seen = i64(seen + 1n);
  }
  if (optionalFind(xs, 99n) === null) {
    seen = i64(seen + 10n);
  }
  return offInt(seen, 11n) + offInt(present(optionalFind(xs, 20n)), 1n);
}

function checkAnOptionalOfAStructAndOfAStringWork(): number {
  const p: OptPoint | null = optionalPoint(1n);
  let seen: bigint = 0n;
  if (p !== null) {
    seen = i64(i64(seen + present(p).x) + present(p).y);
  }
  if (optionalPoint(0n) === null) {
    seen = i64(seen + 100n);
  }
  if (optionalName(1n) !== null) {
    seen = i64(seen + 1000n);
  }
  return offInt(seen, 1115n);
}

function checkUnwrappingNoneThrowsMinusThree(): number {
  return offInt(unwrapCode(null), i64(0n - 3n)) + offInt(unwrapCode(42n), 42n);
}

export { Bag, Pt, pushThree, pushMany, checkAPushInsideAFunctionIsSeenByTheCaller, checkManyPushesInsideAFunctionLeaveTheCallerValid, checkTwoNamesForOneDarrayShareIt, checkAnElementWrittenInAFunctionIsSeenByTheCaller, overwriteFirst, makeFilled, checkAReturnedDarrayIsTheOneTheFunctionBuilt, checkAStructFieldIsTheSameDarray, checkAMethodStylePushOnAFieldWorks, checkConcatenationMakesANewDarray, checkAClosureSharesTheDarrayItCaptures, checkPushGivesBackTheSameDarray, checkReassigningAVarSwitchesDarrays, checkAListOfListsSharesItsInnerLists, checkADarrayOfStructsKeepsTheStructs, checkAThousandPushesKeepEveryElement, checkAListInsideAnEitherIsTheSameList, fillSparse, checkASparseWrittenInAFunctionIsSeenByTheCaller, checkLastWriteWinsForASparse, checkOneDimensionalSparseSharesToo, fillMap, checkAHashmapWrittenInAFunctionIsSeenByTheCaller, checkTwoNamesForOneHashmapShareIt, checkAStringKeyedHashmapIsShared, addWord, checkAHashmapOfListsKeepsTheLists, fillGrid, checkAGridFilledInAFunctionIsSeenByTheCaller, checkTwoNamesForOneGridShareIt, checkAGridStartsZeroed, checkDarrayCopyIsIndependent, checkGridAndTensorCopyAreIndependent, checkSparseCopyIsIndependent, checkHashmapCopyIsIndependent, checkCopyOfAStringKeyedHashmap, makeBag, sumGrid, holdsOnto, checkAReturnedContainerSurvives, checkAContainerLentToACalleeIsStillUsableAfterwards, checkAContainerGivenToACalleeThatKeepsItSurvives, checkAContainerReturnedFromInsideAnIfSurvives, earlyExit, checkAReturnInsideAnIfStillReleasesAndReturnsTheRightValue, greeting, shout, counterFrom, applyTwice, checkAReturnedStringSurvives, checkAStringLentToACalleeIsUsableAfterwards, checkAStringUsedAsAHashmapKeyIsKept, checkAClosureThatIsReturnedKeepsItsBox, checkAClosureGivenToACalleeKeepsWorking, checkAClosureOnlyCalledLocallyInALoop, checkTwoClosuresShareOneCapturedName, Pocket, madeHere, madeThroughAnother, madeInPlace, sameAsArgument, elementOf, fieldOf, keptAndReturned, sometimesTheArgument, checkAFreshResultIsUsableAndReleased, checkAnAliasReturnedByACallIsNotReleasedUnderItsOwner, checkAnObjectKeptByTheCalleeSurvivesItsCaller, handedOn, checkAResultHandedOnAgainIsStillFresh, checkADiscardedFreshResultIsHarmless, madeStruct, makeLabel, checkAStructAndAStringHandedOverByACall, Shelf, Cabinet, Crate, stockedShelf, openDrawer, checkAnOwnedFieldIsUsableThenReleasedWithItsStruct, checkAReplacedFieldValueIsReleasedAndTheNewOneWorks, checkAFieldThatIsReadOutIsNeverReleasedWithItsStruct, checkACopiedStructSharesNothingItMustNotFree, checkAListOfListsReleasesItsElements, firstOf, checkAnElementReadOutOfAListIsNotReleasedWithIt, checkAMapPastItsStartingCapacityKeepsEveryEntry, checkAGrownMapIsStillTheSameMapForItsAliases, checkStringKeysSurviveGrowing, GrowKey, checkStructKeysSurviveGrowing, checkASetPastItsStartingCapacityKeepsEveryMember, readAt, caughtCode, checkAnIndexPastTheEndOfAListThrowsMinusOne, checkANegativeIndexThrowsMinusOne, checkAnIndexInsideTheListStillReads, checkAnEmptyListCannotBeRead, divideBy, checkDividingByZeroThrowsMinusFour, checkAFixedArrayAndAGridAreCheckedPerDimension, OptPoint, optionalFind, optionalPoint, optionalName, unwrapCode, checkAnOptionalHoldsAValueOrNone, checkAnOptionalOfAStructAndOfAStringWork, checkUnwrappingNoneThrowsMinusThree };
