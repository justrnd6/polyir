# Whole-number division and remainder that truncate toward zero, as polyir's do (Python's // and % round down).
def div(a: int, b: int) -> int:
    quotient = abs(a) // abs(b)
    return quotient if (a < 0) == (b < 0) else -quotient

def rem(a: int, b: int) -> int:
    return a - b * div(a, b)

# A whole number brought back into the signed 64-bit range, as i64 arithmetic wraps.
def i64(value: int) -> int:
    return (value + 2**63) % 2**64 - 2**63

def gcd(a: int, b: int) -> int:
    x: int = a
    y: int = b
    while y != 0:
        r: int = rem(x, y)
        x = y
        y = r
    return x

def lcm(a: int, b: int) -> int:
    return i64(div(a, gcd(a, b)) * b)
