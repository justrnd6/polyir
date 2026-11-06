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

function count(n: bigint): Coroutine<bigint> {
  return new Coroutine<bigint>(function* (coroutine: Coroutine<bigint>) {
    let gone: boolean = false;
    for (let i = 0n; i < n; i = i64(i + 1n)) {
      if (!gone) {
        gone = yield* coroutine.give(i);
      }
    }
  });
}

function main(): bigint {
  const c: Coroutine<bigint> = count(5n);
  const first: Variant = c.resume();
  c.close();
  if (c.done()) {
    return i64(first.value + 40n);
  }
  return 1n;
}

const status = main();
if (typeof process !== "undefined") {
  process.exitCode = Number(status);
}
