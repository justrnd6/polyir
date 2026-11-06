import math
import struct

from Math import clamp, fract

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

def sinf(x: float) -> float:
    return f32(math.sin(x))

def fabsf(x: float) -> float:
    return f32(abs(x))

def powf(x: float, y: float) -> float:
    return f32(math.pow(x, y))

def sine(phase: float) -> float:
    return sinf(f32(6.2831853 * fract(phase)))

def square(phase: float) -> float:
    if fract(phase) < 0.5:
        return 1.0
    return f32(0.0 - 1.0)

def sawtooth(phase: float) -> float:
    return f32(2.0 * fract(phase) - 1.0)

def triangle(phase: float) -> float:
    return f32(1.0 - 4.0 * fabsf(f32(fract(phase) - 0.5)))

def noise(seed: int) -> float:
    a: int = rem(i32(i32(seed * 75) + 74), 65537)
    b: int = rem(i32(i32(a * 75) + 74), 65537)
    return f32(2.0 * f32(b) / 65537.0 - 1.0)

def fade(t: float, attack: float, release: float, total: float) -> float:
    level: float = f32(t / attack) if t < attack else 1.0
    if f32(total - t) < release:
        level = f32(level * clamp(f32(f32(total - t) / release), 0.0, 1.0))
    return level

def note_frequency(m: float) -> float:
    return f32(440.0 * powf(2.0, f32(f32(m - 69.0) / 12.0)))
