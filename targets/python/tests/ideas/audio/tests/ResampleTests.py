from __future__ import annotations
import struct

from Check import amplitude_at, off, off_int
from Resample import resampled_count
from Sound import Sound, sound_new, sound_stereo

# A float rounded to 32 bits, as a `float` (polyir's f32) holds it.
def f32(value: float) -> float:
    return struct.unpack("f", struct.pack("f", value))[0]

# A whole number brought back into the signed 64-bit range, as i64 arithmetic wraps.
def i64(value: int) -> int:
    return (value + 2**63) % 2**64 - 2**63

# Whole-number division and remainder that truncate toward zero, as polyir's do (Python's // and % round down).
def div(a: int, b: int) -> int:
    quotient = abs(a) // abs(b)
    return quotient if (a < 0) == (b < 0) else -quotient

def rem(a: int, b: int) -> int:
    return a - b * div(a, b)

from math import sin

def sine_sound(hz: float, seconds: float, rate: int) -> Sound:
    s: Sound = sound_new(seconds, rate)
    i: int = 0
    while i < s.count:
        s.samples[i] = f32(sin(6.283185307179586 * hz * float(i) / float(rate)))
        i = i64(i + 1)
    return s

def check_count_going_up() -> float:
    return off_int(resampled_count(44100, 44100, 48000), 48000)

def check_count_going_down() -> float:
    return off_int(resampled_count(1000, 8000, 4000), 500)

def check_count_rounds() -> float:
    return off_int(resampled_count(10, 3, 4), 13)

def check_sound_keeps_its_length() -> float:
    s: Sound = sine_sound(440.0, 0.5, 8000).resample(12000)
    return off_int(s.rate, 12000) + off_int(s.count, 6000) + off_int(s.channels, 1) + off(s.seconds(), 0.5)

def check_upsampling_follows_the_wave() -> float:
    s: Sound = sine_sound(440.0, 1.0, 8000).resample(12000)
    worst: float = 0.0
    j: int = 1000
    while j < 11000:
        expected: float = sin(6.283185307179586 * 440.0 * float(j) / 12000.0)
        gap: float = s.samples[j] - expected
        if gap < 0.0:
            gap = 0.0 - gap
        if gap > worst:
            worst = gap
        j = i64(j + 7)
    return worst

def check_downsampling_removes_aliases() -> float:
    s: Sound = sine_sound(3000.0, 1.0, 8000).resample(4000)
    return amplitude_at(s.samples, 500, 2000, 4000, 1000.0)

def check_downsampling_keeps_low_tones() -> float:
    s: Sound = sine_sound(1000.0, 1.0, 8000).resample(4000)
    return off(amplitude_at(s.samples, 500, 2000, 4000, 1000.0), 1.0)

def check_round_trip() -> float:
    original: Sound = sine_sound(300.0, 1.0, 8000)
    back: Sound = original.resample(12000).resample(8000)
    worst: float = 0.0
    j: int = 1000
    while j < 7000:
        gap: float = f32(back.samples[j] - original.samples[j])
        if gap < 0.0:
            gap = 0.0 - gap
        if gap > worst:
            worst = gap
        j = i64(j + 11)
    return worst

def check_constant_stays_constant() -> float:
    s: Sound = sound_new(0.5, 8000)
    i: int = 0
    while i < s.count:
        s.samples[i] = 1.0
        i = i64(i + 1)
    up: Sound = s.resample(22050)
    down: Sound = s.resample(3000)
    return off(up.samples[div(up.count, 2)], 1.0) + off(down.samples[div(down.count, 2)], 1.0)

def check_stereo_resampling() -> float:
    left: Sound = sine_sound(440.0, 0.5, 8000)
    right: Sound = sine_sound(880.0, 0.5, 8000)
    out: Sound = sound_stereo(left, right).resample(16000)
    return off_int(out.channels, 2) + off_int(out.count, 8000) + off(amplitude_at(out.channel(0).samples, 2000, 4000, 16000, 440.0), 1.0) + off(amplitude_at(out.channel(1).samples, 2000, 4000, 16000, 880.0), 1.0)

def check_same_rate_is_a_copy() -> float:
    s: Sound = sine_sound(440.0, 0.1, 8000)
    copy_: Sound = s.resample(8000)
    worst: float = 0.0
    i: int = 100
    while i < 700:
        gap: float = f32(copy_.samples[i] - s.samples[i])
        if gap < 0.0:
            gap = 0.0 - gap
        if gap > worst:
            worst = gap
        i = i64(i + 1)
    return worst + off_int(copy_.count, s.count)
