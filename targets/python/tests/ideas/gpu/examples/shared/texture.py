import math
import struct

# A float rounded to 32 bits, as a `float` (polyir's f32) holds it.
def f32(value: float) -> float:
    return struct.unpack("f", struct.pack("f", value))[0]

# A whole number brought back into the signed 32-bit range, as i32 arithmetic wraps.
def i32(value: int) -> int:
    return (value + 2**31) % 2**32 - 2**31

# Whole-number division and remainder that truncate toward zero, as polyir's do (Python's // and % round down).
def div(a: int, b: int) -> int:
    quotient = abs(a) // abs(b)
    return quotient if (a < 0) == (b < 0) else -quotient

def rem(a: int, b: int) -> int:
    return a - b * div(a, b)

# A whole number brought back into the signed 64-bit range, as i64 arithmetic wraps.
def i64(value: int) -> int:
    return (value + 2**63) % 2**64 - 2**63

def floorf(x: float) -> float:
    return f32(math.floor(x))

def wrap(n: int, size: int, mode: int) -> int:
    if mode == 0:
        if n < 0:
            return 0
        if n > i32(size - 1):
            return i32(size - 1)
        return n
    if mode == 1:
        return rem(i32(rem(n, size) + size), size)
    period: int = i32(size * 2)
    m: int = rem(i32(rem(n, period) + period), period)
    if m >= size:
        return i32(i32(period - 1) - m)
    return m

def texel(tex: list[float], offset: int, size: int, x: int, y: int, mode: int) -> float:
    return tex[i32(i32(offset + i32(wrap(y, size, mode) * size)) + wrap(x, size, mode))]

def nearest(tex: list[float], offset: int, size: int, u: float, v: float, mode: int) -> float:
    x: int = i32(int(floorf(f32(u * f32(size)))))
    y: int = i32(int(floorf(f32(v * f32(size)))))
    return texel(tex, offset, size, x, y, mode)

def lerp(a: float, b: float, t: float) -> float:
    return f32(a + f32(f32(b - a) * t))

def bilinear(tex: list[float], offset: int, size: int, u: float, v: float, mode: int) -> float:
    sx: float = f32(f32(u * f32(size)) - 0.5)
    sy: float = f32(f32(v * f32(size)) - 0.5)
    x0: int = i32(int(floorf(sx)))
    y0: int = i32(int(floorf(sy)))
    fx: float = f32(sx - f32(x0))
    fy: float = f32(sy - f32(y0))
    top: float = lerp(texel(tex, offset, size, x0, y0, mode), texel(tex, offset, size, i32(x0 + 1), y0, mode), fx)
    bottom: float = lerp(texel(tex, offset, size, x0, i32(y0 + 1), mode), texel(tex, offset, size, i32(x0 + 1), i32(y0 + 1), mode), fx)
    return lerp(top, bottom, fy)

def level_offset(base: int, level: int) -> int:
    offset: int = 0
    size: int = base
    k: int = 0
    while k < level:
        offset = i32(offset + i32(size * size))
        size = div(size, 2)
        k = i64(k + 1)
    return offset

def level_size(base: int, level: int) -> int:
    size: int = base
    k: int = 0
    while k < level:
        size = div(size, 2)
        k = i64(k + 1)
    return size

def mip_sample(tex: list[float], base: int, levels: int, u: float, v: float, level: float, mode: int) -> float:
    lo: int = i32(int(floorf(level)))
    if lo < 0:
        lo = 0
    if lo > i32(levels - 1):
        lo = i32(levels - 1)
    hi: int = i32(lo + 1)
    if hi > i32(levels - 1):
        hi = i32(levels - 1)
    amount: float = f32(level - f32(lo))
    if amount < 0.0:
        amount = 0.0
    if amount > 1.0:
        amount = 1.0
    a: float = bilinear(tex, level_offset(base, lo), level_size(base, lo), u, v, mode)
    b: float = bilinear(tex, level_offset(base, hi), level_size(base, hi), u, v, mode)
    return lerp(a, b, amount)

def pyramid_size(base: int) -> int:
    total: int = 0
    size: int = base
    k: int = 0
    while k < 16:
        if size >= 1:
            total = i64(total + i64(size * size))
            size = div(size, 2)
        k = i64(k + 1)
    return total

def fill_base(tex: list[float], base: int) -> int:
    y: int = 0
    while y < base:
        x: int = 0
        while x < base:
            level: float = 0.45 if rem(i64(div(x, 4) + div(y, 4)), 2) == 1 else 0.85
            if rem(x, 4) == 0 or rem(y, 4) == 0:
                level = f32(level * 0.6)
            tex[i64(i64(y * base) + x)] = level
            x = i64(x + 1)
        y = i64(y + 1)
    return 0

def build_pyramid(tex: list[float], base: int) -> int:
    fill_base(tex, base)
    from_: int = 0
    size: int = base
    level: int = 0
    while level < 16:
        if size > 1:
            shrink(tex, from_, size)
            from_ = i64(from_ + i64(size * size))
            size = div(size, 2)
        level = i64(level + 1)
    return 0

def shrink(tex: list[float], from_: int, size: int) -> int:
    next_: int = i64(from_ + i64(size * size))
    half: int = div(size, 2)
    y: int = 0
    while y < half:
        x: int = 0
        while x < half:
            a: float = tex[i64(i64(from_ + i64(i64(2 * y) * size)) + i64(2 * x))]
            b: float = tex[i64(i64(i64(from_ + i64(i64(2 * y) * size)) + i64(2 * x)) + 1)]
            c: float = tex[i64(i64(from_ + i64(i64(i64(2 * y) + 1) * size)) + i64(2 * x))]
            d: float = tex[i64(i64(i64(from_ + i64(i64(i64(2 * y) + 1) * size)) + i64(2 * x)) + 1)]
            tex[i64(i64(next_ + i64(y * half)) + x)] = f32(f32(f32(f32(a + b) + c) + d) * 0.25)
            x = i64(x + 1)
        y = i64(y + 1)
    return 0
