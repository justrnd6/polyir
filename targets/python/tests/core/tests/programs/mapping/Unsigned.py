from __future__ import annotations
from dataclasses import dataclass

# Whole-number division and remainder that truncate toward zero, as polyir's do (Python's // and % round down).
def div(a: int, b: int) -> int:
    quotient = abs(a) // abs(b)
    return quotient if (a < 0) == (b < 0) else -quotient

def rem(a: int, b: int) -> int:
    return a - b * div(a, b)

# A whole number brought back into the unsigned 64-bit range, as u64 arithmetic wraps.
def u64(value: int) -> int:
    return value % 2**64

# A whole number brought back into the signed 64-bit range, as i64 arithmetic wraps.
def i64(value: int) -> int:
    return (value + 2**63) % 2**64 - 2**63

# A whole number brought back into the unsigned 8-bit range, as u8 arithmetic wraps.
def u8(value: int) -> int:
    return value % 2**8

# A whole number brought back into the unsigned 16-bit range, as u16 arithmetic wraps.
def u16(value: int) -> int:
    return value % 2**16

# A whole number brought back into the unsigned 32-bit range, as u32 arithmetic wraps.
def u32(value: int) -> int:
    return value % 2**32

@dataclass
class Pixel:
    r: int
    g: int
    b: int

def comparison() -> int:
    big: int = 4000000000
    if big > 3000000000 and big >= big and (big < 5) == False:
        return 1
    return 0

def division() -> int:
    big: int = 4000000000
    half: int = div(big, 2)
    rest: int = rem(big, 7)
    if half == 2000000000 and rest == 3:
        return 1
    return 0

def shifting() -> int:
    big: int = 4000000000
    top: int = u64(i64(0 - 1))
    if big >> 28 == 14 and i64(top >> 63) == 1:
        return 1
    return 0

def widening() -> int:
    big: int = 4000000000
    small: int = 200
    if big == 4000000000 and small == 200:
        return 1
    return 0

def wrapping() -> int:
    a: int = 200
    b: int = 100
    sum_: int = u8(a + b)
    under: int = u8(b - a)
    product: int = 300
    wide: int = u16(product * 300)
    if sum_ == 44 and under == 156 and wide == 24464:
        return 1
    return 0

def floats() -> int:
    big: int = 4000000000
    if i64(int(float(big))) == 4000000000:
        return 1
    return 0

def largest() -> int:
    top: int = u64(i64(0 - 1))
    if top > 5 and div(top, 2) == 9223372036854775807:
        return 1
    return 0

def fnv(n: int) -> int:
    h: int = 2166136261
    i: int = 0
    while i < n:
        h = u32((h ^ u32(i)) * 16777619)
        i = i64(i + 1)
    return h

def hashing() -> int:
    if fnv(3) == 581859880:
        return 1
    return 0

def fields() -> int:
    a: Pixel = Pixel(250, 128, 7)
    b: Pixel = Pixel(250, 128, 7)
    if a == b and a.g == 128 and a.r > a.b:
        return 1
    return 0

def keys() -> int:
    m: dict[int, int] = {}
    k: int = 4000000000
    m[k] = 7
    m[5] = 9
    if m.get(k, 0) == 7 and m.get(5, 0) == 9:
        return 1
    return 0

def main() -> int:
    return i64(i64(i64(i64(i64(i64(i64(i64(i64(comparison() + division()) + shifting()) + widening()) + wrapping()) + floats()) + largest()) + hashing()) + fields()) + keys())

if __name__ == "__main__":
    raise SystemExit(main())
