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

def evens(limit: int) -> Coroutine:
    def run(coroutine: Coroutine, limit: int):
        i: int = 0
        while i < limit:
            yield i
            i = i64(i + 2)

    return Coroutine(run, limit)

def sum_of_evens() -> int:
    total: int = 0
    for_gen_14_8: Coroutine = evens(10)
    for_step_14_8: Variant = for_gen_14_8.resume()
    while for_step_14_8.tag == 0:
        n: int = for_step_14_8.value
        total = i64(total + n)
        for_step_14_8 = for_gen_14_8.resume()
    return total

def first_over(limit: int) -> int:
    for_gen_22_8: Coroutine = evens(100)
    for_step_22_8: Variant = for_gen_22_8.resume()
    while for_step_22_8.tag == 0:
        n: int = for_step_22_8.value
        if n > limit:
            for_gen_22_8.close()
            return n
        for_step_22_8 = for_gen_22_8.resume()
    return -1

def pairs() -> int:
    total: int = 0
    for_gen_33_8: Coroutine = evens(6)
    for_step_33_8: Variant = for_gen_33_8.resume()
    while for_step_33_8.tag == 0:
        a: int = for_step_33_8.value
        for_gen_34_10: Coroutine = evens(4)
        for_step_34_10: Variant = for_gen_34_10.resume()
        while for_step_34_10.tag == 0:
            b: int = for_step_34_10.value
            total = i64(i64(total + i64(a * 10)) + b)
            for_step_34_10 = for_gen_34_10.resume()
        for_step_33_8 = for_gen_33_8.resume()
    return total

def main() -> int:
    return i64(i64(sum_of_evens() + first_over(5)) + pairs())

if __name__ == "__main__":
    raise SystemExit(main())
