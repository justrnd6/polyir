"use strict";

// A 64-bit integer is a BigInt, brought back into 64 bits after an operation that can leave them.
function i64(value) {
  return BigInt.asIntN(64, value);
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

// A variant: the number of its case and the value it holds.
class Variant {
  constructor(tag, value) {
    this.tag = tag;
    this.value = value;
  }
}

// A coroutine: its body one resume at a time. `resume` gives case 0 with the value it yields, or case 1 when the body has
// finished; `give` is a yield that hears whether the coroutine was closed.
class Coroutine {
  constructor(body) {
    this.finished = false;
    this.waiting = false;
    this.source = body(this);
  }

  resume() {
    if (this.finished) {
      return new Variant(1n, null);
    }
    this.waiting = false;
    const next = this.source.next(false);
    this.finished = next.done;
    return next.done ? new Variant(1n, null) : new Variant(0n, next.value);
  }

  done() {
    return this.finished;
  }

  *give(value) {
    this.waiting = true;
    return (yield value) === true;
  }

  close() {
    if (!this.finished && this.waiting) {
      this.source.next(true);
    }
    this.finished = true;
  }
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

let flag = false;
let ratio = 1.5;
let hits = 0n;

class Node {
  constructor(value, next) {
    this.value = value;
    this.next = next;
  }
}

class Holder {
  constructor(op, name) {
    this.op = op;
    this.name = name;
  }

  run(x) {
    const f = this.op;
    return f(x);
  }

  viaLater() {
    return (y) => {
      const f = this.op;
      return i64(f(y) + textLength(this.name));
    };
  }
}

function pairs(n) {
  return new Coroutine(function* (coroutine) {
    for (let i = 0n; i < n; i = i64(i + 1n)) {
      yield new Node(i, null);
    }
  });
}

function sumNodes(n) {
  const g = pairs(n);
  let s = 0n;
  let done = false;
  for (let k = 0n; k < 20n && done === false; k = i64(k + 1n)) {
    const r = g.resume();
    if (r.tag === 1n) {
      done = true;
    } else {
      s = i64(s + r.value.value);
    }
  }
  return s;
}

function chain() {
  const a = new Node(1n, null);
  const b = new Node(2n, a);
  const c = new Node(3n, b);
  let sum = 0n;
  let cur = c;
  for (let i = 0n; i < 5n && cur !== null; i = i64(i + 1n)) {
    const n = present(cur);
    sum = i64(sum + n.value);
    cur = n.next;
  }
  a.value = 10n;
  return i64(i64(sum * 100n) + present(b.next).value);
}

function nestedClosures(a) {
  let total = 0n;
  const outer = (x) => {
    const inner = (y) => {
      total = i64(total + y);
      return i64(i64(x * y) + a);
    };
    return i64(inner(i64(x + 1n)) + inner(2n));
  };
  const r = outer(3n);
  return i64(i64(r * 1000n) + total);
}

function strClosure(prefix) {
  const tail = "!";
  const f = (s) => prefix + s + tail;
  return textLength(f("abc"));
}

function holders() {
  const h = new Holder((x) => i64(x * x), "hh");
  const later = h.viaLater();
  return i64(h.run(5n) + later(2n));
}

function globals() {
  hits = i64(hits + 5n);
  flag = true;
  ratio = ratio * 2.0;
  let r = hits;
  if (flag) {
    r = i64(r + 100n);
  }
  if (ratio > 2.5) {
    r = i64(r + 1000n);
  }
  return r;
}

function findFirst(limit) {
  for (let i = 0n; i < limit; i = i64(i + 1n)) {
    for (let j = 0n; j < limit; j = i64(j + 1n)) {
      if (i64(i * j) === 12n) {
        return i64(i64(i * 100n) + j);
      }
    }
  }
  return i64(0n - 1n);
}

function tryInLoop(n) {
  let s = 0n;
  for (let i = 0n; i < n; i = i64(i + 1n)) {
    try {
      if (i % 2n === 1n) {
        throw i;
      }
      s = i64(s + 10n);
    } catch (code) {
      s = i64(s + code);
    }
  }
  return s;
}

function lines(a, b) {
  let label = 0n;
  let acc = "";
  let n = 0n;
  machine: for (;;) {
    switch (label) {
      case 0n: {
        acc = a;
        n = 0n;
        label = 1n;
        continue machine;
      }
      case 1n: {
        if (n < 3n) {
          label = 2n;
          continue machine;
        } else {
          label = 3n;
          continue machine;
        }
      }
      case 2n: {
        acc = acc + b;
        n = i64(n + 1n);
        label = 1n;
        continue machine;
      }
      case 3n: {
        return acc;
      }
    }
  }
}

function lineLen() {
  return textLength(lines("x", "yz"));
}

function machineStrings(n) {
  let label = 0n;
  let p = 0n;
  let s = "";
  let q = 0n;
  let r = 0n;
  machine: for (;;) {
    switch (label) {
      case 0n: {
        if (n === 0n) {
          p = n;
          s = "p";
          label = 1n;
          continue machine;
        } else {
          q = n;
          s = "q";
          label = 2n;
          continue machine;
        }
      }
      case 1n: {
        if (p > 4n) {
          r = p;
          label = 3n;
          continue machine;
        } else {
          q = i64(p + 1n);
          s = s + "a";
          label = 2n;
          continue machine;
        }
      }
      case 2n: {
        if (q > 4n) {
          r = q;
          label = 3n;
          continue machine;
        } else {
          p = i64(q + 1n);
          s = s + "b";
          label = 1n;
          continue machine;
        }
      }
      case 3n: {
        return i64(i64(r * 100n) + textLength(s));
      }
    }
  }
}

function precedence(a, b) {
  const x = i64(i64(a + b) * i64(a - b)) / 3n;
  const y = a & b | a ^ b;
  const z = i64(a << i64(2n + b)) >> 1n;
  let s = i64(i64(x + y) + z);
  if ((a & 1n) === 1n && b > 2n || a === 0n) {
    s = i64(s + 1n);
  }
  const w = Number(BigInt.asIntN(32, a));
  if (BigInt(w) < 100n && Number(BigInt.asIntN(32, i64(a + b))) < 1000) {
    s = i64(s + 2n);
  }
  return s;
}

function main() {
  let score = i64(i64(i64(sumNodes(5n) + chain()) + nestedClosures(2n)) + strClosure("ab"));
  score = i64(i64(i64(i64(score + holders()) + globals()) + findFirst(10n)) + tryInLoop(6n));
  score = i64(i64(i64(i64(score + lineLen()) + machineStrings(0n)) + machineStrings(1n)) + precedence(13n, 7n));
  return score % 251n;
}

const status = main();
if (typeof process !== "undefined") {
  process.exitCode = Number(status);
}
