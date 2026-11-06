import struct

# A whole number brought back into the signed 64-bit range, as i64 arithmetic wraps.
def i64(value: int) -> int:
    return (value + 2**63) % 2**64 - 2**63

# A float rounded to 32 bits, as a `float` (polyir's f32) holds it.
def f32(value: float) -> float:
    return struct.unpack("f", struct.pack("f", value))[0]

# Whole-number division and remainder that truncate toward zero, as polyir's do (Python's // and % round down).
def div(a: int, b: int) -> int:
    quotient = abs(a) // abs(b)
    return quotient if (a < 0) == (b < 0) else -quotient

def rem(a: int, b: int) -> int:
    return a - b * div(a, b)

def draw_stems(pic: list[float], w: int, h: int, values: list[float], n: int, low: float, high: float) -> int:
    k: int = 0
    while k < i64(w * h):
        pic[k] = 0.96
        k = i64(k + 1)
    zero: int = i64(int(f32(f32(high / f32(high - low)) * f32(h))))
    x: int = 0
    while x < w:
        pic[i64(i64(zero * w) + x)] = 0.55
        x = i64(x + 1)
    j: int = 0
    while j < n:
        column: int = i64(int(f32(f32(f32(f32(j) + 0.5) * f32(w)) / f32(n))))
        top: int = i64(int(f32(f32(f32(high - values[j]) / f32(high - low)) * f32(i64(h - 1)))))
        if top < 0:
            top = 0
        if top > i64(h - 1):
            top = i64(h - 1)
        from_: int = min_of(top, zero)
        to: int = max_of(top, zero)
        y: int = from_
        while y <= to:
            pic[i64(i64(y * w) + column)] = 0.1
            y = i64(y + 1)
        dy: int = i64(0 - 1)
        while dy <= 1:
            dx: int = i64(0 - 1)
            while dx <= 1:
                yy: int = i64(top + dy)
                xx: int = i64(column + dx)
                if yy >= 0 and yy < h and xx >= 0 and xx < w:
                    pic[i64(i64(yy * w) + xx)] = 0.1
                dx = i64(dx + 1)
            dy = i64(dy + 1)
        j = i64(j + 1)
    return 0

def min_of(a: int, b: int) -> int:
    if a < b:
        return a
    return b

def max_of(a: int, b: int) -> int:
    if a > b:
        return a
    return b

def draw_wave(pic: list[float], w: int, h: int, values: list[float], n: int, low: float, high: float) -> int:
    k: int = 0
    while k < i64(w * h):
        pic[k] = 0.96
        k = i64(k + 1)
    zero: int = i64(int(f32(f32(high / f32(high - low)) * f32(h))))
    x: int = 0
    while x < w:
        pic[i64(i64(zero * w) + x)] = 0.55
        x = i64(x + 1)
    x: int = 0
    while x < w:
        from_: int = div(i64(x * n), w)
        upto: int = div(i64(i64(x + 1) * n), w)
        if upto <= from_:
            upto = i64(from_ + 1)
        lowest: float = values[from_]
        highest: float = values[from_]
        j: int = from_
        while j < upto:
            if values[j] < lowest:
                lowest = values[j]
            if values[j] > highest:
                highest = values[j]
            j = i64(j + 1)
        top: int = i64(int(f32(f32(f32(high - highest) / f32(high - low)) * f32(i64(h - 1)))))
        bottom: int = i64(int(f32(f32(f32(high - lowest) / f32(high - low)) * f32(i64(h - 1)))))
        if top < 0:
            top = 0
        if bottom > i64(h - 1):
            bottom = i64(h - 1)
        y: int = top
        while y <= bottom:
            pic[i64(i64(y * w) + x)] = 0.1
            y = i64(y + 1)
        x = i64(x + 1)
    return 0

def draw_curve(pic: list[float], w: int, h: int, values: list[float], n: int, low: float, high: float) -> int:
    k: int = 0
    while k < i64(w * h):
        pic[k] = 0.96
        k = i64(k + 1)
    zero: int = i64(int(f32(f32(high / f32(high - low)) * f32(h))))
    x: int = 0
    while x < w:
        pic[i64(i64(zero * w) + x)] = 0.55
        x = i64(x + 1)
    previous: int = curve_row(values[0], h, low, high)
    x: int = 0
    while x < w:
        row: int = curve_row(values[div(i64(x * n), w)], h, low, high)
        y: int = min_of(row, previous)
        while y <= max_of(row, previous):
            pic[i64(i64(y * w) + x)] = 0.1
            y = i64(y + 1)
        previous = row
        x = i64(x + 1)
    return 0

def curve_row(value: float, h: int, low: float, high: float) -> int:
    row: int = i64(int(f32(f32(f32(high - value) / f32(high - low)) * f32(i64(h - 1)))))
    if row < 0:
        row = 0
    if row > i64(h - 1):
        row = i64(h - 1)
    return row
