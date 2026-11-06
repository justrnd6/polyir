from __future__ import annotations
import struct
from dataclasses import dataclass

# A float rounded to 32 bits, as a `float` (polyir's f32) holds it.
def f32(value: float) -> float:
    return struct.unpack("f", struct.pack("f", value))[0]

# A whole number brought back into the signed 64-bit range, as i64 arithmetic wraps.
def i64(value: int) -> int:
    return (value + 2**63) % 2**64 - 2**63

from math import sin
from math import cos
from math import pow
from math import sqrt

@dataclass
class Biquad:
    b0: float
    b1: float
    b2: float
    a1: float
    a2: float

    def run_(self, data: list[float], count: int) -> int:
        x1: float = 0.0
        x2: float = 0.0
        y1: float = 0.0
        y2: float = 0.0
        i: int = 0
        while i < count:
            x: float = data[i]
            y: float = self.b0 * x + self.b1 * x1 + self.b2 * x2 - self.a1 * y1 - self.a2 * y2
            x2 = x1
            x1 = x
            y2 = y1
            y1 = y
            data[i] = f32(y)
            i = i64(i + 1)
        return 0

    def gain(self, hz: float, rate: int) -> float:
        w: float = 6.283185307179586 * hz / float(rate)
        c1: float = cos(w)
        s1: float = sin(w)
        c2: float = cos(2.0 * w)
        s2: float = sin(2.0 * w)
        top_real: float = self.b0 + self.b1 * c1 + self.b2 * c2
        top_imag: float = 0.0 - (self.b1 * s1 + self.b2 * s2)
        bottom_real: float = 1.0 + self.a1 * c1 + self.a2 * c2
        bottom_imag: float = 0.0 - (self.a1 * s1 + self.a2 * s2)
        strength: float = (top_real * top_real + top_imag * top_imag) / (bottom_real * bottom_real + bottom_imag * bottom_imag)
        return f32(sqrt(strength))

def biquad_from(b0: float, b1: float, b2: float, a0: float, a1: float, a2: float) -> Biquad:
    return Biquad(b0 / a0, b1 / a0, b2 / a0, a1 / a0, a2 / a0)

def biquad_angle(rate: int, hz: float) -> float:
    return 6.283185307179586 * hz / float(rate)

def biquad_low_pass(rate: int, hz: float, q: float) -> Biquad:
    w: float = biquad_angle(rate, hz)
    c: float = cos(w)
    alpha: float = sin(w) / (2.0 * q)
    return biquad_from((1.0 - c) / 2.0, 1.0 - c, (1.0 - c) / 2.0, 1.0 + alpha, 0.0 - 2.0 * c, 1.0 - alpha)

def biquad_high_pass(rate: int, hz: float, q: float) -> Biquad:
    w: float = biquad_angle(rate, hz)
    c: float = cos(w)
    alpha: float = sin(w) / (2.0 * q)
    return biquad_from((1.0 + c) / 2.0, 0.0 - (1.0 + c), (1.0 + c) / 2.0, 1.0 + alpha, 0.0 - 2.0 * c, 1.0 - alpha)

def biquad_band_pass(rate: int, hz: float, q: float) -> Biquad:
    w: float = biquad_angle(rate, hz)
    c: float = cos(w)
    alpha: float = sin(w) / (2.0 * q)
    return biquad_from(alpha, 0.0, 0.0 - alpha, 1.0 + alpha, 0.0 - 2.0 * c, 1.0 - alpha)

def biquad_notch(rate: int, hz: float, q: float) -> Biquad:
    w: float = biquad_angle(rate, hz)
    c: float = cos(w)
    alpha: float = sin(w) / (2.0 * q)
    return biquad_from(1.0, 0.0 - 2.0 * c, 1.0, 1.0 + alpha, 0.0 - 2.0 * c, 1.0 - alpha)

def biquad_bell(rate: int, hz: float, q: float, gain_db: float) -> Biquad:
    w: float = biquad_angle(rate, hz)
    c: float = cos(w)
    alpha: float = sin(w) / (2.0 * q)
    a: float = pow(10.0, gain_db / 40.0)
    return biquad_from(1.0 + alpha * a, 0.0 - 2.0 * c, 1.0 - alpha * a, 1.0 + alpha / a, 0.0 - 2.0 * c, 1.0 - alpha / a)

def biquad_low_shelf(rate: int, hz: float, gain_db: float) -> Biquad:
    w: float = biquad_angle(rate, hz)
    c: float = cos(w)
    a: float = pow(10.0, gain_db / 40.0)
    beta: float = sqrt(2.0 * a) * sin(w)
    return biquad_from(a * (a + 1.0 - (a - 1.0) * c + beta), 2.0 * a * (a - 1.0 - (a + 1.0) * c), a * (a + 1.0 - (a - 1.0) * c - beta), a + 1.0 + (a - 1.0) * c + beta, 0.0 - 2.0 * (a - 1.0 + (a + 1.0) * c), a + 1.0 + (a - 1.0) * c - beta)

def biquad_high_shelf(rate: int, hz: float, gain_db: float) -> Biquad:
    w: float = biquad_angle(rate, hz)
    c: float = cos(w)
    a: float = pow(10.0, gain_db / 40.0)
    beta: float = sqrt(2.0 * a) * sin(w)
    return biquad_from(a * (a + 1.0 + (a - 1.0) * c + beta), 0.0 - 2.0 * a * (a - 1.0 + (a + 1.0) * c), a * (a + 1.0 + (a - 1.0) * c - beta), a + 1.0 - (a - 1.0) * c + beta, 2.0 * (a - 1.0 - (a + 1.0) * c), a + 1.0 - (a - 1.0) * c - beta)
