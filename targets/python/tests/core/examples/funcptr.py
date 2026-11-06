from collections.abc import Callable

# A whole number brought back into the signed 64-bit range, as i64 arithmetic wraps.
def i64(value: int) -> int:
    return (value + 2**63) % 2**64 - 2**63

def double_it(x: int) -> int:
    return i64(x * 2)

def apply_twice(f: list[Callable[[int], int]], x: int) -> int:
    once: int = f(x)
    return f(once)

def apply_double_twice(x: int) -> int:
    return apply_twice(double_it, x)

def direct_double_check(x: int) -> int:
    return double_it(x)

def add_one(x: int) -> int:
    return i64(x + 1)

def pick_and_apply(x: int, use_add_one: int) -> int:
    fn: list[Callable[[int], int]] = double_it
    if use_add_one != 0:
        fn = add_one
    return fn(x)

def apply_n_times(f: list[Callable[[int], int]], x: int, n: int) -> int:
    acc: int = x
    i: int = 0
    while i < n:
        acc = f(acc)
        i = i64(i + 1)
    return acc

def double_four_times(x: int) -> int:
    return apply_n_times(double_it, x, 4)
