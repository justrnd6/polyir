import struct

# A float rounded to 32 bits, as a `float` (polyir's f32) holds it.
def f32(value: float) -> float:
    return struct.unpack("f", struct.pack("f", value))[0]

# A whole number brought back into the signed 64-bit range, as i64 arithmetic wraps.
def i64(value: int) -> int:
    return (value + 2**63) % 2**64 - 2**63

from math import sin
from math import cos
from math import sqrt

def off(got: float, expected: float) -> float:
    return f32(got - expected)

def off_int(got: int, expected: int) -> float:
    return float(i64(got - expected))

def off_relative(got: float, expected: float) -> float:
    return f32(f32(got - expected) / expected)

def amplitude_at(data: list[float], start: int, count: int, rate: int, hz: float) -> float:
    re: float = 0.0
    im: float = 0.0
    k: int = 0
    while k < count:
        angle: float = 6.283185307179586 * hz * float(i64(start + k)) / float(rate)
        re = re + data[i64(start + k)] * cos(angle)
        im = im + data[i64(start + k)] * sin(angle)
        k = i64(k + 1)
    return f32(2.0 * sqrt(re * re + im * im) / float(count))
