from __future__ import annotations
import struct

from Check import off, off_int
from Sound import Sound, sound_new
from Wave import sine

# A float rounded to 32 bits, as a `float` (polyir's f32) holds it.
def f32(value: float) -> float:
    return struct.unpack("f", struct.pack("f", value))[0]

# A whole number brought back into the signed 64-bit range, as i64 arithmetic wraps.
def i64(value: int) -> int:
    return (value + 2**63) % 2**64 - 2**63

def quiet_tone() -> Sound:
    s: Sound = sound_new(0.1, 1000)
    s.tone(0, 125.0, 0.5, 0.0, 0.1)
    return s

def check_new_sound_is_mono() -> float:
    return off_int(sound_new(1.0, 8000).channels, 1)

def check_new_sound_length() -> float:
    return off_int(sound_new(2.5, 8000).count, 20000)

def check_seconds() -> float:
    return off(sound_new(2.5, 8000).seconds(), 2.5)

def check_tone_peak() -> float:
    return off(quiet_tone().peak(), 0.5)

def check_square_tone_peak() -> float:
    s: Sound = sound_new(0.1, 1000)
    s.tone(1, 125.0, 0.4, 0.0, 0.1)
    return off(s.peak(), 0.4)

def check_scale() -> float:
    s: Sound = quiet_tone()
    s.scale(0.5)
    return off(s.peak(), 0.25)

def check_mix_adds() -> float:
    a: Sound = quiet_tone()
    a.add(quiet_tone(), 0.0, 1.0)
    return off(a.peak(), 1.0)

def check_normalize() -> float:
    s: Sound = sound_new(0.1, 1000)
    s.tone(0, 125.0, 0.3, 0.0, 0.1)
    s.normalize(0.9)
    return off(s.peak(), 0.9)

def check_delayed_mix_keeps_length() -> float:
    a: Sound = sound_new(1.0, 1000)
    a.add(quiet_tone(), 0.5, 1.0)
    return off_int(a.count, 1000)

def check_delayed_mix_is_silent_before() -> float:
    a: Sound = sound_new(1.0, 1000)
    a.add(quiet_tone(), 0.5, 1.0)
    return off(a.samples[499], 0.0)

def check_delayed_mix_starts_later() -> float:
    a: Sound = sound_new(1.0, 1000)
    a.add(quiet_tone(), 0.5, 1.0)
    return off(a.samples[525], 0.35355339)

def check_tone_fades_in() -> float:
    s: Sound = sound_new(0.5, 8000)
    s.tone(0, 440.0, 1.0, 0.0, 0.5)
    return off(s.samples[0], 0.0)

def check_noise_amplitude() -> float:
    s: Sound = sound_new(0.1, 8000)
    s.noise(0.25, 7)
    loudest: float = s.peak()
    if loudest > 0.25:
        return 1.0
    if loudest < 0.2:
        return 1.0
    return 0.0

def filter_gain(kind: int, hz: float, cutoff: float) -> float:
    s: Sound = sound_new(1.0, 8000)
    data: list[float] = s.samples
    i: int = 0
    while i < 8000:
        data[i] = sine(f32(f32(hz * f32(i)) / 8000.0))
        i = i64(i + 1)
    if kind == 0:
        s.low_pass(cutoff)
    else:
        s.high_pass(cutoff)
    loudest: float = 0.0
    i: int = 6000
    while i < 8000:
        level: float = data[i]
        if level < 0.0:
            level = f32(0.0 - level)
        if level > loudest:
            loudest = level
        i = i64(i + 1)
    return loudest

def check_low_pass_cuts_high_tone() -> float:
    return off(filter_gain(0, 1000.0, 500.0), 0.3987)

def check_low_pass_keeps_low_tone() -> float:
    return off(filter_gain(0, 200.0, 500.0), 0.9031)

def check_high_pass_keeps_high_tone() -> float:
    return off(filter_gain(1, 1000.0, 500.0), 0.7225)

def check_high_pass_cuts_low_tone() -> float:
    return off(filter_gain(1, 200.0, 500.0), 0.3613)

def check_echo_first_repeat() -> float:
    s: Sound = sound_new(0.5, 1000)
    s.samples[0] = 1.0
    s.echo(0.1, 0.5)
    return off(s.samples[100], 0.5)

def check_echo_second_repeat() -> float:
    s: Sound = sound_new(0.5, 1000)
    s.samples[0] = 1.0
    s.echo(0.1, 0.5)
    return off(s.samples[200], 0.25)

def convolved() -> Sound:
    a: Sound = sound_new(0.002, 1000)
    r: Sound = sound_new(0.003, 1000)
    a.samples[0] = 1.0
    a.samples[1] = 2.0
    r.samples[0] = 1.0
    r.samples[2] = 3.0
    return a.convolve(r)

def check_convolution_length() -> float:
    return off_int(convolved().count, 4)

def check_convolution_values() -> float:
    y: list[float] = convolved().samples
    return off(y[0], 1.0) + off(y[1], 2.0) + off(y[2], 3.0) + off(y[3], 6.0)

def reverb_of_impulse() -> Sound:
    s: Sound = sound_new(1.0, 10000)
    s.samples[0] = 1.0
    s.reverb(1.0)
    return s

def check_reverb_start() -> float:
    return off(reverb_of_impulse().samples[0], 0.20525)

def check_reverb_first_echo() -> float:
    return off(reverb_of_impulse().samples[297], 0.03924)
