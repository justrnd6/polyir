fun concertAHeight(): Long {
    val quarterCycle: Float = (0.25 / (noteFrequency(69.0f).toDouble())).toFloat()
    return (sine(noteFrequency(69.0f) * quarterCycle) * 1000.0f).toLong()
}

fun softAttack(): Long {
    return (fade(0.5f, 1.0f, 1.0f, 4.0f) * 1000.0f).toLong()
}

fun chordSample(t: Float): Float {
    return (0.3 * ((sine(noteFrequency(60.0f) * t) + sine(noteFrequency(64.0f) * t) + sine(noteFrequency(67.0f) * t)).toDouble())).toFloat()
}
