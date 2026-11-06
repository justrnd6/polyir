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

def count(n: int) -> Coroutine:
    def run(coroutine: Coroutine, n: int):
        i: int = 0
        while i < n:
            yield i
            i = i64(i + 1)

    return Coroutine(run, n)

def main() -> int:
    c: Coroutine = count(5)
    first: Variant = c.resume()
    c.close()
    return i64(first.value + 40)

if __name__ == "__main__":
    raise SystemExit(main())
