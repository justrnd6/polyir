import struct

from Math import clamp, fract, mix, sign
from Check import off, off_int
from Wave import fade, noise, note_frequency, sawtooth, sine, square, triangle

# A float rounded to 32 bits, as a `float` (polyir's f32) holds it.
def f32(value: float) -> float:
    return struct.unpack("f", struct.pack("f", value))[0]

# A whole number brought back into the signed 32-bit range, as i32 arithmetic wraps.
def i32(value: int) -> int:
    return (value + 2**31) % 2**32 - 2**31

# A whole number brought back into the signed 64-bit range, as i64 arithmetic wraps.
def i64(value: int) -> int:
    return (value + 2**63) % 2**64 - 2**63

def check_sign_of_negative() -> float:
    return off(sign(f32(0.0 - 3.0)), f32(0.0 - 1.0))

def check_sign_of_zero() -> float:
    return off(sign(0.0), 0.0)

def check_sign_of_positive() -> float:
    return off(sign(2.0), 1.0)

def check_fract_of_positive() -> float:
    return off(fract(2.75), 0.75)

def check_fract_of_negative() -> float:
    return off(fract(f32(0.0 - 0.25)), 0.75)

def check_clamp_above() -> float:
    return off(clamp(5.0, f32(0.0 - 1.0), 2.0), 2.0)

def check_clamp_below() -> float:
    return off(clamp(f32(0.0 - 5.0), f32(0.0 - 1.0), 2.0), f32(0.0 - 1.0))

def check_mix() -> float:
    return off(mix(10.0, 20.0, 0.25), 12.5)

def check_sine_peak() -> float:
    return off(sine(0.25), 1.0)

def check_sine_trough() -> float:
    return off(sine(0.75), f32(0.0 - 1.0))

def check_sine_zero_crossing() -> float:
    return off(sine(1.5), 0.0)

def check_square_high() -> float:
    return off(square(0.25), 1.0)

def check_square_low() -> float:
    return off(square(0.75), f32(0.0 - 1.0))

def check_sawtooth_start() -> float:
    return off(sawtooth(0.0), f32(0.0 - 1.0))

def check_sawtooth_three_quarters() -> float:
    return off(sawtooth(0.75), 0.5)

def check_triangle_start() -> float:
    return off(triangle(0.0), f32(0.0 - 1.0))

def check_triangle_peak() -> float:
    return off(triangle(0.5), 1.0)

def check_triangle_middle() -> float:
    return off(triangle(0.25), 0.0)

def check_concert_a() -> float:
    return off(note_frequency(69.0), 440.0)

def check_octave_above_concert_a() -> float:
    return off(note_frequency(81.0), 880.0)

def check_fade_in_halfway() -> float:
    return off(fade(0.5, 1.0, 1.0, 4.0), 0.5)

def check_fade_holds() -> float:
    return off(fade(2.0, 1.0, 1.0, 4.0), 1.0)

def check_fade_out_halfway() -> float:
    return off(fade(3.5, 1.0, 1.0, 4.0), 0.5)

def check_noise_range() -> float:
    outside: int = 0
    seed: int = 0
    while seed < 1000:
        if noise(i32(seed)) > 1.0 or noise(i32(seed)) < f32(0.0 - 1.0):
            outside = i64(outside + 1)
        seed = i64(seed + 1)
    return off_int(outside, 0)

def check_noise_repeats() -> float:
    return off(f32(noise(42) - noise(42)), 0.0)

def check_noise_varies() -> float:
    if noise(1) == noise(2) and noise(2) == noise(3):
        return 1.0
    return 0.0
