from Metal import metal_buffer, metal_dispatch, metal_empty_buffer

# A whole number brought back into the signed 64-bit range, as i64 arithmetic wraps.
def i64(value: int) -> int:
    return (value + 2**63) % 2**64 - 2**63

# Whole-number division and remainder that truncate toward zero, as polyir's do (Python's // and % round down).
def div(a: int, b: int) -> int:
    quotient = abs(a) // abs(b)
    return quotient if (a < 0) == (b < 0) else -quotient

def rem(a: int, b: int) -> int:
    return a - b * div(a, b)

# A whole number brought back into the signed 32-bit range, as i32 arithmetic wraps.
def i32(value: int) -> int:
    return (value + 2**31) % 2**32 - 2**31

def fft_stages_gpu(device: int, stage: int, first: int, second: int, total: int, batch: int, direction: int) -> int:
    source: int = first
    target: int = second
    scratch: int = metal_empty_buffer(device, i64(i64(batch * div(total, 2)) * 4))
    n: int = total
    s: int = 1
    pass_: int = 0
    while pass_ < 30:
        if n > 1:
            settings: list[int] = [0 for _ in range(4)]
            settings[0] = i32(total)
            settings[1] = i32(n)
            settings[2] = i32(s)
            settings[3] = i32(direction)
            if metal_dispatch(device, stage, i64(batch * div(total, 2)), source, metal_buffer(device, i64(settings), 16), target, scratch) == 0:
                return 0
            swap: int = source
            source = target
            target = swap
            n = div(n, 2)
            s = i64(s * 2)
        pass_ = i64(pass_ + 1)
    return source
