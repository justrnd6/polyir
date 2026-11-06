import kotlin.math.*

private fun sqrtf(x: Float): Float = sqrt(x)

fun scale(x: Float, k: Float): Float {
    return x * k
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
