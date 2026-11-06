import math
import struct

# A float rounded to 32 bits, as a `float` (polyir's f32) holds it.
def f32(value: float) -> float:
    return struct.unpack("f", struct.pack("f", value))[0]

def floorf(x: float) -> float:
    return f32(math.floor(x))

def sign(x: float) -> float:
    if x > 0.0:
        return 1.0
    if x < 0.0:
        return f32(0.0 - 1.0)
    return 0.0

def fract(x: float) -> float:
    return f32(x - floorf(x))

def clamp(x: float, low: float, high: float) -> float:
    if x < low:
        return low
    if x > high:
        return high
    return x

def mix(a: float, b: float, t: float) -> float:
    return f32(a + f32(f32(b - a) * t))
