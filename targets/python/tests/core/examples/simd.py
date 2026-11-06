# An operator lane by lane: on two vectors, or on a vector and one number.
def lanes(a: list, b, op) -> list:
    return [op(x, b[i] if isinstance(b, list) else b) for i, x in enumerate(a)]

# A whole number brought back into the signed 64-bit range, as i64 arithmetic wraps.
def i64(value: int) -> int:
    return (value + 2**63) % 2**64 - 2**63

def vec_mul_sum(a: float, b: float) -> float:
    v1: list[float] = [a, a + 1.0, a + 2.0, a + 3.0]
    v2: list[float] = [b, b, b, b]
    p: list[float] = lanes(v1, v2, lambda x, y: x * y)
    return p[0] + p[1] + p[2] + p[3]

def sum_i64_vec(a: int, b: int, c: int) -> int:
    v: list[int] = [a, b, c]
    return i64(i64(v[0] + v[1]) + v[2])

def vec_mul_sum_via_var(a: float, b: float) -> float:
    v1: list[float] = [a, a + 1.0, a + 2.0, a + 3.0]
    v2: list[float] = [b, b, b, b]
    p: list[float] = lanes(v1, v2, lambda x, y: x * y)
    total: float = 0.0
    i: int = 0
    while i < 4:
        total = total + p[i]
        i = i64(i + 1)
    return total
