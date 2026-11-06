# A whole number brought back into the unsigned 8-bit range, as u8 arithmetic wraps.
def u8(value: int) -> int:
    return value % 2**8
