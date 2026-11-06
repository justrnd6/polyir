import kotlin.math.*

private fun floorf(x: Float): Float = floor(x)
private fun sinf(x: Float): Float = sin(x)

fun synth(i: Int, p: MutableList<Float>): Float {
    val t: Float = (i.toFloat()) / p[0]
    val envelope: Float = fade(t, p[2], p[3], p[4])
    val wobble: Float = p[6] * sine(p[5] * t)
    var sum: Float = 0.0f
    run {
        var k: Long = 0L
        while (k < (p[1].toInt()).toLong()) {
            val cycles: Float = p[(8L + 2L * k).toInt()] * t
            sum = sum + p[(9L + 2L * k).toInt()] * sinf((6.2831853 * ((cycles - floorf(cycles)).toDouble()) + (wobble.toDouble())).toFloat())
            k = k + 1L
        }
    }
    return p[7] * envelope * sum
}

fun squareWave(i: Int, p: MutableList<Float>): Float {
    return sign(sine(p[1] * (i.toFloat()) / p[0]))
}

fun partialSum(i: Int, p: MutableList<Float>): Float {
    val x: Float = (6.2831853 * (((i % (p[0].toInt())).toFloat()).toDouble()) / (p[0].toDouble())).toFloat()
    var sum: Float = 0.0f
    run {
        var h: Long = 1L
        while (h <= (p[2].toInt()).toLong()) {
            val number: Float = h.toFloat()
            val wave: Float = sinf(number * x)
            if (p[1] < 0.5f) {
                if (h % 2L == 1L) {
                    sum = sum + wave / number
                }
            }
            if (p[1] > 0.5f && p[1] < 1.5f) {
                if (h % 2L == 1L) {
                    sum = sum + wave / number
                }
                if (h % 2L == 0L) {
                    sum = sum - wave / number
                }
            }
            if (p[1] > 1.5f) {
                if (h % 4L == 1L) {
                    sum = sum + wave / (number * number)
                }
                if (h % 4L == 3L) {
                    sum = sum - wave / (number * number)
                }
            }
            h = h + 1L
        }
    }
    if (p[1] < 0.5f) {
        return (1.2732395 * (sum.toDouble())).toFloat()
    }
    if (p[1] < 1.5f) {
        return (0.6366198 * (sum.toDouble())).toFloat()
    }
    return (0.8105695 * (sum.toDouble())).toFloat()
}

fun melody(i: Int, notes: MutableList<Float>, p: MutableList<Float>): Float {
    val t: Float = (i.toFloat()) / p[0]
    var sum: Float = 0.0f
    run {
        var n: Long = 0L
        while (n < (p[1].toInt()).toLong()) {
            val start: Float = notes[(3L * n).toInt()]
            val length: Float = notes[(3L * n + 1L).toInt()]
            val local: Float = t - start
            if (local >= 0.0f && local < length) {
                val envelope: Float = fade(local, 0.02f, 0.08f, length)
                val f: Float = notes[(3L * n + 2L).toInt()]
                sum = sum + envelope * (sine(f * t) + ((0.5 * (sine((2.0 * (f.toDouble()) * (t.toDouble())).toFloat()).toDouble())).toFloat()) + ((0.25 * (sine((3.0 * (f.toDouble()) * (t.toDouble())).toFloat()).toDouble())).toFloat()))
            }
            n = n + 1L
        }
    }
    return p[2] * sum
}
