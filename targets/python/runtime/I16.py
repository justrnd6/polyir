# A whole number brought back into the signed 16-bit range, as i16 arithmetic wraps.
def i16(value: int) -> int:
    return (value + 2**15) % 2**16 - 2**15
