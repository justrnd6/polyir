# A whole number brought back into the unsigned 32-bit range, as u32 arithmetic wraps.
def u32(value: int) -> int:
    return value % 2**32
