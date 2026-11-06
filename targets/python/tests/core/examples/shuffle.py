# A whole number brought back into the signed 64-bit range, as i64 arithmetic wraps.
def i64(value: int) -> int:
    return (value + 2**63) % 2**64 - 2**63

def reverse4(a: int, b: int, c: int) -> int:
    v: list[int] = [a, b, c, 40]
    r: list[int] = [v[lane] for lane in [3, 2, 1, 0]]
    return i64(i64(i64(i64(r[0] * 1000) + i64(r[1] * 100)) + i64(r[2] * 10)) + r[3])

def duplicate_first_two(a: int, b: int, c: int) -> int:
    v: list[int] = [a, b, c, 4]
    r: list[int] = [v[lane] for lane in [0, 0, 1, 1]]
    return i64(i64(i64(i64(r[0] * 1000) + i64(r[1] * 100)) + i64(r[2] * 10)) + r[3])
