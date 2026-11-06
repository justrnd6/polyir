import kotlin.math.*

fun off(got: Float, expected: Float): Double {
    return (got - expected).toDouble()
}

fun offInt(got: Long, expected: Long): Double {
    return (got - expected).toDouble()
}

fun offRelative(got: Float, expected: Float): Double {
    return ((got - expected) / expected).toDouble()
}

fun amplitudeAt(data: MutableList<Float>, start: Long, count: Long, rate: Long, hz: Float): Float {
    var re: Double = 0.0
    var im: Double = 0.0
    run {
        var k: Long = 0L
        while (k < count) {
            val angle: Double = 6.283185307179586 * (hz.toDouble()) * ((start + k).toDouble()) / (rate.toDouble())
            re = re + (data[(start + k).toInt()].toDouble()) * cos(angle)
            im = im + (data[(start + k).toInt()].toDouble()) * sin(angle)
            k = k + 1L
        }
    }
    return (2.0 * sqrt(re * re + im * im) / (count.toDouble())).toFloat()
}
