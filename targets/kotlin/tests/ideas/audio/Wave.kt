import kotlin.math.*

private fun sinf(x: Float): Float = sin(x)
private fun fabsf(x: Float): Float = abs(x)
private fun powf(x: Float, y: Float): Float = x.pow(y)

fun sine(phase: Float): Float {
    return sinf((6.2831853 * (fract(phase).toDouble())).toFloat())
}

fun square(phase: Float): Float {
    if (fract(phase) < 0.5f) {
        return 1.0f
    }
    return (0.0 - 1.0).toFloat()
}

fun sawtooth(phase: Float): Float {
    return (2.0 * (fract(phase).toDouble()) - 1.0).toFloat()
}

fun triangle(phase: Float): Float {
    return (1.0 - 4.0 * (fabsf(fract(phase) - 0.5f).toDouble())).toFloat()
}

fun noise(seed: Int): Float {
    val a: Int = (seed * 75 + 74) % 65537
    val b: Int = (a * 75 + 74) % 65537
    return (2.0 * ((b.toFloat()).toDouble()) / 65537.0 - 1.0).toFloat()
}

fun fade(t: Float, attack: Float, release: Float, total: Float): Float {
    var level: Float = if (t < attack) (t / attack) else 1.0f
    if (total - t < release) {
        level = level * clamp((total - t) / release, 0.0f, 1.0f)
    }
    return level
}

fun noteFrequency(m: Float): Float {
    return (440.0 * (powf(2.0f, (m - 69.0f) / 12.0f).toDouble())).toFloat()
}
