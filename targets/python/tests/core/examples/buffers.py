# A whole number brought back into the signed 64-bit range, as i64 arithmetic wraps.
def i64(value: int) -> int:
    return (value + 2**63) % 2**64 - 2**63

def sum_buf(buf: list[float], len_: int) -> float:
    i: int = 0
    total: float = 0.0
    while i < len_:
        i = i64(i + 1)
        total = total + buf[i]
    return total
