"use strict";

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

// A 64-bit integer is a BigInt, brought back into 64 bits after an operation that can leave them.
function i64(value) {
  return BigInt.asIntN(64, value);
}

function fibonacci(count) {
  return new Coroutine(function* (coroutine) {
    let a = 0n;
    let b = 1n;
    for (let i = 0n; i < count; i = i64(i + 1n)) {
      yield a;
      const next = i64(a + b);
      a = b;
      b = next;
    }
  });
}

function countdown(from) {
  return new Coroutine(function* (coroutine) {
    let n = from;
    for (let i = 0n; i < from; i = i64(i + 1n)) {
      yield n;
      n = i64(n - 1n);
    }
  });
}

function total(source) {
  let sum = 0n;
  let finished = false;
  for (let i = 0n; i < 100n && finished === false; i = i64(i + 1n)) {
    const step = source.resume();
    if (step.tag === 1n) {
      finished = true;
    } else {
      sum = i64(sum + step.value);
    }
  }
  return sum;
}

function main() {
  let score = total(fibonacci(7n));
  score = i64(score + total(countdown(4n)));
  const two = countdown(3n);
  const first = two.resume();
  const second = two.resume();
  score = i64(i64(score + i64(first.value * 10n)) + second.value);
  if (two.done() === false) {
    score = i64(score + 100n);
  }
  return score;
}

const status = main();
if (typeof process !== "undefined") {
  process.exitCode = Number(status);
}
