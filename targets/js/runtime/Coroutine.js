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
