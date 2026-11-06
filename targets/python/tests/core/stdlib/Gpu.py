import struct

# A whole number brought back into the signed 32-bit range, as i32 arithmetic wraps.
def i32(value: int) -> int:
    return (value + 2**31) % 2**32 - 2**31

# A float rounded to 32 bits, as a `float` (polyir's f32) holds it.
def f32(value: float) -> float:
    return struct.unpack("f", struct.pack("f", value))[0]

def atomic_add(counts: list[int], slot: int, amount: int) -> int:
    old: int = counts[slot]
    counts[slot] = i32(old + amount)
    return old

def atomic_add_float(sums: list[float], slot: int, amount: float) -> float:
    old: float = sums[slot]
    sums[slot] = f32(old + amount)
    return old

def atomic_min(values: list[int], slot: int, candidate: int) -> int:
    old: int = values[slot]
    if candidate < old:
        values[slot] = candidate
    return old

def atomic_max(values: list[int], slot: int, candidate: int) -> int:
    old: int = values[slot]
    if candidate > old:
        values[slot] = candidate
    return old

def group_size() -> int:
    return 1

def local_index(i: int) -> int:
    return 0

def group_index(i: int) -> int:
    return i

def barrier() -> int:
    return 0
