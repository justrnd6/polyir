# A whole number brought back into the unsigned 64-bit range, as u64 arithmetic wraps.
def u64(value: int) -> int:
    return value % 2**64
