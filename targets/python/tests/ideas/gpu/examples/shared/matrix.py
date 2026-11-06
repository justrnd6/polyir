import struct

# A float rounded to 32 bits, as a `float` (polyir's f32) holds it.
def f32(value: float) -> float:
    return struct.unpack("f", struct.pack("f", value))[0]

# A whole number brought back into the signed 64-bit range, as i64 arithmetic wraps.
def i64(value: int) -> int:
    return (value + 2**63) % 2**64 - 2**63

def transform(p: list[float], m: list[float]) -> list[float]:
    return [f32(f32(f32(f32(m[0] * p[0]) + f32(m[1] * p[1])) + f32(m[2] * p[2])) + f32(m[3] * p[3])), f32(f32(f32(f32(m[4] * p[0]) + f32(m[5] * p[1])) + f32(m[6] * p[2])) + f32(m[7] * p[3])), f32(f32(f32(f32(m[8] * p[0]) + f32(m[9] * p[1])) + f32(m[10] * p[2])) + f32(m[11] * p[3])), f32(f32(f32(f32(m[12] * p[0]) + f32(m[13] * p[1])) + f32(m[14] * p[2])) + f32(m[15] * p[3]))]

def identity(m: list[float]) -> int:
    k: int = 0
    while k < 16:
        m[k] = 0.0
        k = i64(k + 1)
    m[0] = 1.0
    m[5] = 1.0
    m[10] = 1.0
    m[15] = 1.0
    return 0

def multiply4(a: list[float], b: list[float], out: list[float]) -> int:
    row: int = 0
    while row < 4:
        col: int = 0
        while col < 4:
            sum_: float = 0.0
            k: int = 0
            while k < 4:
                sum_ = f32(sum_ + f32(a[i64(i64(row * 4) + k)] * b[i64(i64(k * 4) + col)]))
                k = i64(k + 1)
            out[i64(i64(row * 4) + col)] = sum_
            col = i64(col + 1)
        row = i64(row + 1)
    return 0
