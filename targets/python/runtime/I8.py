# A whole number brought back into the signed 8-bit range, as i8 arithmetic wraps.
def i8(value: int) -> int:
    return (value + 2**7) % 2**8 - 2**7
