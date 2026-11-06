import struct

# A whole number brought back into the signed 32-bit range, as i32 arithmetic wraps.
def i32(value: int) -> int:
    return (value + 2**31) % 2**32 - 2**31

# A float rounded to 32 bits, as a `float` (polyir's f32) holds it.
def f32(value: float) -> float:
    return struct.unpack("f", struct.pack("f", value))[0]

def convolve_sample(i: int, x: list[float], h: list[float], p: list[int]) -> float:
    nx: int = p[0]
    nh: int = p[1]
    first: int = i32(i - i32(nx - 1))
    if first < 0:
        first = 0
    last: int = i
    if last > i32(nh - 1):
        last = i32(nh - 1)
    sum_: float = 0.0
    j: int = first
    while j <= last:
        sum_ = f32(sum_ + f32(x[i32(i - j)] * h[j]))
        j = i32(j + 1)
    return sum_
