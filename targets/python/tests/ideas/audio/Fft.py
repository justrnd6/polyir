import math
import struct

# A float rounded to 32 bits, as a `float` (polyir's f32) holds it.
def f32(value: float) -> float:
    return struct.unpack("f", struct.pack("f", value))[0]

# Whole-number division and remainder that truncate toward zero, as polyir's do (Python's // and % round down).
def div(a: int, b: int) -> int:
    quotient = abs(a) // abs(b)
    return quotient if (a < 0) == (b < 0) else -quotient

def rem(a: int, b: int) -> int:
    return a - b * div(a, b)

# A whole number brought back into the signed 32-bit range, as i32 arithmetic wraps.
def i32(value: int) -> int:
    return (value + 2**31) % 2**32 - 2**31

# A whole number brought back into the signed 64-bit range, as i64 arithmetic wraps.
def i64(value: int) -> int:
    return (value + 2**63) % 2**64 - 2**63

def sqrtf(x: float) -> float:
    return f32(math.sqrt(x))

def sinf(x: float) -> float:
    return f32(math.sin(x))

def cosf(x: float) -> float:
    return f32(math.cos(x))

def logf(x: float) -> float:
    return f32(math.log(x))

def fft_stage(t: int, src: list[float], p: list[int], dst: list[float]) -> int:
    total: int = p[0]
    n: int = p[1]
    s: int = p[2]
    half: int = div(total, 2)
    batch: int = div(t, half)
    local: int = rem(t, half)
    position: int = div(local, s)
    column: int = rem(local, s)
    base: int = i32(i32(batch * total) * 2)
    a: int = i32(base + i32(i64(2 * i32(column + i32(s * position)))))
    b: int = i32(base + i32(i64(2 * i32(column + i32(s * i32(position + div(n, 2)))))))
    sum_re: float = f32(src[a] + src[b])
    sum_im: float = f32(src[i32(a + 1)] + src[i32(b + 1)])
    diff_re: float = f32(src[a] - src[b])
    diff_im: float = f32(src[i32(a + 1)] - src[i32(b + 1)])
    angle: float = f32((0.0 - 6.2831853) * f32(p[3]) * f32(position) / f32(n))
    twiddle_re: float = cosf(angle)
    twiddle_im: float = sinf(angle)
    first: int = i32(base + i32(i64(2 * i32(column + i32(s * i32(i64(2 * position)))))))
    second: int = i32(base + i32(i64(2 * i32(column + i32(s * i32(i64(i64(2 * position) + 1)))))))
    dst[first] = sum_re
    dst[i32(first + 1)] = sum_im
    dst[second] = f32(f32(diff_re * twiddle_re) - f32(diff_im * twiddle_im))
    dst[i32(second + 1)] = f32(f32(diff_re * twiddle_im) + f32(diff_im * twiddle_re))
    return 0

def fft_magnitude(k: int, data: list[float], p: list[int]) -> float:
    total: int = p[0]
    re: float = data[i64(2 * k)]
    im: float = data[i64(i64(2 * k) + 1)]
    return f32(2.0 * sqrtf(f32(f32(re * re) + f32(im * im))) / f32(total))

def fft_frames(t: int, signal: list[float], p: list[int], dst: list[float]) -> int:
    total: int = p[0]
    frame: int = div(t, total)
    k: int = rem(t, total)
    window: float = f32(0.5 - 0.5 * cosf(f32(6.2831853 * f32(k) / f32(i32(total - 1)))))
    dst[i64(2 * t)] = f32(signal[i32(i32(frame * p[1]) + k)] * window)
    dst[i64(i64(2 * t) + 1)] = 0.0
    return 0

def spectrogram_pixel(i: int, data: list[float], p: list[int]) -> float:
    total: int = p[0]
    frames: int = p[1]
    rows: int = p[2]
    x: int = rem(i, frames)
    bin: int = i32(i32(rows - 1) - div(i, frames))
    at: int = i32(i32(i32(x * total) * 2) + i32(i64(2 * bin)))
    re: float = data[at]
    im: float = data[i32(at + 1)]
    magnitude: float = f32(4.0 * sqrtf(f32(f32(re * re) + f32(im * im))) / f32(total))
    level: float = f32((20.0 * logf(f32(magnitude + 0.0000001)) / 2.3025851 + 70.0) / 70.0)
    if level < 0.0:
        level = 0.0
    if level > 1.0:
        level = 1.0
    return level

def complex_multiply(k: int, a: list[float], b: list[float], out: list[float]) -> int:
    out[i64(2 * k)] = f32(f32(a[i64(2 * k)] * b[i64(2 * k)]) - f32(a[i64(i64(2 * k) + 1)] * b[i64(i64(2 * k) + 1)]))
    out[i64(i64(2 * k) + 1)] = f32(f32(a[i64(2 * k)] * b[i64(i64(2 * k) + 1)]) + f32(a[i64(i64(2 * k) + 1)] * b[i64(2 * k)]))
    return 0

def real_scaled(i: int, data: list[float], p: list[float]) -> float:
    return f32(data[i64(2 * i)] / p[0])
