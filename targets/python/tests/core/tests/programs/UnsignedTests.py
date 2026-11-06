from __future__ import annotations
from dataclasses import dataclass

from Check import off_int

# A whole number brought back into the signed 64-bit range, as i64 arithmetic wraps.
def i64(value: int) -> int:
    return (value + 2**63) % 2**64 - 2**63

# Whole-number division and remainder that truncate toward zero, as polyir's do (Python's // and % round down).
def div(a: int, b: int) -> int:
    quotient = abs(a) // abs(b)
    return quotient if (a < 0) == (b < 0) else -quotient

def rem(a: int, b: int) -> int:
    return a - b * div(a, b)

# A whole number brought back into the unsigned 64-bit range, as u64 arithmetic wraps.
def u64(value: int) -> int:
    return value % 2**64

# A whole number brought back into the unsigned 8-bit range, as u8 arithmetic wraps.
def u8(value: int) -> int:
    return value % 2**8

def check_an_unsigned_comparison_does_not_see_the_top_bit_as_a_sign() -> float:
    big: int = 4000000000
    seen: int = 0
    if big > 3000000000:
        seen = i64(seen + 1)
    if big >= big:
        seen = i64(seen + 10)
    if big < 5:
        seen = i64(seen + 100)
    return off_int(seen, 11)

def check_an_unsigned_division_and_remainder() -> float:
    big: int = 4000000000
    half: int = div(big, 2)
    rest: int = rem(big, 7)
    return off_int(half, 2000000000) + off_int(rest, 3)

def check_an_unsigned_right_shift_fills_with_zeros() -> float:
    big: int = 4000000000
    shifted: int = big >> 28
    m: int = u64(i64(0 - 1))
    return off_int(shifted, 14) + off_int(i64(m >> 63), 1)

def check_an_unsigned_widens_with_zero_bits() -> float:
    big: int = 4000000000
    small: int = 200
    return off_int(big, 4000000000) + off_int(small, 200)

def check_narrow_unsigned_arithmetic_wraps_at_its_size() -> float:
    b: int = 200
    c: int = 100
    sum_: int = u8(b + c)
    under: int = u8(c - b)
    return off_int(sum_, 44) + off_int(under, 156)

def check_an_unsigned_goes_to_a_float_as_its_value() -> float:
    big: int = 4000000000
    as_double: float = float(big)
    return off_int(i64(int(as_double)), 4000000000)

def check_the_largest_u64_is_larger_than_five() -> float:
    m: int = u64(i64(0 - 1))
    seen: int = 0
    if m > 5:
        seen = i64(seen + 1)
    if div(m, 2) == 9223372036854775807:
        seen = i64(seen + 10)
    return off_int(seen, 11)

@dataclass
class Pixel:
    r: int
    g: int
    b: int

def check_an_unsigned_field_keeps_its_value_and_structs_compare() -> float:
    a: Pixel = Pixel(250, 128, 7)
    b: Pixel = Pixel(250, 128, 7)
    seen: int = 0
    if a == b:
        seen = i64(seen + 1)
    seen = i64(seen + a.g)
    return off_int(seen, 129)

def check_unsigned_keys_are_ordinary_keys() -> float:
    m: dict[int, int] = {}
    k: int = 4000000000
    m[k] = 7
    m[5] = 9
    return off_int(m.get(k, 0), 7) + off_int(m.get(5, 0), 9)
