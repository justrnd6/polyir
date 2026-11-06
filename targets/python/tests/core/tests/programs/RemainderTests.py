import math
import struct

from Check import off, off_int

# A float rounded to 32 bits, as a `float` (polyir's f32) holds it.
def f32(value: float) -> float:
    return struct.unpack("f", struct.pack("f", value))[0]

# Whole-number division and remainder that truncate toward zero, as polyir's do (Python's // and % round down).
def div(a: int, b: int) -> int:
    quotient = abs(a) // abs(b)
    return quotient if (a < 0) == (b < 0) else -quotient

def rem(a: int, b: int) -> int:
    return a - b * div(a, b)

# A whole number brought back into the signed 64-bit range, as i64 arithmetic wraps.
def i64(value: int) -> int:
    return (value + 2**63) % 2**64 - 2**63

def check_float_remainder() -> float:
    a: float = 7.5
    b: float = 2.0
    return off(math.fmod(a, b), 1.5)

def check_float_remainder_keeps_the_sign_of_the_left_number() -> float:
    a: float = f32(0.0 - 7.5)
    b: float = 2.0
    return off(math.fmod(a, b), f32(0.0 - 1.5))

def check_float_remainder_ignores_the_sign_of_the_right_number() -> float:
    a: float = 7.5
    b: float = f32(0.0 - 2.0)
    return off(math.fmod(a, b), 1.5)

def check_float_remainder_of_numbers_that_divide_evenly() -> float:
    b: float = 47.0
    c: float = 97.0
    return off(f32(math.fmod(2867.0, b)), 0.0) + off(f32(math.fmod(5969.0, b)), 0.0) + off(f32(math.fmod(9409.0, c)), 0.0)

def check_float_remainder_of_zero() -> float:
    a: float = 0.0
    b: float = 5.0
    return off(math.fmod(a, b), 0.0)

def check_float_remainder_of_a_fraction_of_one() -> float:
    a: float = 0.75
    b: float = 1.0
    return off(math.fmod(a, b), 0.75)

def check_whole_number_remainder() -> float:
    a: int = 17
    b: int = 5
    return off_int(rem(a, b), 2) + off_int(rem(i64(0 - a), b), i64(0 - 2))
