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
