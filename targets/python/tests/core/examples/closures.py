from collections.abc import Callable

# A whole number brought back into the signed 64-bit range, as i64 arithmetic wraps.
def i64(value: int) -> int:
    return (value + 2**63) % 2**64 - 2**63

def counter_twice() -> int:
    count: int = 0
    def closure1():
        nonlocal count
        count = i64(count + 1)
        return count
    inc: Callable[[], int] = closure1
    a: int = inc()
    b: int = inc()
    return i64(i64(a * 100) + b)

def make_multiplier(factor: int) -> Callable[[int], int]:
    mul: Callable[[int], int] = lambda x: i64(x * factor)
    return mul

def triple_of(x: int) -> int:
    triple: Callable[[int], int] = make_multiplier(3)
    return triple(x)

def shared_counter() -> int:
    shared: int = 0
    def closure3():
        nonlocal shared
        shared = i64(shared + 1)
        return shared
    bump_by1: Callable[[], int] = closure3
    def closure4():
        nonlocal shared
        shared = i64(shared + 10)
        return shared
    bump_by10: Callable[[], int] = closure4
    a: int = bump_by1()
    b: int = bump_by10()
    c: int = bump_by1()
    return i64(i64(i64(a * 10000) + i64(b * 100)) + c)
