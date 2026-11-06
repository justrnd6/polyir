import struct

from Wave import fade, note_frequency, sine

# A float rounded to 32 bits, as a `float` (polyir's f32) holds it.
def f32(value: float) -> float:
    return struct.unpack("f", struct.pack("f", value))[0]

# A whole number brought back into the signed 64-bit range, as i64 arithmetic wraps.
def i64(value: int) -> int:
    return (value + 2**63) % 2**64 - 2**63

def concert_a_height() -> int:
    quarter_cycle: float = f32(0.25 / note_frequency(69.0))
    return i64(int(f32(sine(f32(note_frequency(69.0) * quarter_cycle)) * 1000.0)))

def soft_attack() -> int:
    return i64(int(f32(fade(0.5, 1.0, 1.0, 4.0) * 1000.0)))

def chord_sample(t: float) -> float:
    return f32(0.3 * f32(f32(sine(f32(note_frequency(60.0) * t)) + sine(f32(note_frequency(64.0) * t))) + sine(f32(note_frequency(67.0) * t))))
