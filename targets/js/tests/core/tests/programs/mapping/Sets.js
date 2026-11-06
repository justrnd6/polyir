"use strict";

// A 64-bit integer is a BigInt, brought back into 64 bits after an operation that can leave them.
function i64(value) {
  return BigInt.asIntN(64, value);
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

// A Map whose keys are records, lists or sets compared by what they hold: the first key set stands for all keys equal to it.
class ValueMap extends Map {
  constructor(entries) {
    super();
    this.byKey = new Map();
    for (const [key, value] of entries ?? []) {
      this.set(key, value);
    }
  }

  set(key, value) {
    const text = valueKey(key);
    if (!this.byKey.has(text)) {
      this.byKey.set(text, key);
    }
    return super.set(this.byKey.get(text), value);
  }

  get(key) {
    return super.get(this.byKey.get(valueKey(key)));
  }

  has(key) {
    return this.byKey.has(valueKey(key));
  }

  delete(key) {
    const text = valueKey(key);
    const held = this.byKey.get(text);
    return this.byKey.delete(text) && super.delete(held);
  }
}

// A string's UTF-8 bytes, and bytes back to a string, as polyir counts a string's positions in bytes. An unpaired
// surrogate is U+FFFD, and so is each byte sequence that is not UTF-8.
function utf8Bytes(text) {
  const bytes = [];
  for (let i = 0; i < text.length; i++) {
    let c = text.charCodeAt(i);
    const low = i + 1 < text.length ? text.charCodeAt(i + 1) : 0;
    if (c >= 0xD800 && c < 0xDC00 && low >= 0xDC00 && low < 0xE000) {
      c = 0x10000 + (c - 0xD800) * 1024 + (low - 0xDC00);
      i++;
    } else if (c >= 0xD800 && c < 0xE000) {
      c = 0xFFFD;
    }
    if (c < 0x80) {
      bytes.push(c);
    } else if (c < 0x800) {
      bytes.push(0xC0 | c >> 6, 0x80 | c & 63);
    } else if (c < 0x10000) {
      bytes.push(0xE0 | c >> 12, 0x80 | c >> 6 & 63, 0x80 | c & 63);
    } else {
      bytes.push(0xF0 | c >> 18, 0x80 | c >> 12 & 63, 0x80 | c >> 6 & 63, 0x80 | c & 63);
    }
  }
  return bytes;
}

function utf8Text(bytes) {
  let text = "";
  for (let i = 0; i < bytes.length;) {
    const lead = bytes[i];
    const size = lead < 0x80 ? 1 : lead >= 0xC2 && lead < 0xE0 ? 2 : lead >= 0xE0 && lead < 0xF0 ? 3 : lead >= 0xF0 && lead < 0xF5 ? 4 : 0;
    const low = lead === 0xE0 ? 0xA0 : lead === 0xF0 ? 0x90 : 0x80;
    const high = lead === 0xED ? 0x9F : lead === 0xF4 ? 0x8F : 0xBF;
    let c = size === 1 ? lead : lead & (0xFF >> (size + 1));
    let k = 1;
    while (k < size && i + k < bytes.length && bytes[i + k] >= (k === 1 ? low : 0x80) && bytes[i + k] <= (k === 1 ? high : 0xBF)) {
      c = c << 6 | bytes[i + k] & 63;
      k++;
    }
    text += size > 0 && k === size ? String.fromCodePoint(c) : "\uFFFD";
    i += size > 0 ? k : 1;
  }
  return text;
}

// How many UTF-8 bytes a string has.
function textLength(text) {
  return BigInt(utf8Bytes(text).length);
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

// Stops the program with a message, as polyir's `fail` does.
function fail(message) {
  console.error("trap: " + message);
  if (typeof process !== "undefined") {
    process.exit(1);
  }
  throw new Error(message);
}

// The value an optional holds; an empty one stops the program.
function present(value) {
  if (value === null) {
    fail("unwrap of an empty optional");
  }
  return value;
}

class Point {
  constructor(x, y) {
    this.x = x;
    this.y = y;
  }
}

class Named {
  constructor(name, tag) {
    this.name = name;
    this.tag = tag;
  }
}

class Cell2 {
  constructor(v, note) {
    this.v = v;
    this.note = note;
  }

  bump() {
    this.v = i64(this.v + 1n);
    return this.v;
  }
}

function double(x) {
  return i64(x * 2n);
}

function inc(x) {
  return i64(x + 1n);
}

function sets() {
  const seen = new ValueSet();
  seen.add(new Point(1n, 2n));
  seen.add(new Point(3n, 4n));
  seen.add(new Point(1n, 2n));
  let score = BigInt(seen.size);
  if (seen.has(new Point(3n, 4n))) {
    score = i64(score + 10n);
  }
  const words = new Set();
  words.add("a");
  words.add("b");
  words.add("a");
  score = i64(score + i64(BigInt(words.size) * 100n));
  return score;
}

function maps() {
  const m = new Map();
  m.set("x", 5n);
  m.set("y", 6n);
  m.set("x", i64((m.get("x") ?? 0n) + 1n));
  const byPoint = new ValueMap();
  byPoint.set(new Point(1n, 1n), "p");
  let score = i64(i64(i64((m.get("x") ?? 0n) * 10n) + (m.get("y") ?? 0n)) + BigInt(m.size));
  if (byPoint.has(new Point(1n, 1n))) {
    score = i64(score + 100n);
  }
  if ((m.get("none") ?? 0n) === 0n) {
    score = i64(score + 1000n);
  }
  return score;
}

function strs() {
  const a = "ab";
  const b = a + "cd" + a;
  let score = textLength(b);
  if (b === "abcdab") {
    score = i64(score + 10n);
  }
  if (b !== a) {
    score = i64(score + 100n);
  }
  const q = "say hi {x}";
  score = i64(score + textLength(q));
  return score;
}

function opts() {
  const n = new Named("a", "t");
  const m = new Named("a", "t");
  const o = new Named("a", null);
  let score = 0n;
  if (same(n, m)) {
    score = i64(score + 1n);
  }
  if (same(n, o)) {
    score = i64(score + 10n);
  }
  if (o.tag === null) {
    score = i64(score + 100n);
  }
  return i64(score + textLength(present(n.tag)));
}

function bufs() {
  const b = Array.from({ length: 4 }, () => 0n);
  b[0] = 7n;
  b[3] = 9n;
  let s = 0n;
  for (let i = 0n; i < 4n; i = i64(i + 1n)) {
    s = i64(s + b[Number(i)]);
  }
  return s;
}

function swaps() {
  const c = new Cell2(1n, "n");
  const d = c;
  d.bump();
  c.note = c.note + "!";
  if (d.note === "n!") {
    return i64(i64(c.v * 10n) + textLength(d.note));
  }
  return 0n;
}

function choose(k) {
  let label = 0n;
  machine: for (;;) {
    switch (label) {
      case 0n: {
        if (k === 0n) {
          label = 1n;
          continue machine;
        } else if (k === 1n) {
          label = 2n;
          continue machine;
        } else {
          label = 3n;
          continue machine;
        }
      }
      case 1n: {
        return 100n;
      }
      case 2n: {
        return 200n;
      }
      case 3n: {
        return 300n;
      }
    }
  }
}

function loops(n) {
  let s = 0n;
  for (let i = 0n, j = n; i < j; i = i64(i + 1n), j = i64(j - 1n)) {
    s = i64(s + i64(i * j));
  }
  let k = 0n;
  for (let t = 0n; t < 4n; t = i64(t + 1n)) {
    k = i64(k + 1n);
  }
  return i64(s + k);
}

function unsigned() {
  const a = 250;
  const b = (a + 10) & 255;
  const c = 4000000000;
  const d = (c + c) >>> 0;
  const e = 2147483647;
  const f = (e + 1) | 0;
  return i64(i64(BigInt(b) + BigInt(d)) + BigInt(f) % 1000n);
}

function floats() {
  const x = 7.5;
  const y = 2.25;
  const z = x / 2.0 + y * 2.0;
  if (z > 8.0 && x !== y) {
    return BigInt(Math.trunc(z));
  }
  return 0n;
}

function main() {
  let score = 0n;
  score = i64(i64(i64(i64(i64(i64(score + sets()) + maps()) + strs()) + opts()) + bufs()) + swaps());
  score = i64(i64(i64(i64(i64(i64(score + choose(0n)) + choose(1n)) + choose(5n)) + loops(6n)) + unsigned()) + floats());
  return score % 251n;
}

const status = main();
if (typeof process !== "undefined") {
  process.exitCode = Number(status);
}
