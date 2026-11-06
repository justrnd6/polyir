import math
import struct

from Check import off

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

def check_sign() -> float:
    return off(sign(f32(0.0 - 3.0)), f32(0.0 - 1.0)) + off(sign(0.0), 0.0) + off(sign(2.0), 1.0)

def check_fract() -> float:
    return off(fract(2.75), 0.75) + off(fract(f32(0.0 - 0.25)), 0.75)

def check_clamp() -> float:
    return off(clamp(5.0, f32(0.0 - 1.0), 2.0), 2.0) + off(clamp(f32(0.0 - 5.0), f32(0.0 - 1.0), 2.0), f32(0.0 - 1.0))

def check_mix() -> float:
    return off(mix(10.0, 20.0, 0.25), 12.5)

def check_nested_calls() -> float:
    return off(clamp(f32(fract(1.75) * 2.0), f32(0.0 - 1.0), 1.0), 1.0) + off(mix(sign(2.0), fract(0.5), 0.5), 0.75)

def rescale(x: float) -> float:
    return f32(x * 3.0)

def check_own_functions_still_work() -> float:
    return off(rescale(fract(1.25)), 0.75)
