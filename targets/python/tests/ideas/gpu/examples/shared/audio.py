import math
import struct

from Math import sign
from Wave import fade, sine

# A float rounded to 32 bits, as a `float` (polyir's f32) holds it.
def f32(value: float) -> float:
    return struct.unpack("f", struct.pack("f", value))[0]

# A whole number brought back into the signed 32-bit range, as i32 arithmetic wraps.
def i32(value: int) -> int:
    return (value + 2**31) % 2**32 - 2**31

# A whole number brought back into the signed 64-bit range, as i64 arithmetic wraps.
def i64(value: int) -> int:
    return (value + 2**63) % 2**64 - 2**63

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

def synth(i: int, p: list[float]) -> float:
    t: float = f32(f32(i) / p[0])
    envelope: float = fade(t, p[2], p[3], p[4])
    wobble: float = f32(p[6] * sine(f32(p[5] * t)))
    sum_: float = 0.0
    k: int = 0
    while k < i32(int(p[1])):
        cycles: float = f32(p[i64(8 + i64(2 * k))] * t)
        sum_ = f32(sum_ + f32(p[i64(9 + i64(2 * k))] * sinf(f32(6.2831853 * f32(cycles - floorf(cycles)) + wobble))))
        k = i64(k + 1)
    return f32(f32(p[7] * envelope) * sum_)

def square_wave(i: int, p: list[float]) -> float:
    return sign(sine(f32(f32(p[1] * f32(i)) / p[0])))

def partial_sum(i: int, p: list[float]) -> float:
    x: float = f32(6.2831853 * f32(rem(i, i32(int(p[0])))) / p[0])
    sum_: float = 0.0
    h: int = 1
    while h <= i32(int(p[2])):
        number: float = f32(h)
        wave: float = sinf(f32(number * x))
        if p[1] < 0.5:
            if rem(h, 2) == 1:
                sum_ = f32(sum_ + f32(wave / number))
        if p[1] > 0.5 and p[1] < 1.5:
            if rem(h, 2) == 1:
                sum_ = f32(sum_ + f32(wave / number))
            if rem(h, 2) == 0:
                sum_ = f32(sum_ - f32(wave / number))
        if p[1] > 1.5:
            if rem(h, 4) == 1:
                sum_ = f32(sum_ + f32(wave / f32(number * number)))
            if rem(h, 4) == 3:
                sum_ = f32(sum_ - f32(wave / f32(number * number)))
        h = i64(h + 1)
    if p[1] < 0.5:
        return f32(1.2732395 * sum_)
    if p[1] < 1.5:
        return f32(0.6366198 * sum_)
    return f32(0.8105695 * sum_)

def melody(i: int, notes: list[float], p: list[float]) -> float:
    t: float = f32(f32(i) / p[0])
    sum_: float = 0.0
    n: int = 0
    while n < i32(int(p[1])):
        start: float = notes[i64(3 * n)]
        length: float = notes[i64(i64(3 * n) + 1)]
        local: float = f32(t - start)
        if local >= 0.0 and local < length:
            envelope: float = fade(local, 0.02, 0.08, length)
            f: float = notes[i64(i64(3 * n) + 2)]
            sum_ = f32(sum_ + f32(envelope * f32(f32(sine(f32(f * t)) + f32(0.5 * sine(f32(2.0 * f * t)))) + f32(0.25 * sine(f32(3.0 * f * t))))))
        n = i64(n + 1)
    return f32(p[2] * sum_)
