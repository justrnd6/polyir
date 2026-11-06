#!/usr/bin/env python3
"""Writes the tiny WAV files the audio tests read (ideas/audio/tests/fixtures/*.wav).

Each file is built byte by byte with the `struct` module (not by polyir's own writer), so the
polyir reader is checked against an independent description of the format. The sample values are
listed in ideas/audio/tests/WavTests.poly next to the checks that read them back.
Run:  python3 make-fixtures.py
"""
import struct

def chunk(name, payload):
    pad = b"\0" if len(payload) % 2 else b""
    return name + struct.pack("<I", len(payload)) + payload + pad

def riff(chunks):
    body = b"WAVE" + b"".join(chunks)
    return b"RIFF" + struct.pack("<I", len(body)) + body

def fmt(tag, channels, rate, bits):
    block = channels * bits // 8
    return chunk(b"fmt ", struct.pack("<HHIIHH", tag, channels, rate, rate * block, block, bits))

def write(name, data):
    open(name, "wb").write(data)

# 8-bit samples are UNSIGNED: 128 is silence.
write("pcm8-mono.wav", riff([fmt(1, 1, 8000, 8), chunk(b"data", bytes([0, 64, 128, 192, 255]))]))

# 16-bit stereo, with an unknown LIST chunk (odd length, so it is padded) between "fmt " and "data".
frames16 = [(0, 0), (16384, -16384), (32767, -32768), (-8192, 8192)]
data16 = b"".join(struct.pack("<hh", l, r) for l, r in frames16)
write("pcm16-stereo.wav", riff([fmt(1, 2, 44100, 16), chunk(b"LIST", b"INFOabc"), chunk(b"data", data16)]))

# 24-bit mono: three bytes per sample, low byte first.
def s24(v):
    return struct.pack("<i", v)[:3]
write("pcm24-mono.wav", riff([fmt(1, 1, 22050, 24), chunk(b"data", b"".join(s24(v) for v in [4194304, -4194304, 8388607, -8388608]))]))

write("pcm32-mono.wav", riff([fmt(1, 1, 48000, 32), chunk(b"data", struct.pack("<4i", 1073741824, -1073741824, 2147483647, -2147483648))]))

write("float32-stereo.wav", riff([fmt(3, 2, 32000, 32), chunk(b"data", struct.pack("<6f", 0.25, -0.75, 1.0, -1.0, 0.125, 0.0))]))

write("float64-mono.wav", riff([fmt(3, 1, 16000, 64), chunk(b"data", struct.pack("<3d", 0.5, -0.5, 0.125))]))

# WAVE_FORMAT_EXTENSIBLE: tag 0xFFFE, a 22-byte extension, and the real format (PCM = 1) inside it.
ext = struct.pack("<HHIIHH", 0xFFFE, 2, 44100, 44100 * 4, 4, 16) + struct.pack("<HHI", 22, 16, 3)
ext += struct.pack("<H", 1) + bytes.fromhex("000000001000800000aa00389b71")
write("extensible16.wav", riff([chunk(b"fmt ", ext), chunk(b"data", struct.pack("<4h", 1000, -1000, 2000, -2000))]))

# A recording cut short: the data chunk claims 1000 bytes but only 8 are present.
body = b"WAVE" + fmt(1, 1, 8000, 16) + b"data" + struct.pack("<I", 1000) + struct.pack("<4h", 100, 200, 300, 400)
write("truncated.wav", b"RIFF" + struct.pack("<I", len(body)) + body)

# Not a WAV file at all.
write("not-audio.wav", b"this is just some text, not a RIFF file at all")
