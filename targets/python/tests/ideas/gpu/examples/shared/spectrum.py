import math
import struct

# A float rounded to 32 bits, as a `float` (polyir's f32) holds it.
def f32(value: float) -> float:
    return struct.unpack("f", struct.pack("f", value))[0]

# A whole number brought back into the signed 32-bit range, as i32 arithmetic wraps.
def i32(value: int) -> int:
    return (value + 2**31) % 2**32 - 2**31

# Whole-number division and remainder that truncate toward zero, as polyir's do (Python's // and % round down).
def div(a: int, b: int) -> int:
    quotient = abs(a) // abs(b)
    return quotient if (a < 0) == (b < 0) else -quotient

def rem(a: int, b: int) -> int:
    return a - b * div(a, b)

# A whole number brought back into the signed 64-bit range, as i64 arithmetic wraps.
def i64(value: int) -> int:
    return (value + 2**63) % 2**64 - 2**63

def sqrtf(x: float) -> float:
    return f32(math.sqrt(x))

def sinf(x: float) -> float:
    return f32(math.sin(x))

def cosf(x: float) -> float:
    return f32(math.cos(x))

def dft(k: int, x: list[float], p: list[float]) -> float:
    n: int = i32(int(p[0]))
    re: float = 0.0
    im: float = 0.0
    j: int = 0
    while j < n:
        turns: float = f32(f32(rem(i32(k * i32(j)), n)) / f32(n))
        re = f32(re + f32(x[j] * cosf(f32(6.2831853 * turns))))
        im = f32(im - f32(x[j] * sinf(f32(6.2831853 * turns))))
        j = i64(j + 1)
    return f32(2.0 * sqrtf(f32(f32(re * re) + f32(im * im))) / f32(n))
