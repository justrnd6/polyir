import { offInt } from "./Check.js";

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

// A variant: the number of its case and the value it holds.
class Variant {
  constructor(readonly tag: bigint, readonly value: any) {}
}

class Point {
  constructor(public x: bigint, public y: bigint) {}
}

class Line {
  constructor(public a: Point, public b: Point) {}
}

class Named {
  constructor(public name: string, public tags: Array<bigint>) {}
}

class Counter {
  constructor(public n: bigint, public id: bigint) {}
}

class Digit {
  constructor(public v: bigint) {}

  equals(other: Digit): boolean {
    return this.v % 10n === other.v % 10n;
  }
}

class Wrapped {
  constructor(public inner: Either<bigint, string>) {}
}

function checkStructsAreEqualFieldByField(): number {
  const p: Point = new Point(1n, 2n);
  const q: Point = new Point(1n, 2n);
  const r: Point = new Point(1n, 3n);
  let score: bigint = 0n;
  if (same(p, q)) {
    score = i64(score + 1n);
  }
  if (!same(p, r)) {
    score = i64(score + 2n);
  }
  if (same(p, r)) {
    score = i64(score + 100n);
  }
  return offInt(score, 3n);
}

function checkNestedStructsAreEqualRecursively(): number {
  const p: Point = new Point(1n, 2n);
  const z: Point = new Point(3n, 4n);
  const a: Line = new Line(p, z);
  const b: Line = new Line(new Point(1n, 2n), new Point(3n, 4n));
  const c: Line = new Line(z, p);
  let score: bigint = 0n;
  if (same(a, b)) {
    score = i64(score + 1n);
  }
  if (!same(a, c)) {
    score = i64(score + 2n);
  }
  return offInt(score, 3n);
}

function checkAStringAndAListInsideAStructAreComparedByContent(): number {
  const t1: Array<bigint> = [];
  t1.push(1n);
  const t2: Array<bigint> = [];
  t2.push(1n);
  const a: Named = new Named("a" + "b", t1);
  const b: Named = new Named("ab", t2);
  let score: bigint = 0n;
  if (same(a, b)) {
    score = i64(score + 1n);
  }
  t2.push(2n);
  if (!same(a, b)) {
    score = i64(score + 2n);
  }
  return offInt(score, 3n);
}

function checkStringsCompareByCharacters(): number {
  const x: string = "match";
  const y: string = "ma" + "tch";
  let score: bigint = 0n;
  if (x === y) {
    score = i64(score + 1n);
  }
  if (x !== "other") {
    score = i64(score + 2n);
  }
  if (x === y) {
    score = i64(score + 100n);
  }
  return offInt(score, 3n);
}

function checkAStructWithVarFieldsIsComparedByWhatItHoldsNow(): number {
  const a: Counter = new Counter(1n, 2n);
  const b: Counter = new Counter(1n, 2n);
  let score: bigint = 0n;
  if (same(a, b)) {
    score = i64(score + 1n);
  }
  b.n = 5n;
  if (!same(a, b)) {
    score = i64(score + 2n);
  }
  return offInt(score, 3n);
}

function checkIdentityIsTheSameObject(): number {
  const a: Counter = new Counter(1n, 2n);
  const b: Counter = new Counter(1n, 2n);
  const alias: Counter = a;
  let score: bigint = 0n;
  if (a === alias) {
    score = i64(score + 1n);
  }
  if (a !== b) {
    score = i64(score + 2n);
  }
  if (a === b) {
    score = i64(score + 100n);
  }
  return offInt(score, 3n);
}

function checkAnImmutableStructComparedByIdentityIsAnObject(): number {
  const p: Point = new Point(1n, 2n);
  const q: Point = new Point(1n, 2n);
  const same_: Point = p;
  let score: bigint = 0n;
  if (p === same_) {
    score = i64(score + 1n);
  }
  if (p !== q) {
    score = i64(score + 2n);
  }
  if (same(p, q)) {
    score = i64(score + 4n);
  }
  return offInt(score, 7n);
}

function checkAValStructIsADifferentObjectThatIsEqual(): number {
  const a: Counter = new Counter(1n, 2n);
  const b: Counter = copyOf(a);
  let score: bigint = 0n;
  if (same(a, b)) {
    score = i64(score + 1n);
  }
  if (a !== b) {
    score = i64(score + 2n);
  }
  return offInt(score, 3n);
}

function checkAnEqualsMethodDecidesWhatEqualMeans(): number {
  const a: Digit = new Digit(13n);
  const b: Digit = new Digit(23n);
  const c: Digit = new Digit(24n);
  let score: bigint = 0n;
  if (same(a, b)) {
    score = i64(score + 1n);
  }
  if (!same(a, c)) {
    score = i64(score + 2n);
  }
  return offInt(score, 3n);
}

function checkListsAreEqualElementByElement(): number {
  const a: Array<bigint> = [];
  const b: Array<bigint> = [];
  a.push(1n);
  a.push(2n);
  b.push(1n);
  b.push(2n);
  let score: bigint = 0n;
  if (same(a, b)) {
    score = i64(score + 1n);
  }
  b.push(3n);
  if (!same(a, b)) {
    score = i64(score + 2n);
  }
  if (a === b) {
    score = i64(score + 100n);
  }
  return offInt(score, 3n);
}

function checkListsOfStructsAreEqualDeeply(): number {
  const a: Array<Point> = [];
  const b: Array<Point> = [];
  a.push(new Point(1n, 2n));
  b.push(new Point(1n, 2n));
  let score: bigint = 0n;
  if (same(a, b)) {
    score = i64(score + 1n);
  }
  a.push(new Point(5n, 6n));
  b.push(new Point(5n, 7n));
  if (!same(a, b)) {
    score = i64(score + 2n);
  }
  return offInt(score, 3n);
}

function checkEithersAreEqualWhenTheSameCaseWithAnEqualPayload(): number {
  const a: Either<bigint, bigint> = Either.ok<bigint, bigint>(5n);
  const b: Either<bigint, bigint> = Either.ok<bigint, bigint>(5n);
  const c: Either<bigint, bigint> = Either.err<bigint, bigint>(5n);
  const d: Either<bigint, bigint> = Either.ok<bigint, bigint>(6n);
  let score: bigint = 0n;
  if (same(a, b)) {
    score = i64(score + 1n);
  }
  if (!same(a, c)) {
    score = i64(score + 2n);
  }
  if (!same(a, d)) {
    score = i64(score + 4n);
  }
  return offInt(score, 7n);
}

function checkVariantsAreEqualWhenTheSameCaseWithAnEqualPayload(): number {
  const a: Variant = new Variant(1n, 2.5);
  const b: Variant = new Variant(1n, 2.5);
  const c: Variant = new Variant(0n, 2n);
  let score: bigint = 0n;
  if (same(a, b)) {
    score = i64(score + 1n);
  }
  if (!same(a, c)) {
    score = i64(score + 2n);
  }
  return offInt(score, 3n);
}

function checkAStructHoldingAnEitherIsComparedThroughIt(): number {
  const a: Wrapped = new Wrapped(Either.ok<bigint, string>(1n));
  const b: Wrapped = new Wrapped(Either.ok<bigint, string>(1n));
  const c: Wrapped = new Wrapped(Either.err<bigint, string>("no"));
  let score: bigint = 0n;
  if (same(a, b)) {
    score = i64(score + 1n);
  }
  if (!same(a, c)) {
    score = i64(score + 2n);
  }
  return offInt(score, 3n);
}

function checkNanIsNotEqualToItself(): number {
  const nan: number = 0.0 / 0.0;
  let score: bigint = 0n;
  if (nan === nan) {
    score = i64(score + 100n);
  }
  if (nan !== nan) {
    score = i64(score + 1n);
  }
  return offInt(score, 1n);
}

export { Point, Line, Named, Counter, Digit, Wrapped, checkStructsAreEqualFieldByField, checkNestedStructsAreEqualRecursively, checkAStringAndAListInsideAStructAreComparedByContent, checkStringsCompareByCharacters, checkAStructWithVarFieldsIsComparedByWhatItHoldsNow, checkIdentityIsTheSameObject, checkAnImmutableStructComparedByIdentityIsAnObject, checkAValStructIsADifferentObjectThatIsEqual, checkAnEqualsMethodDecidesWhatEqualMeans, checkListsAreEqualElementByElement, checkListsOfStructsAreEqualDeeply, checkEithersAreEqualWhenTheSameCaseWithAnEqualPayload, checkVariantsAreEqualWhenTheSameCaseWithAnEqualPayload, checkAStructHoldingAnEitherIsComparedThroughIt, checkNanIsNotEqualToItself };
