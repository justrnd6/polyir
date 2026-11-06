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

fun checkSign(): Double {
    return off(sign((0.0 - 3.0).toFloat()), (0.0 - 1.0).toFloat()) + off(sign(0.0f), 0.0f) + off(sign(2.0f), 1.0f)
}

fun checkFract(): Double {
    return off(fract(2.75f), 0.75f) + off(fract((0.0 - 0.25).toFloat()), 0.75f)
}

fun checkClamp(): Double {
    return off(clamp(5.0f, (0.0 - 1.0).toFloat(), 2.0f), 2.0f) + off(clamp((0.0 - 5.0).toFloat(), (0.0 - 1.0).toFloat(), 2.0f), (0.0 - 1.0).toFloat())
}

fun checkMix(): Double {
    return off(mix(10.0f, 20.0f, 0.25f), 12.5f)
}

fun checkNestedCalls(): Double {
    return off(clamp(fract(1.75f) * 2.0f, (0.0 - 1.0).toFloat(), 1.0f), 1.0f) + off(mix(sign(2.0f), fract(0.5f), 0.5f), 0.75f)
}

fun rescale(x: Float): Float {
    return x * 3.0f
}

fun checkOwnFunctionsStillWork(): Double {
    return off(rescale(fract(1.25f)), 0.75f)
}
