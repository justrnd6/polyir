# Whole-number division and remainder that truncate toward zero, as polyir's do (Python's // and % round down).
def div(a: int, b: int) -> int:
    quotient = abs(a) // abs(b)
    return quotient if (a < 0) == (b < 0) else -quotient

def rem(a: int, b: int) -> int:
    return a - b * div(a, b)

# A whole number brought back into the signed 64-bit range, as i64 arithmetic wraps.
def i64(value: int) -> int:
    return (value + 2**63) % 2**64 - 2**63

# A whole number brought back into the signed 32-bit range, as i32 arithmetic wraps.
def i32(value: int) -> int:
    return (value + 2**31) % 2**32 - 2**31

# A whole number brought back into the unsigned 32-bit range, as u32 arithmetic wraps.
def u32(value: int) -> int:
    return value % 2**32

# A whole number brought back into the signed 8-bit range, as i8 arithmetic wraps.
def i8(value: int) -> int:
    return (value + 2**7) % 2**8 - 2**7

# A whole number brought back into the signed 16-bit range, as i16 arithmetic wraps.
def i16(value: int) -> int:
    return (value + 2**15) % 2**16 - 2**15

# A whole number brought back into the unsigned 8-bit range, as u8 arithmetic wraps.
def u8(value: int) -> int:
    return value % 2**8

# A whole number brought back into the unsigned 16-bit range, as u16 arithmetic wraps.
def u16(value: int) -> int:
    return value % 2**16

# A whole number brought back into the unsigned 64-bit range, as u64 arithmetic wraps.
def u64(value: int) -> int:
    return value % 2**64

def mix(h: int, v: int) -> int:
    return rem(i64(i64(h * 31) + v), 1000003)

def wide() -> int:
    big: int = 9223372036854775807
    wrapped: int = i64(big + 1)
    product: int = i64(3037000500 * 3037000500)
    neg: int = i64(0 - big)
    quotient: int = div(i64(0 - 7), 2)
    rest: int = rem(i64(0 - 7), 3)
    h: int = 7
    h = mix(h, rem(wrapped >> 40, 1000))
    h = mix(h, rem(product >> 33, 1000))
    h = mix(h, rem(neg >> 50, 1000))
    h = mix(h, i64(quotient + 100))
    h = mix(h, i64(rest + 100))
    h = mix(h, i64(big << 3) >> 60)
    h = mix(h, i64(0 - 1) >> i64(5 + 100))
    return h

def thirty_two() -> int:
    big: int = 2147483647
    wrapped: int = i32(big + 1)
    product: int = i32(i64(65536 * 65536))
    other: int = i32(i64(123456789 * 987))
    quotient: int = i32(div(i64(0 - 7), 2))
    rest: int = i32(rem(i64(0 - 7), 3))
    h: int = 11
    h = mix(h, rem(wrapped, 100003))
    h = mix(h, i64(product + 5))
    h = mix(h, rem(other, 100003))
    h = mix(h, i64(quotient + 100))
    h = mix(h, i64(rest + 100))
    h = mix(h, rem(wrapped >> 7, 100003))
    h = mix(h, rem(i32(big << 5), 100003))
    return h

def unsigned32() -> int:
    top: int = 4294967295
    wrapped: int = u32(top + 2)
    product: int = u32(i64(4000000000 * 7))
    under: int = u32(i64(3 - 5))
    shifted: int = u32(top << 4)
    h: int = 13
    h = mix(h, wrapped)
    h = mix(h, rem(product, 100003))
    h = mix(h, rem(under, 100003))
    h = mix(h, rem(shifted, 100003))
    h = mix(h, rem(top >> 3, 100003))
    h = mix(h, rem(div(top, 3), 100003))
    h = mix(h, rem(top, 1000))
    return h

def narrow() -> int:
    a: int = 127
    b: int = i8(a + 1)
    c: int = 32767
    d: int = i16(c + 1)
    e: int = 250
    f: int = u8(e + 10)
    g: int = 65535
    hh: int = u16(g + 3)
    m: int = i8(i64(0 - 128))
    n: int = div(m, i8(i64(0 - 1)))
    h: int = 17
    h = mix(h, i64(b + 200))
    h = mix(h, i64(d + 40000))
    h = mix(h, f)
    h = mix(h, hh)
    h = mix(h, i64(n + 200))
    h = mix(h, i64(b >> 2 + 200))
    h = mix(h, u8(e << 3))
    return h

def casts() -> int:
    big: int = 5000000000
    cut: int = i32(big)
    tiny: int = i8(300)
    uns: int = u32(i64(0 - 5))
    up: int = u64(i64(0 - 1))
    back: int = i64(up)
    f: float = 3.99
    g: float = 0.0 - 3.99
    h: int = 19
    h = mix(h, rem(cut, 100003))
    h = mix(h, i64(tiny + 200))
    h = mix(h, rem(uns, 100003))
    h = mix(h, i64(up >> 60))
    h = mix(h, i64(back + 5))
    h = mix(h, i64(int(f)))
    h = mix(h, i64(i64(int(g)) + 10))
    h = mix(h, i64(int(f * 2.5)))
    return h

def main() -> int:
    h: int = 1
    h = mix(h, wide())
    h = mix(h, thirty_two())
    h = mix(h, unsigned32())
    h = mix(h, narrow())
    h = mix(h, casts())
    return rem(h, 251)

if __name__ == "__main__":
    raise SystemExit(main())
