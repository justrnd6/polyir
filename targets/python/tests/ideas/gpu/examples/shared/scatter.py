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

def floorf(x: float) -> float:
    return f32(math.floor(x))

def sinf(x: float) -> float:
    return f32(math.sin(x))

def cosf(x: float) -> float:
    return f32(math.cos(x))

def atomic_add(counts: list[int], slot: int, amount: int) -> int:
    old: int = counts[slot]
    counts[slot] = i32(old + amount)
    return old

def reverse_copy(i: int, src: list[float], n: int, dst: list[float]) -> int:
    dst[i32(i32(n - 1) - i)] = src[i]
    return 0

def histogram(i: int, data: list[int], bins: int, counts: list[int]) -> int:
    bin: int = rem(data[i], bins)
    atomic_add(counts, bin, 1)
    return 0

def histogram_racy(i: int, data: list[int], bins: int, counts: list[int]) -> int:
    bin: int = rem(data[i], bins)
    counts[bin] = i32(counts[bin] + 1)
    return 0

def splat(i: int, p: list[float], cells: list[int]) -> int:
    w: int = i32(int(p[0]))
    arm: int = rem(i, 2)
    t: float = f32(f32(rem(div(i, 2), 20000)) / 20000.0)
    angle: float = f32(f32(t * 9.0) + f32(f32(arm) * 3.14159265))
    radius: float = f32(0.08 + 0.85 * t)
    h1: int = rem(i32(i32(i * 75) + 74), 65537)
    h2: int = rem(i32(i32(h1 * 75) + 74), 65537)
    spread: float = f32(0.015 + 0.09 * t)
    x: float = f32(f32(radius * cosf(angle)) + f32(f32(f32(f32(f32(h1) / 65537.0) - 0.5) * spread) * 2.0))
    y: float = f32(f32(radius * sinf(angle)) + f32(f32(f32(f32(f32(h2) / 65537.0) - 0.5) * spread) * 2.0))
    px: int = i32(int(floorf(f32(f32(f32(x * 0.5) + 0.5) * f32(w)))))
    py: int = i32(int(floorf(f32(f32(f32(y * 0.5) + 0.5) * f32(w)))))
    if px >= 0 and px < w and py >= 0 and py < w:
        atomic_add(cells, i32(i32(py * w) + px), 1)
    return 0
