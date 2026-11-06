import { offInt } from "./Check.js";

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

let made: bigint = 0n;

function upTo(limit: bigint): Coroutine<bigint> {
  return new Coroutine<bigint>(function* (coroutine: Coroutine<bigint>) {
    for (let i = 0n; i < limit; i = i64(i + 1n)) {
      yield i;
    }
  });
}

function counted(limit: bigint): Coroutine<bigint> {
  made = i64(made + 1n);
  return upTo(limit);
}

function checkSumsEveryValue(): number {
  let total: bigint = 0n;
  const forGen_21_10: Coroutine<bigint> = upTo(5n);
  let forStep_21_10: Variant = forGen_21_10.resume();
  while (forStep_21_10.tag === 0n) {
    const n: bigint = forStep_21_10.value;
    total = i64(total + n);
    forStep_21_10 = forGen_21_10.resume();
  }
  return offInt(total, 10n);
}

function checkEmptyCoroutineRunsNoTurns(): number {
  let turns: bigint = 0n;
  const forGen_29_10: Coroutine<bigint> = upTo(0n);
  let forStep_29_10: Variant = forGen_29_10.resume();
  while (forStep_29_10.tag === 0n) {
    const n: bigint = forStep_29_10.value;
    turns = i64(turns + 1n);
    forStep_29_10 = forGen_29_10.resume();
  }
  return offInt(turns, 0n);
}

function firstAbove(limit: bigint): bigint {
  const forGen_36_10: Coroutine<bigint> = upTo(100n);
  let forStep_36_10: Variant = forGen_36_10.resume();
  while (forStep_36_10.tag === 0n) {
    const n: bigint = forStep_36_10.value;
    if (n > limit) {
      forGen_36_10.close();
      return n;
    }
    forStep_36_10 = forGen_36_10.resume();
  }
  return -1n;
}

function checkReturnLeavesMidway(): number {
  return offInt(firstAbove(6n), 7n);
}

function checkLoopsNest(): number {
  let total: bigint = 0n;
  const forGen_50_10: Coroutine<bigint> = upTo(3n);
  let forStep_50_10: Variant = forGen_50_10.resume();
  while (forStep_50_10.tag === 0n) {
    const a: bigint = forStep_50_10.value;
    const forGen_51_14: Coroutine<bigint> = upTo(2n);
    let forStep_51_14: Variant = forGen_51_14.resume();
    while (forStep_51_14.tag === 0n) {
      const b: bigint = forStep_51_14.value;
      total = i64(i64(total + i64(a * 10n)) + b);
      forStep_51_14 = forGen_51_14.resume();
    }
    forStep_50_10 = forGen_50_10.resume();
  }
  return offInt(total, 63n);
}

function checkSourceIsEvaluatedOnce(): number {
  made = 0n;
  let total: bigint = 0n;
  const forGen_61_10: Coroutine<bigint> = counted(4n);
  let forStep_61_10: Variant = forGen_61_10.resume();
  while (forStep_61_10.tag === 0n) {
    const n: bigint = forStep_61_10.value;
    total = i64(total + n);
    forStep_61_10 = forGen_61_10.resume();
  }
  return offInt(i64(i64(made * 100n) + total), 106n);
}

export { upTo, counted, checkSumsEveryValue, checkEmptyCoroutineRunsNoTurns, firstAbove, checkReturnLeavesMidway, checkLoopsNest, checkSourceIsEvaluatedOnce };
