# A whole number brought back into the signed 64-bit range, as i64 arithmetic wraps.
def i64(value: int) -> int:
    return (value + 2**63) % 2**64 - 2**63

def drop[T](xs: list[T], n: int) -> list[T]:
    out: list[T] = []
    i: int = n
    while i < len(xs):
        out.append(xs[i])
        i = i64(i + 1)
    return out

def first[T](xs: list[T]) -> T:
    return xs[0]

def last[T](xs: list[T]) -> T:
    return xs[i64(len(xs) - 1)]
