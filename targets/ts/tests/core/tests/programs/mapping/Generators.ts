declare const process: any;
export {};

// A variant: the number of its case and the value it holds.
class Variant {
  constructor(readonly tag: bigint, readonly value: any) {}
}

// A coroutine: its body one resume at a time. `resume` gives case 0 with the value it yields, or case 1 when the body has
// finished; `give` is a yield that hears whether the coroutine was closed.
class Coroutine<T> {
  private finished = false;
  private waiting = false;
  private source: Iterator<T, void, boolean>;

  constructor(body: (coroutine: Coroutine<T>) => Iterator<T, void, boolean>) {
    this.source = body(this);
  }

  resume(): Variant {
    if (this.finished) {
      return new Variant(1n, null);
    }
    this.waiting = false;
    const next = this.source.next(false);
    this.finished = next.done === true;
    return next.done ? new Variant(1n, null) : new Variant(0n, next.value);
  }

  done(): boolean {
    return this.finished;
  }

  *give(value: T): Generator<T, boolean, boolean> {
    this.waiting = true;
    return (yield value) === true;
  }

  close(): void {
    if (!this.finished && this.waiting) {
      this.source.next(true);
    }
    this.finished = true;
  }
}

// A 64-bit integer is a bigint, brought back into 64 bits after an operation that can leave them.
function i64(value: bigint): bigint {
  return BigInt.asIntN(64, value);
}

function fibonacci(count: bigint): Coroutine<bigint> {
  return new Coroutine<bigint>(function* (coroutine: Coroutine<bigint>) {
    let a: bigint = 0n;
    let b: bigint = 1n;
    for (let i = 0n; i < count; i = i64(i + 1n)) {
      yield a;
      const next: bigint = i64(a + b);
      a = b;
      b = next;
    }
  });
}

function countdown(from: bigint): Coroutine<bigint> {
  return new Coroutine<bigint>(function* (coroutine: Coroutine<bigint>) {
    let n: bigint = from;
    for (let i = 0n; i < from; i = i64(i + 1n)) {
      yield n;
      n = i64(n - 1n);
    }
  });
}

function total(source: Coroutine<bigint>): bigint {
  let sum: bigint = 0n;
  let finished: boolean = false;
  for (let i = 0n; i < 100n && finished === false; i = i64(i + 1n)) {
    const step: Variant = source.resume();
    if (step.tag === 1n) {
      finished = true;
    } else {
      sum = i64(sum + step.value);
    }
  }
  return sum;
}

function main(): bigint {
  let score: bigint = total(fibonacci(7n));
  score = i64(score + total(countdown(4n)));
  const two: Coroutine<bigint> = countdown(3n);
  const first: Variant = two.resume();
  const second: Variant = two.resume();
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
