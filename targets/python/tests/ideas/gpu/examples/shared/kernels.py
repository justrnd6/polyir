import math
import struct

# A float rounded to 32 bits, as a `float` (polyir's f32) holds it.
def f32(value: float) -> float:
    return struct.unpack("f", struct.pack("f", value))[0]

# Whole-number division and remainder that truncate toward zero, as polyir's do (Python's // and % round down).
def div(a: int, b: int) -> int:
    quotient = abs(a) // abs(b)
    return quotient if (a < 0) == (b < 0) else -quotient

def rem(a: int, b: int) -> int:
    return a - b * div(a, b)

# A whole number brought back into the signed 32-bit range, as i32 arithmetic wraps.
def i32(value: int) -> int:
    return (value + 2**31) % 2**32 - 2**31

# A whole number brought back into the signed 64-bit range, as i64 arithmetic wraps.
def i64(value: int) -> int:
    return (value + 2**63) % 2**64 - 2**63

def sqrtf(x: float) -> float:
    return f32(math.sqrt(x))

def scale(x: float, k: float) -> float:
    return f32(f32(x * k) + 1.0)

def norm(a: float, b: float) -> float:
    s: float = f32(scale(a, 2.0) + f32(b * b))
    return sqrtf(s)

def sum_to(n: int) -> int:
    acc: int = 0
    i: int = 0
    while i < n:
        if rem(i, 2) == 0:
            acc = i32(acc + i32(i))
        i = i64(i + 1)
    return acc

def lane(v: list[float]) -> float:
    return f32(v[0] + v[3])

def smooth(i: int, src: list[float], n: int) -> float:
    if i == 0:
        return src[i]
    if i == i32(n - 1):
        return src[i]
    return f32(f32(f32(src[i32(i - 1)] + src[i]) + src[i32(i + 1)]) / 3.0)

def leftover(a: float, b: float) -> float:
    return math.fmod(a, b)

def close(a: float, b: float) -> int:
    diff: float = f32(a - b)
    if diff < 0.0:
        diff = f32(0.0 - diff)
    size: float = b
    if size < 0.0:
        size = f32(0.0 - size)
    if diff <= f32(0.000001 * f32(size + 1.0)):
        return 1
    return 0
