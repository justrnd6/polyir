# A whole number brought back into the signed 64-bit range, as i64 arithmetic wraps.
def i64(value: int) -> int:
    return (value + 2**63) % 2**64 - 2**63

def classify(x: int) -> int:
    return i64(0 - 1) if x < 0 else (0 if x == 0 else 1)

def max_val(a: int, b: int) -> int:
    result: int = 0
    if a < b:
        result = b
    else:
        result = a
    return result

def abs_via_if(x: int) -> int:
    r: int = x
    if r < 0:
        r = i64(0 - r)
    return r
