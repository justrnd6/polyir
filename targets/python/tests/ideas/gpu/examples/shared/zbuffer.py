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

def fminf(x: float, y: float) -> float:
    return f32(min(x, y))

def fmaxf(x: float, y: float) -> float:
    return f32(max(x, y))

def floorf(x: float) -> float:
    return f32(math.floor(x))

def ceilf(x: float) -> float:
    return f32(math.ceil(x))

def atomic_min(values: list[int], slot: int, candidate: int) -> int:
    old: int = values[slot]
    if candidate < old:
        values[slot] = candidate
    return old

def empty_key() -> int:
    return 2147483647

def rasterize(t: int, tris: list[float], width: int, height: int, depth: list[int]) -> int:
    base: int = i32(t * 10)
    x0: float = tris[base]
    y0: float = tris[i32(base + 1)]
    z0: float = tris[i32(base + 2)]
    x1: float = tris[i32(base + 3)]
    y1: float = tris[i32(base + 4)]
    z1: float = tris[i32(base + 5)]
    x2: float = tris[i32(base + 6)]
    y2: float = tris[i32(base + 7)]
    z2: float = tris[i32(base + 8)]
    area: float = f32(f32(f32(x1 - x0) * f32(y2 - y0)) - f32(f32(x2 - x0) * f32(y1 - y0)))
    left: int = i32(int(fmaxf(0.0, floorf(fminf(x0, fminf(x1, x2))))))
    right: int = i32(int(fminf(f32(f32(width) - 1.0), ceilf(fmaxf(x0, fmaxf(x1, x2))))))
    top: int = i32(int(fmaxf(0.0, floorf(fminf(y0, fminf(y1, y2))))))
    bottom: int = i32(int(fminf(f32(f32(height) - 1.0), ceilf(fmaxf(y0, fmaxf(y1, y2))))))
    y: int = top
    while y <= bottom:
        x: int = left
        while x <= right:
            px: float = f32(f32(x) + 0.5)
            py: float = f32(f32(y) + 0.5)
            w0: float = f32(f32(f32(f32(x1 - px) * f32(y2 - py)) - f32(f32(x2 - px) * f32(y1 - py))) / area)
            w1: float = f32(f32(f32(f32(x2 - px) * f32(y0 - py)) - f32(f32(x0 - px) * f32(y2 - py))) / area)
            w2: float = f32(1.0 - w0 - w1)
            if w0 >= 0.0 and w1 >= 0.0 and w2 >= 0.0:
                z: float = f32(f32(f32(w0 * z0) + f32(w1 * z1)) + f32(w2 * z2))
                atomic_min(depth, i32(i32(y * width) + x), i32(i32(i32(int(f32(z * 1048575.0))) * 512) + t))
            x = i32(x + 1)
        y = i32(y + 1)
    return 0

def shade(p: int, depth: list[int], tris: list[float]) -> float:
    key: int = depth[p]
    if key == 2147483647:
        return 0.06
    id: int = rem(key, 512)
    z: float = f32(f32(div(key, 512)) / 1048575.0)
    return f32(tris[i32(i32(id * 10) + 9)] * f32(1.0 - 0.45 * z))

def make_scene(tris: list[float], count: int, width: int, height: int) -> int:
    seed: int = 12345
    t: int = 0
    while t < count:
        cx: float = f32(next_random(seed) * f32(width))
        seed = next_seed(seed)
        cy: float = f32(next_random(seed) * f32(height))
        seed = next_seed(seed)
        size: float = f32(30.0 + f32(next_random(seed) * 110.0))
        seed = next_seed(seed)
        corner: int = 0
        while corner < 3:
            tris[i64(i64(t * 10) + i64(corner * 3))] = f32(cx + f32(f32(f32(next_random(seed) - 0.5) * 2.0) * size))
            seed = next_seed(seed)
            tris[i64(i64(i64(t * 10) + i64(corner * 3)) + 1)] = f32(cy + f32(f32(f32(next_random(seed) - 0.5) * 2.0) * size))
            seed = next_seed(seed)
            tris[i64(i64(i64(t * 10) + i64(corner * 3)) + 2)] = next_random(seed)
            seed = next_seed(seed)
            corner = i64(corner + 1)
        tris[i64(i64(t * 10) + 9)] = f32(0.25 + 0.75 * next_random(seed))
        seed = next_seed(seed)
        t = i64(t + 1)
    return 0

def next_seed(seed: int) -> int:
    return rem(i64(i64(seed * 1103515245) + 12345), 2147483648)

def next_random(seed: int) -> float:
    return f32(f32(rem(div(seed, 65536), 32768)) / 32768.0)
