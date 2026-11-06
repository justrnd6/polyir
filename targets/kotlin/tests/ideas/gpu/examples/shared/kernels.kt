import kotlin.math.*

private fun sqrtf(x: Float): Float = sqrt(x)

fun scale(x: Float, k: Float): Float {
    return x * k + 1.0f
}

fun norm(a: Float, b: Float): Float {
    val s: Float = scale(a, 2.0f) + b * b
    return sqrtf(s)
}

fun sumTo(n: Int): Int {
    var acc: Int = 0
    run {
        var i: Long = 0L
        while (i < n.toLong()) {
            if (i % 2L == 0L) {
                acc = acc + (i.toInt())
            }
            i = i + 1L
        }
    }
    return acc
}

fun lane(v: MutableList<Float>): Float {
    return v[0] + v[3]
}

fun smooth(i: Int, src: MutableList<Float>, n: Int): Float {
    if (i == 0) {
        return src[i]
    }
    if (i == n - 1) {
        return src[i]
    }
    return (src[i - 1] + src[i] + src[i + 1]) / 3.0f
}

fun leftover(a: Float, b: Float): Float {
    return a % b
}

fun close(a: Float, b: Float): Long {
    var diff: Float = a - b
    if (diff < 0.0f) {
        diff = (0.0 - (diff.toDouble())).toFloat()
    }
    var size: Float = b
    if (size < 0.0f) {
        size = (0.0 - (size.toDouble())).toFloat()
    }
    if (diff <= (0.000001 * ((size + 1.0f).toDouble())).toFloat()) {
        return 1L
    }
    return 0L
}
