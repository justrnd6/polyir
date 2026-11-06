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

def fmaxf(x: float, y: float) -> float:
    return f32(max(x, y))

def atomic_add_float(sums: list[float], slot: int, amount: float) -> float:
    old: float = sums[slot]
    sums[slot] = f32(old + amount)
    return old

def group_size() -> int:
    return 1

def local_index(i: int) -> int:
    return 0

def group_index(i: int) -> int:
    return i

def barrier() -> int:
    return 0

def block_sum(i: int, src: list[float], n: int, tile: list[float], partial: list[float]) -> int:
    lane: int = local_index(i)
    if i < n:
        tile[lane] = src[i]
    else:
        tile[lane] = 0.0
    barrier()
    s: int = div(group_size(), 2)
    while s > 0:
        if lane < s:
            tile[lane] = f32(tile[lane] + tile[i32(lane + s)])
        barrier()
        s = div(s, 2)
    if lane == 0:
        partial[group_index(i)] = tile[0]
    return 0

def block_max(i: int, src: list[float], n: int, tile: list[float], partial: list[float]) -> int:
    lane: int = local_index(i)
    if i < n:
        tile[lane] = src[i]
    else:
        tile[lane] = src[0]
    barrier()
    s: int = div(group_size(), 2)
    while s > 0:
        if lane < s:
            tile[lane] = fmaxf(tile[lane], tile[i32(lane + s)])
        barrier()
        s = div(s, 2)
    if lane == 0:
        partial[group_index(i)] = tile[0]
    return 0

def total(i: int, src: list[float], n: int, tile: list[float], sum_: list[float]) -> int:
    lane: int = local_index(i)
    if i < n:
        tile[lane] = src[i]
    else:
        tile[lane] = 0.0
    barrier()
    s: int = div(group_size(), 2)
    while s > 0:
        if lane < s:
            tile[lane] = f32(tile[lane] + tile[i32(lane + s)])
        barrier()
        s = div(s, 2)
    if lane == 0:
        atomic_add_float(sum_, 0, tile[0])
    return 0

def groups_for(n: int) -> int:
    return div(i64(n + 255), 256)

def launch_count(n: int) -> int:
    launches: int = 0
    left: int = n
    k: int = 0
    while k < 10:
        if left > 1:
            left = groups_for(left)
            launches = i64(launches + 1)
        k = i64(k + 1)
    return launches
