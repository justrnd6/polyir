import kotlin.math.*

private fun floorf(x: Float): Float = floor(x)

fun sign(x: Float): Float {
    if (x > 0.0f) {
        return 1.0f
    }
    if (x < 0.0f) {
        return (0.0 - 1.0).toFloat()
    }
    return 0.0f
}

fun fract(x: Float): Float {
    return x - floorf(x)
}

fun clamp(x: Float, low: Float, high: Float): Float {
    if (x < low) {
        return low
    }
    if (x > high) {
        return high
    }
    return x
}

fun mix(a: Float, b: Float, t: Float): Float {
    return a + (b - a) * t
}
