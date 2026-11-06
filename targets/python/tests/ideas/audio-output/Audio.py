from __future__ import annotations
from collections.abc import Callable

from Stream import Stream, sound_stream, stream_new
from macos import audio_stream_run
from Sound import Sound

def audio_stream_at(fill: list[Callable[[int, list[float], int, int], int]], state: int, channels: int, rate: int, seconds: float, volume: float) -> int:
    return audio_stream_run(stream_new(fill, state, channels, rate, seconds, volume))

def audio_stream(fill: list[Callable[[int, list[float], int, int], int]], state: int, channels: int, rate: int, seconds: float) -> int:
    return audio_stream_at(fill, state, channels, rate, seconds, 1.0)

def sound_play(s: Sound) -> int:
    return audio_stream_run(sound_stream(s, 1.0))

def audio_play(samples: list[float], count: int, rate: int) -> int:
    return sound_play(Sound(samples, count, rate, 1))
