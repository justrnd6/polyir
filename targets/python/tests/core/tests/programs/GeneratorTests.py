from dataclasses import dataclass

from Check import off_int

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

made: int = 0

def up_to(limit: int) -> Coroutine:
    def run(coroutine: Coroutine, limit: int):
        i: int = 0
        while i < limit:
            yield i
            i = i64(i + 1)

    return Coroutine(run, limit)

def counted(limit: int) -> Coroutine:
    global made
    made = i64(made + 1)
    return up_to(limit)

def check_sums_every_value() -> float:
    total: int = 0
    for_gen_21_10: Coroutine = up_to(5)
    for_step_21_10: Variant = for_gen_21_10.resume()
    while for_step_21_10.tag == 0:
        n: int = for_step_21_10.value
        total = i64(total + n)
        for_step_21_10 = for_gen_21_10.resume()
    return off_int(total, 10)

def check_empty_coroutine_runs_no_turns() -> float:
    turns: int = 0
    for_gen_29_10: Coroutine = up_to(0)
    for_step_29_10: Variant = for_gen_29_10.resume()
    while for_step_29_10.tag == 0:
        n: int = for_step_29_10.value
        turns = i64(turns + 1)
        for_step_29_10 = for_gen_29_10.resume()
    return off_int(turns, 0)

def first_above(limit: int) -> int:
    for_gen_36_10: Coroutine = up_to(100)
    for_step_36_10: Variant = for_gen_36_10.resume()
    while for_step_36_10.tag == 0:
        n: int = for_step_36_10.value
        if n > limit:
            for_gen_36_10.close()
            return n
        for_step_36_10 = for_gen_36_10.resume()
    return -1

def check_return_leaves_midway() -> float:
    return off_int(first_above(6), 7)

def check_loops_nest() -> float:
    total: int = 0
    for_gen_50_10: Coroutine = up_to(3)
    for_step_50_10: Variant = for_gen_50_10.resume()
    while for_step_50_10.tag == 0:
        a: int = for_step_50_10.value
        for_gen_51_14: Coroutine = up_to(2)
        for_step_51_14: Variant = for_gen_51_14.resume()
        while for_step_51_14.tag == 0:
            b: int = for_step_51_14.value
            total = i64(i64(total + i64(a * 10)) + b)
            for_step_51_14 = for_gen_51_14.resume()
        for_step_50_10 = for_gen_50_10.resume()
    return off_int(total, 63)

def check_source_is_evaluated_once() -> float:
    global made
    made = 0
    total: int = 0
    for_gen_61_10: Coroutine = counted(4)
    for_step_61_10: Variant = for_gen_61_10.resume()
    while for_step_61_10.tag == 0:
        n: int = for_step_61_10.value
        total = i64(total + n)
        for_step_61_10 = for_gen_61_10.resume()
    return off_int(i64(i64(made * 100) + total), 106)
