# A whole number brought back into the signed 64-bit range, as i64 arithmetic wraps.
def i64(value: int) -> int:
    return (value + 2**63) % 2**64 - 2**63

def off_text(got: str, want: str) -> float:
    if got == want:
        return 0.0
    return 1.0

def check_first_and_last() -> float:
    xs: list[int] = [3, 1, 4]
    words: list[str] = ["a", "b"]
    return off_text(f"{first(xs)} {last(xs)} {first(words)}{last(words)}", "3 4 ab")

def check_drop() -> float:
    xs: list[int] = [1, 2, 3]
    rest: list[int] = drop(xs, 1)
    none: list[int] = drop(xs, 5)
    return off_text(f"{len(rest)} {rest[0]} {rest[1]} {len(none)}", "2 2 3 0")

def first[T](xs: list[T]) -> T:
    return xs[0]

def last[T](xs: list[T]) -> T:
    return xs[i64(len(xs) - 1)]

def drop[T](xs: list[T], n: int) -> list[T]:
    out: list[T] = []
    i: int = n
    while i < len(xs):
        out.append(xs[i])
        i = i64(i + 1)
    return out
