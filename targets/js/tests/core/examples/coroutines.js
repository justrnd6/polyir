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

function putchar(c) {
  process.stdout.write(String.fromCharCode(Number(c) & 255));
  return c;
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

function main() {
  const fib = fibonacci(7n);
  let sum = 0n;
  let done = false;
  for (let i = 0n; i < 8n && done === false; i = i64(i + 1n)) {
    const step = fib.resume();
    if (step.tag === 1n) {
      done = true;
    } else {
      const value = step.value;
      putchar(i64(48n + value));
      sum = i64(sum + value);
    }
  }
  putchar(10n);
  if (fib.done() === false) {
    return -1n;
  }
  return sum;
}

const status = main();
if (typeof process !== "undefined") {
  process.exitCode = Number(status);
}
