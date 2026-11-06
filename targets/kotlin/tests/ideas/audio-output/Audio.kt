fun audioStreamAt(fill: (Long, MutableList<Float>, Long, Long) -> Long, state: Long, channels: Long, rate: Long, seconds: Float, volume: Float): Long {
    return audioStreamRun(streamNew(fill, state, channels, rate, seconds, volume))
}

fun audioStream(fill: (Long, MutableList<Float>, Long, Long) -> Long, state: Long, channels: Long, rate: Long, seconds: Float): Long {
    return audioStreamAt(fill, state, channels, rate, seconds, 1.0f)
}

fun soundPlay(s: Sound): Long {
    return audioStreamRun(soundStream(s, 1.0f))
}

fun audioPlay(samples: MutableList<Float>, count: Long, rate: Long): Long {
    return soundPlay(Sound(samples, count, rate, 1L))
}
