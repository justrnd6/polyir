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

def sinf(x: float) -> float:
    return f32(math.sin(x))

def atomic_add(counts: list[int], slot: int, amount: int) -> int:
    old: int = counts[slot]
    counts[slot] = i32(old + amount)
    return old

def atomic_add_float(sums: list[float], slot: int, amount: float) -> float:
    old: float = sums[slot]
    sums[slot] = f32(old + amount)
    return old

def atomic_min(values: list[int], slot: int, candidate: int) -> int:
    old: int = values[slot]
    if candidate < old:
        values[slot] = candidate
    return old

def atomic_max(values: list[int], slot: int, candidate: int) -> int:
    old: int = values[slot]
    if candidate > old:
        values[slot] = candidate
    return old

def blend7(a: float, b: float, c: float, d: float, e: float, f: float, g: float, k: float) -> float:
    return f32(f32(k * f32(f32(a + f32(2.0 * b)) + f32(3.0 * c))) + f32(f32(d - e) * f32(f + g)))

def plasma(x: int, y: int, width: int, height: int, phase: float) -> float:
    u: float = f32(f32(x) / f32(width))
    v: float = f32(f32(y) / f32(height))
    a: float = sinf(f32(10.0 * u + phase))
    b: float = sinf(f32(8.0 * v - phase))
    c: float = sinf(f32(6.0 * f32(u + v) + 2.0 * phase))
    return f32(0.5 + 0.1667 * f32(f32(a + b) + c))

def vignette(pixel: float, x: int, y: int, width: int, height: int) -> float:
    dx: float = f32(f32(f32(x) / f32(width)) - 0.5)
    dy: float = f32(f32(f32(y) / f32(height)) - 0.5)
    edge: float = f32(1.0 - 1.6 * f32(f32(dx * dx) + f32(dy * dy)))
    return f32(pixel * edge)

def tally(i: int, counts: list[int], sums: list[float], lows: list[int], highs: list[int]) -> int:
    value: int = rem(i32(i * 7919), 10007)
    atomic_add(counts, rem(i, 4), 1)
    atomic_add_float(sums, rem(i, 4), 0.5)
    atomic_min(lows, 0, value)
    atomic_max(highs, 0, value)
    return 0

def reported(i: int) -> int:
    return rem(i32(i * 7919), 10007)

def nearly(a: float, b: float, tolerance: float) -> int:
    gap: float = f32(a - b)
    if gap < 0.0:
        gap = f32(0.0 - gap)
    if gap <= tolerance:
        return 1
    return 0
