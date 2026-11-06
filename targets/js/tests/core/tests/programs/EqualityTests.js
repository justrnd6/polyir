"use strict";

import { offInt } from "./Check.js";

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

// An `either` holds its value as an `Ok` or an `Err`.
class Ok {
  constructor(value) {
    this.value = value;
  }
}

class Err {
  constructor(value) {
    this.value = value;
  }
}

// A variant: the number of its case and the value it holds.
class Variant {
  constructor(tag, value) {
    this.tag = tag;
    this.value = value;
  }
}

class Point {
  constructor(x, y) {
    this.x = x;
    this.y = y;
  }
}

class Line {
  constructor(a, b) {
    this.a = a;
    this.b = b;
  }
}

class Named {
  constructor(name, tags) {
    this.name = name;
    this.tags = tags;
  }
}

class Counter {
  constructor(n, id) {
    this.n = n;
    this.id = id;
  }
}

class Digit {
  constructor(v) {
    this.v = v;
  }

  equals(other) {
    return this.v % 10n === other.v % 10n;
  }
}

class Wrapped {
  constructor(inner) {
    this.inner = inner;
  }
}

function checkStructsAreEqualFieldByField() {
  const p = new Point(1n, 2n);
  const q = new Point(1n, 2n);
  const r = new Point(1n, 3n);
  let score = 0n;
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

function checkNestedStructsAreEqualRecursively() {
  const p = new Point(1n, 2n);
  const z = new Point(3n, 4n);
  const a = new Line(p, z);
  const b = new Line(new Point(1n, 2n), new Point(3n, 4n));
  const c = new Line(z, p);
  let score = 0n;
  if (same(a, b)) {
    score = i64(score + 1n);
  }
  if (!same(a, c)) {
    score = i64(score + 2n);
  }
  return offInt(score, 3n);
}

function checkAStringAndAListInsideAStructAreComparedByContent() {
  const t1 = [];
  t1.push(1n);
  const t2 = [];
  t2.push(1n);
  const a = new Named("a" + "b", t1);
  const b = new Named("ab", t2);
  let score = 0n;
  if (same(a, b)) {
    score = i64(score + 1n);
  }
  t2.push(2n);
  if (!same(a, b)) {
    score = i64(score + 2n);
  }
  return offInt(score, 3n);
}

function checkStringsCompareByCharacters() {
  const x = "match";
  const y = "ma" + "tch";
  let score = 0n;
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

function checkAStructWithVarFieldsIsComparedByWhatItHoldsNow() {
  const a = new Counter(1n, 2n);
  const b = new Counter(1n, 2n);
  let score = 0n;
  if (same(a, b)) {
    score = i64(score + 1n);
  }
  b.n = 5n;
  if (!same(a, b)) {
    score = i64(score + 2n);
  }
  return offInt(score, 3n);
}

function checkIdentityIsTheSameObject() {
  const a = new Counter(1n, 2n);
  const b = new Counter(1n, 2n);
  const alias = a;
  let score = 0n;
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

function checkAnImmutableStructComparedByIdentityIsAnObject() {
  const p = new Point(1n, 2n);
  const q = new Point(1n, 2n);
  const same_ = p;
  let score = 0n;
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

function checkAValStructIsADifferentObjectThatIsEqual() {
  const a = new Counter(1n, 2n);
  const b = copyOf(a);
  let score = 0n;
  if (same(a, b)) {
    score = i64(score + 1n);
  }
  if (a !== b) {
    score = i64(score + 2n);
  }
  return offInt(score, 3n);
}

function checkAnEqualsMethodDecidesWhatEqualMeans() {
  const a = new Digit(13n);
  const b = new Digit(23n);
  const c = new Digit(24n);
  let score = 0n;
  if (same(a, b)) {
    score = i64(score + 1n);
  }
  if (!same(a, c)) {
    score = i64(score + 2n);
  }
  return offInt(score, 3n);
}

function checkListsAreEqualElementByElement() {
  const a = [];
  const b = [];
  a.push(1n);
  a.push(2n);
  b.push(1n);
  b.push(2n);
  let score = 0n;
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

function checkListsOfStructsAreEqualDeeply() {
  const a = [];
  const b = [];
  a.push(new Point(1n, 2n));
  b.push(new Point(1n, 2n));
  let score = 0n;
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

function checkEithersAreEqualWhenTheSameCaseWithAnEqualPayload() {
  const a = new Ok(5n);
  const b = new Ok(5n);
  const c = new Err(5n);
  const d = new Ok(6n);
  let score = 0n;
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

function checkVariantsAreEqualWhenTheSameCaseWithAnEqualPayload() {
  const a = new Variant(1n, 2.5);
  const b = new Variant(1n, 2.5);
  const c = new Variant(0n, 2n);
  let score = 0n;
  if (same(a, b)) {
    score = i64(score + 1n);
  }
  if (!same(a, c)) {
    score = i64(score + 2n);
  }
  return offInt(score, 3n);
}

function checkAStructHoldingAnEitherIsComparedThroughIt() {
  const a = new Wrapped(new Ok(1n));
  const b = new Wrapped(new Ok(1n));
  const c = new Wrapped(new Err("no"));
  let score = 0n;
  if (same(a, b)) {
    score = i64(score + 1n);
  }
  if (!same(a, c)) {
    score = i64(score + 2n);
  }
  return offInt(score, 3n);
}

function checkNanIsNotEqualToItself() {
  const nan = 0.0 / 0.0;
  let score = 0n;
  if (nan === nan) {
    score = i64(score + 100n);
  }
  if (nan !== nan) {
    score = i64(score + 1n);
  }
  return offInt(score, 1n);
}

export { Point, Line, Named, Counter, Digit, Wrapped, checkStructsAreEqualFieldByField, checkNestedStructsAreEqualRecursively, checkAStringAndAListInsideAStructAreComparedByContent, checkStringsCompareByCharacters, checkAStructWithVarFieldsIsComparedByWhatItHoldsNow, checkIdentityIsTheSameObject, checkAnImmutableStructComparedByIdentityIsAnObject, checkAValStructIsADifferentObjectThatIsEqual, checkAnEqualsMethodDecidesWhatEqualMeans, checkListsAreEqualElementByElement, checkListsOfStructsAreEqualDeeply, checkEithersAreEqualWhenTheSameCaseWithAnEqualPayload, checkVariantsAreEqualWhenTheSameCaseWithAnEqualPayload, checkAStructHoldingAnEitherIsComparedThroughIt, checkNanIsNotEqualToItself };
