from dataclasses import dataclass

# A variant: the number of its case and the value it holds.
@dataclass
class Variant:
    tag: int
    value: object

# A coroutine: its body one resume at a time. `resume` gives case 0 with the value it yields, or case 1 when the body has
# finished; `give` is a yield that hears whether the coroutine was closed.
class Coroutine:
    def __init__(self, body, *args):
        self.finished = False
        self.started = False
        self.waiting = False
        self.source = body(self, *args)

    def resume(self) -> Variant:
        if self.finished:
            return Variant(1, None)
        self.waiting = False
        try:
            value = self.source.send(False) if self.started else next(self.source)
        except StopIteration:
            self.finished = True
            return Variant(1, None)
        self.started = True
        return Variant(0, value)

    def done(self) -> bool:
        return self.finished

    def give(self, value):
        self.waiting = True
        return (yield value) is True

    def close(self):
        if not self.finished and self.waiting:
            try:
                self.source.send(True)
            except StopIteration:
                pass
        self.finished = True

# A whole number brought back into the signed 64-bit range, as i64 arithmetic wraps.
def i64(value: int) -> int:
    return (value + 2**63) % 2**64 - 2**63

def fibonacci(count: int) -> Coroutine:
    def run(coroutine: Coroutine, count: int):
        a: int = 0
        b: int = 1
        i: int = 0
        while i < count:
            yield a
            next_: int = i64(a + b)
            a = b
            b = next_
            i = i64(i + 1)

    return Coroutine(run, count)

def countdown(from_: int) -> Coroutine:
    def run(coroutine: Coroutine, from_: int):
        n: int = from_
        i: int = 0
        while i < from_:
            yield n
            n = i64(n - 1)
            i = i64(i + 1)

    return Coroutine(run, from_)

def total(source: Coroutine) -> int:
    sum_: int = 0
    finished: bool = False
    i: int = 0
    while i < 100 and finished == False:
        step: Variant = source.resume()
        if step.tag == 1:
            finished = True
        else:
            sum_ = i64(sum_ + step.value)
        i = i64(i + 1)
    return sum_

def main() -> int:
    score: int = total(fibonacci(7))
    score = i64(score + total(countdown(4)))
    two: Coroutine = countdown(3)
    first: Variant = two.resume()
    second: Variant = two.resume()
    score = i64(i64(score + i64(first.value * 10)) + second.value)
    if two.done() == False:
        score = i64(score + 100)
    return score

if __name__ == "__main__":
    raise SystemExit(main())
