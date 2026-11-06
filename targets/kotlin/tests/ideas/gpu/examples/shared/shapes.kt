import kotlin.math.*

private fun sinf(x: Float): Float = sin(x)

fun atomicAdd(counts: MutableList<Int>, slot: Int, amount: Int): Int {
    val old: Int = counts[slot]
    counts[slot] = old + amount
    return old
}

fun atomicAddFloat(sums: MutableList<Float>, slot: Int, amount: Float): Float {
    val old: Float = sums[slot]
    sums[slot] = old + amount
    return old
}

fun atomicMin(values: MutableList<Int>, slot: Int, candidate: Int): Int {
    val old: Int = values[slot]
    if (candidate < old) {
        values[slot] = candidate
    }
    return old
}

fun atomicMax(values: MutableList<Int>, slot: Int, candidate: Int): Int {
    val old: Int = values[slot]
    if (candidate > old) {
        values[slot] = candidate
    }
    return old
}

fun blend7(a: Float, b: Float, c: Float, d: Float, e: Float, f: Float, g: Float, k: Float): Float {
    return k * (a + ((2.0 * (b.toDouble())).toFloat()) + ((3.0 * (c.toDouble())).toFloat())) + (d - e) * (f + g)
}

fun plasma(x: Int, y: Int, width: Int, height: Int, phase: Float): Float {
    val u: Float = (x.toFloat()) / (width.toFloat())
    val v: Float = (y.toFloat()) / (height.toFloat())
    val a: Float = sinf((10.0 * (u.toDouble()) + (phase.toDouble())).toFloat())
    val b: Float = sinf((8.0 * (v.toDouble()) - (phase.toDouble())).toFloat())
    val c: Float = sinf((6.0 * ((u + v).toDouble()) + 2.0 * (phase.toDouble())).toFloat())
    return (0.5 + 0.1667 * ((a + b + c).toDouble())).toFloat()
}

fun vignette(pixel: Float, x: Int, y: Int, width: Int, height: Int): Float {
    val dx: Float = (x.toFloat()) / (width.toFloat()) - 0.5f
    val dy: Float = (y.toFloat()) / (height.toFloat()) - 0.5f
    val edge: Float = (1.0 - 1.6 * ((dx * dx + dy * dy).toDouble())).toFloat()
    return pixel * edge
}

fun tally(i: Int, counts: MutableList<Int>, sums: MutableList<Float>, lows: MutableList<Int>, highs: MutableList<Int>): Int {
    val value: Int = i * 7919 % 10007
    atomicAdd(counts, i % 4, 1)
    atomicAddFloat(sums, i % 4, 0.5f)
    atomicMin(lows, 0, value)
    atomicMax(highs, 0, value)
    return 0
}

fun reported(i: Int): Int {
    return i * 7919 % 10007
}

fun nearly(a: Float, b: Float, tolerance: Float): Long {
    var gap: Float = a - b
    if (gap < 0.0f) {
        gap = (0.0 - (gap.toDouble())).toFloat()
    }
    if (gap <= tolerance) {
        return 1L
    }
    return 0L
}
