import struct

# A whole number brought back into the signed 32-bit range, as i32 arithmetic wraps.
def i32(value: int) -> int:
    return (value + 2**31) % 2**32 - 2**31

# A whole number brought back into the signed 64-bit range, as i64 arithmetic wraps.
def i64(value: int) -> int:
    return (value + 2**63) % 2**64 - 2**63

# A float rounded to 32 bits, as a `float` (polyir's f32) holds it.
def f32(value: float) -> float:
    return struct.unpack("f", struct.pack("f", value))[0]

# Whole-number division and remainder that truncate toward zero, as polyir's do (Python's // and % round down).
def div(a: int, b: int) -> int:
    quotient = abs(a) // abs(b)
    return quotient if (a < 0) == (b < 0) else -quotient

def rem(a: int, b: int) -> int:
    return a - b * div(a, b)

def pair_sum(i: int, src: list[float], n: int) -> float:
    a: int = i32(i64(2 * i))
    total: float = src[a]
    if i32(a + 1) < n:
        total = f32(total + src[i32(a + 1)])
    return total

def pair_sum_int(i: int, src: list[int], n: int) -> int:
    a: int = i32(i64(2 * i))
    total: int = src[a]
    if i32(a + 1) < n:
        total = i32(total + src[i32(a + 1)])
    return total

def pair_max(i: int, src: list[float], n: int) -> float:
    a: int = i32(i64(2 * i))
    best: float = src[a]
    if i32(a + 1) < n:
        if src[i32(a + 1)] > best:
            best = src[i32(a + 1)]
    return best

def pass_count(n: int) -> int:
    passes: int = 0
    left: int = n
    k: int = 0
    while k < 40:
        if left > 1:
            left = div(i64(left + 1), 2)
            passes = i64(passes + 1)
        k = i64(k + 1)
    return passes

def cpu_sum(values: list[float], n: int) -> float:
    total: float = 0.0
    i: int = 0
    while i < n:
        total = f32(total + values[i])
        i = i64(i + 1)
    return total

def cpu_max(values: list[float], n: int) -> float:
    best: float = values[0]
    i: int = 1
    while i < n:
        if values[i] > best:
            best = values[i]
        i = i64(i + 1)
    return best

def fill_test_data(values: list[float], n: int) -> int:
    i: int = 0
    while i < n:
        values[i] = f32(rem(i, 7))
        i = i64(i + 1)
    values[div(n, 3)] = 1000.0
    return 0
