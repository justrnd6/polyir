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

function evens(limit: bigint): Coroutine<bigint> {
  return new Coroutine<bigint>(function* (coroutine: Coroutine<bigint>) {
    for (let i = 0n; i < limit; i = i64(i + 2n)) {
      yield i;
    }
  });
}

function sumOfEvens(): bigint {
  let total: bigint = 0n;
  const forGen_14_8: Coroutine<bigint> = evens(10n);
  let forStep_14_8: Variant = forGen_14_8.resume();
  while (forStep_14_8.tag === 0n) {
    const n: bigint = forStep_14_8.value;
    total = i64(total + n);
    forStep_14_8 = forGen_14_8.resume();
  }
  return total;
}

function firstOver(limit: bigint): bigint {
  const forGen_22_8: Coroutine<bigint> = evens(100n);
  let forStep_22_8: Variant = forGen_22_8.resume();
  while (forStep_22_8.tag === 0n) {
    const n: bigint = forStep_22_8.value;
    if (n > limit) {
      forGen_22_8.close();
      return n;
    }
    forStep_22_8 = forGen_22_8.resume();
  }
  return -1n;
}

function pairs(): bigint {
  let total: bigint = 0n;
  const forGen_33_8: Coroutine<bigint> = evens(6n);
  let forStep_33_8: Variant = forGen_33_8.resume();
  while (forStep_33_8.tag === 0n) {
    const a: bigint = forStep_33_8.value;
    const forGen_34_10: Coroutine<bigint> = evens(4n);
    let forStep_34_10: Variant = forGen_34_10.resume();
    while (forStep_34_10.tag === 0n) {
      const b: bigint = forStep_34_10.value;
      total = i64(i64(total + i64(a * 10n)) + b);
      forStep_34_10 = forGen_34_10.resume();
    }
    forStep_33_8 = forGen_33_8.resume();
  }
  return total;
}

function main(): bigint {
  return i64(i64(sumOfEvens() + firstOver(5n)) + pairs());
}

const status = main();
if (typeof process !== "undefined") {
  process.exitCode = Number(status);
}
