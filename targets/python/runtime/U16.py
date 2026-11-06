# A whole number brought back into the unsigned 16-bit range, as u16 arithmetic wraps.
def u16(value: int) -> int:
    return value % 2**16
