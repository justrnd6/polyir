import sys
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

def putchar(c: int) -> int:
    sys.stdout.write(chr(c & 255))
    return c

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

def main() -> int:
    fib: Coroutine = fibonacci(7)
    sum_: int = 0
    done: bool = False
    i: int = 0
    while i < 8 and done == False:
        step: Variant = fib.resume()
        if step.tag == 1:
            done = True
        else:
            value: int = step.value
            putchar(i64(48 + value))
            sum_ = i64(sum_ + value)
        i = i64(i + 1)
    putchar(10)
    if fib.done() == False:
        return -1
    return sum_

if __name__ == "__main__":
    raise SystemExit(main())
