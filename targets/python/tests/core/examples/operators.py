# A whole number brought back into the signed 64-bit range, as i64 arithmetic wraps.
def i64(value: int) -> int:
    return (value + 2**63) % 2**64 - 2**63

def bitwise_precedence_check() -> int:
    a: int = 6
    b: int = 3
    c: int = 1
    d: int = 2
    return a | b & c ^ d

def shift_precedence_check() -> int:
    x: int = 1
    return i64(x << 2) & 3
