from dataclasses import dataclass

# An `either` holds its value as an `Ok` or an `Err`.
@dataclass
class Ok:
    value: object

@dataclass
class Err:
    value: object

# Whole-number division and remainder that truncate toward zero, as polyir's do (Python's // and % round down).
def div(a: int, b: int) -> int:
    quotient = abs(a) // abs(b)
    return quotient if (a < 0) == (b < 0) else -quotient

def rem(a: int, b: int) -> int:
    return a - b * div(a, b)

# A whole number brought back into the signed 64-bit range, as i64 arithmetic wraps.
def i64(value: int) -> int:
    return (value + 2**63) % 2**64 - 2**63

# A variant: the number of its case and the value it holds.
@dataclass
class Variant:
    tag: int
    value: object

def safe_divide(a: int, b: int) -> Ok | Err:
    if b == 0:
        return Err(99)
    return Ok(div(a, b))

def divide_or_default(a: int, b: int) -> int:
    r: Ok | Err = safe_divide(a, b)
    if isinstance(r, Err):
        return r.value
    return r.value

def either_coercion() -> int:
    r: Ok | Err = Ok(3.0)
    return i64(int(r.value))

def variant_sum(which: int) -> int:
    v: Variant = Variant(0, 0)
    if which == 0:
        v = Variant(0, 10)
    if which == 1:
        v = Variant(1, 2.5)
    if which == 2:
        v = Variant(2, 30)
    if v.tag == 0:
        return v.value
    if v.tag == 1:
        return i64(i64(int(v.value)) * 100)
    return i64(v.value * 1000)

def cast_round_trip() -> int:
    d: float = 9.0
    as_i64: int = i64(int(d))
    back_to_d: float = float(as_i64)
    return i64(i64(int(back_to_d)) + as_i64)
